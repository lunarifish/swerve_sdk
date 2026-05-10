#pragma once

#include <librm.hpp>

#include "config.hpp"
#include "arm_controller.hpp"
#include "action_sequencer.hpp"
#include "actuators.hpp"

struct ArmResources {
  explicit ArmResources(const ArmConfig &config)
      : config{config},
        actuator{config},
        controller{config.joint_limits, config.trajectory_limiters, config.dynamics_params} {
    actuator.j1.SetName("j1");
    actuator.j2.SetName("j2");
    actuator.j3.SetName("j3");
    actuator.j4.SetName("j4");
    actuator.j5.SetName("j5");

    device_manager << &actuator.j1 << &actuator.j2 << &actuator.j3 << &actuator.j4 << &actuator.j5;
  }

  ArmConfig config;

  Actuator actuator;
  ArmController controller;               ///< 轨迹规划、控制输出计算
  ActionSequencer sequencer{controller};  ///< 动作序列执行器

  rm::device::DeviceManager<10> device_manager;  ///< 管理所有电机的在线状态
};