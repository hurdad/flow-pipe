#pragma once

#include "flowpipe/stage.h"

namespace flowpipe {

// Export macros for plugin symbols (default visibility)
#if defined(_WIN32) || defined(_WIN64)
#define FLOWPIPE_PLUGIN_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define FLOWPIPE_PLUGIN_API __attribute__((visibility("default")))
#else
#define FLOWPIPE_PLUGIN_API
#endif

// Factory functions exported by plugins
using CreateStageFn = IStage* (*)();
using DestroyStageFn = void (*)(IStage*);

// Required symbol names
constexpr const char* FLOWPIPE_CREATE_STAGE_SYMBOL = "flowpipe_create_stage";
constexpr const char* FLOWPIPE_DESTROY_STAGE_SYMBOL = "flowpipe_destroy_stage";

}  // namespace flowpipe
