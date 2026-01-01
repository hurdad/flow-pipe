#pragma once

#include <google/protobuf/struct.pb.h>

namespace flowpipe {

class ConfigurableStage {
 public:
  virtual ~ConfigurableStage() = default;

  // Called once after stage construction.
  // The config is opaque to the runtime.
  // The plugin is responsible for parsing & validating.
  virtual bool Configure(const google::protobuf::Struct& config) = 0;
};

}  // namespace flowpipe
