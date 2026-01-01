#include "flowpipe/runtime.h"

#include <atomic>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include "flowpipe/queue_runtime.h"
#include "flowpipe/signal_handler.h"
#include "flowpipe/stage_runner.h"

// Logging
#include "flowpipe/observability/logging.h"

namespace flowpipe {

Runtime::Runtime() = default;

int Runtime::run(const flowpipe::v1::FlowSpec& spec) {
  FP_LOG_INFO_FMT("runtime starting: {} stages, {} queues", spec.stages_size(), spec.queues_size());

  std::atomic<bool> stop_flag{false};
  SignalHandler::install(stop_flag);
  StopToken stop{&stop_flag};

  // ------------------------------------------------------------
  // Create runtime queues (QueueRuntime)
  // ------------------------------------------------------------
  std::unordered_map<std::string, std::shared_ptr<QueueRuntime>> queues;

  for (const auto& q : spec.queues()) {
    FP_LOG_DEBUG_FMT("configuring queue '{}' type={} capacity={}", q.name(), q.type(),
                     q.capacity());

    if (q.capacity() == 0) {
      FP_LOG_ERROR_FMT("invalid queue '{}': capacity must be > 0", q.name());
      throw std::runtime_error("queue capacity must be > 0: " + q.name());
    }

    if (q.type() == flowpipe::v1::QUEUE_TYPE_UNSPECIFIED) {
      FP_LOG_ERROR_FMT("invalid queue '{}': type unspecified", q.name());
      throw std::runtime_error("queue type unspecified: " + q.name());
    }

    auto qr = std::make_shared<QueueRuntime>();
    qr->name = q.name();
    qr->type = q.type();
    qr->capacity = q.capacity();

    // For now both MPSC / MPMC map to the same implementation
    qr->queue = std::make_shared<BoundedQueue<Payload>>(q.capacity());

    queues.emplace(qr->name, std::move(qr));
  }

  FP_LOG_INFO_FMT("initialized {} runtime queues", queues.size());

  // ------------------------------------------------------------
  // Shared context + metrics
  // ------------------------------------------------------------
  StageContext ctx{stop};
  StageMetrics metrics;  // runtime-owned metrics facade

  std::vector<std::thread> threads;

  // ------------------------------------------------------------
  // Wire stages (runtime owns execution)
  // ------------------------------------------------------------
  for (const auto& s : spec.stages()) {
    FP_LOG_INFO_FMT("initializing stage '{}' type={} threads={}", s.name(), s.type(), s.threads());

    const bool has_input = s.has_input_queue();
    const bool has_output = s.has_output_queue();

    IStage* stage =
        registry_.create_stage(s.has_plugin() ? s.plugin() : "libstage_" + s.type() + ".so");

    // ----------------------------
    // Source stage
    // ----------------------------
    if (auto* src = dynamic_cast<ISourceStage*>(stage)) {
      FP_LOG_DEBUG_FMT("stage '{}' detected as SOURCE", s.name());

      if (has_input || !has_output) {
        FP_LOG_ERROR_FMT("invalid source stage wiring for '{}'", s.name());
        throw std::runtime_error("invalid source stage wiring: " + s.name());
      }

      auto out = queues.at(s.output_queue());

      for (uint32_t i = 0; i < s.threads(); ++i) {
        threads.emplace_back([&, src, out, i]() {
          FP_LOG_DEBUG_FMT("stage '{}' source worker {} started", s.name(), i);

          RunSourceStage(src, ctx, *out, &metrics);

          FP_LOG_DEBUG_FMT("stage '{}' source worker {} stopped", s.name(), i);
        });
      }
      continue;
    }

    // ----------------------------
    // Transform stage
    // ----------------------------
    if (auto* xf = dynamic_cast<ITransformStage*>(stage)) {
      FP_LOG_DEBUG_FMT("stage '{}' detected as TRANSFORM", s.name());

      if (!has_input || !has_output) {
        FP_LOG_ERROR_FMT("invalid transform stage wiring for '{}'", s.name());
        throw std::runtime_error("invalid transform stage wiring: " + s.name());
      }

      auto in = queues.at(s.input_queue());
      auto out = queues.at(s.output_queue());

      for (uint32_t i = 0; i < s.threads(); ++i) {
        threads.emplace_back([&, xf, in, out, i]() {
          FP_LOG_DEBUG_FMT("stage '{}' transform worker {} started", s.name(), i);

          RunTransformStage(xf, ctx, *in, *out, &metrics);

          FP_LOG_DEBUG_FMT("stage '{}' transform worker {} stopped", s.name(), i);
        });
      }
      continue;
    }

    // ----------------------------
    // Sink stage
    // ----------------------------
    if (auto* sink = dynamic_cast<ISinkStage*>(stage)) {
      FP_LOG_DEBUG_FMT("stage '{}' detected as SINK", s.name());

      if (!has_input || has_output) {
        FP_LOG_ERROR_FMT("invalid sink stage wiring for '{}'", s.name());
        throw std::runtime_error("invalid sink stage wiring: " + s.name());
      }

      auto in = queues.at(s.input_queue());

      for (uint32_t i = 0; i < s.threads(); ++i) {
        threads.emplace_back([&, sink, in, i]() {
          FP_LOG_DEBUG_FMT("stage '{}' sink worker {} started", s.name(), i);

          RunSinkStage(sink, ctx, *in, &metrics);

          FP_LOG_DEBUG_FMT("stage '{}' sink worker {} stopped", s.name(), i);
        });
      }
      continue;
    }

    FP_LOG_ERROR_FMT("stage '{}' does not implement a valid interface", s.name());
    throw std::runtime_error("stage does not implement a valid interface: " + s.name());
  }

  FP_LOG_INFO_FMT("runtime started {} worker threads", threads.size());

  // ------------------------------------------------------------
  // Join
  // ------------------------------------------------------------
  for (auto& t : threads) {
    t.join();
  }

  FP_LOG_INFO("runtime shutting down");

  registry_.shutdown();

  FP_LOG_INFO("runtime exited cleanly");
  return 0;
}

}  // namespace flowpipe
