#include "flowpipe/stage.h"

#include <iostream>

using namespace flowpipe;

class StdoutSink final : public ISinkStage {
public:
  std::string name() const override {
    return "stdout_sink";
  }

  void run(StageContext& ctx,
           BoundedQueue<Payload>& in) override {
    while (!ctx.stop.stop_requested()) {
      auto item = in.pop(ctx.stop);
      if (!item.has_value()) {
        break;
      }
      std::cout << "payload received\n";
    }
  }
};

extern "C" {

  IStage* flowpipe_create_stage() {
    return new StdoutSink();
  }

  void flowpipe_destroy_stage(IStage* stage) {
    delete stage;
  }

}
