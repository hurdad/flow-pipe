#include "flowpipe/stage_factory.h"

#include <dlfcn.h>
#include <google/protobuf/struct.pb.h>

#include <sstream>
#include <stdexcept>

#include "flowpipe/configurable_stage.h"

namespace flowpipe {

StageFactory::StageFactory(std::string plugin_dir) : plugin_dir_(std::move(plugin_dir)) {}

LoadedPlugin StageFactory::load(const std::string& plugin_name) {
  std::string path = resolve_path(plugin_name);

  void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    throw std::runtime_error(dlerror());
  }

  auto create = reinterpret_cast<CreateStageFn>(dlsym(handle, FLOWPIPE_CREATE_STAGE_SYMBOL));
  auto destroy = reinterpret_cast<DestroyStageFn>(dlsym(handle, FLOWPIPE_DESTROY_STAGE_SYMBOL));

  if (!create || !destroy) {
    dlclose(handle);
    throw std::runtime_error("invalid stage plugin ABI");
  }

  return LoadedPlugin{
      .handle = handle,
      .create = create,
      .destroy = destroy,
      .path = path,
  };
}

void StageFactory::unload(LoadedPlugin& plugin) {
  if (plugin.handle) {
    dlclose(plugin.handle);
    plugin.handle = nullptr;
  }
}

// ------------------------------------------------------------
// Create + configure a stage instance (OPAQUE CONFIG)
// ------------------------------------------------------------
std::unique_ptr<IStage> StageFactory::create_stage(const LoadedPlugin& plugin,
                                                   const google::protobuf::Struct* config) {
  IStage* stage = plugin.create();
  if (!stage) {
    throw std::runtime_error("stage plugin returned null");
  }

  // Forward config verbatim if the stage supports it
  if (config) {
    if (auto* configurable = dynamic_cast<ConfigurableStage*>(stage)) {
      if (!configurable->Configure(*config)) {
        plugin.destroy(stage);
        throw std::runtime_error("stage rejected configuration");
      }
    }
  }

  return std::unique_ptr<IStage>(stage);
}

std::string StageFactory::resolve_path(const std::string& plugin_name) {
  if (!plugin_name.empty() && plugin_name[0] == '/') {
    return plugin_name;
  }

  std::ostringstream oss;
  oss << plugin_dir_ << "/" << plugin_name;
  return oss.str();
}

}  // namespace flowpipe
