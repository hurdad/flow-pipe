#include <flowpipe/runtime.h>
#include <flowpipe/stage_registry.h>

namespace example {
    std::shared_ptr<flowpipe::IStage> make_csv_source(const flowpipe::v1::StageSpec &);

    std::shared_ptr<flowpipe::IStage> make_csv_parser(const flowpipe::v1::StageSpec &);

    std::shared_ptr<flowpipe::IStage> make_csv_sink(const flowpipe::v1::StageSpec &);
}

int main() {
    using namespace flowpipe;

    StageRegistry registry;
    registry.add("csv_source", example::make_csv_source);
    registry.add("csv_parser", example::make_csv_parser);
    registry.add("csv_sink", example::make_csv_sink);
    flowpipe::v1::FlowSpec spec;

    // --------------------------------------------------
    // Flow metadata
    // --------------------------------------------------

    spec.set_name("csv_pipeline");
    spec.set_version(1);

    // Execution mode: JOB
    {
        auto *exec = spec.mutable_execution();
        exec->set_mode(flowpipe::v1::EXECUTION_MODE_JOB);
    }

    // Runtime: builtin stages
    spec.set_runtime(flowpipe::v1::FLOW_RUNTIME_BUILTIN);

    // --------------------------------------------------
    // Queues
    // --------------------------------------------------

    // raw: MPSC, capacity 256
    {
        auto *q = spec.add_queues();
        q->set_name("raw");
        q->set_type(flowpipe::v1::QUEUE_TYPE_MPSC);
        q->set_capacity(256);
    }

    // parsed: MPMC, capacity 256
    {
        auto *q = spec.add_queues();
        q->set_name("parsed");
        q->set_type(flowpipe::v1::QUEUE_TYPE_MPMC);
        q->set_capacity(256);
    }

    // --------------------------------------------------
    // Stage: src (csv_source)
    // --------------------------------------------------

    {
        auto *s = spec.add_stages();
        s->set_name("src");
        s->set_type("csv_source");
        s->set_threads(1);
        s->set_output_queue("raw");

        // params: { path = "data.csv" }
        flowpipe::v1::Value v;
        v.set_string_value("data.csv");
        (*s->mutable_params())["path"] = v;
    }

    // --------------------------------------------------
    // Stage: parse (csv_parser)
    // --------------------------------------------------

    {
        auto *s = spec.add_stages();
        s->set_name("parse");
        s->set_type("csv_parser");
        s->set_threads(2);
        s->set_input_queue("raw");
        s->set_output_queue("parsed");
    }

    // --------------------------------------------------
    // Stage: out (csv_sink)
    // --------------------------------------------------

    {
        auto *s = spec.add_stages();
        s->set_name("out");
        s->set_type("csv_sink");
        s->set_threads(1);
        s->set_input_queue("parsed");
    }
    Runtime rt(std::move(registry));
    return rt.run(spec);
}
