#pragma once

#include <atomic>

namespace flowpipe {

class SignalHandler {
 public:
  // Install handlers for SIGINT and SIGTERM.
  // stop_flag must remain valid for the lifetime of the process.
  static void install(std::atomic<bool>& stop_flag);

  // Relay any received signal to the stop flag using release semantics.
  // Must be called periodically from the main thread (not from a signal handler).
  // Returns true if a signal was relayed.
  static bool relay() noexcept;
};

}  // namespace flowpipe
