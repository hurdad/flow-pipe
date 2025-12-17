#include <flowpipe/runtime.h>
#include <flowpipe/stage_registry.h>

namespace example {
    std::shared_ptr<flowpipe::IStage> make_csv_source(const flowpipe::StageSpec&);
    std::shared_ptr<flowpipe::IStage> make_csv_parser(const flowpipe::StageSpec&);
    std::shared_ptr<flowpipe::IStage> make_csv_sink(const flowpipe::StageSpec&);
}

int main() {
    using namespace flowpipe;

    StageRegistry registry;
    registry.add("csv_source", example::make_csv_source);
    registry.add("csv_parser", example::make_csv_parser);
    registry.add("csv_sink", example::make_csv_sink);

    FlowSpec spec{
        .name = "csv_pipeline",
        .mode = ExecMode::JOB,
        .queues = {
          {"raw", "mpsc", 256},
          {"parsed", "mpmc", 256}
        },
        .stages = {
          {"src", "csv_source", 1, "", "raw", {{"path","data.csv"}}},
          {"parse", "csv_parser", 2, "raw", "parsed", {}},
          {"out", "csv_sink", 1, "parsed", "", {}}
        }
    };

    Runtime rt(std::move(registry));
    return rt.run(spec);
}
