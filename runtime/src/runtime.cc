#include "flowpipe/runtime.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <optional>
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "flowpipe/queue_runtime.h"
#include "flowpipe/signal_handler.h"
#include "flowpipe/stage_runner.h"

// Logging
#include "flowpipe/observability/logging_runtime.h"

namespace flowpipe {
namespace {

std::optional<std::vector<uint32_t>> ResolveCpuPinning(const flowpipe::v1::FlowSpec& spec,
                                                       const std::string& stage_name) {
  // CPU pinning is only configurable when a Kubernetes execution context is present.
  if (!spec.has_kubernetes()) {
    return std::nullopt;
  }

  const auto& settings = spec.kubernetes();
  const auto& pinning = settings.cpu_pinning();
  const auto it = pinning.find(stage_name);
  if (it == pinning.end()) {
    return std::nullopt;
  }

  std::vector<uint32_t> cpus;
  cpus.reserve(it->second.cpu_size());
  for (const auto cpu : it->second.cpu()) {
    cpus.push_back(cpu);
  }

  return cpus;
}

std::string FormatCpuList(const std::vector<uint32_t>& cpus) {
  std::ostringstream oss;
  for (size_t i = 0; i < cpus.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << cpus[i];
  }
  return oss.str();
}

void ApplyCpuPinning(const std::string& stage_name, uint32_t worker_index,
                     const std::vector<uint32_t>& cpus) {
#ifdef __linux__
  // Empty pinning lists indicate configuration errors, but the worker can still run.
  if (cpus.empty()) {
    FP_LOG_WARN_FMT("cpu pinning requested for stage '{}' but no CPUs configured", stage_name);
    return;
  }

  cpu_set_t mask;
  CPU_ZERO(&mask);
  for (const auto cpu : cpus) {
    CPU_SET(cpu, &mask);
  }

  const int result = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
  if (result != 0) {
    FP_LOG_WARN_FMT("stage '{}' worker {} failed to set cpu affinity: {}", stage_name, worker_index,
                    std::strerror(result));
    return;
  }

  FP_LOG_INFO_FMT("stage '{}' worker {} pinned to CPUs [{}]", stage_name, worker_index,
                  FormatCpuList(cpus));
#else
  (void)stage_name;
  (void)worker_index;
  (void)cpus;
  FP_LOG_WARN_FMT("cpu pinning requested but not supported on this platform");
#endif
}

}  // namespace

Runtime::Runtime() = default;

int Runtime::run(const flowpipe::v1::FlowSpec& spec) {
  FP_LOG_INFO_FMT("runtime starting: {} stages, {} queues", spec.stages_size(), spec.queues_size());

  // Shared stop flag toggled by the signal handler for coordinated shutdown.
  std::atomic<bool> stop_flag{false};
  StopToken stop{&stop_flag};
  const bool auto_shutdown =
      spec.has_execution() && spec.execution().mode() == flowpipe::v1::EXECUTION_MODE_JOB;
  std::atomic<size_t> active_workers{0};

  // ------------------------------------------------------------
  // Create runtime queues (QueueRuntime)
  // ------------------------------------------------------------
  std::unordered_map<std::string, std::shared_ptr<QueueRuntime>> queues;

  for (const auto& q : spec.queues()) {
    // Validate queue configuration before allocating runtime structures.
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
    // Track the concrete queues so we can close them on shutdown.
    runtime_queues.push_back(queue_runtime->queue);
  }

  SignalHandler::install(stop_flag);

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
    const std::string stage_name = s.name();
    FP_LOG_INFO_FMT("initializing stage '{}' type={} threads={}", stage_name, s.type(), s.threads());

    if (s.threads() < 1) {
      FP_LOG_ERROR_FMT("invalid stage '{}': threads must be >= 1", s.name());
      throw std::runtime_error("stage threads must be >= 1: " + s.name());
    }

    const bool has_input = s.has_input_queue();
    const bool has_output = s.has_output_queue();
    const auto stage_pinning = ResolveCpuPinning(spec, stage_name);
    std::vector<uint32_t> pinning_cpus;
    if (stage_pinning.has_value()) {
      pinning_cpus = stage_pinning.value();
      if (pinning_cpus.empty()) {
        FP_LOG_WARN_FMT("cpu pinning configured for stage '{}' but no CPUs specified", stage_name);
      }
    }
    const bool should_pin = !pinning_cpus.empty();

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
      FP_LOG_DEBUG_FMT("stage '{}' detected as SOURCE", stage_name);

      if (has_input || !has_output) {
        FP_LOG_ERROR_FMT("invalid source stage wiring for '{}'", stage_name);
        registry_.destroy_stage(stage);
        throw std::runtime_error("invalid source stage wiring: " + stage_name);
      }

    } else if (dynamic_cast<ITransformStage*>(stage)) {
      kind = StageKind::kTransform;
      FP_LOG_DEBUG_FMT("stage '{}' detected as TRANSFORM", stage_name);

      if (!has_input || !has_output) {
        FP_LOG_ERROR_FMT("invalid transform stage wiring for '{}'", stage_name);
        registry_.destroy_stage(stage);
        throw std::runtime_error("invalid transform stage wiring: " + stage_name);
      }
    } else if (dynamic_cast<ISinkStage*>(stage)) {
      kind = StageKind::kSink;
      FP_LOG_DEBUG_FMT("stage '{}' detected as SINK", stage_name);

      if (!has_input || has_output) {
        FP_LOG_ERROR_FMT("invalid sink stage wiring for '{}'", stage_name);
        registry_.destroy_stage(stage);
        throw std::runtime_error("invalid sink stage wiring: " + stage_name);
      }
    } else {
      FP_LOG_ERROR_FMT("stage '{}' does not implement a valid interface", stage_name);
      registry_.destroy_stage(stage);
      throw std::runtime_error("stage does not implement a valid interface: " + stage_name);
    }

