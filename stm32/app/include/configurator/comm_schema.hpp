#pragma once

#include <cstdint>
#include <cstring>

#include <etl/platform.h>

#include "librm/modules/crc.hpp"
#include "config_schema.hpp"

namespace configurator {

namespace details {
ETL_PACKED_STRUCT(LogString) {
  constexpr static uint16_t kCmdId = 0x100;

  enum LogLevel {
    kDebug = 0u,
    kInfo,
    kWarning,
    kError,
  } level;
  uint8_t str[64 - 9];
};
ETL_PACKED_STRUCT(AllConfig) {
  constexpr static uint16_t kCmdId = 0x102;

  PersistableConfigV1 config;
};
ETL_PACKED_STRUCT(CommitId) {
  constexpr static uint16_t kCmdId = 0x103;

  uint8_t str[7];
};
ETL_PACKED_STRUCT(ConfigWriteAck) {
  constexpr static uint16_t kCmdId = 0x104;

  bool ok;
  uint8_t str_reason[64 - 9];
};

ETL_PACKED_STRUCT(ReadRequest) {
  constexpr static uint16_t kCmdId = 0x200;
  enum Type {
    kAllConfig = 0u,
    kFirmwareCommit,
  } type;
};
ETL_PACKED_STRUCT(RebootRequest) {
  constexpr static uint16_t kCmdId = 0x201;

  bool into_dfu_mode;
};
}  // namespace details

template <typename PayloadType>
ETL_PACKED_STRUCT(CmdFrame) {
  // frame header
  const uint8_t header{0xa5};
  uint16_t payload_len{sizeof(PayloadType)};
  uint8_t seq;
  uint8_t header_crc8;

  // cmd_id
  uint16_t cmd_id{PayloadType::kCmdId};

  // payload
  PayloadType payload;

  // tail
  uint16_t crc16;
};

template <typename PayloadType>
void PreProcessFrame(CmdFrame<PayloadType> &frame) {
  auto *raw = reinterpret_cast<uint8_t *>(&frame);
  constexpr size_t total_len = sizeof(CmdFrame<PayloadType>);

  frame.header_crc8 = rm::modules::Crc8(raw, 4, rm::modules::CRC8_INIT);
  frame.crc16 = rm::modules::Crc16(raw, total_len - 2, rm::modules::CRC16_INIT);
}

}  // namespace configurator