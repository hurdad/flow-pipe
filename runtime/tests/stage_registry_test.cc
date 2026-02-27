#include "flowpipe/stage_registry.h"

#include <google/protobuf/struct.pb.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
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


class ThreadSafeRecordingLoader : public StageLoader {
 public:
  LoadedPlugin load(const std::string& plugin_name) override {
    load_calls.fetch_add(1, std::memory_order_relaxed);
    LoadedPlugin plugin = loaded_plugin;
    plugin.path = plugin_name + ".so";
    return plugin;
  }

  void unload(LoadedPlugin&) override {
    unload_calls.fetch_add(1, std::memory_order_relaxed);
  }

  LoadedPlugin loaded_plugin{
      .handle = reinterpret_cast<void*>(0x1),
      .create = nullptr,
      .destroy = nullptr,
      .path = "fake.so",
  };

  std::atomic<int> load_calls{0};
  std::atomic<int> unload_calls{0};
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


static std::atomic<int>* g_atomic_destroy_counter = nullptr;

static void AtomicDestroyStageThunk(IStage* stage) {
  EXPECT_NE(g_atomic_destroy_counter, nullptr);
  g_atomic_destroy_counter->fetch_add(1, std::memory_order_relaxed);
  delete stage;
}

TEST(StageRegistryTest, ConcurrentCreateDestroyAndShutdownAreSynchronized) {
  std::atomic<int> destroy_count{0};
  g_atomic_destroy_counter = &destroy_count;

  auto loader = std::make_unique<ThreadSafeRecordingLoader>();
  loader->loaded_plugin.create = &CreateDummyStage;
  loader->loaded_plugin.destroy = &AtomicDestroyStageThunk;
  auto* loader_ptr = loader.get();

  constexpr int kThreads = 8;
  constexpr int kIterationsPerThread = 200;

  {
    StageRegistry registry(std::move(loader));

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
      workers.emplace_back([&registry]() {
        for (int j = 0; j < kIterationsPerThread; ++j) {
          IStage* stage = registry.create_stage("dummy", nullptr);
          registry.destroy_stage(stage);
        }
      });
    }

    for (auto& worker : workers) {
      worker.join();
    }

    registry.shutdown();
    registry.shutdown();
  }

  EXPECT_EQ(destroy_count.load(std::memory_order_relaxed), kThreads * kIterationsPerThread);
  EXPECT_EQ(loader_ptr->load_calls.load(std::memory_order_relaxed), 1);
  EXPECT_EQ(loader_ptr->unload_calls.load(std::memory_order_relaxed), 1);

  g_atomic_destroy_counter = nullptr;
}

}  // namespace
}  // namespace flowpipe
