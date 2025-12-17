#include <flowpipe/runtime.h>
#include <flowpipe/stage_registry.h>

namespace flowpipe {
    std::shared_ptr<IStage> MakeNoopSource(const StageSpec&);
    std::shared_ptr<IStage> MakeStdoutSink(const StageSpec&);
}

namespace example {
    std::shared_ptr<flowpipe::IStage>
    make_uppercase_stage(const flowpipe::StageSpec&);
}

int main() {
    using namespace flowpipe;

    StageRegistry registry;
    registry.add("noop_source", MakeNoopSource);
    registry.add("stdout_sink", MakeStdoutSink);
    registry.add("uppercase", example::make_uppercase_stage);

    FlowSpec spec{
        .name = "uppercase_example",
        .mode = ExecMode::JOB,
        .queues = {
          {"q1", "mpsc", 128},
          {"q2", "mpmc", 128}
        },
        .stages = {
          {"src", "noop_source", 1, "", "q1", {{"count","5"}}},
          {"up",  "uppercase",   2, "q1", "q2", {}},
          {"out", "stdout_sink", 1, "q2", "", {}}
        }
    };

    Runtime rt(std::move(registry));
    return rt.run(spec);
}
