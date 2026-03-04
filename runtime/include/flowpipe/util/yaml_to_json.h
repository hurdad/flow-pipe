#pragma once

#include <yaml-cpp/yaml.h>

#include <cstdio>
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
 *   YAML boolean words (true/false/yes/no/on/off) are emitted as JSON
 *   true/false — protobuf JSON rejects quoted booleans for bool fields.
 *   All other scalars are emitted as strings; protobuf JSON accepts
 *   quoted integers/floats for numeric fields.
 */

namespace flowpipe::util {

inline void yaml_to_json(const YAML::Node& node, std::ostream& out);

// Emit a JSON-escaped quoted string. Handles all characters requiring escaping
// per RFC 8259 §7: backslash, double-quote, and control characters (U+0000–U+001F).
inline void json_escape_string(const std::string& s, std::ostream& out) {
  out << '"';
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out << buf;
        } else {
          out << static_cast<char>(c);
        }
    }
  }
  out << '"';
}

inline void yaml_map_to_json(const YAML::Node& node, std::ostream& out) {
  out << "{";
  bool first = true;
  for (const auto& it : node) {
    if (!first)
      out << ",";
    first = false;

    json_escape_string(it.first.as<std::string>(), out);
    out << ":";

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
  const std::string val = node.as<std::string>();

  // Emit YAML boolean words as bare JSON true/false.
  // Protobuf JSON does not accept quoted booleans for bool fields.
  if (val == "true" || val == "True" || val == "TRUE" || val == "yes" || val == "Yes" ||
      val == "YES" || val == "on" || val == "On" || val == "ON") {
    out << "true";
    return;
  }
  if (val == "false" || val == "False" || val == "FALSE" || val == "no" || val == "No" ||
      val == "NO" || val == "off" || val == "Off" || val == "OFF") {
    out << "false";
    return;
  }

  json_escape_string(val, out);
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
