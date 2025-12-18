#pragma once

#include "bounded_queue.h"
#include "flowpipe/v1/flow.pb.h"
#include "payload.h"
#include <memory>
#include <string>

namespace flowpipe {

struct StageContext {
  StopToken stop;
};

struct IStage {
  virtual ~IStage() = default;
  virtual std::string name() const = 0;
};

struct ISourceStage : IStage {
  virtual void run(StageContext &ctx, BoundedQueue<Payload> &output) = 0;
};

struct ITransformStage : IStage {
  virtual void run(StageContext &ctx, BoundedQueue<Payload> &input,
                   BoundedQueue<Payload> &output) = 0;
};

struct ISinkStage : IStage {
  virtual void run(StageContext &ctx, BoundedQueue<Payload> &input) = 0;
};

} // namespace flowpipe
