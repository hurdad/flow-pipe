#pragma once

#include <google/protobuf/struct.pb.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "flowpipe/stage.h"
#include "flowpipe/stage_factory.h"

namespace flowpipe {

class StageRegistry {
 public:
  explicit StageRegistry(std::unique_ptr<StageLoader> loader = nullptr);
  ~StageRegistry();

  StageRegistry(const StageRegistry&) = delete;
  StageRegistry& operator=(const StageRegistry&) = delete;

  IStage* create_stage(const std::string& plugin_name,
                       const google::protobuf::Struct* config = nullptr);
  void destroy_stage(IStage* stage);
  void shutdown();

 private:
  struct StageInstance {
    std::string plugin_name;
    IStage* stage = nullptr;
  };

  std::unique_ptr<StageLoader> loader_;
  // Guards the full registry lifecycle state (plugins_ + instances_).
  // Ordering contract:
  //  - shutdown() holds this lock while it tears down instances/plugins so it
  //    cannot race with concurrent destroy_stage() calls.
  //  - create_stage() and destroy_stage() both hold this lock while mutating
  //    registry containers, so workers cannot insert while another thread is
  //    erasing.
  mutable std::mutex lifecycle_mutex_;
  std::unordered_map<std::string, LoadedPlugin> plugins_;
  std::vector<StageInstance> instances_;
};

}  // namespace flowpipe
