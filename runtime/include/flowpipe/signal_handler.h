#pragma once

#include <atomic>
#include <functional>

namespace flowpipe {

class SignalHandler {
 public:
  static void install(std::atomic<bool>& stop_flag, std::function<void()> on_stop);
};

}  // namespace flowpipe
