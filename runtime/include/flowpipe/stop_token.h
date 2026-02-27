#pragma once

#include <atomic>

namespace flowpipe {

/**
 * Lightweight cooperative cancellation token.
 *
 * Runtime owns the atomic flag.
 * Stages observe it via this token.
 */
class StopToken {
 public:
  StopToken() noexcept : flag_(nullptr) {}

  explicit StopToken(std::atomic<bool>* flag) noexcept : flag_(flag) {}

  // Returns true if stop has been requested
  bool stop_requested() const noexcept {
    return flag_ && flag_->load(std::memory_order_relaxed);
  }

  void request_stop() const noexcept {
    if (flag_) {
      flag_->store(true, std::memory_order_relaxed);
    }
  }

 private:
  std::atomic<bool>* flag_;
};

}  // namespace flowpipe
