#include "flowpipe/runtime.h"
#include "flowpipe/stage_registry.h"

namespace flowpipe {
    std::shared_ptr<IStage> MakeNoopSource(const StageSpec&);
    std::shared_ptr<IStage> MakeNoopTransform(const StageSpec&);
    std::shared_ptr<IStage> MakeStdoutSink(const StageSpec&);
}

int main() {
    using namespace flowpipe;

    StageRegistry registry;
    registry.add("noop_source", MakeNoopSource);
    registry.add("noop_transform", MakeNoopTransform);
    registry.add("stdout_sink", MakeStdoutSink);

    FlowSpec spec;
    spec.name = "demo";
    spec.mode = ExecMode::JOB;

    spec.queues = {
        {"q1", "mpsc", 256},
        {"q2", "mpmc", 256}
    };

    spec.stages = {
        {"src", "noop_source", 1, "", "q1", {{"count","5"}}},
        {"xf", "noop_transform", 2, "q1", "q2", {}},
        {"out", "stdout_sink", 1, "q2", "", {}}
    };

    Runtime rt(std::move(registry));
    return rt.run(spec);
}
