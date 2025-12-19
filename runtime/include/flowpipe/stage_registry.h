#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "flowpipe/stage.h"
#include "flowpipe/stage_factory.h"

namespace flowpipe {

class StageRegistry {
 public:
  StageRegistry() = default;
  ~StageRegistry();

  StageRegistry(const StageRegistry&) = delete;
  StageRegistry& operator=(const StageRegistry&) = delete;

  IStage* create_stage(const std::string& plugin_name);
  void destroy_stage(IStage* stage);
  void shutdown();

 private:
  struct StageInstance {
    std::string plugin_name;
    IStage* stage = nullptr;
  };

  StageFactory factory_;
  std::unordered_map<std::string, LoadedPlugin> plugins_;
  std::vector<StageInstance> instances_;
};

}  // namespace flowpipe
