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

  explicit StopToken(const std::atomic<bool>* flag) noexcept : flag_(flag) {}

  // Returns true if stop has been requested
  bool stop_requested() const noexcept {
    return flag_ && flag_->load(std::memory_order_relaxed);
  }

 private:
  const std::atomic<bool>* flag_;
};

}  // namespace flowpipe
