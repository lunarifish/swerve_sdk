
#pragma once

#include <librm.hpp>
#include <utility>
#include <etl/fsm.h>
#include <etl/debounce.h>

#include "kinematics.hpp"
#include "resources.hpp"
#include "shared_resources.hpp"
#include "action_sequencer.hpp"
#include "host/schema.hpp"

namespace fsm_arm {

struct EventId {
  enum {
    kControlLoop,      ///< 控制循环时钟事件
    kForceModeSwitch,  ///< 切换模式命令
    kFaultClear,       ///< 故障清除命令
    kNextJoint,        ///< Manual模式下切换到下一个关节
    kActionSequence,   ///< 动作序列命令

    // 上位机命令
    kEnableCommand,
    kModeCtrlCommand,
    kJointCtrlCommand,
    kMitCtrlCommand,
  };
};

struct StateId {
  enum : etl::fsm_state_id_t {
    kInit,        ///< 开机初始化中
    kNoForce,     ///< 强制无力
    kFreeMove,    ///< 仅重力补偿，不锁定位置
    kManual,      ///< 遥控器接入，手动模式
    kIdle,        ///< 空闲，可执行轨迹
    kExecuting,   ///< 正在执行命令
    kSequencing,  ///< 正在执行动作序列
    kFault,       ///< 故障状态
  };
};

namespace event {
/*******************************************/
struct ControlLoop : etl::message<EventId::kControlLoop> {};

/*******************************************/
struct ForceModeSwitch : etl::message<EventId::kForceModeSwitch> {
  explicit ForceModeSwitch(etl::fsm_state_id_t target) : target_mode(target) {}
  etl::fsm_state_id_t target_mode;
};

/*******************************************/
struct FaultClear : etl::message<EventId::kFaultClear> {};

/*******************************************/
struct NextJoint : etl::message<EventId::kNextJoint> {};

/*******************************************/
struct ActionSequence : etl::message<EventId::kActionSequence> {
  explicit ActionSequence(::ActionSequence seq) : sequence(std::move(seq)) {}
  ::ActionSequence sequence;
};

/*******************************************/
struct EnableCommand : etl::message<EventId::kEnableCommand>, host_schema::EnableCommand {
  EnableCommand() : host_schema::EnableCommand{} {}
  explicit EnableCommand(host_schema::EnableCommand ec) : host_schema::EnableCommand({ec}) {}
};

/*******************************************/
struct ModeCtrlCommand : etl::message<EventId::kModeCtrlCommand>, host_schema::ModeCtrlCommand {
  ModeCtrlCommand() : host_schema::ModeCtrlCommand{} {}
  explicit ModeCtrlCommand(host_schema::ModeCtrlCommand ec) : host_schema::ModeCtrlCommand({ec}) {}
};

/*******************************************/
struct JointCtrlCommand : etl::message<EventId::kJointCtrlCommand>, host_schema::JointCtrlCommand {
  JointCtrlCommand() : host_schema::JointCtrlCommand{} {}
  explicit JointCtrlCommand(host_schema::JointCtrlCommand ec) : host_schema::JointCtrlCommand({ec}) {}
};

/*******************************************/
struct MitCtrlCommand : etl::message<EventId::kMitCtrlCommand>, host_schema::MitCtrlCommand {
  MitCtrlCommand() : host_schema::MitCtrlCommand{} {}
  explicit MitCtrlCommand(host_schema::MitCtrlCommand mc) : host_schema::MitCtrlCommand({mc}) {}
};
}  // namespace event

class Arm : public etl::fsm {
  constexpr static etl::message_router_id_t kMessageRouterId = 0;

 public:
  explicit Arm(ArmResources &r) : fsm(kMessageRouterId), resources_(&r) {}

  bool planner_need_reset_{
      true};  ///< 从故障中恢复或刚上电的时候，规划器不知道当前关节位置/丢失跟踪了一段时间，需要在下一个ControlLoop到来的时候强制把规划器的当前位置重置为当前关节位置

  [[nodiscard]] ArmResources &resources() const { return *resources_; }

