#pragma once

#include <atomic>

namespace flowpipe {

    class SignalHandler {
    public:
        static void install(std::atomic<bool>& stop_flag);
    };

}  // namespace flowpipe
