#include "flowpipe/stage.h"

#include <iostream>

using namespace flowpipe;

class StdoutSink final : public ISinkStage {
public:
  std::string name() const override {
    return "stdout_sink";
  }

  // Called once per payload by the runtime
  void consume(StageContext& ctx, const Payload& payload) override {
    if (ctx.stop.stop_requested()) {
      return;
    }

    // Example usage
    std::cout << "payload received (size=" << payload.size << ")\n";

    // You may inspect metadata if you want:
    // payload.meta.enqueue_ts_ns
    // payload.meta.trace_id / span_id
  }
};

extern "C" {

  IStage* flowpipe_create_stage() {
    return new StdoutSink();
  }

  void flowpipe_destroy_stage(IStage* stage) {
    delete stage;
  }

}  // extern "C"
