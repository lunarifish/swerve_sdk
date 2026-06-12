#include "configurator/configurator.hpp"

#include "main.h"
#include "shared_resources.hpp"

namespace {
ConfiguratorParser *g_parser{nullptr};
}  // namespace

extern "C" void ConfiguratorFeedBytes(uint8_t *buf, uint32_t len) {
  if (g_parser) {
    for (uint32_t i = 0; i < len; ++i) {
      *g_parser << buf[i];
    }
  }
}

void Configurator::Init() {
  g_parser = &parser_;

  parser_.AttachCallback([&](uint16_t cmd_id, etl::span<const uint8_t> payload, uint8_t seq) {
    switch (cmd_id) {
      case configurator::details::ReadRequest::kCmdId: {
        const auto &incoming = *reinterpret_cast<const configurator::details::ReadRequest *>(payload.data());
        switch (incoming.type) {
          case configurator::details::ReadRequest::kAllConfig: {
            using ReplyFrame = configurator::CmdFrame<configurator::details::AllConfig>;
            ReplyFrame reply{};
            reply.payload.config = SharedResources::GetInstance().flash_prefs.data();
            reply.seq = seq;
            configurator::PreProcessFrame(reply);
            CDC_Transmit_HS(reinterpret_cast<uint8_t *>(&reply), sizeof(ReplyFrame));
            break;
          }
          case configurator::details::ReadRequest::kFirmwareCommit: {
            using ReplyFrame = configurator::CmdFrame<configurator::details::CommitId>;
            ReplyFrame reply{};
            std::memcpy(reply.payload.str, GIT_COMMIT_ID, sizeof(reply.payload.str));
            reply.seq = seq;
            configurator::PreProcessFrame(reply);
            CDC_Transmit_HS(reinterpret_cast<uint8_t *>(&reply), sizeof(ReplyFrame));
            break;
          }
        }
        break;
      }
      case configurator::details::RebootRequest::kCmdId: {
        __ASM volatile("cpsid i");  // 进入临界区，禁止所有中断
        NVIC_SystemReset();         // 重启
      }
      case configurator::details::AllConfig::kCmdId: {
        if (payload.size() < sizeof(configurator::details::AllConfig)) break;

        const auto &incoming = *reinterpret_cast<const configurator::details::AllConfig *>(payload.data());
        const auto &cfg = incoming.config;

        using AckFrame = configurator::CmdFrame<configurator::details::ConfigWriteAck>;
        AckFrame ack{};
        ack.seq = seq;

        if (Validate(cfg)) {
          auto &prefs = SharedResources::GetInstance().flash_prefs;
          prefs.data() = cfg;
          prefs.Commit();
          ack.payload.ok = true;
        } else {
          ack.payload.ok = false;
          const char reason[] = "Config validation failed, rejected";
          std::memcpy(ack.payload.str_reason, reason, sizeof(reason));
        }

        configurator::PreProcessFrame(ack);
        CDC_Transmit_HS(reinterpret_cast<uint8_t *>(&ack), sizeof(AckFrame));
        break;
      }
    }
  });
}