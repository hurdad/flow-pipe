#pragma once

#include "flowpipe/stage_registry.h"
#include "flowpipe/v1/flow.pb.h"

namespace flowpipe {

class Runtime {
 public:
  Runtime();

  int run(const flowpipe::v1::FlowSpec& spec);

 private:
  StageRegistry registry_;
};

}  // namespace flowpipe
