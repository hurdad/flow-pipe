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
#include <unordered_set>
#include <vector>

#include "flowpipe/bounded_queue.h"
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

std::optional<int> ResolveRealtimePriority(const flowpipe::v1::StageSpec& stage) {
  if (!stage.has_realtime_priority()) {
    return std::nullopt;
  }

  return static_cast<int>(stage.realtime_priority());
}

void ValidateRealtimePriority(const std::string& stage_name, int priority) {
#ifdef __linux__
  const int policy = SCHED_FIFO;
  const int min_priority = sched_get_priority_min(policy);
  const int max_priority = sched_get_priority_max(policy);
  if (min_priority == -1 || max_priority == -1) {
    FP_LOG_WARN_FMT("unable to resolve real-time priority range for stage '{}'", stage_name);
    return;
  }

  if (priority < min_priority || priority > max_priority) {
    FP_LOG_ERROR_FMT("real-time priority configured for stage '{}' is {} but valid range is {}-{}",
                     stage_name, priority, min_priority, max_priority);
    throw std::runtime_error("invalid real-time priority for stage: " + stage_name);
  }
#else
  (void)stage_name;
  (void)priority;
#endif
}

void ApplyRealtimePriority(const std::string& stage_name, uint32_t worker_index, int priority) {
#ifdef __linux__
  const int policy = SCHED_FIFO;
  sched_param params{};
  params.sched_priority = priority;
  const int result = pthread_setschedparam(pthread_self(), policy, &params);
  if (result != 0) {
    FP_LOG_WARN_FMT("stage '{}' worker {} failed to set real-time priority {}: {}", stage_name,
                    worker_index, priority, std::strerror(result));
    return;
  }

  FP_LOG_INFO_FMT("stage '{}' worker {} set real-time priority {} (policy=FIFO)", stage_name,
                  worker_index, priority);
#else
  (void)stage_name;
  (void)worker_index;
  (void)priority;
  FP_LOG_WARN_FMT("real-time priority requested but not supported on this platform");
#endif
}

