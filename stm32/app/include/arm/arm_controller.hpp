#pragma once

#include <array>

#include <librm.hpp>

#include "dynamics.hpp"

class ArmController {
 public:
  struct Output {
    static Output Zero() {
      Output out;
      out.positions.fill(0.f);
      out.velocities.fill(0.f);
      out.efforts.fill(0.f);
      return out;
    }

    Output() = default;

    std::array<float, 5> positions{};   ///< 关节位置输出，单位：rad
    std::array<float, 5> velocities{};  ///< 关节速度输出，单位：rad/s
    std::array<float, 5> efforts{};     ///< 关节力矩输出，单位：Nm
  };

  enum Status {
    kIdle,
    kMoving,
    kFault,
  };

  ArmController() = delete;
  explicit ArmController(std::array<std::pair<float, float>, 5> joint_limits,  //
                         std::array<rm::modules::TrajectoryLimiter, 5> trajectory_limiters,
                         ArmDynamics::Params dynamics_params)
      : trajectory_limiter_(std::move(trajectory_limiters)),
        joint_limits_(std::move(joint_limits)),
        dynamics_(dynamics_params) {
    for (auto &f : acc_lpf_) {
      f.set_cutoff_frequency(500.f, 9.f);
    }
  }

  void GoTo(const std::array<float, 5> &target_positions) {
    mit_mode_active_ = false;  // GoTo 退出 MIT 模式
    for (size_t i = 0; i < 5; ++i) {
      trajectory_limiter_[i].SetTarget(
          std::clamp(target_positions[i], joint_limits_[i].first, joint_limits_[i].second));
    }
    UpdateStatus();
  }

  /// @brief 上位机 MIT 直通指令，position 经 trajectory limiter 平滑，velocity/torque 直接透传
  void SetRawMitCommand(const std::array<float, 5> &positions, const std::array<float, 5> &velocities,
                        const std::array<float, 5> &torques) {
    mit_mode_active_ = true;
    for (size_t i = 0; i < 5; ++i) {
      trajectory_limiter_[i].SetTarget(
          std::clamp(positions[i], joint_limits_[i].first, joint_limits_[i].second));
    }
    mit_velocities_ = velocities;
    mit_torques_ = torques;
    UpdateStatus();
  }

  void Update(const std::array<float, 5> &current_positions, const std::array<float, 5> &current_velocities,
              Eigen::Vector3f imu_acc, float dt = 1.f) {
    current_positions_ = current_positions;

    // position 始终从 trajectory limiter 获取（MIT 模式下 limiter 仍会平滑插值）
    for (size_t i = 0; i < 5; ++i) {
      trajectory_limiter_[i].Update(dt);
      output_.positions[i] = trajectory_limiter_[i].current_position();
    }

    if (mit_mode_active_) {
      // MIT 模式：velocity/torque 由上位机直接指定，跳过动力学前馈
      for (size_t i = 0; i < 5; ++i) {
        output_.velocities[i] = mit_velocities_[i];
        output_.efforts[i] = mit_torques_[i];
      }
    } else {
      // 普通模式：velocity 清零，torque 由动力学前馈计算
      for (size_t i = 0; i < 5; ++i) {
        output_.velocities[i] = 0.f;
        output_.efforts[i] = 0.f;
      }

      const float acc_x = acc_lpf_[0].apply(imu_acc[0]);
      const float acc_y = acc_lpf_[1].apply(imu_acc[1]);
      const float acc_z = acc_lpf_[2].apply(imu_acc[2]);

      // 对j1 j2 j3叠加动力学前馈力矩
      const auto acc_comp_efforts = dynamics_.Compensate(
          {current_positions[0], current_positions[1], current_positions[2]}, {acc_x, acc_y, acc_z});
      for (int i = 0; i < 3; ++i) {
        output_.efforts[i] += acc_comp_efforts(i);
      }
    }

    UpdateStatus();

    if (status_ == kFault) {  // 故障状态下输出全零
      output_.velocities.fill(0.f);
      output_.efforts.fill(0.f);
    }
  }

  void ResetAt(const std::array<float, 5> &positions) {
    current_positions_ = positions;
    for (size_t i = 0; i < 5; ++i) {
      trajectory_limiter_[i].ResetAt(positions[i]);
    }
  }

  /**
   * @brief 控制器进入故障状态之后，如果确认故障已经排除，可以调用此函数清除故障状态
   * @note  把这个函数绑定到遥控器的一个通道上，当机械臂进入故障状态后，可以且仅能通过遥控器来清除故障状态
   */
  void FaultClear() {
    if (status_ == kFault) {
      ResetAt(current_positions_);
      fault_counter_ = 0;
      status_ = kIdle;
    }
  }

  /// @brief 退出 MIT 直通模式，恢复正常的重力补偿/动力学前馈
  void ForceExitMitMode() { mit_mode_active_ = false; }

  const auto &joint_limits() { return joint_limits_; }
  [[nodiscard]] const auto &current_positions() const { return current_positions_; }
  [[nodiscard]] const auto &output() const { return output_; }
  [[nodiscard]] Status status() const { return status_; }

 private:
  void UpdateStatus() {
    const bool all_limiters_converged = std::ranges::all_of(
        trajectory_limiter_, [](const rm::modules::TrajectoryLimiter &limiter) { return limiter.IsAtTarget(); });

    if (all_limiters_converged) {
      // 方便调试先禁用下面的判断逻辑
      status_ = kIdle;
      return;

      // 所有 limiter 都已经到达目标，检查机械臂是否真的到位
      const auto position_delta = [&] {
        std::array<float, 5> deltas;
        for (size_t i = 0; i < 5; ++i) {
          deltas[i] = std::abs(trajectory_limiter_[i].target_position() - current_positions_[i]);
        }
        return deltas;
      }();
      const bool arm_is_actually_at_target =
          std::ranges::all_of(position_delta, [](const float delta) { return delta < 0.1f; });  // 0.1 rad 容差
      if (arm_is_actually_at_target) {
        // limiter 已经执行到位，机械臂也确实到达了目标位置，认为任务完成，进入空闲状态
        fault_counter_ = 0;
        status_ = kIdle;
        return;
      }
      fault_counter_++;  // limiter 已经执行到位，但机械臂并没有到达目标位置，递增一次故障计数
      status_ =
          fault_counter_ >= 500
              ? kFault  // 500个周期仍然没有到达目标，可能是撞到东西了，也可能是电机离线没有反馈了，总之进入故障状态停止运行
              : kMoving;  // 500个周期以内暂时认为机械臂还在执行中
      return;
    }
    status_ = kMoving;
  }

  std::array<float, 5> current_positions_{};
  Output output_{};
  std::array<rm::modules::TrajectoryLimiter, 5> trajectory_limiter_;
  std::array<std::pair<float, float>, 5> joint_limits_;
  ArmDynamics dynamics_;
  rm::modules::LowPassFilter2p<float> acc_lpf_[3];
  Status status_{kIdle};
  int fault_counter_{0};

  bool mit_mode_active_{false};
  std::array<float, 5> mit_velocities_{};
  std::array<float, 5> mit_torques_{};
};