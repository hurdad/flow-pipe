#include "flowpipe/runtime.h"

#include <atomic>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include "flowpipe/queue_runtime.h"
#include "flowpipe/signal_handler.h"
#include "flowpipe/stage_runner.h"

namespace flowpipe {

Runtime::Runtime() = default;

int Runtime::run(const flowpipe::v1::FlowSpec& spec) {
  std::atomic<bool> stop_flag{false};
  SignalHandler::install(stop_flag);
  StopToken stop{reinterpret_cast<bool*>(&stop_flag)};

  // ------------------------------------------------------------
  // Create runtime queues (QueueRuntime)
  // ------------------------------------------------------------
  std::unordered_map<std::string, std::shared_ptr<QueueRuntime>> queues;

  for (const auto& q : spec.queues()) {
    if (q.capacity() == 0) {
      throw std::runtime_error("queue capacity must be > 0: " + q.name());
    }

    if (q.type() == flowpipe::v1::QUEUE_TYPE_UNSPECIFIED) {
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
    const bool has_input = s.has_input_queue();
    const bool has_output = s.has_output_queue();

    IStage* stage =
        registry_.create_stage(s.has_plugin() ? s.plugin() : "libstage_" + s.type() + ".so");

    // ----------------------------
    // Source stage
    // ----------------------------
    if (auto* src = dynamic_cast<ISourceStage*>(stage)) {
      if (has_input || !has_output) {
        throw std::runtime_error("invalid source stage wiring: " + s.name());
      }

      auto out = queues.at(s.output_queue());

      for (uint32_t i = 0; i < s.threads(); ++i) {
        threads.emplace_back([&, src, out]() { RunSourceStage(src, ctx, *out, &metrics); });
      }
      continue;
    }

    // ----------------------------
    // Transform stage
    // ----------------------------
    if (auto* xf = dynamic_cast<ITransformStage*>(stage)) {
      if (!has_input || !has_output) {
        throw std::runtime_error("invalid transform stage wiring: " + s.name());
      }

      auto in = queues.at(s.input_queue());
      auto out = queues.at(s.output_queue());

      for (uint32_t i = 0; i < s.threads(); ++i) {
        threads.emplace_back(
            [&, xf, in, out]() { RunTransformStage(xf, ctx, *in, *out, &metrics); });
      }
      continue;
    }

    // ----------------------------
    // Sink stage
    // ----------------------------
    if (auto* sink = dynamic_cast<ISinkStage*>(stage)) {
      if (!has_input || has_output) {
        throw std::runtime_error("invalid sink stage wiring: " + s.name());
      }

      auto in = queues.at(s.input_queue());

      for (uint32_t i = 0; i < s.threads(); ++i) {
        threads.emplace_back([&, sink, in]() { RunSinkStage(sink, ctx, *in, &metrics); });
      }
      continue;
    }

    throw std::runtime_error("stage does not implement a valid interface: " + s.name());
  }

  // ------------------------------------------------------------
  // Join
  // ------------------------------------------------------------
  for (auto& t : threads) {
    t.join();
  }

  registry_.shutdown();
  return 0;
}

}  // namespace flowpipe
