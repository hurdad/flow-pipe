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

TEST(BoundedQueueTest, StopRequestUnblocksWaitingPushAndPop) {
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
  stop_pop.request_stop();

  EXPECT_EQ(blocked_push.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  EXPECT_EQ(blocked_pop.wait_for(std::chrono::seconds(1)), std::future_status::ready);

  EXPECT_FALSE(blocked_push.get());
  EXPECT_FALSE(blocked_pop.get().has_value());
}

}  // namespace
}  // namespace flowpipe
