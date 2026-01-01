#include "flowpipe/stage.h"

using namespace flowpipe;

class NoopSource final : public ISourceStage {
public:
  std::string name() const override {
    return "noop_source";
  }

  // Produce a single payload.
  // Return false to signal end-of-stream.
  bool produce(StageContext& ctx, Payload& out) override {
    if (ctx.stop.stop_requested()) {
      return false;
    }

    // Empty payload (noop)
    out.data = nullptr;
    out.size = 0;

    // Metadata (trace + timestamps) will be filled by runtime
    return true;
  }
};

extern "C" {

  IStage* flowpipe_create_stage() {
    return new NoopSource();
  }

  void flowpipe_destroy_stage(IStage* stage) {
    delete stage;
  }

}  // extern "C"
