#include "flowpipe/signal_handler.h"

#include <csignal>

namespace flowpipe {

// Written only from the signal handler using async-signal-safe assignment.
static volatile sig_atomic_t g_signaled = 0;

// Pointer to the runtime stop flag, set once from the main thread before any
// signal can be delivered.
static std::atomic<bool>* g_stop_flag = nullptr;

static void handle_signal(int) {
  // Only sig_atomic_t assignment is guaranteed async-signal-safe by POSIX.
  // The actual atomic<bool> store is done from the main thread via relay().
  g_signaled = 1;
}

void SignalHandler::install(std::atomic<bool>& stop_flag) {
  g_stop_flag = &stop_flag;

  struct sigaction sa {};
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  // SA_RESTART: automatically restart interrupted syscalls where possible.
  sa.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}

bool SignalHandler::relay() noexcept {
  if (g_signaled && g_stop_flag) {
    g_stop_flag->store(true, std::memory_order_release);
    return true;
  }
  return false;
}

}  // namespace flowpipe
