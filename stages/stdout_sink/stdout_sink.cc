#include "flowpipe/stage.h"
#include "flowpipe/plugin.h"

// Logging (plugin-safe)
#include "flowpipe/observability/logging.h"

using namespace flowpipe;

class StdoutSink final : public ISinkStage {
public:
  std::string name() const override {
    return "stdout_sink";
  }

  StdoutSink() {
    FP_LOG_INFO("stdout_sink constructed");
  }

  ~StdoutSink() override {
    FP_LOG_INFO("stdout_sink destroyed");
  }

  // Called once per payload by the runtime
  void consume(StageContext& ctx, const Payload& /*payload*/) override {
    if (ctx.stop.stop_requested()) {
      FP_LOG_DEBUG("stdout_sink stop requested, skipping payload");
      return;
    }

    // Plugin-safe per-payload intent log (no formatting)
    FP_LOG_DEBUG("stdout_sink consumed payload");
  }
};

extern "C" {

  FLOWPIPE_PLUGIN_API IStage* flowpipe_create_stage() {
    FP_LOG_INFO("creating stdout_sink stage");
    return new StdoutSink();
  }

  FLOWPIPE_PLUGIN_API void flowpipe_destroy_stage(IStage* stage) {
    FP_LOG_INFO("destroying stdout_sink stage");
    delete stage;
  }

}  // extern "C"
