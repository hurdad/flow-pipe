#pragma once

#include <google/protobuf/struct.pb.h>
#include <google/protobuf/util/json_util.h>

#include <string>

namespace flowpipe {

template <typename Config>
class ProtobufConfigParser {
 public:
  static bool Parse(const google::protobuf::Struct& config,
                    Config* out,
                    std::string* error = nullptr) {
    if (!out) {
      if (error) {
        *error = "config output pointer is null";
      }
      return false;
    }

    std::string json;
    auto status = google::protobuf::util::MessageToJsonString(config, &json);
    if (!status.ok()) {
      if (error) {
        *error = status.ToString();
      }
      return false;
    }

    status = google::protobuf::util::JsonStringToMessage(json, out);
    if (!status.ok()) {
      if (error) {
        *error = status.ToString();
      }
      return false;
    }

    return true;
  }
};

}  // namespace flowpipe
