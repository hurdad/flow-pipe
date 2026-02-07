#include "flowpipe/stage.h"
#include "flowpipe/configurable_stage.h"
#include "flowpipe/observability/logging.h"
#include "flowpipe/plugin.h"
#include "flowpipe/protobuf_config.h"

#include "noop_transform.pb.h"

#include <google/protobuf/struct.pb.h>

#include <thread>
#include <chrono>
#include <string>

using namespace flowpipe;

using NoopTransformConfig =
    flowpipe::stages::noop::v1::NoopTransformConfig;

class NoopTransform final
    : public ITransformStage
    , public ConfigurableStage {
public:
  std::string name() const override {
    return "noop_transform";
  }

  NoopTransform() {
    FP_LOG_INFO("noop_transform constructed");
  }

  ~NoopTransform() override {
    FP_LOG_INFO("noop_transform destroyed");
  }

  // ------------------------------------------------------------
  // ConfigurableStage
  // ------------------------------------------------------------
  bool configure(const google::protobuf::Struct& config) override {
    NoopTransformConfig cfg;
    std::string error;
    if (!ProtobufConfigParser<NoopTransformConfig>::Parse(config, &cfg, &error)) {
      FP_LOG_ERROR(std::string("noop_transform invalid config: ") + error);
      return false;
    }

    config_ = std::move(cfg);

    FP_LOG_INFO("noop_transform configured");

    if (config_.verbose()) {
      FP_LOG_INFO("noop_transform verbose logging enabled");
    }

    if (config_.delay_ms() > 0) {
      FP_LOG_INFO("noop_transform delay enabled");
    }

    return true;
  }

  // ------------------------------------------------------------
  // ITransformStage
  // ------------------------------------------------------------
  void process(StageContext& ctx,
               const Payload& input,
               Payload& output) override {
    if (ctx.stop.stop_requested()) {
      return;
    }

    if (config_.verbose()) {
      FP_LOG_DEBUG("noop_transform processing payload");
    }

    if (config_.delay_ms() > 0) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(config_.delay_ms()));
    }

    // Pass-through
    output = input;
  }

private:
  NoopTransformConfig config_{};
};

extern "C" {

FLOWPIPE_PLUGIN_API IStage* flowpipe_create_stage() {
  FP_LOG_INFO("creating noop_transform stage");
  return new NoopTransform();
}

FLOWPIPE_PLUGIN_API void flowpipe_destroy_stage(IStage* stage) {
  FP_LOG_INFO("destroying noop_transform stage");
  delete stage;
}

}  // extern "C"
