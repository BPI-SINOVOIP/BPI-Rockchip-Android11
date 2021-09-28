/*
 * Copyright (C) 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vector>

#include <gtest/gtest.h>

#include "wificond/client/native_wifi_client.h"

using ::android::net::wifi::nl80211::NativeWifiClient;
using std::vector;

namespace android {
namespace wificond {

namespace {

const vector<uint8_t> kMacAddress = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
const vector<uint8_t> kMacAddressOther = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x0F };

}  // namespace

class NativeWifiClientTest : public ::testing::Test {
};

TEST_F(NativeWifiClientTest, NativeWifiClientParcelableTest) {
  NativeWifiClient wifi_client;
  wifi_client.mac_address_ = kMacAddress;

  Parcel parcel;
  EXPECT_EQ(::android::OK, wifi_client.writeToParcel(&parcel));

  NativeWifiClient wifi_client_copy;
  parcel.setDataPosition(0);
  EXPECT_EQ(::android::OK, wifi_client_copy.readFromParcel(&parcel));

  EXPECT_EQ(wifi_client, wifi_client_copy);

  NativeWifiClient wifi_client_other;
  wifi_client_other.mac_address_ = kMacAddressOther;
  EXPECT_FALSE(wifi_client == wifi_client_other);
}

}  // namespace wificond
}  // namespace android
