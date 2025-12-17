#include "flowpipe/stage.h"
#include <iostream>

namespace flowpipe {

class StdoutSink final : public ISinkStage {
public:
  explicit StdoutSink(const StageSpec &spec) : name_(spec.name) {}

  std::string name() const override { return name_; }

  void run(StageContext &ctx, BoundedQueue<Payload> &in) override {
    while (!ctx.stop.stop_requested()) {
      auto item = in.pop(ctx.stop);
      if (!item)
        break;
      std::cout << *item << std::endl;
    }
  }

private:
  std::string name_;
};

std::shared_ptr<IStage> MakeStdoutSink(const StageSpec &spec) {
  return std::make_shared<StdoutSink>(spec);
}

} // namespace flowpipe
