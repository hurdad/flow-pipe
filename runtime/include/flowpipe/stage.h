#pragma once

#include <string>

#include "bounded_queue.h"
#include "payload.h"

namespace flowpipe {

struct StageContext {
  StopToken stop;
};

struct IStage {
  virtual ~IStage() = default;
  virtual std::string name() const = 0;
};

struct ISourceStage : IStage {
  virtual void run(StageContext& ctx, BoundedQueue<Payload>& output) = 0;
};

struct ITransformStage : IStage {
  virtual void run(StageContext& ctx, BoundedQueue<Payload>& input,
                   BoundedQueue<Payload>& output) = 0;
};

struct ISinkStage : IStage {
  virtual void run(StageContext& ctx, BoundedQueue<Payload>& input) = 0;
};

}  // namespace flowpipe
