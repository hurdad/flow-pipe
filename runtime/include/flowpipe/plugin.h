#pragma once

#include "flowpipe/stage.h"

namespace flowpipe {

// Factory functions exported by plugins
using CreateStageFn = IStage* (*)();
using DestroyStageFn = void (*)(IStage*);

// Required symbol names
constexpr const char* FLOWPIPE_CREATE_STAGE_SYMBOL = "flowpipe_create_stage";
constexpr const char* FLOWPIPE_DESTROY_STAGE_SYMBOL = "flowpipe_destroy_stage";

}  // namespace flowpipe
