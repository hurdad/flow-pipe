#include "uppercase_stage.h"
#include <algorithm>

namespace example {

    UppercaseStage::UppercaseStage(const flowpipe::StageSpec& spec)
        : name_(spec.name) {}

    std::string UppercaseStage::name() const {
        return name_;
    }

    void UppercaseStage::run(flowpipe::StageContext& ctx,
                             flowpipe::BoundedQueue<flowpipe::Payload>& in,
                             flowpipe::BoundedQueue<flowpipe::Payload>& out) {
        while (!ctx.stop.stop_requested()) {
            auto item = in.pop(ctx.stop);
            if (!item) break;

            auto& s = *item;
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);

            out.push(std::move(s), ctx.stop);
        }
        out.close();
    }

    std::shared_ptr<flowpipe::IStage>
    make_uppercase_stage(const flowpipe::StageSpec& spec) {
        return std::make_shared<UppercaseStage>(spec);
    }

}  // namespace example
