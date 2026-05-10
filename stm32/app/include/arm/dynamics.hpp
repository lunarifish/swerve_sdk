#pragma once

#include <Eigen/Dense>

/// @brief 动力学重力补偿+平动加速度补偿
class ArmDynamics {
 public:
  struct Params {
    float b1;
    float b2;
  };

  ArmDynamics() = default;
  explicit ArmDynamics(Params params) : params_(params) {}

  Eigen::Vector3f Compensate(const Eigen::Vector3f& q, const Eigen::Vector3f& acc) const {
    const float q1 = q(0);
    const float q2 = q(1);
    const float q3 = q(2);
    const float gx = acc(0);
    const float gy = acc(1);
    const float gz = acc(2);

    const float cos_q1 = std::cos(q1);
    const float sin_q1 = std::sin(q1);
    const float cos_q2 = std::cos(q2);
    const float sin_q2 = std::sin(q2);
    const float cos_q2_q3 = std::cos(q2 + q3);
    const float sin_q2_q3 = std::sin(q2 + q3);

    return {
        (params_.b1 * cos_q2 + params_.b2 * cos_q2_q3) * (gx * sin_q1 - gy * cos_q1),           // q1
        params_.b1 * (gx * sin_q2 * cos_q1 + gy * sin_q1 * sin_q2 - gz * cos_q2) +              // q2
            params_.b2 * (gx * sin_q2_q3 * cos_q1 + gy * sin_q1 * sin_q2_q3 - gz * cos_q2_q3),  //
        params_.b2 * (gx * sin_q2_q3 * cos_q1 + gy * sin_q1 * sin_q2_q3 - gz * cos_q2_q3)       // q3
    };
  }

 private:
  Params params_;
};
