#pragma once

#include <cstdint>
#include <string>

namespace flowpipe {

// v1 payload = string
// later: arena-backed buffer, views, ref-counting
using Payload = std::string;

}  // namespace flowpipe
