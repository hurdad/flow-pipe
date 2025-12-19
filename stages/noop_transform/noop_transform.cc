#include "flowpipe/stage.h"

using namespace flowpipe;

class NoopTransform final : public ITransformStage {
public:
  std::string name() const override {
    return "noop_transform";
  }

  void run(StageContext& ctx,
           BoundedQueue<Payload>& in,
           BoundedQueue<Payload>& out) override {
    while (!ctx.stop.stop_requested()) {
      auto item = in.pop(ctx.stop);
      if (!item.has_value()) {
        break;
      }

      if (!out.push(std::move(*item), ctx.stop)) {
        break;
      }
    }
  }
};

extern "C" {

  IStage* flowpipe_create_stage() {
    return new NoopTransform();
  }

  void flowpipe_destroy_stage(IStage* stage) {
    delete stage;
  }

}
