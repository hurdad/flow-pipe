#include "flowpipe/stage.h"

using namespace flowpipe;

class NoopSource final : public ISourceStage {
public:
  std::string name() const override {
    return "noop_source";
  }

  void run(StageContext& ctx,
           BoundedQueue<Payload>& out) override {
    while (!ctx.stop.stop_requested()) {
      Payload p{};
      if (!out.push(std::move(p), ctx.stop)) {
        break;
      }
    }
  }
};

extern "C" {

  IStage* flowpipe_create_stage() {
    return new NoopSource();
  }

  void flowpipe_destroy_stage(IStage* stage) {
    delete stage;
  }

}
