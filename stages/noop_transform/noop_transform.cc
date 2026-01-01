#include "flowpipe/stage.h"

// Logging (plugin-safe)
#include "flowpipe/observability/logging.h"

using namespace flowpipe;

class NoopTransform final : public ITransformStage {
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

  // Process a single payload.
  // Runtime handles dequeue/enqueue.
  void process(StageContext& ctx,
               const Payload& input,
               Payload& output) override {
    if (ctx.stop.stop_requested()) {
      FP_LOG_DEBUG("noop_transform stop requested, skipping payload");
      return;
    }

    // Plugin-safe debug logging (no formatting helpers)
    FP_LOG_DEBUG("noop_transform processing payload");

    // Pass-through (shallow copy of view + metadata)
    output = input;
  }
};

extern "C" {

  IStage* flowpipe_create_stage() {
    FP_LOG_INFO("creating noop_transform stage");
    return new NoopTransform();
  }

  void flowpipe_destroy_stage(IStage* stage) {
    FP_LOG_INFO("destroying noop_transform stage");
    delete stage;
  }

}  // extern "C"
