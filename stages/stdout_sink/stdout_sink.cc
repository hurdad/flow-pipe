#include "flowpipe/stage.h"
#include "flowpipe/plugin.h"

// Logging (plugin-safe)
#include "flowpipe/observability/logging.h"

#include <cstdio>

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
  void consume(StageContext& ctx, const Payload& payload) override {
    if (ctx.stop.stop_requested()) {
      FP_LOG_DEBUG("stdout_sink stop requested, skipping payload");
      return;
    }

    if (payload.empty()) {
      FP_LOG_DEBUG("stdout_sink received empty payload");
      return;
    }

    // ----------------------------------------------------------
    // Write payload bytes to stdout
    // ----------------------------------------------------------
    std::fwrite(payload.data(), 1, payload.size, stdout);
    std::fwrite("\n", 1, 1, stdout);
    std::fflush(stdout);
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
