#include "flowpipe/stage.h"
#include "flowpipe/plugin.h"

// Logging (plugin-safe)
#include "flowpipe/observability/logging.h"

using namespace flowpipe;

class NoopSource final : public ISourceStage {
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

  // Produce a single payload.
  // Return false to signal end-of-stream.
  bool produce(StageContext& ctx, Payload& out) override {
    if (ctx.stop.stop_requested()) {
      FP_LOG_DEBUG("noop_source stop requested, terminating source");
      return false;
    }

    // Intentionally no per-payload debug logging here.
    // Sources can be very hot; detailed logging belongs in the runtime.

    // Empty payload (noop)
    out.data = nullptr;
    out.size = 0;

    // Metadata (trace + timestamps) will be filled by runtime
    return true;
  }
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
