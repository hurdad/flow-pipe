#include <flowpipe/stage.h>
#include <fstream>

namespace example {

    class CsvSource final : public flowpipe::ISourceStage {
    public:
        explicit CsvSource(const flowpipe::v1::StageSpec& spec)
            : name_(spec.name()),
              path_("/foo" /*spec.params.at("path")*/) {}

        std::string name() const override { return name_; }

        void run(flowpipe::StageContext& ctx,
                 flowpipe::BoundedQueue<flowpipe::Payload>& out) override {
            std::ifstream file(path_);
            std::string line;

            while (std::getline(file, line) && !ctx.stop.stop_requested()) {
                out.push(line, ctx.stop);
            }
            out.close();
        }

    private:
        std::string name_;
        std::string path_;
    };

    std::shared_ptr<flowpipe::IStage>
    make_csv_source(const flowpipe::v1::StageSpec& spec) {
        return std::make_shared<CsvSource>(spec);
    }

}  // namespace example
