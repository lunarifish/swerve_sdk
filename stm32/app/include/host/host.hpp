#pragma once

#include <librm.hpp>

#include <etl/unordered_map.h>

#include "schema.hpp"

class Host : rm::device::CanDevice {
  using Callback = std::function<void()>;

 public:
  explicit Host(rm::hal::CanInterface &can, uint16_t base_frame_id)
      : CanDevice{can, base_frame_id + host_schema::EnableCommand::kFrameIdOffset,
                  base_frame_id + host_schema::ModeCtrlCommand::kFrameIdOffset,
                  base_frame_id + host_schema::JointCtrlCommand::kFrameIdOffset},
        base_frame_id_(base_frame_id) {}

  void RxCallback(const rm::hal::CanFrame *msg) override {
    switch (msg->rx_std_id - base_frame_id_) {
      case host_schema::EnableCommand::kFrameIdOffset: {
        std::memcpy(&rx_data_.enable_command, msg, sizeof(host_schema::EnableCommand));
        if (rx_callbacks_.contains(host_schema::EnableCommand::kFrameIdOffset)) {
          rx_callbacks_[host_schema::EnableCommand::kFrameIdOffset]();
        }
        break;
      }
      case host_schema::ModeCtrlCommand::kFrameIdOffset: {
        std::memcpy(&rx_data_.mode_ctrl_command, msg, sizeof(host_schema::ModeCtrlCommand));
        if (rx_callbacks_.contains(host_schema::ModeCtrlCommand::kFrameIdOffset)) {
          rx_callbacks_[host_schema::ModeCtrlCommand::kFrameIdOffset]();
        }
        break;
      }
      case host_schema::JointCtrlCommand::kFrameIdOffset: {
        std::memcpy(&rx_data_.joint_ctrl_command, msg, sizeof(host_schema::JointCtrlCommand));
        if (rx_callbacks_.contains(host_schema::JointCtrlCommand::kFrameIdOffset)) {
          rx_callbacks_[host_schema::JointCtrlCommand::kFrameIdOffset]();
        }
        break;
      }
      default:
        break;
    }
  }

  void OnEnableCommand(Callback cb) {
    rx_callbacks_.insert({host_schema::EnableCommand::kFrameIdOffset, std::move(cb)});
  }
  void OnModeCtrlCommand(Callback cb) {
    rx_callbacks_.insert({host_schema::ModeCtrlCommand::kFrameIdOffset, std::move(cb)});
  }
  void OnJointCtrlCommand(Callback cb) {
    rx_callbacks_.insert({host_schema::JointCtrlCommand::kFrameIdOffset, std::move(cb)});
  }

  [[nodiscard]] const auto &rx_data() const { return rx_data_; }

 protected:
  uint16_t base_frame_id_{};
  etl::unordered_map<uint16_t, Callback, 5> rx_callbacks_;

  struct {
    host_schema::EnableCommand enable_command{};
    host_schema::ModeCtrlCommand mode_ctrl_command{};
    host_schema::JointCtrlCommand joint_ctrl_command{};
  } rx_data_{};
};