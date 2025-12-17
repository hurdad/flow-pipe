#include <flowpipe/stage.h>

namespace example {

    class FanoutStage final : public flowpipe::ITransformStage {
    public:
        explicit FanoutStage(const flowpipe::StageSpec& spec)
            : name_(spec.name) {}

        std::string name() const override { return name_; }

        void run(flowpipe::StageContext& ctx,
                 flowpipe::BoundedQueue<flowpipe::Payload>& in,
                 flowpipe::BoundedQueue<flowpipe::Payload>& out) override {
            while (!ctx.stop.stop_requested()) {
                auto item = in.pop(ctx.stop);
                if (!item) break;

                out.push(*item + "-A", ctx.stop);
                out.push(*item + "-B", ctx.stop);
            }
            out.close();
        }

    private:
        std::string name_;
    };

    std::shared_ptr<flowpipe::IStage>
    make_fanout_stage(const flowpipe::StageSpec& spec) {
        return std::make_shared<FanoutStage>(spec);
    }

}  // namespace example
