#include "flowpipe/signal_handler.h"
#include <csignal>

namespace flowpipe {

    static std::atomic<bool>* global_stop{nullptr};

    static void handle_signal(int) {
        if (global_stop) {
            global_stop->store(true);
        }
    }

    void SignalHandler::install(std::atomic<bool>& stop_flag) {
        global_stop = &stop_flag;
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);
    }

}  // namespace flowpipe
