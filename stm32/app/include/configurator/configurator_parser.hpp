#pragma once

#include <cstdint>
#include <cstring>
#include <array>
#include <functional>

#include <etl/span.h>
#include <etl/vector.h>

#include "librm/modules/crc.hpp"
#include "configurator/comm_schema.hpp"

/**
 * @brief 上位机配置协议字节流分包器。
 *
 * 参考 librm Referee 的设计模式（FSM + operator<< 逐字节喂入），
 * 解析 schema.hpp 中定义的帧格式：
 *
 *   [header:1] [payload_len:2] [seq:1] [header_crc8:1]
 *   [cmd_id:2] [payload:N] [crc16:2]
 *
 * 帧头 0xA5，header_crc8 校验前 4 字节，crc16 校验帧尾之前的所有数据。
 *
 * Usage:
 * @code
 *   ConfiguratorParser parser;
 *   parser.AttachCallback([](uint16_t cmd_id, etl::span<const uint8_t> payload,
 *                            uint8_t seq) {
 *     if (cmd_id == configurator_schema::details::JointConfig::kCmdId) {
 *       auto &cfg = *reinterpret_cast<const
 * configurator_schema::details::JointConfig *>(payload.data());
 *       float kp = cfg.kp;
 *     }
 *   });
 *   // 逐字节喂入
 *   while (serial.available()) {
 *     parser << serial.read();
 *   }
 * @endcode
 */
class ConfiguratorParser {
 public:
  /** @brief 协议常量 */
  static constexpr uint8_t kSof = 0xA5;
  static constexpr size_t kMaxPayloadLen = 128;
  static constexpr size_t kHeaderLen = 5;  // header + payload_len(2) + seq + header_crc8
  static constexpr size_t kCmdIdLen = 2;
  static constexpr size_t kCrc16Len = 2;
  static constexpr size_t kMaxFrameLen = kHeaderLen + kCmdIdLen + kMaxPayloadLen + kCrc16Len;

  /**
   * @brief 接收回调类型
   * @param cmd_id  命令码 (0x100 / 0x101 / 0x102)
   * @param payload 载荷数据视图（指向 parser 内部缓冲区，仅回调内有效）
   * @param seq     包序号
   */
  using RxCallback = std::function<void(uint16_t cmd_id, etl::span<const uint8_t> payload, uint8_t seq)>;

  /**
   * @brief 逐字节喂入数据，内部 FSM 自动完成分包与校验
   */
  void operator<<(uint8_t data) {
    switch (state_) {
      case State::kSof: {
        if (data == kSof) {
          buf_[0] = data;
          buf_idx_ = 1;
          state_ = State::kLenLsb;
        }
        break;
      }

      case State::kLenLsb: {
        buf_[buf_idx_++] = data;
        state_ = State::kLenMsb;
        break;
      }

      case State::kLenMsb: {
        buf_[buf_idx_++] = data;
        payload_len_ = buf_[1] | (static_cast<uint16_t>(buf_[2]) << 8);
        if (payload_len_ <= kMaxPayloadLen) {
          state_ = State::kSeq;
        } else {
          error_count_++;
          Reset();
        }
        break;
      }

      case State::kSeq: {
        buf_[buf_idx_++] = data;
        state_ = State::kCrc8;
        break;
      }

      case State::kCrc8: {
        buf_[buf_idx_++] = data;
        if (rm::modules::Crc8(buf_, kHeaderLen - 1, rm::modules::CRC8_INIT) == data) {
          state_ = State::kCmdIdLsb;
        } else {
          error_count_++;
          Reset();
        }
        break;
      }

      case State::kCmdIdLsb: {
        buf_[buf_idx_++] = data;
        state_ = State::kCmdIdMsb;
        break;
      }

      case State::kCmdIdMsb: {
        buf_[buf_idx_++] = data;
        state_ = State::kPayload;
        break;
      }

      case State::kPayload: {
        if (state_ == State::kPayload) {
          buf_[buf_idx_++] = data;
        }

        const size_t total_len = kHeaderLen + kCmdIdLen + payload_len_ + kCrc16Len;
        if (buf_idx_ >= total_len) {
          const uint16_t received_crc =
              buf_[total_len - 2] | (static_cast<uint16_t>(buf_[total_len - 1]) << 8);
          const uint16_t computed_crc =
              rm::modules::Crc16(buf_, total_len - kCrc16Len, rm::modules::CRC16_INIT);

          if (received_crc == computed_crc) {
            const uint16_t cmd_id = buf_[5] | (static_cast<uint16_t>(buf_[6]) << 8);
            const uint8_t seq = buf_[3];
            auto payload = etl::span<const uint8_t>{buf_ + kHeaderLen + kCmdIdLen, payload_len_};

            for (auto &cb : rx_callbacks_) {
              if (cb) {
                cb(cmd_id, payload, seq);
              }
            }
          } else {
            error_count_++;
          }
          Reset();
        }
        break;
      }
    }
  }

  /** @brief 注册接收回调 */
  void AttachCallback(const RxCallback &callback) { rx_callbacks_.push_back(callback); }

  /** @brief 校验错误累计次数 */
  size_t error_count() const { return error_count_; }

 private:
  enum class State {
    kSof,        // 等待 0xA5
    kLenLsb,     // payload_len 低字节
    kLenMsb,     // payload_len 高字节 + 长度校验
    kSeq,        // 包序号
    kCrc8,       // header CRC8 校验
    kCmdIdLsb,   // cmd_id 低字节
    kCmdIdMsb,   // cmd_id 高字节
    kPayload,    // 吸收 payload + crc16 → 校验 → 分发
  };

  /** @brief 原子重置 FSM */
  void Reset() {
    state_ = State::kSof;
    buf_idx_ = 0;
  }

  State state_{State::kSof};
  uint8_t buf_[kMaxFrameLen]{};
  size_t buf_idx_{0};
  uint16_t payload_len_{0};
  etl::vector<RxCallback, 4> rx_callbacks_;
  size_t error_count_{0};
};
