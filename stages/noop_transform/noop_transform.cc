#include "flowpipe/stage.h"

using namespace flowpipe;

class NoopTransform final : public ITransformStage {
public:
  std::string name() const override {
    return "noop_transform";
  }

  // Process a single payload.
  // Runtime handles dequeue/enqueue.
  void process(StageContext& ctx,
               const Payload& input,
               Payload& output) override {
    if (ctx.stop.stop_requested()) {
      return;
    }

    // Pass-through (shallow copy of view + metadata)
    output = input;
  }
};

extern "C" {

  IStage* flowpipe_create_stage() {
    return new NoopTransform();
  }

  void flowpipe_destroy_stage(IStage* stage) {
    delete stage;
  }

}  // extern "C"
