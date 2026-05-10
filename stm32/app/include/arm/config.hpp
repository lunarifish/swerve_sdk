#pragma once

#include <librm.hpp>

#include "et16s.hpp"
#include "dynamics.hpp"
#include "m_pi_f.hpp"
#include "shared_resources.hpp"

namespace detail {
struct PDParams {
  float kp;
  float kd;
};
}  // namespace detail

using namespace rm::modules::angle_literals;

struct ArmConfig {
  rm::hal::CanInterface *can;
  size_t host_comm_id;  ///< 此臂在通信总线上发送反馈帧的 CAN ID

  // 电机配置
  rm::device::DmMotorMit::Settings dm_settings[5];           ///< j1~j5 达妙电机参数
  rm::modules::MotorCalibrator::MotorConfig calibrators[5];  ///< 关节零点、转向校准配置

  // 控制参数
  detail::PDParams pd_params[5];  ///< j1~j5 PD 增益

  // 运动学 / 动力学
  std::array<std::pair<float, float>, 5> joint_limits{{
      {-(90_deg).rad(), (90_deg).rad()},    // j1 yaw
      {-M_PIf * 2.f, M_PIf * 2.f},          // j2 pitch1
      {-M_PIf * 2.f, M_PIf * 2.f},          // j3 pitch2
      {-M_PIf, M_PIf},                      // j4 roll1
      {-(100_deg).rad(), (100_deg).rad()},  // j5 pitch3
  }};  ///< {min, max}（逻辑坐标，rad），两边的臂是一样的
  ArmDynamics::Params dynamics_params;  ///< 动力学参数

  std::array<rm::modules::TrajectoryLimiter, 5> trajectory_limiters;

  // 遥控器通道映射
  int rc_ch_mode_switch;            ///< 模式切换拨杆通道
  int rc_ch_next_joint{rc_ch::SH};  ///< 切换关节拨杆通道

  static ArmConfig Make() {
    using namespace rm::modules::angle_literals;
    return {
        .can = &SharedResources::GetInstance().right_can,
        .host_comm_id = 0x232,
        .dm_settings = {{.master_id = 0xb1,
                         .slave_id = 0xa1,
                         .p_max = 12.5f,
                         .v_max = 10.f,
                         .t_max = 28.f,
                         .kp_range = {0, 500},
                         .kd_range = {0, 50}},
                        {.master_id = 0xb2,
                         .slave_id = 0xa2,
                         .p_max = 12.5f,
                         .v_max = 10.f,
                         .t_max = 28.f,
                         .kp_range = {0, 500},
                         .kd_range = {0, 50}},
                        {.master_id = 0xb3,
                         .slave_id = 0xa3,
                         .p_max = 12.5f,
                         .v_max = 30.f,
                         .t_max = 10.f,
                         .kp_range = {0, 500},
                         .kd_range = {0, 50}},
                        {.master_id = 0xb4,
                         .slave_id = 0xa4,
                         .p_max = 12.5f,
                         .v_max = 30.f,
                         .t_max = 10.f,
                         .kp_range = {0, 500},
                         .kd_range = {0, 50}},
                        {.master_id = 0xb5,
                         .slave_id = 0xa5,
                         .p_max = 12.5f,
                         .v_max = 30.f,
                         .t_max = 10.f,
                         .kp_range = {0, 500},
                         .kd_range = {0, 50}}},
        .calibrators =
            {
                {.reverse = false, .zero_offset = 0.f},
                {.reverse = true, .zero_offset = (90_deg - 13.1_deg).rad()},
                {.reverse = true, .zero_offset = -(90_deg - 13.1_deg).rad()},
                {.reverse = false, .zero_offset = 0.f},
                {.reverse = false, .zero_offset = 0.f},
            },
        .pd_params =
            {
                {.kp = 25.f, .kd = 5.f},
                {.kp = 40.f, .kd = 10.f},
                {.kp = 40.f, .kd = 10.f},
                {.kp = 13.5f, .kd = 3.f},
                {.kp = 9.f, .kd = 0.5f},
                // {.kp = 0.f, .kd = 0.f},
                // {.kp = 0.f, .kd = 0.f},
                // {.kp = 0.f, .kd = 0.f},
                // {.kp = 0.f, .kd = 0.f},
                // {.kp = 0.f, .kd = 0.f},
            },
        .dynamics_params = {.b1 = 0.5665f, .b2 = 0.2648f},
        .trajectory_limiters = {{{10.775f, 10.f},  //
                                 {16.575f, 20.f},  //
                                 {16.575f, 20.f},  //
                                 {17.f, 50.f},     //
                                 {17.f, 50.f}}},
        .rc_ch_mode_switch = rc_ch::SC,
    };
  }
};