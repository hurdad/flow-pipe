#include "flowpipe/stage_registry.h"

#include <google/protobuf/struct.pb.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "flowpipe/configurable_stage.h"
#include "flowpipe/stage.h"
#include "flowpipe/stage_factory.h"

namespace flowpipe {
namespace {

class RecordingLoader : public StageLoader {
 public:
  LoadedPlugin load(const std::string& plugin_name) override {
    load_calls.push_back(plugin_name);
    if (throw_on_load) {
      throw std::runtime_error("load failure");
    }
    return loaded_plugin;
  }

  void unload(LoadedPlugin& plugin) override {
    ++unload_calls;
    last_unloaded_path = plugin.path;
  }

  LoadedPlugin loaded_plugin{
      .handle = reinterpret_cast<void*>(0x1),
      .create = nullptr,
      .destroy = nullptr,
      .path = "fake.so",
  };

  bool throw_on_load = false;
  int unload_calls = 0;
  std::string last_unloaded_path;
  std::vector<std::string> load_calls;
};

class DummyStage : public IStage {
 public:
  explicit DummyStage(std::string name = "dummy") : name_(std::move(name)) {}
  std::string name() const override {
    return name_;
  }

 private:
  std::string name_;
};

class RejectingConfigStage : public ConfigurableStage, public IStage {
 public:
  explicit RejectingConfigStage(bool accept) : accept_(accept) {}

  bool configure(const google::protobuf::Struct&) override {
    ++configure_calls;
    return accept_;
  }

  std::string name() const override {
    return "rejecting";
  }

  int configure_calls = 0;

 private:
  bool accept_;
};

IStage* CreateNullStage() {
  return nullptr;
}
IStage* CreateDummyStage() {
  return new DummyStage();
}
IStage* CreateRejectingStage() {
  return new RejectingConfigStage(false);
}

struct DestroyCounter {
  void operator()(IStage* stage) {
    ++count;
    delete stage;
  }
  int count = 0;
};

// ------------------------------------------------------------------
// Function-pointer-friendly destroy thunk (no captures).
// ------------------------------------------------------------------
static DestroyCounter* g_destroy_counter = nullptr;

static void DestroyStageThunk(IStage* stage) {
  // In these tests we always set this before using the plugin.
  EXPECT_NE(g_destroy_counter, nullptr);
  (*g_destroy_counter)(stage);
}

TEST(StageRegistryTest, PropagatesPluginLoadFailures) {
  auto loader = std::make_unique<RecordingLoader>();
  loader->throw_on_load = true;
  auto* loader_ptr = loader.get();

  StageRegistry registry(std::move(loader));
  EXPECT_THROW(registry.create_stage("missing", nullptr), std::runtime_error);
  EXPECT_EQ(loader_ptr->load_calls.size(), 1u);
  EXPECT_EQ(loader_ptr->unload_calls, 0);
}

TEST(StageRegistryTest, ThrowsWhenPluginCreateReturnsNull) {
  DestroyCounter destroy_counter;
  g_destroy_counter = &destroy_counter;

  auto loader = std::make_unique<RecordingLoader>();
  loader->loaded_plugin.create = &CreateNullStage;
  loader->loaded_plugin.destroy = &DestroyStageThunk;
  auto* loader_ptr = loader.get();

  {
    StageRegistry registry(std::move(loader));
    EXPECT_THROW(registry.create_stage("bad", nullptr), std::runtime_error);
  }

  EXPECT_EQ(loader_ptr->unload_calls, 1);
  EXPECT_EQ(loader_ptr->last_unloaded_path, "fake.so");
  EXPECT_EQ(destroy_counter.count, 0);

  g_destroy_counter = nullptr;
}

TEST(StageRegistryTest, RejectsConfigurationAndDestroysInstance) {
  DestroyCounter destroy_counter;
  g_destroy_counter = &destroy_counter;

  auto loader = std::make_unique<RecordingLoader>();
  loader->loaded_plugin.create = &CreateRejectingStage;
  loader->loaded_plugin.destroy = &DestroyStageThunk;
  auto* loader_ptr = loader.get();

  {
    StageRegistry registry(std::move(loader));
    google::protobuf::Struct cfg;
    (*cfg.mutable_fields())["value"].set_number_value(7.0);
    EXPECT_THROW(registry.create_stage("reject", &cfg), std::runtime_error);
  }

  EXPECT_EQ(destroy_counter.count, 1);
  EXPECT_EQ(loader_ptr->unload_calls, 1);

  g_destroy_counter = nullptr;
}

TEST(StageRegistryTest, DestroyAndShutdownReleaseInstances) {
  DestroyCounter destroy_counter;
  g_destroy_counter = &destroy_counter;

  auto loader = std::make_unique<RecordingLoader>();
  loader->loaded_plugin.create = &CreateDummyStage;
  loader->loaded_plugin.destroy = &DestroyStageThunk;
  auto* loader_ptr = loader.get();

  {
    StageRegistry registry(std::move(loader));
    IStage* first = registry.create_stage("dummy", nullptr);
    [[maybe_unused]] IStage* second = registry.create_stage("dummy", nullptr);

    registry.destroy_stage(first);
    EXPECT_EQ(destroy_counter.count, 1);

    // Remaining instance should be cleaned up on shutdown
  }

  EXPECT_EQ(destroy_counter.count, 2);
  EXPECT_EQ(loader_ptr->unload_calls, 1);

  g_destroy_counter = nullptr;
}

}  // namespace
}  // namespace flowpipe
