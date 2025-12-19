#include "flowpipe/stage_registry.h"

#include <stdexcept>

namespace flowpipe {

StageRegistry::~StageRegistry() {
  shutdown();
}

IStage* StageRegistry::create_stage(const std::string& plugin_name) {
  auto it = plugins_.find(plugin_name);
  if (it == plugins_.end()) {
    auto plugin = factory_.load(plugin_name);
    it = plugins_.emplace(plugin_name, std::move(plugin)).first;
  }

  IStage* stage = it->second.create();
  if (!stage) {
    throw std::runtime_error("plugin returned null stage");
  }

  instances_.push_back(StageInstance{
      .plugin_name = plugin_name,
      .stage = stage,
  });

  return stage;
}

void StageRegistry::destroy_stage(IStage* stage) {
  for (auto it = instances_.begin(); it != instances_.end(); ++it) {
    if (it->stage == stage) {
      auto& plugin = plugins_.at(it->plugin_name);
      plugin.destroy(stage);
      instances_.erase(it);
      return;
    }
  }
}

void StageRegistry::shutdown() {
  for (auto& inst : instances_) {
    auto& plugin = plugins_.at(inst.plugin_name);
    plugin.destroy(inst.stage);
  }
  instances_.clear();

  for (auto& kv : plugins_) {
    factory_.unload(kv.second);
  }
  plugins_.clear();
}

}  // namespace flowpipe
