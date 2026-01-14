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
#include "flowpipe/observability/logging_runtime.h"

namespace flowpipe {

Runtime::Runtime() = default;

int Runtime::run(const flowpipe::v1::FlowSpec& spec) {
  FP_LOG_INFO_FMT("runtime starting: {} stages, {} queues", spec.stages_size(), spec.queues_size());

  // Shared stop flag toggled by the signal handler for coordinated shutdown.
  std::atomic<bool> stop_flag{false};
  StopToken stop{&stop_flag};

  // ------------------------------------------------------------
  // Create runtime queues (QueueRuntime)
  // ------------------------------------------------------------
  std::unordered_map<std::string, std::shared_ptr<QueueRuntime>> queues;

  for (const auto& q : spec.queues()) {
    FP_LOG_DEBUG_FMT("configuring queue '{}' capacity={}", q.name(), q.capacity());

    if (q.capacity() == 0) {
      FP_LOG_ERROR_FMT("invalid queue '{}': capacity must be > 0", q.name());
      throw std::runtime_error("queue capacity must be > 0: " + q.name());
    }

    if (q.has_schema() && q.schema().schema_id().empty()) {
      FP_LOG_ERROR_FMT("invalid queue '{}': schema_id is required when schema is set", q.name());
      throw std::runtime_error("queue schema_id is required: " + q.name());
    }

    if (queues.find(q.name()) != queues.end()) {
      FP_LOG_ERROR_FMT("duplicate queue name '{}'", q.name());
      throw std::runtime_error("duplicate queue name: " + q.name());
    }

    auto qr = std::make_shared<QueueRuntime>();
    qr->name = q.name();
    qr->capacity = q.capacity();
    if (q.has_schema()) {
      qr->schema_id = q.schema().schema_id();
    }

    qr->queue = std::make_shared<BoundedQueue<Payload>>(q.capacity());

    queues.emplace(qr->name, std::move(qr));
  }

  FP_LOG_INFO_FMT("initialized {} runtime queues", queues.size());

  std::vector<std::shared_ptr<BoundedQueue<Payload>>> runtime_queues;
  runtime_queues.reserve(queues.size());
  for (const auto& [name, queue_runtime] : queues) {
    (void)name;
    runtime_queues.push_back(queue_runtime->queue);
  }

  SignalHandler::install(stop_flag, [runtime_queues]() {
    for (const auto& queue : runtime_queues) {
      queue->close();
    }
  });

  // ------------------------------------------------------------
  // Shared context + metrics
  // ------------------------------------------------------------
  // Runtime-owned context and metrics facade shared by all stage workers.
  StageContext ctx{stop};
  StageMetrics metrics;

  std::vector<std::thread> threads;

  // ------------------------------------------------------------
  // Wire stages (runtime owns execution)
  // ------------------------------------------------------------
  for (const auto& s : spec.stages()) {
    FP_LOG_INFO_FMT("initializing stage '{}' type={} threads={}", s.name(), s.type(), s.threads());

    if (s.threads() < 1) {
      FP_LOG_ERROR_FMT("invalid stage '{}': threads must be >= 1", s.name());
      throw std::runtime_error("stage threads must be >= 1: " + s.name());
    }

    const bool has_input = s.has_input_queue();
    const bool has_output = s.has_output_queue();

    // Resolve plugin name: explicit plugin wins, otherwise default to type-based naming.
    const std::string plugin_name =
        s.has_plugin() ? s.plugin() : "libstage_" + s.type() + ".so";
    IStage* stage = registry_.create_stage(plugin_name, &s.config());
    enum class StageKind { kSource, kTransform, kSink };
    StageKind kind;

    // ----------------------------
    // Source stage
    // ----------------------------
    if (dynamic_cast<ISourceStage*>(stage)) {
      kind = StageKind::kSource;
      FP_LOG_DEBUG_FMT("stage '{}' detected as SOURCE", s.name());

      if (has_input || !has_output) {
        FP_LOG_ERROR_FMT("invalid source stage wiring for '{}'", s.name());
        registry_.destroy_stage(stage);
        throw std::runtime_error("invalid source stage wiring: " + s.name());
      }

    } else if (dynamic_cast<ITransformStage*>(stage)) {
      kind = StageKind::kTransform;
      FP_LOG_DEBUG_FMT("stage '{}' detected as TRANSFORM", s.name());

      if (!has_input || !has_output) {
        FP_LOG_ERROR_FMT("invalid transform stage wiring for '{}'", s.name());
        registry_.destroy_stage(stage);
        throw std::runtime_error("invalid transform stage wiring: " + s.name());
      }
    } else if (dynamic_cast<ISinkStage*>(stage)) {
      kind = StageKind::kSink;
      FP_LOG_DEBUG_FMT("stage '{}' detected as SINK", s.name());

      if (!has_input || has_output) {
        FP_LOG_ERROR_FMT("invalid sink stage wiring for '{}'", s.name());
        registry_.destroy_stage(stage);
        throw std::runtime_error("invalid sink stage wiring: " + s.name());
      }
    } else {
      FP_LOG_ERROR_FMT("stage '{}' does not implement a valid interface", s.name());
      registry_.destroy_stage(stage);
      throw std::runtime_error("stage does not implement a valid interface: " + s.name());
    }

    registry_.destroy_stage(stage);

    if (kind == StageKind::kSource) {
      auto out = queues.at(s.output_queue());
      for (uint32_t i = 0; i < s.threads(); ++i) {
        IStage* worker_stage = registry_.create_stage(plugin_name, &s.config());
        auto* src = dynamic_cast<ISourceStage*>(worker_stage);
        if (!src) {
          FP_LOG_ERROR_FMT("source worker stage '{}' does not implement source interface", s.name());
          registry_.destroy_stage(worker_stage);
          throw std::runtime_error("worker stage is not a source: " + s.name());
        }
        threads.emplace_back([&, src, out, i]() {
          FP_LOG_DEBUG_FMT("stage '{}' source worker {} started", s.name(), i);

          RunSourceStage(src, ctx, *out, &metrics);

          FP_LOG_DEBUG_FMT("stage '{}' source worker {} stopped", s.name(), i);
        });
      }
    } else if (kind == StageKind::kTransform) {
      auto in = queues.at(s.input_queue());
      auto out = queues.at(s.output_queue());
      for (uint32_t i = 0; i < s.threads(); ++i) {
        IStage* worker_stage = registry_.create_stage(plugin_name, &s.config());
        auto* xf = dynamic_cast<ITransformStage*>(worker_stage);
        if (!xf) {
          FP_LOG_ERROR_FMT("transform worker stage '{}' does not implement transform interface", s.name());
          registry_.destroy_stage(worker_stage);
          throw std::runtime_error("worker stage is not a transform: " + s.name());
        }
        threads.emplace_back([&, xf, in, out, i]() {
          FP_LOG_DEBUG_FMT("stage '{}' transform worker {} started", s.name(), i);

          RunTransformStage(xf, ctx, *in, *out, &metrics);

          FP_LOG_DEBUG_FMT("stage '{}' transform worker {} stopped", s.name(), i);
        });
      }
    } else if (kind == StageKind::kSink) {
      auto in = queues.at(s.input_queue());
      for (uint32_t i = 0; i < s.threads(); ++i) {
        IStage* worker_stage = registry_.create_stage(plugin_name, &s.config());
        auto* sink = dynamic_cast<ISinkStage*>(worker_stage);
        if (!sink) {
          FP_LOG_ERROR_FMT("sink worker stage '{}' does not implement sink interface", s.name());
          registry_.destroy_stage(worker_stage);
          throw std::runtime_error("worker stage is not a sink: " + s.name());
        }
        threads.emplace_back([&, sink, in, i]() {
          FP_LOG_DEBUG_FMT("stage '{}' sink worker {} started", s.name(), i);

          RunSinkStage(sink, ctx, *in, &metrics);

          FP_LOG_DEBUG_FMT("stage '{}' sink worker {} stopped", s.name(), i);
        });
      }
    }
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
