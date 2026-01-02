#pragma once

#include <google/protobuf/struct.pb.h>

#include <memory>
#include <string>

#include "flowpipe/plugin.h"
#include "flowpipe/stage.h"

namespace flowpipe {

struct LoadedPlugin {
  void* handle = nullptr;
  CreateStageFn create = nullptr;
  DestroyStageFn destroy = nullptr;
  std::string path;
};

class StageFactory {
 public:
  explicit StageFactory(std::string plugin_dir = "/opt/flow-pipe/plugins");

  LoadedPlugin load(const std::string& plugin_name);
  void unload(LoadedPlugin& plugin);

  // Create a stage instance and pass opaque config to the plugin
  std::unique_ptr<IStage> create_stage(const LoadedPlugin& plugin,
                                       const google::protobuf::Struct* config);

 private:
  std::string resolve_path(const std::string& plugin_name);

  std::string plugin_dir_;
};

}  // namespace flowpipe
