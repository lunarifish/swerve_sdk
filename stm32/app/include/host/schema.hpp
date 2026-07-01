#pragma once

#include <cstdint>

#include <etl/platform.h>

namespace host_schema {

constexpr static uint16_t kBaseFrameId = 0x233;

//////////////////
/// ARM TO HOST
//////////////////

ETL_PACKED_STRUCT(SystemStatus) {
  constexpr static uint16_t kFrameIdOffset = 1;
  uint8_t j1_motor_status;
  uint8_t j2_motor_status;
  uint8_t j3_motor_status;
  uint8_t j4_motor_status;
  uint8_t j5_motor_status;
  bool is_pressure_sensor_online;

  // struct StateId {
  //   enum : etl::fsm_state_id_t {
  //     kInit,         ///< 开机初始化中
  //     kNoForce,      ///< 强制无力
  //     kManual,       ///< 遥控器接入，手动模式
  //     kIdle,         ///< 空闲，可执行轨迹
  //     kHostControl,  ///< 正在被上位机控制
  //     kSequencing,   ///< 正在执行动作序列
  //     kFault,        ///< 故障状态
  //   };
  // };
  uint8_t fsm_state;

  float vbus;
};
static_assert(sizeof(SystemStatus) <= 64);

ETL_PACKED_STRUCT(JointState) {
  constexpr static uint16_t kFrameIdOffset = 2;
  float position[5];  ///< rad
  float velocity[5];  ///< rad/s
  float effort[5];    ///< N*m
};
static_assert(sizeof(JointState) <= 64);

ETL_PACKED_STRUCT(PressureSensorData) {
  constexpr static uint16_t kFrameIdOffset = 3;
  float pressure;  ///< 压力测量值，单位N
};
static_assert(sizeof(PressureSensorData) <= 64);

ETL_PACKED_STRUCT(SensorDataMisc1) {
  constexpr static uint16_t kFrameIdOffset = 7;
  uint8_t motor_coil_temp[5];
  uint8_t motor_mos_temp[5];
  uint8_t resv[64 - 10];
};
static_assert(sizeof(SensorDataMisc1) <= 64);

//////////////////
/// HOST TO ARM
//////////////////

/// @brief 使能/失能机械臂
ETL_PACKED_STRUCT(EnableCommand) {
  constexpr static uint16_t kFrameIdOffset = 4;
  bool enable;
};
static_assert(sizeof(EnableCommand) <= 64);

/// @brief 切换控制模式
ETL_PACKED_STRUCT(ModeCtrlCommand) {
  constexpr static uint16_t kFrameIdOffset = 5;
  enum : uint8_t {
    kFreeMove,  ///< 机械臂不使力，外力可以拖动机械臂运动，可用于拖动示教应用
    kNormal,    ///< 正常关节位控模式
  } mode;
};
static_assert(sizeof(ModeCtrlCommand) <= 64);

/// @brief 5轴关节目标角度，驱动机械臂运动到指定关节位姿。
ETL_PACKED_STRUCT(JointCtrlCommand) {
  constexpr static uint16_t kFrameIdOffset = 6;
  float position[5];
};
static_assert(sizeof(JointCtrlCommand) <= 64);

}  // namespace host_schema