 private:
  ArmResources *resources_;
};
extern Arm arm;

namespace state {

///*********************************
///*******  helper macros  *********
///*********************************
#define ACCEPT_MODE_SWITCH() \
  etl::fsm_state_id_t on_event(const event::ForceModeSwitch &e) { return e.target_mode; }
#define IGNORE_UNINTEREST_EVENT() \
  etl::fsm_state_id_t on_event_unknown(const etl::imessage &) { return No_State_Change; }
#define ENTER_STATE etl::fsm_state_id_t on_enter_state() override
#define EXIT_STATE void on_exit_state() override
#define REACT(EventType) etl::fsm_state_id_t on_event(const EventType &e)
///*********************************
///*********************************
///*********************************

/*******************************************/
struct Init : etl::fsm_state<Arm, Init, StateId::kInit,  //
                             event::ControlLoop> {
  // ACCEPT_MODE_SWITCH();
  IGNORE_UNINTEREST_EVENT();
  ENTER_STATE {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    shared.led_controller.SetPattern<led_pattern::YellowFlash>();
    get_fsm_context().planner_need_reset_ = true;
    resources.actuator.SetEnable(false);
    return No_State_Change;
  }
  EXIT_STATE {
    auto &shared = SharedResources::GetInstance();
    shared.led_controller.SetPattern<rm::modules::led_pattern::GreenBreath>();
    shared.buzzer_controller.Play<rm::modules::buzzer_melody::Startup>();
  }
  REACT(event::ControlLoop) {
    auto &resources = get_fsm_context().resources();

    if (resources.device_manager.all_device_ok()) {  // 所有电机在线，初始化完成
      return StateId::kNoForce;
    }

    resources.actuator.SetEnable(false);
    return No_State_Change;
  }
};

/*******************************************/
struct NoForce : etl::fsm_state<Arm, NoForce, StateId::kNoForce,  //
                                event::ForceModeSwitch,           //
                                event::ControlLoop,               //
                                event::EnableCommand> {
  ACCEPT_MODE_SWITCH();
  IGNORE_UNINTEREST_EVENT();
  ENTER_STATE {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    get_fsm_context().planner_need_reset_ = true;
    shared.buzzer_controller.Play<rm::modules::buzzer_melody::Tone<rm::modules::NoteFreqStandard::kC6>>();
    resources.actuator.SetEnable(false);
    return No_State_Change;
  }
  REACT(event::ControlLoop) {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    if (!resources.device_manager.all_device_ok()) {  // 若有电机离线，则进入故障状态
      return StateId::kFault;
    }

    const Eigen::Vector3f g_vec{shared.onboard_imu.accel_z(), -shared.onboard_imu.accel_y(),
                                -shared.onboard_imu.accel_x()};
    resources.controller.ResetAt(resources.actuator.GetPositions());
    resources.controller.Update(resources.actuator.GetPositions(), resources.actuator.GetVelocities(), g_vec, 0.002f);
    resources.actuator.ApplyOutput(resources.controller.output(), false);
    resources.actuator.SetEnable(false, 1);
    return No_State_Change;
  }
  REACT(event::EnableCommand) {
    if (e.enable) {
      return StateId::kIdle;
    }
    return No_State_Change;
  }
};

/*******************************************/
struct FreeMove : etl::fsm_state<Arm, FreeMove, StateId::kFreeMove,  //
                                 event::ForceModeSwitch,             //
                                 event::ControlLoop,                 //
                                 event::EnableCommand,               //
                                 event::ModeCtrlCommand> {
  ACCEPT_MODE_SWITCH();
  IGNORE_UNINTEREST_EVENT();
  ENTER_STATE {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    resources.controller.ForceExitMitMode();  ///< 强制退出MIT模式，恢复重力补偿
    shared.buzzer_controller.Play<rm::modules::buzzer_melody::Beeps<1>>();
    resources.actuator.SetEnable(true);
    return No_State_Change;
  }
  REACT(event::ControlLoop) {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    if (!resources.device_manager.all_device_ok()) {  // 若有电机离线，则进入故障状态
      return StateId::kFault;
    }

    const Eigen::Vector3f g_vec{shared.onboard_imu.accel_z(), -shared.onboard_imu.accel_y(),
                                -shared.onboard_imu.accel_x()};
    resources.controller.ResetAt(resources.actuator.GetPositions());
    resources.controller.Update(resources.actuator.GetPositions(), resources.actuator.GetVelocities(), g_vec, 0.002f);
    resources.actuator.ApplyOutput(resources.controller.output(), false);
    return No_State_Change;
  }
  REACT(event::EnableCommand) {
    if (!e.enable) {
      return StateId::kNoForce;
    }
    return No_State_Change;
  }
  REACT(event::ModeCtrlCommand) {
    if (e.mode == host_schema::ModeCtrlCommand::kNormal) {
      return StateId::kIdle;
    }
    return No_State_Change;
  }
};

/*******************************************/
struct Manual : etl::fsm_state<Arm, Manual, StateId::kManual,  //
                               event::ForceModeSwitch,         //
                               event::ControlLoop,             //
                               event::NextJoint,               //
                               event::EnableCommand,           //
                               event::ModeCtrlCommand> {
  ACCEPT_MODE_SWITCH();
  IGNORE_UNINTEREST_EVENT();
  ENTER_STATE {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    ctrl_joint_idx_ = 0;
    // 进入Manual时强制重置规划器，防止位置跳变
    get_fsm_context().planner_need_reset_ = true;
    // 注意: target_positions_ 的初始化移动到第一个 ControlLoop 中进行，
    // 因为此时 planner_need_reset_ 为 true，controller.output() 不是最新位置
    target_positions_initialized_ = false;
    shared.buzzer_controller.Play<buzzer_melody::EnterManualMode>();
    resources.actuator.SetEnable(true);
    return No_State_Change;
  }
  REACT(event::ControlLoop) {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    if (!resources.device_manager.all_device_ok()) {  // 若有电机离线，则进入故障状态
      return StateId::kFault;
    }

    if (get_fsm_context().planner_need_reset_) {
      resources.controller.ResetAt(resources.actuator.GetPositions());
      get_fsm_context().planner_need_reset_ = false;
    }
    // 确保在 ResetAt 之后再初始化 target_positions_，避免位置跳变
    if (!target_positions_initialized_) {
      target_positions_ = resources.controller.current_positions();
      target_positions_initialized_ = true;
    }
    const auto &joint_limits = resources.controller.joint_limits();
    target_positions_[ctrl_joint_idx_] =
        std::clamp(target_positions_[ctrl_joint_idx_] + (shared.rc.right_y() * 0.003f),  //
                   joint_limits[ctrl_joint_idx_].first, joint_limits[ctrl_joint_idx_].second);
    resources.controller.GoTo(target_positions_);

    const Eigen::Vector3f g_vec{shared.onboard_imu.accel_z(), -shared.onboard_imu.accel_y(),
                                -shared.onboard_imu.accel_x()};
    resources.controller.Update(resources.actuator.GetPositions(), resources.actuator.GetVelocities(), g_vec, 0.002f);
    resources.actuator.ApplyOutput(resources.controller.output(), true);
    return No_State_Change;
  }
  REACT(event::NextJoint) {
    UNUSED(e);
    auto &shared = SharedResources::GetInstance();
    ctrl_joint_idx_ = (ctrl_joint_idx_ + 1) % 5;
    switch (ctrl_joint_idx_) {
        // clang-format off
      case 0: shared.buzzer_controller.Play<buzzer_melody::HiBeeps<1>>(); break;
      case 1: shared.buzzer_controller.Play<buzzer_melody::HiBeeps<2>>(); break;
      case 2: shared.buzzer_controller.Play<buzzer_melody::HiBeeps<3>>(); break;
      case 3: shared.buzzer_controller.Play<buzzer_melody::HiBeeps<4>>(); break;
      case 4: shared.buzzer_controller.Play<buzzer_melody::HiBeeps<5>>(); break;
      default: break;
        // clang-format on
    }
    return No_State_Change;
  }
  REACT(event::EnableCommand) {
    if (!e.enable) {
      return StateId::kNoForce;
    }
    return No_State_Change;
  }
  REACT(event::ModeCtrlCommand) {
    if (e.mode == host_schema::ModeCtrlCommand::kFreeMove) {
      return StateId::kFreeMove;
    }
    return No_State_Change;
  }

 private:
  std::array<float, 5> target_positions_{};
  int ctrl_joint_idx_{0};
  bool target_positions_initialized_{false};
};

/*******************************************/
struct Idle : etl::fsm_state<Arm, Idle, StateId::kIdle,  //
                             event::ForceModeSwitch,     //
                             event::ControlLoop,         //
                             event::ActionSequence,      //
                             event::EnableCommand,       //
                             event::ModeCtrlCommand,     //
                             event::JointCtrlCommand,    //
                             event::MitCtrlCommand> {
  ACCEPT_MODE_SWITCH();
  IGNORE_UNINTEREST_EVENT();
  ENTER_STATE {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    get_fsm_context().planner_need_reset_ = true;  // 进入Idle时强制重置规划器，防止位置跳变
    shared.buzzer_controller.Play<rm::modules::buzzer_melody::Tone<rm::modules::NoteFreqStandard::kD6>>();
    resources.actuator.SetEnable(true);
    return No_State_Change;
  }
  REACT(event::ControlLoop) {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();
    // if (resources.controller.status() == ArmController::kFault) {
    //   return StateId::kFault;
    // }
    if (!resources.device_manager.all_device_ok()) {  // 若有电机离线，则进入故障状态
      return StateId::kFault;
    }
    if (get_fsm_context().planner_need_reset_) {
      resources.controller.ResetAt(resources.actuator.GetPositions());
      get_fsm_context().planner_need_reset_ = false;
    }

    const Eigen::Vector3f g_vec{shared.onboard_imu.accel_z(), -shared.onboard_imu.accel_y(),
                                -shared.onboard_imu.accel_x()};
    resources.controller.Update(resources.actuator.GetPositions(), resources.actuator.GetVelocities(), g_vec, 0.002f);
    resources.actuator.ApplyOutput(resources.controller.output(), true);
    return No_State_Change;
  }
  REACT(event::ActionSequence) {
    auto &resources = get_fsm_context().resources();
    resources.sequencer.Execute(e.sequence);
    return StateId::kSequencing;
  }
  REACT(event::EnableCommand) {
    if (!e.enable) {
      return StateId::kNoForce;
    }
    return No_State_Change;
  }
  REACT(event::ModeCtrlCommand) {
    if (e.mode == host_schema::ModeCtrlCommand::kFreeMove) {
      return StateId::kFreeMove;
    }
    return No_State_Change;
  }
  REACT(event::JointCtrlCommand) {
    auto &resources = get_fsm_context().resources();

    const std::array<float, 5> target_pos{
        e.position[0], e.position[1], e.position[2], e.position[3], e.position[4],
    };
    resources.controller.GoTo(target_pos);
    return StateId::kExecuting;
  }
  REACT(event::MitCtrlCommand) {
    auto &resources = get_fsm_context().resources();

    const std::array<float, 5> pos{e.position[0], e.position[1], e.position[2], e.position[3], e.position[4]};
    const std::array<float, 5> vel{e.velocity[0], e.velocity[1], e.velocity[2], e.velocity[3], e.velocity[4]};
    const std::array<float, 5> tor{e.torque[0], e.torque[1], e.torque[2], e.torque[3], e.torque[4]};
    resources.controller.SetRawMitCommand(pos, vel, tor);
    return StateId::kExecuting;
  }
};

/*******************************************/
struct Executing : etl::fsm_state<Arm, Executing, StateId::kExecuting,  //
                                  event::ControlLoop,                   //
                                  event::ForceModeSwitch,               //
                                  event::EnableCommand,                 //
                                  event::ModeCtrlCommand,               //
                                  event::JointCtrlCommand,              //
                                  event::MitCtrlCommand> {
  ACCEPT_MODE_SWITCH();
  IGNORE_UNINTEREST_EVENT();
  ENTER_STATE {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    is_finished_ = etl::debounce<100, 100, 100>{};  // reset debouncer
    shared.buzzer_controller.Play<rm::modules::buzzer_melody::Tone<rm::modules::NoteFreqStandard::kE6>>();
    resources.actuator.SetEnable(true);
    return No_State_Change;
  }
  REACT(event::ControlLoop) {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    if (!resources.device_manager.all_device_ok()) {  // 若有电机离线，则进入故障状态
      return StateId::kFault;
    }

    is_finished_.add(resources.controller.status() == ArmController::kIdle);
    switch (resources.controller.status()) {
      case ArmController::kIdle: {
        if (is_finished_.is_set()) {
          return StateId::kIdle;
        }
        break;
      }
      case ArmController::kMoving:
      default: {
        // no state change
        break;
      }
    }
    if (get_fsm_context().planner_need_reset_) {
      resources.controller.ResetAt(resources.actuator.GetPositions());
      get_fsm_context().planner_need_reset_ = false;
    }
    const Eigen::Vector3f g_vec{shared.onboard_imu.accel_z(), -shared.onboard_imu.accel_y(),
                                -shared.onboard_imu.accel_x()};
    resources.controller.Update(resources.actuator.GetPositions(), resources.actuator.GetVelocities(), g_vec, 0.002f);
    resources.actuator.ApplyOutput(resources.controller.output(), true);
    return No_State_Change;
  }
  REACT(event::EnableCommand) {
    if (!e.enable) {
      return StateId::kNoForce;
    }
    return No_State_Change;
  }
  REACT(event::ModeCtrlCommand) {
    if (e.mode == host_schema::ModeCtrlCommand::kFreeMove) {
      return StateId::kFreeMove;
    }
    return No_State_Change;
  }
  REACT(event::JointCtrlCommand) {
    auto &resources = get_fsm_context().resources();

    const std::array<float, 5> target_pos{
        e.position[0], e.position[1], e.position[2], e.position[3], e.position[4],
    };
    resources.controller.GoTo(target_pos);
    return No_State_Change;
  }
  REACT(event::MitCtrlCommand) {
    auto &resources = get_fsm_context().resources();

    const std::array<float, 5> pos{e.position[0], e.position[1], e.position[2], e.position[3], e.position[4]};
    const std::array<float, 5> vel{e.velocity[0], e.velocity[1], e.velocity[2], e.velocity[3], e.velocity[4]};
    const std::array<float, 5> tor{e.torque[0], e.torque[1], e.torque[2], e.torque[3], e.torque[4]};
    resources.controller.SetRawMitCommand(pos, vel, tor);
    return No_State_Change;
  }

 private:
  etl::debounce<100, 100, 100> is_finished_{};
};

/*******************************************/
struct Sequencing : etl::fsm_state<Arm, Sequencing, StateId::kSequencing,  //
                                   event::ForceModeSwitch,                 //
                                   event::ControlLoop,                     //
                                   event::FaultClear,                      //
                                   event::EnableCommand,                   //
                                   event::ModeCtrlCommand> {
  // ACCEPT_MODE_SWITCH();
  IGNORE_UNINTEREST_EVENT();
  ENTER_STATE {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    get_fsm_context().planner_need_reset_ = true;  // 强制重置规划器
    shared.buzzer_controller.Play<rm::modules::buzzer_melody::Beeps<2>>();
    resources.actuator.SetEnable(true);
    return No_State_Change;
  }
  REACT(event::ForceModeSwitch) {
    if (e.target_mode == StateId::kNoForce) {
      return StateId::kNoForce;
    }
    return No_State_Change;
  }
  REACT(event::ControlLoop) {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    if (!resources.device_manager.all_device_ok()) {  // 若有电机离线，则进入故障状态
      return StateId::kFault;
    }

    if (get_fsm_context().planner_need_reset_) {
      resources.controller.ResetAt(resources.actuator.GetPositions());
      get_fsm_context().planner_need_reset_ = false;
    }

    // tick sequencer
    constexpr float kDt = 0.002f;
    constexpr uint32_t kDtMs = 2;
    resources.sequencer.Update(kDtMs);

    const Eigen::Vector3f g_vec{shared.onboard_imu.accel_z(), -shared.onboard_imu.accel_y(),
                                -shared.onboard_imu.accel_x()};
    resources.controller.Update(resources.actuator.GetPositions(), resources.actuator.GetVelocities(), g_vec, kDt);
    resources.actuator.ApplyOutput(resources.controller.output(), true);

    // 序列结束，回到 Idle
    if (resources.sequencer.IsFinished()) {
      return StateId::kIdle;
    }
    return No_State_Change;
  }
  REACT(event::FaultClear) {
    UNUSED(e);
    auto &resources = get_fsm_context().resources();

    resources.controller.FaultClear();
    return StateId::kNoForce;
  }
  REACT(event::EnableCommand) {
    if (!e.enable) {
      return StateId::kNoForce;
    }
    return No_State_Change;
  }
  REACT(event::ModeCtrlCommand) {
    if (e.mode == host_schema::ModeCtrlCommand::kFreeMove) {
      return StateId::kFreeMove;
    }
    return No_State_Change;
  }
};

/*******************************************/
struct Fault : etl::fsm_state<Arm, Fault, StateId::kFault,  //
                              event::ControlLoop,           //
                              event::FaultClear,            //
                              event::EnableCommand> {
  // ACCEPT_MODE_SWITCH();
  IGNORE_UNINTEREST_EVENT();
  ENTER_STATE {
    auto &shared = SharedResources::GetInstance();
    auto &resources = get_fsm_context().resources();

    shared.led_controller.SetPattern<rm::modules::led_pattern::RedFlash>();
    shared.buzzer_controller.Play<rm::modules::buzzer_melody::Error>();
    get_fsm_context().planner_need_reset_ = true;
    resources.actuator.SetEnable(false);
    return No_State_Change;
  }
  REACT(event::ControlLoop) {
    UNUSED(e);
    auto &resources = get_fsm_context().resources();

    resources.actuator.SetEnable(false, 1);
    return No_State_Change;
  }
  REACT(event::FaultClear) {
    UNUSED(e);
    auto &resources = get_fsm_context().resources();

    resources.actuator.ClearError();
    resources.controller.FaultClear();
    return StateId::kInit;
  }
  REACT(event::EnableCommand) {
    auto &resources = get_fsm_context().resources();
    if (!e.enable) {
      resources.actuator.ClearError();
      resources.controller.FaultClear();
      return StateId::kInit;
    }
    return No_State_Change;
  }
};

#undef IGNORE_UNINTEREST_EVENT
#undef ENTER_STATE
#undef EXIT_STATE
#undef REACT

}  // namespace state

void Init();

}  // namespace fsm_arm