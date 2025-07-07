/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "chre_host/concurrent_fixed_capacity_queue.h"

#include <thread>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace android::chre {

namespace {
constexpr size_t kQueueCapacity = 3;

// Define a test struct that is printable
struct MyTestElement {
  int value;
  [[nodiscard]] std::string toString() const {
    std::ostringstream ss;
    ss << '[' << value << "],";
    return ss.str();
  }
};
}  // namespace

// Test fixture for ConcurrentFixedCapacityQueue
class ConcurrentFixedCapacityQueueTest : public testing::Test {
 protected:
  // Setup (called before each test)
  void SetUp() override {
    mQueue = std::make_unique<ConcurrentFixedCapacityQueue<MyTestElement>>(
        kQueueCapacity);
  }

  // Tear down (called after each test)
  void TearDown() override {
    mQueue.reset();
  }

  std::unique_ptr<ConcurrentFixedCapacityQueue<MyTestElement>> mQueue;
};

TEST_F(ConcurrentFixedCapacityQueueTest, PushAndPop) {
  mQueue->push({1});
  mQueue->push({2});
  ASSERT_EQ(mQueue->size(), 2);

  mQueue->pop();
  ASSERT_EQ(mQueue->size(), 1);

  mQueue->pop();
  ASSERT_EQ(mQueue->size(), 0);

  mQueue->pop();
  ASSERT_EQ(mQueue->size(), 0);
}

TEST_F(ConcurrentFixedCapacityQueueTest, OverPushed) {
  for (int i = 1; i <= kQueueCapacity + 2; i++) {
    mQueue->push({i});
  }

  ASSERT_EQ(mQueue->size(), kQueueCapacity);

  std::string queue_content = mQueue->toString();
  std::string elementMustHave = MyTestElement(kQueueCapacity + 2).toString();
  // Elements purged out:
  ASSERT_EQ(queue_content.find("[1]"), std::string::npos);
  ASSERT_EQ(queue_content.find("[2]"), std::string::npos);
  // Element must have:
  ASSERT_NE(queue_content.find(elementMustHave), std::string::npos);
}

TEST_F(ConcurrentFixedCapacityQueueTest, EmptyQueue) {
  ASSERT_EQ(mQueue->size(), 0);
  ASSERT_EQ(mQueue->toString(), "[EMPTY]\n");
}

TEST_F(ConcurrentFixedCapacityQueueTest, MultipleThreadsPush) {
  constexpr int kNumThreads = 10;
  constexpr int kElementsPerThread = 100;

  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < kElementsPerThread; ++j) {
        mQueue->push({i * kElementsPerThread + j});
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  // The queue should have at most the capacity size elements
  ASSERT_LE(mQueue->size(), kQueueCapacity);
}

TEST_F(ConcurrentFixedCapacityQueueTest, MultipleThreadsPushAndPop) {
  constexpr int kNumThreads = 10;
  constexpr int kElementsPerThread = 100;

  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < kElementsPerThread; ++j) {
        mQueue->push({i * kElementsPerThread + j});
        mQueue->pop();
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  // Check that the queue is not holding more than the capacity
  ASSERT_LE(mQueue->size(), kQueueCapacity);
}
}  // namespace android::chre