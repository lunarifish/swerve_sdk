#pragma once

#include <cstring>

#include "usbd_cdc_if.h"

#include "configurator_parser.hpp"
#include "comm_schema.hpp"

class Configurator {
 public:
  void Init();

  void LogInfo(const char *str) {
    using Frame = configurator::CmdFrame<configurator::details::LogString>;
    Frame frame{};
    frame.payload.level = configurator::details::LogString::kInfo;
    std::strncpy(reinterpret_cast<char *>(frame.payload.str), str, sizeof(frame.payload.str) - 1);
    configurator::PreProcessFrame(frame);
    CDC_Transmit_HS(reinterpret_cast<uint8_t *>(&frame), sizeof(Frame));
  }

 private:
  ConfiguratorParser parser_;
};