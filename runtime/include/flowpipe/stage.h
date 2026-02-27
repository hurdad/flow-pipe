#pragma once

#include <string>

#include "payload.h"
#include "stop_token.h"

namespace flowpipe {

/**
 * Execution context passed to all stages.
 * Contains cooperative cancellation only.
 *
 * Runtime concerns (metrics, queues, timing)
 * are intentionally NOT exposed here.
 */
struct StageContext {
  StopToken stop;

  void request_stop() {
    stop.request_stop();
  }
};

/**
 * Base stage interface.
 */
struct IStage {
  virtual ~IStage() = default;
  virtual std::string name() const = 0;
};

/**
 * Source stage
 *
 * Produces payloads.
 * Return false to indicate end-of-stream.
 */
struct ISourceStage : IStage {
  virtual bool produce(StageContext& ctx, Payload& out) = 0;
};

/**
 * Transform stage
 *
 * Transforms one input payload into one output payload.
 * (Later you can extend this to zero-or-more outputs.)
 */
struct ITransformStage : IStage {
  virtual void process(StageContext& ctx, const Payload& input, Payload& output) = 0;
};

/**
 * Sink stage
 *
 * Consumes payloads.
 */
struct ISinkStage : IStage {
  virtual void consume(StageContext& ctx, const Payload& input) = 0;
};

}  // namespace flowpipe
