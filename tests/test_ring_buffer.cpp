#include <iostream>
#include <thread>
#include <cassert>
#include <gtest/gtest.h>

#include "core/ring_buffer.hpp"

TEST(RingBuffer, RingBufferProcess)
{
  constexpr std::size_t QueueSize = 1024;
  constexpr int TotalItems = 100'000;

  iot::core::SPSCQueue<int, QueueSize> queue;

  std::thread producer([&]()
                       {
        for (int i = 0; i < TotalItems; ++i) {
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
        } });

  std::thread consumer([&]()
                       {
        for (int i = 0; i < TotalItems; ++i) {
            std::optional<int> val;
            while (!(val = queue.pop())) {
                std::this_thread::yield();
            }
            ASSERT_EQ(*val, i);
        } });

  producer.join();
  consumer.join();
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}