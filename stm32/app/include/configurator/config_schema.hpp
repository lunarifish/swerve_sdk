#pragma once

#include <cstdint>

/**
 * @brief 持久化机械臂参数（存入 FlashPreferences<PersistableArmConfig>）。
 *
 * 修改此结构体后务必递增 schema_version 以触发 flash 自动重置。
 */
struct __attribute__((packed)) PersistableConfigV1 {
  uint32_t schema_version{1};
  float pd_kp[5]{25.f, 40.f, 40.f, 13.5f, 9.f};
  float pd_kd[5]{5.f, 10.f, 10.f, 3.f, 0.5f};
  float max_joint_vel[5]{10.775f, 16.575f, 16.575f, 17.f, 17.f};
  float max_joint_accel[5]{10.f, 20.f, 20.f, 50.f, 50.f};
  // float zero_offsets[5]{};
  // bool reverse[5]{false, true, true, false, false};
};

inline bool Validate(const PersistableConfigV1 &c) {
  if (c.schema_version != 1) return false;
  for (int i = 0; i < 5; ++i) {
    if (c.pd_kp[i] < 0.f || c.pd_kp[i] > 100.f) return false;
    if (c.pd_kd[i] < 0.f || c.pd_kd[i] > 30.f) return false;
    if (c.max_joint_vel[i] < 0.f || c.max_joint_vel[i] > 30.f) return false;
    if (c.max_joint_accel[i] < 0.f || c.max_joint_accel[i] > 100.f) return false;
  }
  return true;
}