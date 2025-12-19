#include "flowpipe/runtime.h"

#include <atomic>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

#include "flowpipe/signal_handler.h"

namespace flowpipe {

Runtime::Runtime() = default;

int Runtime::run(const flowpipe::v1::FlowSpec& spec) {
  std::atomic<bool> stop_flag{false};
  SignalHandler::install(stop_flag);
  StopToken stop{reinterpret_cast<bool*>(&stop_flag)};

  std::unordered_map<std::string, std::shared_ptr<BoundedQueue<Payload>>> queues;
  for (const auto& q : spec.queues()) {
    queues.emplace(q.name(), std::make_shared<BoundedQueue<Payload>>(q.capacity()));
  }

  StageContext ctx{stop};
  std::vector<std::thread> threads;

  for (const auto& s : spec.stages()) {
    const bool has_input = s.has_input_queue();
    const bool has_output = s.has_output_queue();

    IStage* stage =
        registry_.create_stage(s.has_plugin() ? s.plugin() : "libstage_" + s.type() + ".so");

    if (auto* src = dynamic_cast<ISourceStage*>(stage)) {
      if (has_input || !has_output) {
        throw std::runtime_error("invalid source stage wiring: " + s.name());
      }

      auto out = queues.at(s.output_queue());
      for (uint32_t i = 0; i < s.threads(); ++i) {
        threads.emplace_back([&, src, out] { src->run(ctx, *out); });
      }
      continue;
    }

    if (auto* xf = dynamic_cast<ITransformStage*>(stage)) {
      if (!has_input || !has_output) {
        throw std::runtime_error("invalid transform stage wiring: " + s.name());
      }

      auto in = queues.at(s.input_queue());
      auto out = queues.at(s.output_queue());
      for (uint32_t i = 0; i < s.threads(); ++i) {
        threads.emplace_back([&, xf, in, out] { xf->run(ctx, *in, *out); });
      }
      continue;
    }

    if (auto* sink = dynamic_cast<ISinkStage*>(stage)) {
      if (!has_input || has_output) {
        throw std::runtime_error("invalid sink stage wiring: " + s.name());
      }

      auto in = queues.at(s.input_queue());
      for (uint32_t i = 0; i < s.threads(); ++i) {
        threads.emplace_back([&, sink, in] { sink->run(ctx, *in); });
      }
      continue;
    }

    throw std::runtime_error("stage does not implement a valid interface: " + s.name());
  }

  for (auto& t : threads) {
    t.join();
  }

  registry_.shutdown();
  return 0;
}

}  // namespace flowpipe
