#pragma once

#include <yaml-cpp/yaml.h>

#include <ostream>
#include <string>

/*
 * Minimal YAML → JSON emitter.
 *
 * Purpose:
 *   - Convert YAML syntax into JSON text
 *   - Let Protobuf JSON parser handle schema + validation
 *
 * Supported:
 *   - maps
 *   - sequences
 *   - scalars (string)
 *   - null
 *
 * NOTE:
 *   Scalars are emitted as strings.
 *   This is intentional and safe for protobuf JSON parsing.
 */

namespace flowpipe::util {

inline void yaml_to_json(const YAML::Node& node, std::ostream& out);

inline void yaml_map_to_json(const YAML::Node& node, std::ostream& out) {
  out << "{";
  bool first = true;
  for (const auto& it : node) {
    if (!first)
      out << ",";
    first = false;

    out << "\"" << it.first.as<std::string>() << "\":";

    yaml_to_json(it.second, out);
  }
  out << "}";
}

inline void yaml_seq_to_json(const YAML::Node& node, std::ostream& out) {
  out << "[";
  for (std::size_t i = 0; i < node.size(); ++i) {
    if (i > 0)
      out << ",";
    yaml_to_json(node[i], out);
  }
  out << "]";
}

inline void yaml_scalar_to_json(const YAML::Node& node, std::ostream& out) {
  out << "\"" << node.as<std::string>() << "\"";
}

inline void yaml_to_json(const YAML::Node& node, std::ostream& out) {
  switch (node.Type()) {
    case YAML::NodeType::Map:
      yaml_map_to_json(node, out);
      break;

    case YAML::NodeType::Sequence:
      yaml_seq_to_json(node, out);
      break;

    case YAML::NodeType::Scalar:
      yaml_scalar_to_json(node, out);
      break;

    case YAML::NodeType::Null:
    default:
      out << "null";
      break;
  }
}

}  // namespace flowpipe::util
