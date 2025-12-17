#pragma once

#include <flowpipe/stage.h>

namespace example {

    class UppercaseStage final : public flowpipe::ITransformStage {
    public:
        explicit UppercaseStage(const flowpipe::StageSpec& spec);

        std::string name() const override;

        void run(flowpipe::StageContext& ctx,
                 flowpipe::BoundedQueue<flowpipe::Payload>& in,
                 flowpipe::BoundedQueue<flowpipe::Payload>& out) override;

    private:
        std::string name_;
    };

    std::shared_ptr<flowpipe::IStage>
    make_uppercase_stage(const flowpipe::StageSpec& spec);

}  // namespace example
