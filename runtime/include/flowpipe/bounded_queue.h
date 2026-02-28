#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

#include "flowpipe/queue.h"

namespace flowpipe {

template <typename T>
class BoundedQueue : public IQueue<T> {
 public:
  explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

  bool push(T item, const StopToken& stop) override {
    std::unique_lock lock(mu_);
    // Block until there is space, the queue is closed, or stop is requested.
    // close() notifies not_full_, so the runtime's close_runtime_queues() call
    // (which always follows a stop request) wakes blocked producers immediately.
    not_full_.wait(lock, [this, &stop] {
      return stop.stop_requested() || closed_ || queue_.size() < capacity_;
    });

    if (stop.stop_requested() || closed_)
      return false;

    queue_.push_back(std::move(item));
    not_empty_.notify_one();
    return true;
  }

  std::optional<T> pop(const StopToken& stop) override {
    std::unique_lock lock(mu_);
    // Block until an item is available, the queue is closed, or stop is requested.
    not_empty_.wait(lock, [this, &stop] {
      return stop.stop_requested() || closed_ || !queue_.empty();
    });

    if (!queue_.empty()) {
      T item = std::move(queue_.front());
      queue_.pop_front();
      not_full_.notify_one();
      return item;
    }
    return std::nullopt;
  }

  void close() override {
    std::lock_guard lock(mu_);
    closed_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
  }

 private:
  std::size_t capacity_;
  std::mutex mu_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  std::deque<T> queue_;
  bool closed_{false};
};

}  // namespace flowpipe
