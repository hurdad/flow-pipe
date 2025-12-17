#include "flowpipe/stage.h"

namespace flowpipe {

    class NoopSource final : public ISourceStage {
    public:
        explicit NoopSource(const StageSpec& spec)
            : name_(spec.name) {
            if (auto it = spec.config.find("count"); it != spec.config.end()) {
                count_ = std::stoll(it->second);
            }
        }

        std::string name() const override { return name_; }

        void run(StageContext& ctx,
                 BoundedQueue<Payload>& out) override {
            for (int64_t i = 0; i < count_ && !ctx.stop.stop_requested(); ++i) {
                out.push("msg-" + std::to_string(i), ctx.stop);
            }
            out.close();
        }

    private:
        std::string name_;
        int64_t count_{10};
    };

    std::shared_ptr<IStage> MakeNoopSource(const StageSpec& spec) {
        return std::make_shared<NoopSource>(spec);
    }

}  // namespace flowpipe
