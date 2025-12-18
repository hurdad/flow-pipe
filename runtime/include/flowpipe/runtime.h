#pragma once

#include "flowpipe/v1/flow.pb.h"
#include "stage_registry.h"

namespace flowpipe {

class Runtime {
public:
  explicit Runtime(StageRegistry registry);

  int run(const flowpipe::v1::FlowSpec &spec);

private:
  StageRegistry registry_;
};

} // namespace flowpipe
