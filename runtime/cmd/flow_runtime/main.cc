#include <google/protobuf/util/json_util.h>
#include <yaml-cpp/yaml.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "flowpipe/runtime.h"
#include "flowpipe/otel/telemetry.h"
#include "flowpipe/util/yaml_to_json.h"
#include "flowpipe/v1/flow.pb.h"

static bool load_from_yaml(const std::string& path, flowpipe::v1::FlowSpec& flow) {
  YAML::Node root;
  try {
    root = YAML::LoadFile(path);
  } catch (const YAML::Exception& e) {
    std::cerr << "yaml parse error: " << e.what() << "\n";
    return false;
  }

  // Convert YAML → JSON text
  std::stringstream json;
  flowpipe::util::yaml_to_json(root, json);

  // Parse JSON → Protobuf
  google::protobuf::util::JsonParseOptions opts;
  opts.ignore_unknown_fields = false;

  auto status = google::protobuf::util::JsonStringToMessage(json.str(), &flow, opts);

  if (!status.ok()) {
    std::cerr << "yaml → json → protobuf failed: " << status.ToString() << "\n";
    return false;
  }

  return true;
}

static bool load_from_json(const std::string& path, flowpipe::v1::FlowSpec& flow) {
  // Read file into string
  std::ifstream in(path);
  if (!in) {
    std::cerr << "failed to open json file: " << path << "\n";
    return false;
  }

  std::stringstream buffer;
  buffer << in.rdbuf();
  std::string json = buffer.str();

  // Configure JSON parsing options
  google::protobuf::util::JsonParseOptions opts;
  opts.ignore_unknown_fields = false;  // set true if you want forward compatibility

  // Parse JSON → Protobuf
  auto status = google::protobuf::util::JsonStringToMessage(json, &flow, opts);

  if (!status.ok()) {
    std::cerr << "json → protobuf parse failed: " << status.ToString() << "\n";
    return false;
  }

  return true;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: flow_runtime <flow.yaml|flow.json>\n";
    return 1;
  }

  const std::string path = argv[1];
  flowpipe::v1::FlowSpec flow;

  bool ok = false;
  if (path.ends_with(".yaml") || path.ends_with(".yml")) {
    ok = load_from_yaml(path, flow);
  } else if (path.ends_with(".json")) {
    ok = load_from_json(path, flow);
  } else {
    std::cerr << "unsupported file type (use yaml or json)\n";
    return 1;
  }

  if (!ok) {
    std::cerr << "failed to load flow config\n";
    return 1;
  }

#ifdef FLOWPIPE_ENABLE_OTEL
  const char* endpoint_env = std::getenv("FLOWPIPE_OTEL_GRPC_ENDPOINT");
  const std::string otel_endpoint = endpoint_env ? endpoint_env : "0.0.0.0:4317";

  flowpipe::otel::Init({
      .service_name = "flowpipe-runtime",
      .endpoint = otel_endpoint,
  });
#endif

  flowpipe::Runtime runtime;
  const int result = runtime.run(flow);

#ifdef FLOWPIPE_ENABLE_OTEL
  flowpipe::otel::Shutdown();
#endif

  return result;
}
