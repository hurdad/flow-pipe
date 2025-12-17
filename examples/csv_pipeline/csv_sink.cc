#include <flowpipe/stage.h>
#include <iostream>

namespace example {

    class CsvSink final : public flowpipe::ISinkStage {
    public:
        explicit CsvSink(const flowpipe::StageSpec& spec)
            : name_(spec.name) {}

        std::string name() const override { return name_; }

        void run(flowpipe::StageContext& ctx,
                 flowpipe::BoundedQueue<flowpipe::Payload>& in) override {
            while (!ctx.stop.stop_requested()) {
                auto row = in.pop(ctx.stop);
                if (!row) break;
                std::cout << *row << "\n";
            }
        }

    private:
        std::string name_;
    };

    std::shared_ptr<flowpipe::IStage>
    make_csv_sink(const flowpipe::StageSpec& spec) {
        return std::make_shared<CsvSink>(spec);
    }

}  // namespace example
