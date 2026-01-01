#pragma once

#include <google/protobuf/message.h>

namespace flowpipe {

// Optional mixin interface
class IConfigurableStage {
 public:
  virtual ~IConfigurableStage() = default;

  // Called once after stage creation
  virtual bool set_config(const google::protobuf::Message& config) = 0;
};

}  // namespace flowpipe
