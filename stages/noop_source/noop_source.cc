#include "flowpipe/stage.h"
#include "flowpipe/configurable_stage.h"
#include "flowpipe/observability/logging.h"
#include "flowpipe/plugin.h"
#include "flowpipe/protobuf_config.h"

#include "noop_source.pb.h"

#include <google/protobuf/struct.pb.h>

#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

using namespace flowpipe;

using NoopSourceConfig =
    flowpipe::stages::noop::source::v1::NoopSourceConfig;

class NoopSource final
    : public ISourceStage,
      public ConfigurableStage {
public:
  std::string name() const override {
    return "noop_source";
  }

  NoopSource() {
    FP_LOG_INFO("noop_source constructed");
  }

  ~NoopSource() override {
    FP_LOG_INFO("noop_source destroyed");
  }

  // ------------------------------------------------------------
  // ConfigurableStage
  // ------------------------------------------------------------
  bool Configure(const google::protobuf::Struct& config) override {
    NoopSourceConfig cfg;
    std::string error;
    if (!ProtobufConfigParser<NoopSourceConfig>::Parse(config, &cfg, &error)) {
      FP_LOG_ERROR_FMT("noop_source invalid config: {}", error);
      return false;
    }

    config_ = std::move(cfg);

    base_message_ =
        config_.message().empty()
            ? std::string("noop_source")
            : config_.message();

    delay_ = std::chrono::milliseconds(config_.delay_ms());
    max_messages_ = config_.max_messages();

    configured_ = true;

    FP_LOG_INFO("noop_source configured");
    return true;
  }

  // ------------------------------------------------------------
  // ISourceStage
  // ------------------------------------------------------------
  bool produce(StageContext& ctx, Payload& out) override {
    if (ctx.stop.stop_requested()) {
      FP_LOG_DEBUG("noop_source stop requested, terminating source");
      return false;
    }

    if (!configured_) {
      FP_LOG_ERROR("noop_source used before configuration");
      return false;
    }

    if (max_messages_ > 0 && counter_ >= max_messages_) {
      FP_LOG_INFO("noop_source reached max_messages, terminating");
      return false;
    }

    if (delay_.count() > 0) {
      std::this_thread::sleep_for(delay_);
    }

    // ----------------------------------------------------------
    // Build payload
    // ----------------------------------------------------------
    std::string msg =
        base_message_ + " #" + std::to_string(counter_);

    const size_t size = msg.size();

    // ----------------------------------------------------------
    // Allocate payload buffer with shared ownership
    // ----------------------------------------------------------
    auto buffer = AllocatePayloadBuffer(size);
    if (!buffer) {
      FP_LOG_ERROR("noop_source failed to allocate payload");
      return false;
    }

    std::memcpy(buffer.get(), msg.data(), size);

    out = Payload(std::move(buffer), size);

    // ----------------------------------------------------------
    // Debug: payload produced
    // ----------------------------------------------------------
    FP_LOG_DEBUG("noop_source produced payload");

    ++counter_;
    return true;
  }

private:
  // ------------------------------------------------------------
  // Configuration
  // ------------------------------------------------------------
  NoopSourceConfig config_{};
  bool configured_{false};

  // ------------------------------------------------------------
  // Runtime state
  // ------------------------------------------------------------
  uint64_t counter_{0};
  uint64_t max_messages_{0};
  std::string base_message_;
  std::chrono::milliseconds delay_{0};
};

extern "C" {

FLOWPIPE_PLUGIN_API IStage* flowpipe_create_stage() {
  FP_LOG_INFO("creating noop_source stage");
  return new NoopSource();
}

FLOWPIPE_PLUGIN_API void flowpipe_destroy_stage(IStage* stage) {
  FP_LOG_INFO("destroying noop_source stage");
  delete stage;
}

}  // extern "C"
