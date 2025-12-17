#include "flowpipe/runtime.h"
#include "flowpipe/signal_handler.h"
#include <atomic>
#include <thread>
#include <unordered_map>
#include <vector>

namespace flowpipe {

Runtime::Runtime(StageRegistry registry) : registry_(std::move(registry)) {}

int Runtime::run(const FlowSpec &spec) {
  std::atomic<bool> stop_flag{false};
  SignalHandler::install(stop_flag);
  StopToken stop{reinterpret_cast<bool *>(&stop_flag)};

  std::unordered_map<std::string, std::shared_ptr<BoundedQueue<Payload>>>
      queues;

  for (const auto &q : spec.queues) {
    queues[q.name] = std::make_shared<BoundedQueue<Payload>>(q.capacity);
  }

  std::vector<std::thread> threads;
  StageContext ctx{stop};

  for (const auto &s : spec.stages) {
    auto stage = registry_.create(s);

    if (auto src = std::dynamic_pointer_cast<ISourceStage>(stage)) {
      auto out = queues.at(s.output);
      for (int i = 0; i < s.threads; ++i) {
        threads.emplace_back([=, &ctx] { src->run(ctx, *out); });
      }
      continue;
    }

    if (auto xf = std::dynamic_pointer_cast<ITransformStage>(stage)) {
      auto in = queues.at(s.input);
      auto out = queues.at(s.output);
      for (int i = 0; i < s.threads; ++i) {
        threads.emplace_back([=, &ctx] { xf->run(ctx, *in, *out); });
      }
      continue;
    }

    if (auto sink = std::dynamic_pointer_cast<ISinkStage>(stage)) {
      auto in = queues.at(s.input);
      for (int i = 0; i < s.threads; ++i) {
        threads.emplace_back([=, &ctx] { sink->run(ctx, *in); });
      }
      continue;
    }
  }

  for (auto &t : threads) {
    t.join();
  }

  return 0;
}

} // namespace flowpipe
