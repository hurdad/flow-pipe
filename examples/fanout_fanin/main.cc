#include <flowpipe/runtime.h>
#include <flowpipe/stage_registry.h>

namespace flowpipe {
    std::shared_ptr<IStage> MakeNoopSource(const flowpipe::v1::StageSpec&);
}

namespace example {
    std::shared_ptr<flowpipe::IStage> make_fanout_stage(const flowpipe::v1::StageSpec&);
    std::shared_ptr<flowpipe::IStage> make_aggregate_stage(const flowpipe::v1::StageSpec&);
}

int main() {
    using namespace flowpipe;

    StageRegistry registry;
    registry.add("noop_source", MakeNoopSource);
    registry.add("fanout", example::make_fanout_stage);
    registry.add("aggregate", example::make_aggregate_stage);
    flowpipe::v1::FlowSpec spec;

    // --------------------------------------------------
    // Flow metadata
    // --------------------------------------------------

    spec.set_name("fanout_fanin");
    spec.set_version(1);

    // Execution mode: JOB
    {
        auto* exec = spec.mutable_execution();
        exec->set_mode(flowpipe::v1::EXECUTION_MODE_JOB);
    }

    // Runtime: builtin stages
    spec.set_runtime(flowpipe::v1::FLOW_RUNTIME_BUILTIN);

    // --------------------------------------------------
    // Queues
    // --------------------------------------------------

    // q1: MPSC, capacity 128
    {
        auto* q = spec.add_queues();
        q->set_name("q1");
        q->set_type(flowpipe::v1::QUEUE_TYPE_MPSC);
        q->set_capacity(128);
    }

    // q2: MPMC, capacity 256
    {
        auto* q = spec.add_queues();
        q->set_name("q2");
        q->set_type(flowpipe::v1::QUEUE_TYPE_MPMC);
        q->set_capacity(256);
    }

    // --------------------------------------------------
    // Stage: src (noop_source)
    // --------------------------------------------------

    {
        auto* s = spec.add_stages();
        s->set_name("src");
        s->set_type("noop_source");
        s->set_threads(1);
        s->set_output_queue("q1");

        // params: { count = 3 }
        flowpipe::v1::Value v;
        v.set_int_value(3);
        (*s->mutable_params())["count"] = v;
    }

    // --------------------------------------------------
    // Stage: fan (fanout)
    // --------------------------------------------------

    {
        auto* s = spec.add_stages();
        s->set_name("fan");
        s->set_type("fanout");
        s->set_threads(2);
        s->set_input_queue("q1");
        s->set_output_queue("q2");
    }

    // --------------------------------------------------
    // Stage: agg (aggregate)
    // --------------------------------------------------

    {
        auto* s = spec.add_stages();
        s->set_name("agg");
        s->set_type("aggregate");
        s->set_threads(1);
        s->set_input_queue("q2");
    }

    Runtime rt(std::move(registry));
    return rt.run(spec);
}
