#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include "flowpipe/bounded_queue.h"
#include "flowpipe/stop_token.h"

namespace flowpipe {
namespace {

TEST(BoundedQueueTest, PushAndPop) {
  std::atomic<bool> stop_flag{false};
  StopToken stop{&stop_flag};

  BoundedQueue<int> queue(2);

  EXPECT_TRUE(queue.push(42, stop));
  auto item = queue.pop(stop);

  ASSERT_TRUE(item.has_value());
  EXPECT_EQ(*item, 42);
}

// BoundedQueue CVs are notified by close(), not by stop alone.
// The runtime always calls close() after requesting stop, so the correct
// pattern to unblock waiters is stop + close.
TEST(BoundedQueueTest, StopPlusCloseUnblocksWaitingPushAndPop) {
  std::atomic<bool> stop_push_flag{false};
  StopToken stop_push{&stop_push_flag};
  BoundedQueue<int> push_queue(1);
  ASSERT_TRUE(push_queue.push(1, stop_push));
  auto blocked_push =
      std::async(std::launch::async, [&]() { return push_queue.push(2, stop_push); });

  std::atomic<bool> stop_pop_flag{false};
  StopToken stop_pop{&stop_pop_flag};
  BoundedQueue<int> pop_queue(1);
  auto blocked_pop = std::async(std::launch::async, [&]() { return pop_queue.pop(stop_pop); });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  stop_push.request_stop();
  push_queue.close();  // close() notifies CVs, waking the blocked push
  stop_pop.request_stop();
  pop_queue.close();  // close() notifies CVs, waking the blocked pop

  EXPECT_EQ(blocked_push.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  EXPECT_EQ(blocked_pop.wait_for(std::chrono::seconds(1)), std::future_status::ready);

  EXPECT_FALSE(blocked_push.get());
  EXPECT_FALSE(blocked_pop.get().has_value());
}

TEST(BoundedQueueTest, CloseWakesBlockedPop) {
  std::atomic<bool> stop_flag{false};
  StopToken stop{&stop_flag};
  BoundedQueue<int> queue(2);

  auto blocked = std::async(std::launch::async, [&]() { return queue.pop(stop); });

  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  queue.close();

  EXPECT_EQ(blocked.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  EXPECT_FALSE(blocked.get().has_value());
}

TEST(BoundedQueueTest, ItemsDrainedAfterClose) {
  std::atomic<bool> stop_flag{false};
  StopToken stop{&stop_flag};
  BoundedQueue<int> queue(4);

  ASSERT_TRUE(queue.push(1, stop));
  ASSERT_TRUE(queue.push(2, stop));
  queue.close();

  auto first = queue.pop(stop);
  auto second = queue.pop(stop);
  auto closed = queue.pop(stop);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(*first, 1);
  EXPECT_EQ(*second, 2);
  EXPECT_FALSE(closed.has_value());
}

TEST(BoundedQueueTest, PushAfterCloseReturnsFalse) {
  std::atomic<bool> stop_flag{false};
  StopToken stop{&stop_flag};
  BoundedQueue<int> queue(2);

  queue.close();
  EXPECT_FALSE(queue.push(42, stop));
}

}  // namespace
}  // namespace flowpipe
