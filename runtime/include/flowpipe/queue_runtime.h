#pragma once

#include <memory>
#include <string>

#include "flowpipe/bounded_queue.h"
#include "flowpipe/payload.h"

namespace flowpipe {

// Runtime representation of a queue derived from QueueSpec.
struct QueueRuntime {
  // Logical queue name (unique within a flow)
  std::string name;

  // Cached capacity from the spec
  uint32_t capacity = 0;

  // Runtime queue used by producers and consumers
  std::shared_ptr<BoundedQueue<Payload>> queue;

  // Optional schema identifier for payload validation.
  std::string schema_id;
};

}  // namespace flowpipe
