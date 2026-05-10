#pragma once

#include <cstdint>

#include <etl/platform.h>

namespace configurator_schema {

template <uint16_t PayloadLen, uint16_t CmdId>
ETL_PACKED_STRUCT(CmdFrame) {
  // frame header
  const uint8_t header{0xa5};
  uint16_t payload_len{PayloadLen};
  uint8_t seq;
  uint8_t header_crc8;

  // cmd_id
  uint16_t cmd_id{CmdId};

  // payload
  uint8_t payload[PayloadLen];

  // tail
  uint16_t crc16;
};
}  // namespace configurator_schema