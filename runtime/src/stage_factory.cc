#include "flowpipe/stage_factory.h"

#include <dlfcn.h>
#include <stdexcept>
#include <sstream>

namespace flowpipe {

    StageFactory::StageFactory(std::string plugin_dir)
        : plugin_dir_(std::move(plugin_dir)) {}

    LoadedPlugin StageFactory::load(const std::string& plugin_name) {
        std::string path = resolve_path(plugin_name);

        void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            throw std::runtime_error(dlerror());
        }

        auto create = reinterpret_cast<CreateStageFn>(
            dlsym(handle, FLOWPIPE_CREATE_STAGE_SYMBOL));
        auto destroy = reinterpret_cast<DestroyStageFn>(
            dlsym(handle, FLOWPIPE_DESTROY_STAGE_SYMBOL));

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

    std::string StageFactory::resolve_path(const std::string& plugin_name) {
        if (!plugin_name.empty() && plugin_name[0] == '/') {
            return plugin_name;
        }
        std::ostringstream oss;
        oss << plugin_dir_ << "/" << plugin_name;
        return oss.str();
    }

} // namespace flowpipe
