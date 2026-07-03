#pragma once

#include <librm.hpp>

#include "arm_controller.hpp"
#include "config.hpp"

/// @brief 储存所有电机，以及实现一些电机相关的helper（使能、处理反转、设零点）
class Actuator {
 public:
  explicit Actuator(const ArmConfig& config)
      : j1{*config.can, config.dm_settings[0]},
        j2{*config.can, config.dm_settings[1]},
        j3{*config.can, config.dm_settings[2]},
        j4{*config.can, config.dm_settings[3]},
        j5{*config.can, config.dm_settings[4]} {
    for (int i = 0; i < 5; ++i) {
      calibrators[i].UpdateConfig(config.calibrators[i]);
    }
    for (int i = 0; i < 5; ++i) {
      pd_params_[i] = config.pd_params[i];
    }
  }

  void ClearError() {
    j1.SendInstruction(rm::device::DmMotorInstructions::kClearError);
    j2.SendInstruction(rm::device::DmMotorInstructions::kClearError);
    j3.SendInstruction(rm::device::DmMotorInstructions::kClearError);
    j4.SendInstruction(rm::device::DmMotorInstructions::kClearError);
    j5.SendInstruction(rm::device::DmMotorInstructions::kClearError);
  }

  void SetEnable(bool enable, int repeats = 2) {
    const auto dm_cmd = enable ? rm::device::DmMotorInstructions::kEnable : rm::device::DmMotorInstructions::kDisable;
    for (int i = 0; i < repeats; ++i) {
      j1.SendInstruction(dm_cmd);
      j2.SendInstruction(dm_cmd);
      j3.SendInstruction(dm_cmd);
      j4.SendInstruction(dm_cmd);
      j5.SendInstruction(dm_cmd);
    }
  }

  void ApplyOutput(const ArmController::Output& output, bool apply_position = false) {
    const auto& pd_config = pd_params_;
    if (apply_position) {
      j1.SetMitCommand(calibrators[0].LogicalToReal(output.positions[0]),                  //
                       calibrators[0].reverse() ? -output.velocities[0] : output.velocities[0],                                                                  //
                       calibrators[0].reverse() ? -output.efforts[0] : output.efforts[0],  //
                       pd_config[0].kp,                                                    //
                       pd_config[0].kd);
      j2.SetMitCommand(calibrators[1].LogicalToReal(output.positions[1]),                  //
                       calibrators[1].reverse() ? -output.velocities[1] : output.velocities[1],                                                                  //
                       calibrators[1].reverse() ? -output.efforts[1] : output.efforts[1],  //
                       pd_config[1].kp,                                                    //
                       pd_config[1].kd);
      j3.SetMitCommand(calibrators[2].LogicalToReal(output.positions[2]),                  //
                       calibrators[2].reverse() ? -output.velocities[2] : output.velocities[2],                                                                  //
                       calibrators[2].reverse() ? -output.efforts[2] : output.efforts[2],  //
                       pd_config[2].kp,                                                    //
                       pd_config[2].kd);
      j4.SetMitCommand(calibrators[3].LogicalToReal(output.positions[3]),  //
                       calibrators[3].reverse() ? -output.velocities[3] : output.velocities[3],                                                  //
                       calibrators[3].reverse() ? -output.efforts[3] : output.efforts[3],  //
                       pd_config[3].kp,                                    //
                       pd_config[3].kd);
      j5.SetMitCommand(calibrators[4].LogicalToReal(output.positions[4]),  //
                       calibrators[4].reverse() ? -output.velocities[4] : output.velocities[4],                                                  //
                       calibrators[4].reverse() ? -output.efforts[4] : output.efforts[4],  //
                       pd_config[4].kp,                                    //
                       pd_config[4].kd);
    } else {
      j1.SetMitCommand(0.f,                                                                //
                       0,                                                                  //
                       calibrators[0].reverse() ? -output.efforts[0] : output.efforts[0],  //
                       0.f,                                                                //
                       0.f);
      j2.SetMitCommand(0.f,                                                                //
                       0,                                                                  //
                       calibrators[1].reverse() ? -output.efforts[1] : output.efforts[1],  //
                       0.f,                                                                //
                       0.f);
      j3.SetMitCommand(0.f,                                                                //
                       0,                                                                  //
                       calibrators[2].reverse() ? -output.efforts[2] : output.efforts[2],  //
                       0.f,                                                                //
                       0.f);
      j4.SetMitCommand(0.f,  //
                       0,    //
                       0.f,  //
                       0.f,  //
                       0.f);
      j5.SetMitCommand(0.f,  //
                       0,    //
                       0.f,  //
                       0.f,  //
                       0.f);
    }
  }

  /// @brief 给达妙电机设零点
  void DmSetZero() {
    SetEnable(true);
    for (int i = 0; i < 10; i++) {
      // j1.SendInstruction(rm::device::DmMotorInstructions::kSetZeroPosition);
      // j2.SendInstruction(rm::device::DmMotorInstructions::kSetZeroPosition);
      // j3.SendInstruction(rm::device::DmMotorInstructions::kSetZeroPosition);
      // j4.SendInstruction(rm::device::DmMotorInstructions::kSetZeroPosition);
      // j5.SendInstruction(rm::device::DmMotorInstructions::kSetZeroPosition);
      HAL_Delay(10);
    }
    SetEnable(false);
  }

  std::array<float, 5> GetPositions() const {
    std::array<float, 5> positions{
        j1.pos(),  //
        j2.pos(),  //
        j3.pos(),  //
        j4.pos(),  //
        j5.pos(),  //
    };
    for (int i = 0; i < 5; ++i) {
      positions[i] = calibrators[i].RealToLogical(positions[i]);
    }
    return positions;
  }

  std::array<float, 5> GetVelocities() const {
    std::array<float, 5> velocities{
        j1.vel(),  //
        j2.vel(),  //
        j3.vel(),  //
        j4.vel(),  //
        j5.vel(),  //
    };
    for (int i = 0; i < 5; ++i) {
      if (calibrators[i].reverse()) {
        velocities[i] = -velocities[i];
      }
    }
    return velocities;
  }

  std::array<float, 5> GetEfforts() const {
    std::array<float, 5> efforts{
        j1.tau(),  //
        j2.tau(),  //
        j3.tau(),  //
        j4.tau(),  //
        j5.tau(),  //
    };
    for (int i = 0; i < 5; ++i) {
      if (calibrators[i].reverse()) {
        efforts[i] = -efforts[i];
      }
    }
    return efforts;
  }

  rm::modules::MotorCalibrator calibrators[5];
  rm::device::DmMotorMit j1, j2, j3, j4, j5;

 private:
  detail::PDParams pd_params_[5]{};  ///< 从 ArmConfig 拷贝的 PD 参数
};