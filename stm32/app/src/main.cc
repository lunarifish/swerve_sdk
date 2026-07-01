
#include "bsp/timer_task_scheduler.hpp"
#include "shared_resources.hpp"
#include "arm/arm.hpp"
#include "host/schema.hpp"

__attribute__((section(".sram4"))) SharedResourcesNoDtcm shared_no_dtcm;

Arm *arm;

namespace {
void AhrsLoop1khz() {
  auto &shared = SharedResources::GetInstance();
  shared.onboard_imu.Update();
}

void ArmControlLoop300hz() { arm->Tick(); }

void FastIoLoop150hz() {
  auto &shared = SharedResources::GetInstance();
  shared.rc.Update();
  shared.vbus = shared_no_dtcm.vbus_adc.vbus();
  shared.device_manager.Update();

  // 向上位机反馈状态
  {
    const auto ss = [&] {
      host_schema::SystemStatus ret;
      ret.j1_motor_status = arm->resources.actuator.j1.status();
      ret.j2_motor_status = arm->resources.actuator.j2.status();
      ret.j3_motor_status = arm->resources.actuator.j3.status();
      ret.j4_motor_status = arm->resources.actuator.j4.status();
      ret.j5_motor_status = arm->resources.actuator.j5.status();
      ret.is_pressure_sensor_online = (shared.press_sensor.online_status() == rm::device::Device::kOk);
      ret.fsm_state = arm->fsm.get_state_id();
      ret.vbus = shared_no_dtcm.vbus_adc.vbus();
      return ret;
    }();
    shared.comm_can.Write(host_schema::kBaseFrameId + ss.kFrameIdOffset, reinterpret_cast<const uint8_t *>(&ss),
                          sizeof(ss));
  }
  {
    const auto js = [&] {
      host_schema::JointState ret;
      const auto pos = arm->resources.actuator.GetPositions();
      const auto vel = arm->resources.actuator.GetVelocities();
      const auto effort = arm->resources.actuator.GetEfforts();
      for (int i = 0; i < 5; i++) {
        ret.position[i] = pos[i];
        ret.velocity[i] = vel[i];
        ret.effort[i] = effort[i];
      }
      return ret;
    }();
    shared.comm_can.Write(host_schema::kBaseFrameId + js.kFrameIdOffset, reinterpret_cast<const uint8_t *>(&js),
                          sizeof(js));
  }
  {
    const auto sdm = [&] {
      host_schema::SensorDataMisc1 ret;
      ret.motor_coil_temp[0] = arm->resources.actuator.j1.coil_temperature();
      ret.motor_mos_temp[0] = arm->resources.actuator.j1.mos_temperature();
      ret.motor_coil_temp[1] = arm->resources.actuator.j2.coil_temperature();
      ret.motor_mos_temp[1] = arm->resources.actuator.j2.mos_temperature();
      ret.motor_coil_temp[2] = arm->resources.actuator.j3.coil_temperature();
      ret.motor_mos_temp[2] = arm->resources.actuator.j3.mos_temperature();
      ret.motor_coil_temp[3] = arm->resources.actuator.j4.coil_temperature();
      ret.motor_mos_temp[3] = arm->resources.actuator.j4.mos_temperature();
      ret.motor_coil_temp[4] = arm->resources.actuator.j5.coil_temperature();
      ret.motor_mos_temp[4] = arm->resources.actuator.j5.mos_temperature();
      return ret;
    }();
    shared.comm_can.Write(host_schema::kBaseFrameId + sdm.kFrameIdOffset, reinterpret_cast<const uint8_t *>(&sdm),
                          sizeof(sdm));
  }
}

void SlowIoLoop50hz() {
  auto &shared = SharedResources::GetInstance();
  // 更新LED和蜂鸣器
  const auto &[r, g, b] = shared.led_controller.Update();
  shared.led.SetColor(r, g, b);
  shared.buzzer.SetFrequency(shared.buzzer_controller.Update().frequency);
}

void VerySlowIoLoop40hz() {
  auto &shared = SharedResources::GetInstance();
  static int aaa = 0;
  if (aaa == 0) {
    shared.press_sensor.RequestWeight();
  } else if (aaa == 1) {
    shared.press_sensor.RequestRawAd();
  } else if (aaa == 2) {
    shared.press_sensor.RequestZeroPosition();
  }
  aaa = (aaa + 1) % 3;
  {
    const auto psd = [&] {
      host_schema::PressureSensorData ret;
      ret.pressure = shared.press_sensor.weight() / 100.f;
      return ret;
    }();
    shared.comm_can.Write(0x236, reinterpret_cast<const uint8_t *>(&psd), sizeof(psd));
  }
}

}  // namespace

extern "C" {

[[noreturn]] void AppMain(void) {
  auto &shared = SharedResources::GetInstance();

  auto arm_config = ArmConfig::Make();
  const auto persist_storage = shared.flash_prefs.data();
  if (Validate(persist_storage)) {
    for (int i = 0; i < 5; i++) {
      arm_config.pd_params[i] = {persist_storage.pd_kp[i], persist_storage.pd_kd[i]};
      arm_config.trajectory_limiters[i].SetMaxVel(persist_storage.max_joint_vel[i]);
      arm_config.trajectory_limiters[i].SetMaxAccel(persist_storage.max_joint_accel[i]);
    }
  }
  arm = new Arm{arm_config};
  arm->Init();

  shared.press_sensor.Begin();

  // 创建调度器，绑定htim13
  static TimerTaskScheduler sched{&htim13};
  sched.AddTask(1000.f, etl::delegate<void()>::create<AhrsLoop1khz>(), "ahrs_1khz")
      .AddTask(300.f, etl::delegate<void()>::create<ArmControlLoop300hz>(), "arm_control_300hz")
      .AddTask(50.f, etl::delegate<void()>::create<SlowIoLoop50hz>(), "slow_io_50hz")
      .AddTask(40.f, etl::delegate<void()>::create<VerySlowIoLoop40hz>(), "very_slow_io_40hz")
      .AddTask(150.f, etl::delegate<void()>::create<FastIoLoop150hz>(), "fast_io_150hz");
  sched.Start();

  for (;;) {
    // 处理CAN定频发送
    shared.right_can.Process();
    shared.comm_can.Process();
  }
}
}
