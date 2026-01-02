#include <google/protobuf/util/json_util.h>
#include <yaml-cpp/yaml.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "flowpipe/observability/logging_runtime.h"
#include "flowpipe/observability/observability.h"
#include "flowpipe/runtime.h"
#include "flowpipe/util/yaml_to_json.h"
#include "flowpipe/v1/flow.pb.h"

// ============================================================
// Helpers
// ============================================================

// ------------------------------------------------------------
// Load flow spec from YAML
// ------------------------------------------------------------
static bool LoadFromYaml(const std::string& path, flowpipe::v1::FlowSpec& flow) {
  FP_LOG_DEBUG_FMT("loading flow spec from YAML: %s", path.c_str());

  YAML::Node root;
  try {
    root = YAML::LoadFile(path);
  } catch (const YAML::Exception& e) {
    std::cerr << "yaml parse error: " << e.what() << "\n";
    return false;
  }

  FP_LOG_DEBUG("yaml parsed successfully, converting to JSON");

  std::stringstream json;
  flowpipe::util::yaml_to_json(root, json);

  google::protobuf::util::JsonParseOptions opts;
  opts.ignore_unknown_fields = false;

  FP_LOG_DEBUG("parsing JSON into FlowSpec protobuf");

  auto status = google::protobuf::util::JsonStringToMessage(json.str(), &flow, opts);

  if (!status.ok()) {
    std::cerr << "yaml → json → protobuf failed: " << status.ToString() << "\n";
    return false;
  }

  FP_LOG_DEBUG("flow spec loaded successfully from YAML");
  return true;
}

// ------------------------------------------------------------
// Load flow spec from JSON
// ------------------------------------------------------------
static bool LoadFromJson(const std::string& path, flowpipe::v1::FlowSpec& flow) {
  FP_LOG_DEBUG_FMT("loading flow spec from JSON: %s", path.c_str());

  std::ifstream in(path);
  if (!in) {
    std::cerr << "failed to open json file: " << path << "\n";
    return false;
  }

  std::stringstream buffer;
  buffer << in.rdbuf();

  google::protobuf::util::JsonParseOptions opts;
  opts.ignore_unknown_fields = false;

  FP_LOG_DEBUG("parsing JSON into FlowSpec protobuf");

  auto status = google::protobuf::util::JsonStringToMessage(buffer.str(), &flow, opts);

  if (!status.ok()) {
    std::cerr << "json → protobuf parse failed: " << status.ToString() << "\n";
    return false;
  }

  FP_LOG_DEBUG("flow spec loaded successfully from JSON");
  return true;
}

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv) {
  FP_LOG_DEBUG("flow_runtime starting");

  // ----------------------------------------------------------
  // Argument parsing
  // ----------------------------------------------------------
  if (argc != 2) {
    std::cerr << "usage: flow_runtime <flow.yaml|flow.json>\n";
    return 1;
  }

  const std::string path = argv[1];
  FP_LOG_DEBUG_FMT("flow spec path: %s", path.c_str());

  flowpipe::v1::FlowSpec flow;

  // ----------------------------------------------------------
  // Load flow specification
  // ----------------------------------------------------------
  bool ok = false;
  if (path.ends_with(".yaml") || path.ends_with(".yml")) {
    ok = LoadFromYaml(path, flow);
  } else if (path.ends_with(".json")) {
    ok = LoadFromJson(path, flow);
  } else {
    std::cerr << "unsupported file type (use .yaml or .json)\n";
    return 1;
  }

  if (!ok) {
    std::cerr << "failed to load flow config\n";
    return 1;
  }

  FP_LOG_DEBUG("flow spec loaded successfully");

  // ----------------------------------------------------------
  // Observability initialization
  // ----------------------------------------------------------
  FP_LOG_DEBUG("initializing observability");

  flowpipe::observability::InitFromProto(flow.has_observability() ? &flow.observability()
                                                                  : nullptr);

  // ----------------------------------------------------------
  // Runtime execution
  // ----------------------------------------------------------
  FP_LOG_DEBUG("starting runtime execution");

  flowpipe::Runtime runtime;
  int result = runtime.run(flow);

  FP_LOG_DEBUG_FMT("runtime execution complete (exit_code=%d)", result);

  // ----------------------------------------------------------
  // Observability shutdown
  // ----------------------------------------------------------
  FP_LOG_DEBUG("shutting down observability");

  flowpipe::observability::ShutdownObservability();

  FP_LOG_DEBUG("flow_runtime exiting");
  return result;
}
