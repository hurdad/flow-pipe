#include <flowpipe/runtime.h>
#include <flowpipe/stage_registry.h>

namespace flowpipe {
    std::shared_ptr<IStage> MakeNoopSource(const StageSpec&);
}

namespace example {
    std::shared_ptr<flowpipe::IStage> make_fanout_stage(const flowpipe::StageSpec&);
    std::shared_ptr<flowpipe::IStage> make_aggregate_stage(const flowpipe::StageSpec&);
}

int main() {
    using namespace flowpipe;

    StageRegistry registry;
    registry.add("noop_source", MakeNoopSource);
    registry.add("fanout", example::make_fanout_stage);
    registry.add("aggregate", example::make_aggregate_stage);

    FlowSpec spec{
        .name = "fanout_fanin",
        .mode = ExecMode::JOB,
        .queues = {
          {"q1", "mpsc", 128},
          {"q2", "mpmc", 256}
        },
        .stages = {
          {"src", "noop_source", 1, "", "q1", {{"count","3"}}},
          {"fan", "fanout", 2, "q1", "q2", {}},
          {"agg", "aggregate", 1, "q2", "", {}}
        }
    };

    Runtime rt(std::move(registry));
    return rt.run(spec);
}
