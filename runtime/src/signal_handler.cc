#include "flowpipe/signal_handler.h"

#include <csignal>
#include <functional>
#include <utility>

namespace flowpipe {

static std::atomic<bool>* global_stop{nullptr};
static std::function<void()> global_on_stop;

static void handle_signal(int) {
  if (global_stop) {
    global_stop->store(true);
    if (global_on_stop) {
      global_on_stop();
    }
  }
}

void SignalHandler::install(std::atomic<bool>& stop_flag, std::function<void()> on_stop) {
  global_stop = &stop_flag;
  global_on_stop = std::move(on_stop);
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
}

}  // namespace flowpipe
