#include <flowpipe/stage.h>
#include <unordered_map>
#include <iostream>

namespace example {

    class AggregateStage final : public flowpipe::ISinkStage {
    public:
        explicit AggregateStage(const flowpipe::StageSpec& spec)
            : name_(spec.name) {}

        std::string name() const override { return name_; }

        void run(flowpipe::StageContext& ctx,
                 flowpipe::BoundedQueue<flowpipe::Payload>& in) override {
            std::unordered_map<std::string, int> counts;

            while (!ctx.stop.stop_requested()) {
                auto item = in.pop(ctx.stop);
                if (!item) break;
                counts[*item]++;
            }

            for (const auto& [k, v] : counts) {
                std::cout << k << " => " << v << "\n";
            }
        }

    private:
        std::string name_;
    };

    std::shared_ptr<flowpipe::IStage>
    make_aggregate_stage(const flowpipe::StageSpec& spec) {
        return std::make_shared<AggregateStage>(spec);
    }

}  // namespace example