void ValidateCpuPinning(const std::string& stage_name, const std::vector<uint32_t>& cpus) {
#ifdef __linux__
  if (cpus.empty()) {
    return;
  }

  long configured_cpus = sysconf(_SC_NPROCESSORS_CONF);
  uint32_t max_cpu_id = CPU_SETSIZE;
  if (configured_cpus > 0 && configured_cpus < static_cast<long>(CPU_SETSIZE)) {
    max_cpu_id = static_cast<uint32_t>(configured_cpus);
  }

  std::unordered_set<uint32_t> seen;
  seen.reserve(cpus.size());
  for (const auto cpu : cpus) {
    if (cpu >= max_cpu_id) {
      FP_LOG_ERROR_FMT(
          "cpu pinning configured for stage '{}' includes invalid CPU id {} (valid range 0-{})",
          stage_name, cpu, max_cpu_id - 1);
      throw std::runtime_error("invalid cpu pinning for stage: " + stage_name);
    }

    if (!seen.insert(cpu).second) {
      FP_LOG_ERROR_FMT("cpu pinning configured for stage '{}' includes duplicate CPU id {}",
                       stage_name, cpu);
      throw std::runtime_error("duplicate cpu pinning for stage: " + stage_name);
    }
  }
#else
  (void)stage_name;
  (void)cpus;
#endif
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

    auto queue_type = q.type();
    if (queue_type == flowpipe::v1::QUEUE_TYPE_UNSPECIFIED) {
      queue_type = flowpipe::v1::QUEUE_TYPE_IN_MEMORY;
    }

    if (queue_type != flowpipe::v1::QUEUE_TYPE_IN_MEMORY) {
      FP_LOG_ERROR_FMT("unsupported queue type {} for queue '{}'", static_cast<int>(queue_type),
                       q.name());
      throw std::runtime_error("unsupported queue type for queue: " + q.name());
    }

    qr->queue = std::make_shared<BoundedQueue<Payload>>(q.capacity());

    queues.emplace(qr->name, std::move(qr));
  }

  FP_LOG_INFO_FMT("initialized {} runtime queues", queues.size());

  std::vector<std::shared_ptr<IQueue<Payload>>> runtime_queues;
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
  std::unordered_map<std::string, std::shared_ptr<std::atomic<uint32_t>>> queue_producer_workers;

  auto join_workers = [&threads]() {
    for (auto& t : threads) {
      if (t.joinable()) {
        t.join();
      }
    }
  };

  auto close_runtime_queues = [&runtime_queues]() {
    for (const auto& queue : runtime_queues) {
      queue->close();
    }
  };

  // ------------------------------------------------------------
  // Wire stages (runtime owns execution)
  // ------------------------------------------------------------
  try {
    for (const auto& stage_spec : spec.stages()) {
      if (!stage_spec.has_output_queue()) {
        continue;
      }

      auto& producer_count = queue_producer_workers[stage_spec.output_queue()];
      if (!producer_count) {
        producer_count = std::make_shared<std::atomic<uint32_t>>(0);
      }
      producer_count->fetch_add(stage_spec.threads());
    }

    for (const auto& s : spec.stages()) {
      const std::string stage_name = s.name();
      FP_LOG_INFO_FMT("initializing stage '{}' type={} threads={}", stage_name, s.type(),
                      s.threads());

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
          FP_LOG_WARN_FMT("cpu pinning configured for stage '{}' but no CPUs specified",
                          stage_name);
        }
        ValidateCpuPinning(stage_name, pinning_cpus);
      }
      const bool should_pin = !pinning_cpus.empty();
      const auto realtime_priority = ResolveRealtimePriority(s);
      if (realtime_priority.has_value()) {
        ValidateRealtimePriority(stage_name, realtime_priority.value());
      }
      const bool should_set_realtime = realtime_priority.has_value();

      // Resolve plugin name: explicit plugin wins, otherwise default to type-based naming.
      const std::string plugin_name = s.has_plugin() ? s.plugin() : "libstage_" + s.type() + ".so";
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

      std::vector<IStage*> worker_stages;
      worker_stages.reserve(s.threads());
      worker_stages.push_back(stage);

      try {
        for (uint32_t i = 1; i < s.threads(); ++i) {
          worker_stages.push_back(registry_.create_stage(plugin_name, &s.config()));
        }
      } catch (...) {
        for (auto* worker_stage : worker_stages) {
          registry_.destroy_stage(worker_stage);
        }
        throw;
      }

      if (kind == StageKind::kSource) {
        auto out = queues.at(s.output_queue());
        auto queue_remaining_producers = queue_producer_workers.at(s.output_queue());
        for (uint32_t i = 0; i < worker_stages.size(); ++i) {
          auto* worker_stage = worker_stages[i];
          auto* src = dynamic_cast<ISourceStage*>(worker_stage);
          if (!src) {
            FP_LOG_ERROR_FMT("source worker stage '{}' does not implement source interface",
                             stage_name);
            throw std::runtime_error("worker stage is not a source: " + stage_name);
          }

          active_workers.fetch_add(1);
          try {
            threads.emplace_back([&, src, worker_stage, out, i, stage_name, should_pin, pinning_cpus,
                                  should_set_realtime, realtime_priority,
                                  queue_remaining_producers]() {
            if (should_pin) {
              ApplyCpuPinning(stage_name, i, pinning_cpus);
            }
            if (should_set_realtime) {
              ApplyRealtimePriority(stage_name, i, realtime_priority.value());
            }
            FP_LOG_DEBUG_FMT("stage '{}' source worker {} started", stage_name, i);

            RunSourceStage(src, ctx, *out, &metrics);

            if (queue_remaining_producers->fetch_sub(1) == 1) {
              FP_LOG_DEBUG_FMT("stage '{}' source worker {} closing shared output queue",
                               stage_name, i);
              out->queue->close();
            }

            registry_.destroy_stage(worker_stage);

            FP_LOG_DEBUG_FMT("stage '{}' source worker {} stopped", stage_name, i);
            if (auto_shutdown) {
              if (active_workers.fetch_sub(1) == 1) {
                stop.request_stop();
              }
            } else {
              active_workers.fetch_sub(1);
            }
            });
          } catch (...) {
            active_workers.fetch_sub(1);
            throw;
          }
        }
      } else if (kind == StageKind::kTransform) {
        auto in = queues.at(s.input_queue());
        auto out = queues.at(s.output_queue());
        auto queue_remaining_producers = queue_producer_workers.at(s.output_queue());
        for (uint32_t i = 0; i < worker_stages.size(); ++i) {
          auto* worker_stage = worker_stages[i];
          auto* xf = dynamic_cast<ITransformStage*>(worker_stage);
          if (!xf) {
            FP_LOG_ERROR_FMT("transform worker stage '{}' does not implement transform interface",
                             stage_name);
            throw std::runtime_error("worker stage is not a transform: " + stage_name);
          }

          active_workers.fetch_add(1);
          try {
            threads.emplace_back([&, xf, worker_stage, in, out, i, stage_name, should_pin,
                                  pinning_cpus, should_set_realtime, realtime_priority,
                                  queue_remaining_producers]() {
            if (should_pin) {
              ApplyCpuPinning(stage_name, i, pinning_cpus);
            }
            if (should_set_realtime) {
              ApplyRealtimePriority(stage_name, i, realtime_priority.value());
            }
            FP_LOG_DEBUG_FMT("stage '{}' transform worker {} started", stage_name, i);

            RunTransformStage(xf, ctx, *in, *out, &metrics);

            if (queue_remaining_producers->fetch_sub(1) == 1) {
              FP_LOG_DEBUG_FMT("stage '{}' transform worker {} closing shared output queue",
                               stage_name, i);
              out->queue->close();
            }

            registry_.destroy_stage(worker_stage);

            FP_LOG_DEBUG_FMT("stage '{}' transform worker {} stopped", stage_name, i);
            if (auto_shutdown) {
              if (active_workers.fetch_sub(1) == 1) {
                stop.request_stop();
              }
            } else {
              active_workers.fetch_sub(1);
            }
            });
          } catch (...) {
            active_workers.fetch_sub(1);
            throw;
          }
        }
      } else if (kind == StageKind::kSink) {
        auto in = queues.at(s.input_queue());
        for (uint32_t i = 0; i < worker_stages.size(); ++i) {
          auto* worker_stage = worker_stages[i];
          auto* sink = dynamic_cast<ISinkStage*>(worker_stage);
          if (!sink) {
            FP_LOG_ERROR_FMT("sink worker stage '{}' does not implement sink interface",
                             stage_name);
            throw std::runtime_error("worker stage is not a sink: " + stage_name);
          }

          active_workers.fetch_add(1);
          try {
            threads.emplace_back([&, sink, worker_stage, in, i, stage_name, should_pin, pinning_cpus,
                                  should_set_realtime, realtime_priority]() {
            if (should_pin) {
              ApplyCpuPinning(stage_name, i, pinning_cpus);
            }
            if (should_set_realtime) {
              ApplyRealtimePriority(stage_name, i, realtime_priority.value());
            }
            FP_LOG_DEBUG_FMT("stage '{}' sink worker {} started", stage_name, i);

            RunSinkStage(sink, ctx, *in, &metrics);

            registry_.destroy_stage(worker_stage);

            FP_LOG_DEBUG_FMT("stage '{}' sink worker {} stopped", stage_name, i);
            if (auto_shutdown) {
              if (active_workers.fetch_sub(1) == 1) {
                stop.request_stop();
              }
            } else {
              active_workers.fetch_sub(1);
            }
            });
          } catch (...) {
            active_workers.fetch_sub(1);
            throw;
          }
        }
      }
    }

    FP_LOG_INFO_FMT("runtime started {} worker threads", threads.size());

    if (auto_shutdown && active_workers.load() == 0) {
      stop.request_stop();
    }

    // ------------------------------------------------------------
    // Join
    // ------------------------------------------------------------
    while (!stop.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    close_runtime_queues();
    join_workers();
  } catch (...) {
    stop.request_stop();
    close_runtime_queues();
    join_workers();
    registry_.shutdown();
    throw;
  }

  FP_LOG_INFO("runtime shutting down");

  registry_.shutdown();

  FP_LOG_INFO("runtime exited cleanly");
  return 0;
}

}  // namespace flowpipe
