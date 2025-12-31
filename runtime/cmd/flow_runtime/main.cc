#include <google/protobuf/util/json_util.h>
#include <yaml-cpp/yaml.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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
// YAML is first converted to JSON text and then parsed into
// the protobuf FlowSpec. This allows us to reuse protobuf's
// JSON mapping logic and keep YAML handling minimal.
//
static bool LoadFromYaml(const std::string& path, flowpipe::v1::FlowSpec& flow) {
  YAML::Node root;
  try {
    root = YAML::LoadFile(path);
  } catch (const YAML::Exception& e) {
    std::cerr << "yaml parse error: " << e.what() << "\n";
    return false;
  }

  std::stringstream json;
  flowpipe::util::yaml_to_json(root, json);

  google::protobuf::util::JsonParseOptions opts;
  opts.ignore_unknown_fields = false;

  auto status = google::protobuf::util::JsonStringToMessage(json.str(), &flow, opts);

  if (!status.ok()) {
    std::cerr << "yaml → json → protobuf failed: " << status.ToString() << "\n";
    return false;
  }

  return true;
}

// ------------------------------------------------------------
// Load flow spec from JSON
// ------------------------------------------------------------
// JSON is parsed directly into the protobuf FlowSpec using
// protobuf's JSON utilities.
//
static bool LoadFromJson(const std::string& path, flowpipe::v1::FlowSpec& flow) {
  std::ifstream in(path);
  if (!in) {
    std::cerr << "failed to open json file: " << path << "\n";
    return false;
  }

  std::stringstream buffer;
  buffer << in.rdbuf();

  google::protobuf::util::JsonParseOptions opts;
  opts.ignore_unknown_fields = false;

  auto status = google::protobuf::util::JsonStringToMessage(buffer.str(), &flow, opts);

  if (!status.ok()) {
    std::cerr << "json → protobuf parse failed: " << status.ToString() << "\n";
    return false;
  }

  return true;
}

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv) {
  // ----------------------------------------------------------
  // Argument parsing
  // ----------------------------------------------------------
  //
  // The runtime expects exactly one argument:
  //   - a flow specification file (YAML or JSON)
  //
  // Keeping the CLI minimal makes the runtime easy to script,
  // embed, and run inside containers.
  //
  if (argc != 2) {
    std::cerr << "usage: flow_runtime <flow.yaml|flow.json>\n";
    return 1;
  }

  const std::string path = argv[1];
  flowpipe::v1::FlowSpec flow;

  // ----------------------------------------------------------
  // Load flow specification
  // ----------------------------------------------------------
  //
  // Supported formats:
  //   - YAML (.yaml / .yml): human-friendly, ConfigMaps, files
  //   - JSON (.json): API-driven, tooling, gateways
  //
  // Regardless of input format, the runtime operates purely
  // on the protobuf FlowSpec.
  //
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

  // ----------------------------------------------------------
  // Observability initialization
  // ----------------------------------------------------------
  //
  // Observability is initialized ONCE per process, before any
  // runtime work begins.
  //
  // - Global defaults are loaded from the environment
  // - Flow-level intent (if present) is merged with policy
  // - Logs, traces, and metrics are wired up as needed
  //
  // Passing nullptr means:
  //   "no flow-specific overrides, use runtime defaults"
  //
  flowpipe::observability::InitFromProto(flow.has_observability() ? &flow.observability()
                                                                  : nullptr);

  // ----------------------------------------------------------
  // Runtime execution
  // ----------------------------------------------------------
  //
  // The runtime consumes a fully-validated FlowSpec and
  // executes it according to its declared execution mode
  // (job, service, etc.).
  //
  // All observability signals emitted during execution are
  // captured via the providers initialized above.
  //
  flowpipe::Runtime runtime;
  int result = runtime.run(flow);

  // ----------------------------------------------------------
  // Observability shutdown
  // ----------------------------------------------------------
  //
  // Graceful shutdown is critical for:
  //   - flushing batch logs and spans
  //   - stopping periodic metric readers
  //   - preventing background threads from leaking
  //
  // Shutdown order is handled internally:
  //   Logs → Traces → Metrics
  //
  flowpipe::observability::ShutdownObservability();

  // Propagate the runtime's exit code to the OS.
  return result;
}
