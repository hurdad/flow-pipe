#include "flowpipe/runtime.h"
#include "flowpipe/stage_registry.h"

namespace flowpipe {
    std::shared_ptr<IStage> MakeNoopSource(const flowpipe::v1::StageSpec &);

    std::shared_ptr<IStage> MakeNoopTransform(const flowpipe::v1::StageSpec &);

    std::shared_ptr<IStage> MakeStdoutSink(const flowpipe::v1::StageSpec &);
} // namespace flowpipe

int main() {
    using namespace flowpipe;

    StageRegistry registry;
    registry.add("noop_source", MakeNoopSource);
    registry.add("noop_transform", MakeNoopTransform);
    registry.add("stdout_sink", MakeStdoutSink);

    flowpipe::v1::FlowSpec spec;

    // --------------------------------------------------
    // Flow metadata
    // --------------------------------------------------

    spec.set_name("demo");
    spec.set_version(1);

    // Execution mode (job)
    auto *exec = spec.mutable_execution();
    exec->set_mode(flowpipe::v1::EXECUTION_MODE_JOB);

    // Runtime type (builtin stages)
    spec.set_runtime(flowpipe::v1::FLOW_RUNTIME_BUILTIN);

    // --------------------------------------------------
    // Queues
    // --------------------------------------------------

    // q1: MPSC, capacity 256
    {
        auto *q = spec.add_queues();
        q->set_name("q1");
        q->set_type(flowpipe::v1::QUEUE_TYPE_MPSC);
        q->set_capacity(256);
    }

    // q2: MPMC, capacity 256
    {
        auto *q = spec.add_queues();
        q->set_name("q2");
        q->set_type(flowpipe::v1::QUEUE_TYPE_MPMC);
        q->set_capacity(256);
    }

    // --------------------------------------------------
    // Stage: src (noop_source)
    // --------------------------------------------------

    {
        auto *s = spec.add_stages();
        s->set_name("src");
        s->set_type("noop_source");
        s->set_threads(1);
        s->set_output_queue("q1");

        // params: { count = 5 }
        flowpipe::v1::Value v;
        v.set_int_value(5);
        (*s->mutable_params())["count"] = v;
    }

    // --------------------------------------------------
    // Stage: xf (noop_transform)
    // --------------------------------------------------

    {
        auto *s = spec.add_stages();
        s->set_name("xf");
        s->set_type("noop_transform");
        s->set_threads(2);
        s->set_input_queue("q1");
        s->set_output_queue("q2");
    }

    // --------------------------------------------------
    // Stage: out (stdout_sink)
    // --------------------------------------------------

    {
        auto *s = spec.add_stages();
        s->set_name("out");
        s->set_type("stdout_sink");
        s->set_threads(1);
        s->set_input_queue("q2");
    }

    Runtime rt(std::move(registry));
    return rt.run(spec);
}
