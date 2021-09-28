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
#include <thread>

#include "common/metric_id_allocator.h"

namespace testing {

using bluetooth::common::MetricIdAllocator;

RawAddress kthAddress(uint32_t k) {
  uint8_t array[6] = {0, 0, 0, 0, 0, 0};
  for (int i = 5; i >= 2; i--) {
    array[i] = k % 256;
    k = k / 256;
  }
  RawAddress addr(array);
  return addr;
}

std::unordered_map<RawAddress, int> generateAddresses(const uint32_t num) {
  // generate first num of mac address -> id pairs
  // input may is always valid 256^6 = 2^48 > 2^32
  std::unordered_map<RawAddress, int> device_map;
  for (size_t key = 0; key < num; key++) {
    device_map[kthAddress(key)] = key + MetricIdAllocator::kMinId;
  }
  return device_map;
}

TEST(BluetoothMetricIdAllocatorTest, MetricIdAllocatorInitCloseTest) {
  auto& allocator = MetricIdAllocator::GetInstance();
  std::unordered_map<RawAddress, int> paired_device_map;
  MetricIdAllocator::Callback callback = [](const RawAddress&, const int) {
    return true;
  };
  EXPECT_TRUE(allocator.Init(paired_device_map, callback, callback));
  EXPECT_FALSE(allocator.Init(paired_device_map, callback, callback));
  EXPECT_TRUE(allocator.Close());
}

TEST(BluetoothMetricIdAllocatorTest, MetricIdAllocatorNotCloseTest) {
  auto& allocator = MetricIdAllocator::GetInstance();
  std::unordered_map<RawAddress, int> paired_device_map;
  MetricIdAllocator::Callback callback = [](const RawAddress&, const int) {
    return true;
  };
  EXPECT_TRUE(allocator.Init(paired_device_map, callback, callback));

  // should fail because it isn't closed
  EXPECT_FALSE(allocator.Init(paired_device_map, callback, callback));
  EXPECT_TRUE(allocator.Close());
}

TEST(BluetoothMetricIdAllocatorTest, MetricIdAllocatorScanDeviceFromEmptyTest) {
  auto& allocator = MetricIdAllocator::GetInstance();
  std::unordered_map<RawAddress, int> paired_device_map;
  MetricIdAllocator::Callback callback = [](const RawAddress&, const int) {
    return true;
  };
  // test empty map, next id should be kMinId
  EXPECT_TRUE(allocator.Init(paired_device_map, callback, callback));
  EXPECT_EQ(allocator.AllocateId(kthAddress(0)), MetricIdAllocator::kMinId);
  EXPECT_EQ(allocator.AllocateId(kthAddress(1)), MetricIdAllocator::kMinId + 1);
  EXPECT_EQ(allocator.AllocateId(kthAddress(0)), MetricIdAllocator::kMinId);
  EXPECT_EQ(allocator.AllocateId(kthAddress(2)), MetricIdAllocator::kMinId + 2);
  EXPECT_TRUE(allocator.Close());
}

TEST(BluetoothMetricIdAllocatorTest,
     MetricIdAllocatorScanDeviceFromFilledTest) {
  auto& allocator = MetricIdAllocator::GetInstance();
  std::unordered_map<RawAddress, int> paired_device_map;
  MetricIdAllocator::Callback callback = [](const RawAddress&, const int) {
    return true;
  };
  int id = static_cast<int>(MetricIdAllocator::kMaxNumPairedDevicesInMemory) +
           MetricIdAllocator::kMinId;
  // next id should be MetricIdAllocator::kMaxNumPairedDevicesInMemory
  paired_device_map =
      generateAddresses(MetricIdAllocator::kMaxNumPairedDevicesInMemory);
  EXPECT_TRUE(allocator.Init(paired_device_map, callback, callback));
  // try new values not in the map, should get new id.
  EXPECT_EQ(allocator.AllocateId(kthAddress(INT_MAX)), id);
  EXPECT_EQ(allocator.AllocateId(kthAddress(INT_MAX - 1)), id + 1);
  EXPECT_EQ(allocator.AllocateId(kthAddress(INT_MAX)), id);
  EXPECT_EQ(allocator.AllocateId(kthAddress(INT_MAX - 2)), id + 2);
  EXPECT_TRUE(allocator.Close());
}

TEST(BluetoothMetricIdAllocatorTest, MetricIdAllocatorAllocateExistingTest) {
  auto& allocator = MetricIdAllocator::GetInstance();
  std::unordered_map<RawAddress, int> paired_device_map =
      generateAddresses(MetricIdAllocator::kMaxNumPairedDevicesInMemory);

  MetricIdAllocator::Callback callback = [](const RawAddress&, const int) {
    return true;
  };
  int id = MetricIdAllocator::kMinId;
  // next id should be MetricIdAllocator::kMaxNumPairedDevicesInMemory
  EXPECT_TRUE(allocator.Init(paired_device_map, callback, callback));

  // try values already in the map, should get new id.
  EXPECT_EQ(allocator.AllocateId(RawAddress({0, 0, 0, 0, 0, 0})), id);
  EXPECT_EQ(allocator.AllocateId(RawAddress({0, 0, 0, 0, 0, 1})), id + 1);
  EXPECT_EQ(allocator.AllocateId(RawAddress({0, 0, 0, 0, 0, 0})), id);
  EXPECT_EQ(allocator.AllocateId(RawAddress({0, 0, 0, 0, 0, 2})), id + 2);
  EXPECT_TRUE(allocator.Close());
}

TEST(BluetoothMetricIdAllocatorTest, MetricIdAllocatorMainTest1) {
  auto& allocator = MetricIdAllocator::GetInstance();
  std::unordered_map<RawAddress, int> paired_device_map;
  int dummy = 22;
  int* pointer = &dummy;
  MetricIdAllocator::Callback save_callback = [pointer](const RawAddress&,
                                                        const int) {
    *pointer = *pointer * 2;
    return true;
  };
  MetricIdAllocator::Callback forget_callback = [pointer](const RawAddress&,
                                                          const int) {
    *pointer = *pointer / 2;
    return true;
  };

  EXPECT_TRUE(
      allocator.Init(paired_device_map, save_callback, forget_callback));
  EXPECT_EQ(allocator.AllocateId(RawAddress({0, 0, 0, 0, 0, 0})),
            MetricIdAllocator::kMinId);
  // save it and make sure the callback is called
  EXPECT_TRUE(allocator.SaveDevice(RawAddress({0, 0, 0, 0, 0, 0})));
  EXPECT_EQ(dummy, 44);

  // should fail, since id of device is not allocated
  EXPECT_FALSE(allocator.SaveDevice(RawAddress({0, 0, 0, 0, 0, 1})));
  EXPECT_EQ(dummy, 44);

  // save it and make sure the callback is called
  EXPECT_EQ(allocator.AllocateId(RawAddress({0, 0, 0, 0, 0, 2})),
            MetricIdAllocator::kMinId + 1);
  EXPECT_EQ(allocator.AllocateId(RawAddress({0, 0, 0, 0, 0, 3})),
            MetricIdAllocator::kMinId + 2);
  EXPECT_TRUE(allocator.SaveDevice(RawAddress({0, 0, 0, 0, 0, 2})));
  EXPECT_EQ(dummy, 88);
  EXPECT_TRUE(allocator.SaveDevice(RawAddress({0, 0, 0, 0, 0, 3})));
  EXPECT_EQ(dummy, 176);

  // should be true but callback won't be called, since id had been saved
  EXPECT_TRUE(allocator.SaveDevice(RawAddress({0, 0, 0, 0, 0, 0})));
  EXPECT_EQ(dummy, 176);

  // forget
  allocator.ForgetDevice(RawAddress({0, 0, 0, 0, 0, 1}));
  EXPECT_EQ(dummy, 176);
  allocator.ForgetDevice(RawAddress({0, 0, 0, 0, 0, 2}));
  EXPECT_EQ(dummy, 88);

  EXPECT_TRUE(allocator.Close());
}

TEST(BluetoothMetricIdAllocatorTest, MetricIdAllocatorFullPairedMap) {
  auto& allocator = MetricIdAllocator::GetInstance();
  // preset a full map
  std::unordered_map<RawAddress, int> paired_device_map =
      generateAddresses(MetricIdAllocator::kMaxNumPairedDevicesInMemory);
  int dummy = 243;
  int* pointer = &dummy;
  MetricIdAllocator::Callback save_callback = [pointer](const RawAddress&,
                                                        const int) {
    *pointer = *pointer * 2;
    return true;
  };
  MetricIdAllocator::Callback forget_callback = [pointer](const RawAddress&,
                                                          const int) {
    *pointer = *pointer / 3;
    return true;
  };

  EXPECT_TRUE(
      allocator.Init(paired_device_map, save_callback, forget_callback));

  // check if all preset ids are there.
  // comments based on kMaxNumPairedDevicesInMemory = 200. It can change.
  int key = 0;
  for (key = 0;
       key < static_cast<int>(MetricIdAllocator::kMaxNumPairedDevicesInMemory);
       key++) {
    EXPECT_EQ(allocator.AllocateId(kthAddress(key)),
              key + MetricIdAllocator::kMinId);
  }
  // paired: 0, 1, 2 ... 199,
  // scanned:

  int id = static_cast<int>(MetricIdAllocator::kMaxNumPairedDevicesInMemory +
                            MetricIdAllocator::kMinId);
  // next id should be MetricIdAllocator::kMaxNumPairedDevicesInMemory +
  // MetricIdAllocator::kMinId

  EXPECT_EQ(allocator.AllocateId(kthAddress(key)), id++);
  // paired: 0, 1, 2 ... 199,
  // scanned: 200

  // save it and make sure the callback is called
  EXPECT_TRUE(allocator.SaveDevice(kthAddress(key)));
  EXPECT_EQ(dummy, 162);  // one key is evicted, another key is saved so *2/3

  // paired: 1, 2 ... 199, 200,
  // scanned:

  EXPECT_EQ(allocator.AllocateId(kthAddress(0)), id++);
  // paired: 1, 2 ... 199, 200
  // scanned: 0

  // key == 200
  // should fail, since id of device is not allocated
  EXPECT_FALSE(allocator.SaveDevice(kthAddress(key + 1)));
  EXPECT_EQ(dummy, 162);
  // paired: 1, 2 ... 199, 200,
  // scanned: 0

  EXPECT_EQ(allocator.AllocateId(kthAddress(key + 1)), id++);
  EXPECT_TRUE(allocator.SaveDevice(kthAddress(key + 1)));
  EXPECT_EQ(dummy, 108);  // one key is evicted, another key is saved so *2/3,
  // paired: 2 ... 199, 200, 201
  // scanned: 0

  EXPECT_EQ(allocator.AllocateId(kthAddress(1)), id++);
  // paired: 2 ... 199, 200, 201,
  // scanned: 0, 1

  // save it and make sure the callback is called
  EXPECT_EQ(allocator.AllocateId(kthAddress(key + 2)), id++);
  EXPECT_EQ(allocator.AllocateId(kthAddress(key + 3)), id++);
  // paired: 2 ... 199, 200, 201,
  // scanned: 0, 1, 202, 203

  dummy = 9;
  EXPECT_TRUE(allocator.SaveDevice(kthAddress(key + 2)));
  EXPECT_EQ(dummy, 6);  // one key is evicted, another key is saved so *2/3,
  EXPECT_TRUE(allocator.SaveDevice(kthAddress(key + 3)));
  EXPECT_EQ(dummy, 4);  // one key is evicted, another key is saved so *2/3,
  // paired: 4 ... 199, 200, 201, 202, 203
  // scanned: 0, 1

  // should be true but callback won't be called, since id had been saved
  EXPECT_TRUE(allocator.SaveDevice(kthAddress(key + 2)));
  EXPECT_EQ(dummy, 4);

  dummy = 27;
  // forget
  allocator.ForgetDevice(kthAddress(key + 200));
  EXPECT_EQ(dummy, 27);  // should fail, no such a key
  allocator.ForgetDevice(kthAddress(key + 2));
  EXPECT_EQ(dummy, 9);
  // paired: 4 ... 199, 200, 201, 203
  // scanned: 0, 1

  // save it and make sure the callback is called
  EXPECT_EQ(allocator.AllocateId(kthAddress(key + 2)), id++);
  EXPECT_EQ(allocator.AllocateId(kthAddress(key + 4)), id++);
  EXPECT_EQ(allocator.AllocateId(kthAddress(key + 5)), id++);
  // paired: 4 ... 199, 200, 201, 203
  // scanned: 0, 1, 202, 204, 205

  EXPECT_TRUE(allocator.SaveDevice(kthAddress(key + 2)));
  EXPECT_EQ(dummy, 18);  // no key is evicted, a key is saved so *2,

  // should be true but callback won't be called, since id had been saved
  EXPECT_TRUE(allocator.SaveDevice(kthAddress(key + 3)));
  EXPECT_EQ(dummy, 18);  // no such a key in scanned
  EXPECT_TRUE(allocator.SaveDevice(kthAddress(key + 4)));
  EXPECT_EQ(dummy, 12);  // one key is evicted, another key is saved so *2/3,
  // paired: 5 6 ... 199, 200, 201, 203, 202, 204
  // scanned: 0, 1, 205

  // verify paired:
  for (key = 5; key <= 199; key++) {
    dummy = 3;
    allocator.ForgetDevice(kthAddress(key));
    EXPECT_EQ(dummy, 1);
  }
  for (size_t k = MetricIdAllocator::kMaxNumPairedDevicesInMemory;
       k <= MetricIdAllocator::kMaxNumPairedDevicesInMemory + 4; k++) {
    dummy = 3;
    allocator.ForgetDevice(kthAddress(k));
    EXPECT_EQ(dummy, 1);
  }

  // verify scanned
  dummy = 4;
  EXPECT_TRUE(allocator.SaveDevice(kthAddress(0)));
  EXPECT_TRUE(allocator.SaveDevice(kthAddress(1)));
  EXPECT_TRUE(allocator.SaveDevice(
      kthAddress(MetricIdAllocator::kMaxNumPairedDevicesInMemory + 5)));
  EXPECT_EQ(dummy, 32);

  EXPECT_TRUE(allocator.Close());
}

TEST(BluetoothMetricIdAllocatorTest, MetricIdAllocatorFullScannedMap) {
  auto& allocator = MetricIdAllocator::GetInstance();
  std::unordered_map<RawAddress, int> paired_device_map;
  int dummy = 22;
  int* pointer = &dummy;
  MetricIdAllocator::Callback save_callback = [pointer](const RawAddress&,
                                                        const int) {
    *pointer = *pointer * 2;
    return true;
  };
  MetricIdAllocator::Callback forget_callback = [pointer](const RawAddress&,
                                                          const int) {
    *pointer = *pointer / 2;
    return true;
  };

  EXPECT_TRUE(
      allocator.Init(paired_device_map, save_callback, forget_callback));

  // allocate kMaxNumUnpairedDevicesInMemory ids
  // comments based on kMaxNumUnpairedDevicesInMemory = 200
  for (int key = 0;
       key <
       static_cast<int>(MetricIdAllocator::kMaxNumUnpairedDevicesInMemory);
       key++) {
    EXPECT_EQ(allocator.AllocateId(kthAddress(key)),
              key + MetricIdAllocator::kMinId);
  }
  // scanned: 0, 1, 2 ... 199,
  // paired:

  int id = MetricIdAllocator::kMaxNumUnpairedDevicesInMemory +
           MetricIdAllocator::kMinId;
  RawAddress addr =
      kthAddress(MetricIdAllocator::kMaxNumUnpairedDevicesInMemory);
  EXPECT_EQ(allocator.AllocateId(addr), id);
  // scanned: 1, 2 ... 199, 200

  // save it and make sure the callback is called
  EXPECT_TRUE(allocator.SaveDevice(addr));
  EXPECT_EQ(allocator.AllocateId(addr), id);
  EXPECT_EQ(dummy, 44);
  // paired: 200,
  // scanned: 1, 2 ... 199,
  id++;

  addr = kthAddress(MetricIdAllocator::kMaxNumUnpairedDevicesInMemory + 1);
  EXPECT_EQ(allocator.AllocateId(addr), id++);
  // paired: 200,
  // scanned: 1, 2 ... 199, 201

  // try to allocate for device 0, 1, 2, 3, 4....199
  // we should have a new id every time,
  // since the scanned map is full at this point
  for (int key = 0;
       key <
       static_cast<int>(MetricIdAllocator::kMaxNumUnpairedDevicesInMemory);
       key++) {
    EXPECT_EQ(allocator.AllocateId(kthAddress(key)), id++);
  }
  EXPECT_TRUE(allocator.Close());
}

TEST(BluetoothMetricIdAllocatorTest, MetricIdAllocatorMultiThreadPressureTest) {
  std::unordered_map<RawAddress, int> paired_device_map;
  auto& allocator = MetricIdAllocator::GetInstance();
  int dummy = 22;
  int* pointer = &dummy;
  MetricIdAllocator::Callback save_callback = [pointer](const RawAddress&,
                                                        const int) {
    *pointer = *pointer + 1;
    return true;
  };
  MetricIdAllocator::Callback forget_callback = [pointer](const RawAddress&,
                                                          const int) {
    *pointer = *pointer - 1;
    return true;
  };
  EXPECT_TRUE(
      allocator.Init(paired_device_map, save_callback, forget_callback));

  // make sure no deadlock
  std::vector<std::thread> workers;
  for (int key = 0;
       key <
       static_cast<int>(MetricIdAllocator::kMaxNumUnpairedDevicesInMemory);
       key++) {
    workers.push_back(std::thread([key]() {
      auto& allocator = MetricIdAllocator::GetInstance();
      RawAddress fake_mac_address = kthAddress(key);
      allocator.AllocateId(fake_mac_address);
      EXPECT_TRUE(allocator.SaveDevice(fake_mac_address));
      allocator.ForgetDevice(fake_mac_address);
    }));
  }
  for (auto& worker : workers) {
    worker.join();
  }
  EXPECT_TRUE(allocator.IsEmpty());
  EXPECT_TRUE(allocator.Close());
}

TEST(BluetoothMetricIdAllocatorTest, MetricIdAllocatorWrapAroundTest1) {
  std::unordered_map<RawAddress, int> paired_device_map;
  auto& allocator = MetricIdAllocator::GetInstance();
  MetricIdAllocator::Callback callback = [](const RawAddress&, const int) {
    return true;
  };

  // make a sparse paired_device_map
  int min_id = MetricIdAllocator::kMinId;
  paired_device_map[kthAddress(min_id)] = min_id;
  paired_device_map[kthAddress(min_id + 1)] = min_id + 1;
  paired_device_map[kthAddress(min_id + 3)] = min_id + 3;
  paired_device_map[kthAddress(min_id + 4)] = min_id + 4;

  int max_id = MetricIdAllocator::kMaxId;
  paired_device_map[kthAddress(max_id - 3)] = max_id - 3;
  paired_device_map[kthAddress(max_id - 4)] = max_id - 4;

  EXPECT_TRUE(allocator.Init(paired_device_map, callback, callback));

  // next id should be max_id - 2, max_id - 1, max_id, min_id + 2, min_id + 5
  EXPECT_EQ(allocator.AllocateId(kthAddress(max_id - 2)), max_id - 2);
  EXPECT_EQ(allocator.AllocateId(kthAddress(max_id - 1)), max_id - 1);
  EXPECT_EQ(allocator.AllocateId(kthAddress(max_id)), max_id);
  EXPECT_EQ(allocator.AllocateId(kthAddress(min_id + 2)), min_id + 2);
  EXPECT_EQ(allocator.AllocateId(kthAddress(min_id + 5)), min_id + 5);

  EXPECT_TRUE(allocator.Close());
}

TEST(BluetoothMetricIdAllocatorTest, MetricIdAllocatorWrapAroundTest2) {
  std::unordered_map<RawAddress, int> paired_device_map;
  auto& allocator = MetricIdAllocator::GetInstance();
  MetricIdAllocator::Callback callback = [](const RawAddress&, const int) {
    return true;
  };

  // make a sparse paired_device_map
  int min_id = MetricIdAllocator::kMinId;
  int max_id = MetricIdAllocator::kMaxId;
  paired_device_map[kthAddress(max_id)] = max_id;

  EXPECT_TRUE(allocator.Init(paired_device_map, callback, callback));

  // next id should be min_id, min_id + 1
  EXPECT_EQ(allocator.AllocateId(kthAddress(min_id)), min_id);
  EXPECT_EQ(allocator.AllocateId(kthAddress(min_id + 1)), min_id + 1);

  EXPECT_TRUE(allocator.Close());
}

}  // namespace testing
