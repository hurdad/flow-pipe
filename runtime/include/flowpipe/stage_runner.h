#pragma once

#include "flowpipe/queue_runtime.h"
#include "flowpipe/stage.h"
#include "flowpipe/stage_metrics.h"

namespace flowpipe {

/**
 * Runtime wrapper for source stages.
 *
 * Owns:
 *  - execution loop
 *  - stage latency measurement
 *
 * Does NOT:
 *  - modify stage behavior
 *  - expose metrics to plugins
 */
void RunSourceStage(ISourceStage* stage, StageContext& ctx, QueueRuntime& output,
                    StageMetrics* metrics);

/**
 * Runtime wrapper for transform stages.
 *
 * Owns:
 *  - dequeue
 *  - queue latency metrics
 *  - stage execution latency
 *
 * Stage remains unaware of metrics and timing.
 */
void RunTransformStage(ITransformStage* stage, StageContext& ctx, QueueRuntime& input,
                       QueueRuntime& output, StageMetrics* metrics);

/**
 * Runtime wrapper for sink stages.
 *
 * Owns:
 *  - dequeue
 *  - queue latency metrics
 *  - stage execution latency
 */
void RunSinkStage(ISinkStage* stage, StageContext& ctx, QueueRuntime& input, StageMetrics* metrics);

}  // namespace flowpipe
