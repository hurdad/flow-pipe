#include <flowpipe/stage.h>
#include <sstream>

namespace example {

    class CsvParser final : public flowpipe::ITransformStage {
    public:
        explicit CsvParser(const flowpipe::v1::StageSpec& spec)
            : name_(spec.name()) {}

        std::string name() const override { return name_; }

        void run(flowpipe::StageContext& ctx,
                 flowpipe::BoundedQueue<flowpipe::Payload>& in,
                 flowpipe::BoundedQueue<flowpipe::Payload>& out) override {
            while (!ctx.stop.stop_requested()) {
                auto row = in.pop(ctx.stop);
                if (!row) break;

                std::stringstream ss(*row);
                std::string field;
                std::string out_row;

                while (std::getline(ss, field, ',')) {
                    out_row += "[" + field + "]";
                }

                out.push(std::move(out_row), ctx.stop);
            }
            out.close();
        }

    private:
        std::string name_;
    };

    std::shared_ptr<flowpipe::IStage>
    make_csv_parser(const flowpipe::v1::StageSpec& spec) {
        return std::make_shared<CsvParser>(spec);
    }

}  // namespace example
