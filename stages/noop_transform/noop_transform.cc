#include "flowpipe/stage.h"
#include "flowpipe/configurable_stage.h"
#include "flowpipe/observability/logging.h"

#include "noop_transform.pb.h"

#include <thread>
#include <chrono>

using namespace flowpipe;

using NoopTransformConfig =
    flowpipe::stages::noop::v1::NoopTransformConfig;

class NoopTransform final
    : public ITransformStage
    , public IConfigurableStage {
public:
  std::string name() const override {
    return "noop_transform";
  }

  NoopTransform() {
    FP_LOG_INFO("noop_transform constructed");
  }

  ~NoopTransform() override {
    FP_LOG_INFO("noop_transform destroyed");
  }

  // ------------------------------------------------------------
  // IConfigurableStage
  // ------------------------------------------------------------
  bool set_config(const google::protobuf::Message& msg) override {
    const auto* cfg = dynamic_cast<const NoopTransformConfig*>(&msg);
    if (!cfg) {
      FP_LOG_ERROR("noop_transform invalid config type");
      return false;
    }

    config_ = *cfg;

    FP_LOG_INFO("noop_transform configured");

    if (config_.verbose()) {
      FP_LOG_INFO("noop_transform verbose logging enabled");
    }

    if (config_.delay_ms() > 0) {
      FP_LOG_INFO("noop_transform delay enabled");
    }

    return true;
  }

  void process(StageContext& ctx,
               const Payload& input,
               Payload& output) override {
    if (ctx.stop.stop_requested()) {
      return;
    }

    if (config_.verbose()) {
      FP_LOG_DEBUG("noop_transform processing payload");
    }

    if (config_.delay_ms() > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(config_.delay_ms()));
    }

    output = input;
  }

private:
  NoopTransformConfig config_;
};

extern "C" {

  IStage* flowpipe_create_stage() {
    FP_LOG_INFO("creating noop_transform stage");
    return new NoopTransform();
  }

  void flowpipe_destroy_stage(IStage* stage) {
    FP_LOG_INFO("destroying noop_transform stage");
    delete stage;
  }

}
