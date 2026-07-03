#pragma once

#include "config.hpp"
#include "resources.hpp"
#include "fsm.hpp"

#include <atomic>

struct Arm {
  ArmResources resources;

  // 状态机
  fsm_arm::Arm fsm{resources};
  fsm_arm::state::Init state_init;
  fsm_arm::state::NoForce state_no_force;
  fsm_arm::state::FreeMove state_free_move;
  fsm_arm::state::Manual state_manual;
  fsm_arm::state::Idle state_idle;
  fsm_arm::state::Executing state_host_control;
  fsm_arm::state::Sequencing state_sequencing;
  fsm_arm::state::Fault state_fault;

 private:
  // 这个数组需与fsm同生命周期，所以放在这里
  etl::ifsm_state *state_list_[8]  //
      {&state_init,                //
       &state_no_force,            //
       &state_free_move,           //
       &state_manual,              //
       &state_idle,                //
       &state_host_control,        //
       &state_sequencing,          //
       &state_fault};

  std::atomic<bool> enable_cmd_pending_{false};
  std::atomic<bool> mode_ctrl_cmd_pending_{false};
  std::atomic<bool> joint_ctrl_cmd_pending_{false};
  std::atomic<bool> mit_ctrl_cmd_pending_{false};

  void ProcessDeferredEvents() {
    auto &shared = SharedResources::GetInstance();
    if (enable_cmd_pending_.exchange(false, std::memory_order_acquire)) {
      fsm.receive(fsm_arm::event::EnableCommand{shared.host.rx_data().enable_command});
    }
    if (mode_ctrl_cmd_pending_.exchange(false, std::memory_order_acquire)) {
      fsm.receive(fsm_arm::event::ModeCtrlCommand{shared.host.rx_data().mode_ctrl_command});
    }
    if (joint_ctrl_cmd_pending_.exchange(false, std::memory_order_acquire)) {
      fsm.receive(fsm_arm::event::JointCtrlCommand{shared.host.rx_data().joint_ctrl_command});
    }
    if (mit_ctrl_cmd_pending_.exchange(false, std::memory_order_acquire)) {
      fsm.receive(fsm_arm::event::MitCtrlCommand{shared.host.rx_data().mit_ctrl_command});
    }
  }

 public:
  explicit Arm(const ArmConfig &config) : resources{config} {}

  /**
   * @brief 初始化（设备命名、回调注册）
   */
  void Init() {
    // // 设零点
    // HAL_Delay(1500);  // 达妙电机开机需要时间
    // resources.actuator.DmSetZero();

    auto &shared = SharedResources::GetInstance();

    // 定义遥控器拨杆功能
    shared.rc(resources.config.rc_ch_mode_switch).OnValueChange([&](const int &old_value, const int &new_value) {
      // 353-up, 1024-mid, 1695-down
      if (old_value == 0) {
        return;  // 第一次触发不响应
      }
      if (new_value > 1300 && new_value < 1700) {  // 机械臂模式杆在下 - 无力
        fsm.receive(fsm_arm::event::FaultClear{});
        fsm.receive(fsm_arm::event::ForceModeSwitch{fsm_arm::StateId::kNoForce});
      }
      if (new_value > 0 && new_value < 500) {  // 机械臂模式杆在上 - 自动模式，idle
        fsm.receive(fsm_arm::event::ForceModeSwitch{fsm_arm::StateId::kIdle});
      }
      if (new_value > 500 && new_value < 1100) {  // 机械臂模式杆在中 - 手动调试模式
        fsm.receive(fsm_arm::event::ForceModeSwitch{fsm_arm::StateId::kManual});
      }
    });
    shared.rc(resources.config.rc_ch_next_joint).OnValueChange([&](const int &old_value, const int &new_value) {
      UNUSED(old_value);
      if (new_value > 1000) {  // SH按下
        fsm.receive(fsm_arm::event::NextJoint{});
      }
    });

    shared.host.OnEnableCommand(
        [&] { enable_cmd_pending_.store(true, std::memory_order_release); });
    shared.host.OnModeCtrlCommand(
        [&] { mode_ctrl_cmd_pending_.store(true, std::memory_order_release); });
    shared.host.OnJointCtrlCommand(
        [&] { joint_ctrl_cmd_pending_.store(true, std::memory_order_release); });
    shared.host.OnMitCtrlCommand(
        [&] { mit_ctrl_cmd_pending_.store(true, std::memory_order_release); });

    fsm.set_states(state_list_, std::size(state_list_));
    fsm.start();
  }

  /**
   * @brief 500 Hz 控制循环节拍（IMU 数据从 SharedResources 内部读取）
   * @note  调用前须已调用 SharedResources::imu.Update()
   */
  void Tick() {
    resources.device_manager.Update();
    ProcessDeferredEvents();
    fsm.receive(fsm_arm::event::ControlLoop{});
  }

  static ActionSequence GenerateResetSequence(const std::array<float, 5> &current_pos) {
    if (current_pos[1] > (130_deg).rad()) {
      return GenerateLowResetSequence(current_pos);
    } else {
      return GenerateHiResetSequence(current_pos);
    }
  }

  static ActionSequence GenerateLowResetSequence(const std::array<float, 5> &current_pos) {
    using namespace rm::modules::angle_literals;
    const auto rise_j2 = [&] {
      std::array<float, 5> p{current_pos};
      p[1] = (180_deg).rad() - std::atan2f(84.5f, 415.f) - (15_deg).rad();
      return p;
    }();
    const auto rise_j3 = [&] {
      auto p{rise_j2};
      p[2] = -(180_deg).rad() + std::atan2f(84.5f, 415.f) + (35_deg).rad();
      return p;
    }();
    const auto center_remaining = [&] {
      std::array<float, 5> p;
      p.fill(0.f);
      p[1] = rise_j2[1];
      p[2] = rise_j3[2];
      return p;
    }();
    ActionSequence seq;
    seq.GoTo(rise_j2)   //
        .GoTo(rise_j3)  //
        .GoTo(center_remaining);
    return seq;
  }

  static ActionSequence GenerateHiResetSequence(const std::array<float, 5> &current_pos) {
    using namespace rm::modules::angle_literals;
    const auto rise_j3_center_j1 = [&] {
      auto p{current_pos};
      p[0] = 0.f;
      p[2] += (35_deg).rad();
      return p;
    }();
    const auto retract_j2j3_center_remaining = [&] {
      std::array<float, 5> p;
      p.fill(0.f);
      p[1] = (180_deg).rad() - std::atan2f(84.5f, 415.f) - (15_deg).rad();
      p[2] = -(180_deg).rad() + std::atan2f(84.5f, 415.f) + (35_deg).rad();
      return p;
    }();
    ActionSequence seq;
    seq.GoTo(rise_j3_center_j1)  //
        .GoTo(retract_j2j3_center_remaining);
    return seq;
  }
};