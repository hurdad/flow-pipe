#include "flowpipe/stage.h"

namespace flowpipe {

class NoopTransform final : public ITransformStage {
public:
  explicit NoopTransform(const flowpipe::v1::StageSpec &spec) : name_(spec.name()) {}

  std::string name() const override { return name_; }

  void run(StageContext &ctx, BoundedQueue<Payload> &in,
           BoundedQueue<Payload> &out) override {
    while (!ctx.stop.stop_requested()) {
      auto item = in.pop(ctx.stop);
      if (!item)
        break;
      out.push(std::move(*item), ctx.stop);
    }
    out.close();
  }

private:
  std::string name_;
};

std::shared_ptr<IStage> MakeNoopTransform(const flowpipe::v1::StageSpec &spec) {
  return std::make_shared<NoopTransform>(spec);
}

} // namespace flowpipe
