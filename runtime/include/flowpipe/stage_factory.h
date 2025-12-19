#pragma once

#include <string>

#include "flowpipe/plugin.h"

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

    private:
        std::string resolve_path(const std::string& plugin_name);

        std::string plugin_dir_;
    };

} // namespace flowpipe
