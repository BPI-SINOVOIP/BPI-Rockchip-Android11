/******************************************************************************
 *
 *  Copyright 2020 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include <base/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <limits>

#include "common/lru.h"

namespace testing {

using bluetooth::common::LruCache;

TEST(BluetoothLruCacheTest, LruCacheMainTest1) {
  int* value = new int(0);
  LruCache<int, int> cache(3, "testing");  // capacity = 3;
  cache.Put(1, 10);
  EXPECT_EQ(cache.Size(), 1);
  EXPECT_FALSE(cache.Put(2, 20));
  EXPECT_FALSE(cache.Put(3, 30));
  EXPECT_EQ(cache.Size(), 3);

  // 1, 2, 3 should be in cache
  EXPECT_TRUE(cache.Get(1, value));
  EXPECT_EQ(*value, 10);
  EXPECT_TRUE(cache.Get(2, value));
  EXPECT_EQ(*value, 20);
  EXPECT_TRUE(cache.Get(3, value));
  EXPECT_EQ(*value, 30);
  EXPECT_EQ(cache.Size(), 3);

  EXPECT_THAT(cache.Put(4, 40), Optional(Pair(1, 10)));
  // 2, 3, 4 should be in cache, 1 is evicted
  EXPECT_FALSE(cache.Get(1, value));
  EXPECT_TRUE(cache.Get(4, value));
  EXPECT_EQ(*value, 40);
  EXPECT_TRUE(cache.Get(2, value));
  EXPECT_EQ(*value, 20);
  EXPECT_TRUE(cache.Get(3, value));
  EXPECT_EQ(*value, 30);

  EXPECT_THAT(cache.Put(5, 50), Optional(Pair(4, 40)));
  EXPECT_EQ(cache.Size(), 3);
  // 2, 3, 5 should be in cache, 4 is evicted

  EXPECT_TRUE(cache.Remove(3));
  EXPECT_FALSE(cache.Put(6, 60));
  // 2, 5, 6 should be in cache

  EXPECT_FALSE(cache.Get(3, value));
  EXPECT_FALSE(cache.Get(4, value));
  EXPECT_TRUE(cache.Get(2, value));
  EXPECT_EQ(*value, 20);
  EXPECT_TRUE(cache.Get(5, value));
  EXPECT_EQ(*value, 50);
  EXPECT_TRUE(cache.Get(6, value));
  EXPECT_EQ(*value, 60);
}

TEST(BluetoothLruCacheTest, LruCacheMainTest2) {
  int* value = new int(0);
  LruCache<int, int> cache(2, "testing");  // size = 2;
  EXPECT_FALSE(cache.Put(1, 10));
  EXPECT_FALSE(cache.Put(2, 20));
  EXPECT_THAT(cache.Put(3, 30), Optional(Pair(1, 10)));
  EXPECT_FALSE(cache.Put(2, 200));
  EXPECT_EQ(cache.Size(), 2);
  // 3, 2 should be in cache

  EXPECT_FALSE(cache.HasKey(1));
  EXPECT_TRUE(cache.Get(2, value));
  EXPECT_EQ(*value, 200);
  EXPECT_TRUE(cache.Get(3, value));
  EXPECT_EQ(*value, 30);

  EXPECT_THAT(cache.Put(4, 40), Optional(Pair(2, 200)));
  // 3, 4 should be in cache

  EXPECT_FALSE(cache.HasKey(2));
  EXPECT_TRUE(cache.Get(3, value));
  EXPECT_EQ(*value, 30);
  EXPECT_TRUE(cache.Get(4, value));
  EXPECT_EQ(*value, 40);

  EXPECT_TRUE(cache.Remove(4));
  EXPECT_EQ(cache.Size(), 1);
  cache.Put(2, 2000);
  // 3, 2 should be in cache

  EXPECT_FALSE(cache.HasKey(4));
  EXPECT_TRUE(cache.Get(3, value));
  EXPECT_EQ(*value, 30);
  EXPECT_TRUE(cache.Get(2, value));
  EXPECT_EQ(*value, 2000);

  EXPECT_TRUE(cache.Remove(2));
  EXPECT_TRUE(cache.Remove(3));
  cache.Put(5, 50);
  cache.Put(1, 100);
  cache.Put(1, 1000);
  EXPECT_EQ(cache.Size(), 2);
  // 1, 5 should be in cache

  EXPECT_FALSE(cache.HasKey(2));
  EXPECT_FALSE(cache.HasKey(3));
  EXPECT_TRUE(cache.Get(1, value));
  EXPECT_EQ(*value, 1000);
  EXPECT_TRUE(cache.Get(5, value));
  EXPECT_EQ(*value, 50);
}

TEST(BluetoothLruCacheTest, LruCacheFindTest) {
  LruCache<int, int> cache(10, "testing");
  cache.Put(1, 10);
  cache.Put(2, 20);
  int value = 0;
  EXPECT_TRUE(cache.Get(1, &value));
  EXPECT_EQ(value, 10);
  auto value_ptr = cache.Find(1);
  EXPECT_NE(value_ptr, nullptr);
  *value_ptr = 20;
  EXPECT_TRUE(cache.Get(1, &value));
  EXPECT_EQ(value, 20);
  cache.Put(1, 40);
  EXPECT_EQ(*value_ptr, 40);
  EXPECT_EQ(cache.Find(10), nullptr);
}

TEST(BluetoothLruCacheTest, LruCacheGetTest) {
  LruCache<int, int> cache(10, "testing");
  cache.Put(1, 10);
  cache.Put(2, 20);
  int value = 0;
  EXPECT_TRUE(cache.Get(1, &value));
  EXPECT_EQ(value, 10);
  EXPECT_TRUE(cache.HasKey(1));
  EXPECT_TRUE(cache.HasKey(2));
  EXPECT_FALSE(cache.HasKey(3));
  EXPECT_FALSE(cache.Get(3, &value));
  EXPECT_EQ(value, 10);
}

TEST(BluetoothLruCacheTest, LruCacheRemoveTest) {
  LruCache<int, int> cache(10, "testing");
  for (int key = 0; key <= 30; key++) {
    cache.Put(key, key * 100);
  }
  for (int key = 0; key <= 20; key++) {
    EXPECT_FALSE(cache.HasKey(key));
  }
  for (int key = 21; key <= 30; key++) {
    EXPECT_TRUE(cache.HasKey(key));
  }
  for (int key = 21; key <= 30; key++) {
    EXPECT_TRUE(cache.Remove(key));
  }
  for (int key = 21; key <= 30; key++) {
    EXPECT_FALSE(cache.HasKey(key));
  }
}

TEST(BluetoothLruCacheTest, LruCacheClearTest) {
  LruCache<int, int> cache(10, "testing");
  for (int key = 0; key < 10; key++) {
    cache.Put(key, key * 100);
  }
  for (int key = 0; key < 10; key++) {
    EXPECT_TRUE(cache.HasKey(key));
  }
  cache.Clear();
  for (int key = 0; key < 10; key++) {
    EXPECT_FALSE(cache.HasKey(key));
  }

  for (int key = 0; key < 10; key++) {
    cache.Put(key, key * 1000);
  }
  for (int key = 0; key < 10; key++) {
    EXPECT_TRUE(cache.HasKey(key));
  }
}

TEST(BluetoothLruCacheTest, LruCachePressureTest) {
  auto started = std::chrono::high_resolution_clock::now();
  int max_size = 0xFFFFF;  // 2^20 = 1M
  LruCache<int, int> cache(static_cast<size_t>(max_size), "testing");

  // fill the cache
  for (int key = 0; key < max_size; key++) {
    cache.Put(key, key);
  }

  // make sure the cache is full
  for (int key = 0; key < max_size; key++) {
    EXPECT_TRUE(cache.HasKey(key));
  }

  // refresh the entire cache
  for (int key = 0; key < max_size; key++) {
    int new_key = key + max_size;
    cache.Put(new_key, new_key);
    EXPECT_FALSE(cache.HasKey(key));
    EXPECT_TRUE(cache.HasKey(new_key));
  }

  // clear the entire cache
  int* value = new int(0);
  for (int key = max_size; key < 2 * max_size; key++) {
    EXPECT_TRUE(cache.Get(key, value));
    EXPECT_EQ(*value, key);
    EXPECT_TRUE(cache.Remove(key));
  }
  EXPECT_EQ(cache.Size(), 0);

  // test execution time
  auto done = std::chrono::high_resolution_clock::now();
  int execution_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(done - started)
          .count();
  // always around 750ms on flame. 1400 ms on crosshatch, 6800 ms on presubmit
  // Shouldn't be more than 10000ms
  EXPECT_LT(execution_time, 10000);
}

TEST(BluetoothLruCacheTest, BluetoothLruMultiThreadPressureTest) {
  LruCache<int, int> cache(100, "testing");
  auto pointer = &cache;
  // make sure no deadlock
  std::vector<std::thread> workers;
  for (int key = 0; key < 100; key++) {
    workers.push_back(std::thread([key, pointer]() {
      pointer->Put(key, key);
      EXPECT_TRUE(pointer->HasKey(key));
      EXPECT_TRUE(pointer->Remove(key));
    }));
  }
  for (auto& worker : workers) {
    worker.join();
  }
  EXPECT_EQ(cache.Size(), 0);
}

}  // namespace testing