    registry_.destroy_stage(stage);

    if (kind == StageKind::kSource) {
      auto out = queues.at(s.output_queue());
      for (uint32_t i = 0; i < s.threads(); ++i) {
        active_workers.fetch_add(1);
        IStage* worker_stage = registry_.create_stage(plugin_name, &s.config());
        auto* src = dynamic_cast<ISourceStage*>(worker_stage);
        if (!src) {
          FP_LOG_ERROR_FMT("source worker stage '{}' does not implement source interface", stage_name);
          registry_.destroy_stage(worker_stage);
          throw std::runtime_error("worker stage is not a source: " + stage_name);
        }
        threads.emplace_back([&, src, out, i, stage_name, should_pin, pinning_cpus]() {
          if (should_pin) {
            ApplyCpuPinning(stage_name, i, pinning_cpus);
          }
          FP_LOG_DEBUG_FMT("stage '{}' source worker {} started", stage_name, i);

          RunSourceStage(src, ctx, *out, &metrics);

          FP_LOG_DEBUG_FMT("stage '{}' source worker {} stopped", stage_name, i);
          if (auto_shutdown) {
            if (active_workers.fetch_sub(1) == 1) {
              stop_flag.store(true);
            }
          } else {
            active_workers.fetch_sub(1);
          }
        });
      }
    } else if (kind == StageKind::kTransform) {
      auto in = queues.at(s.input_queue());
      auto out = queues.at(s.output_queue());
      for (uint32_t i = 0; i < s.threads(); ++i) {
        active_workers.fetch_add(1);
        IStage* worker_stage = registry_.create_stage(plugin_name, &s.config());
        auto* xf = dynamic_cast<ITransformStage*>(worker_stage);
        if (!xf) {
          FP_LOG_ERROR_FMT("transform worker stage '{}' does not implement transform interface", stage_name);
          registry_.destroy_stage(worker_stage);
          throw std::runtime_error("worker stage is not a transform: " + stage_name);
        }
        threads.emplace_back([&, xf, in, out, i, stage_name, should_pin, pinning_cpus]() {
          if (should_pin) {
            ApplyCpuPinning(stage_name, i, pinning_cpus);
          }
          FP_LOG_DEBUG_FMT("stage '{}' transform worker {} started", stage_name, i);

          RunTransformStage(xf, ctx, *in, *out, &metrics);

          FP_LOG_DEBUG_FMT("stage '{}' transform worker {} stopped", stage_name, i);
          if (auto_shutdown) {
            if (active_workers.fetch_sub(1) == 1) {
              stop_flag.store(true);
            }
          } else {
            active_workers.fetch_sub(1);
          }
        });
      }
    } else if (kind == StageKind::kSink) {
      auto in = queues.at(s.input_queue());
      for (uint32_t i = 0; i < s.threads(); ++i) {
        active_workers.fetch_add(1);
        IStage* worker_stage = registry_.create_stage(plugin_name, &s.config());
        auto* sink = dynamic_cast<ISinkStage*>(worker_stage);
        if (!sink) {
          FP_LOG_ERROR_FMT("sink worker stage '{}' does not implement sink interface", stage_name);
          registry_.destroy_stage(worker_stage);
          throw std::runtime_error("worker stage is not a sink: " + stage_name);
        }
        threads.emplace_back([&, sink, in, i, stage_name, should_pin, pinning_cpus]() {
          if (should_pin) {
            ApplyCpuPinning(stage_name, i, pinning_cpus);
          }
          FP_LOG_DEBUG_FMT("stage '{}' sink worker {} started", stage_name, i);

          RunSinkStage(sink, ctx, *in, &metrics);

          FP_LOG_DEBUG_FMT("stage '{}' sink worker {} stopped", stage_name, i);
          if (auto_shutdown) {
            if (active_workers.fetch_sub(1) == 1) {
              stop_flag.store(true);
            }
          } else {
            active_workers.fetch_sub(1);
          }
        });
      }
    }
  }

  FP_LOG_INFO_FMT("runtime started {} worker threads", threads.size());

  if (auto_shutdown && active_workers.load() == 0) {
    stop_flag.store(true);
  }

  // ------------------------------------------------------------
  // Join
  // ------------------------------------------------------------
  while (!stop_flag.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  for (const auto& queue : runtime_queues) {
    queue->close();
  }

  for (auto& t : threads) {
    t.join();
  }

  FP_LOG_INFO("runtime shutting down");

  registry_.shutdown();

  FP_LOG_INFO("runtime exited cleanly");
  return 0;
}

}  // namespace flowpipe
