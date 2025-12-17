#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace flowpipe {

enum class ExecMode { STREAMING, JOB };

struct QueueSpec {
  std::string name;
  std::string type;
  std::size_t capacity{1024};
};

struct StageSpec {
  std::string name;
  std::string type;
  int threads{1};
  std::string input;
  std::string output;
  std::unordered_map<std::string, std::string> config;
};

struct FlowSpec {
  std::string name;
  ExecMode mode{ExecMode::STREAMING};
  std::vector<QueueSpec> queues;
  std::vector<StageSpec> stages;
};

} // namespace flowpipe
