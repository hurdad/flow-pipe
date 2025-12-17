#pragma once

#include "flow_spec.h"
#include "stage_registry.h"

namespace flowpipe {

class Runtime {
public:
  explicit Runtime(StageRegistry registry);

  int run(const FlowSpec &spec);

private:
  StageRegistry registry_;
};

} // namespace flowpipe
