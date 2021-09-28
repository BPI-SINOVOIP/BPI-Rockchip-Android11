/*
 *  Copyright 2020 The Android Open Source Project
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
 */

#include "btif/include/btif_config_cache.h"

#include <filesystem>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

const int kCapacity = 3;
const int kTestRepeatCount = 30;
const std::string kBtAddr1 = "11:22:33:44:55:66";
const std::string kBtAddr2 = "AA:BB:CC:DD:EE:FF";
const std::string kBtAddr3 = "AB:CD:EF:12:34:56";
const std::string kBtAddr4 = "11:AA:22:BB:33:CC";
const std::string kBtAddr5 = "11:AA:22:BB:33:CD";
const std::string kBtLocalAddr = "12:34:56:78:90:AB";
const std::string kBtInfo = "Info";
const std::string kBtMetrics = "Metrics";
const std::string kBtAdapter = "Adapter";
const std::string kBtAddrInvalid1 = "AB:CD:EF:12:34";
const std::string kBtAddrInvalid2 = "AB:CD:EF:12:34:56:78";
const std::string kBtAddrInvalid3 = "ABCDEF123456";
const std::string kBtAddrInvalid4 = "AB-CD-EF-12-34-56";
const std::string kBtSectionInvalid1 = "Invalid Section";
const std::filesystem::path kTestConfigFile =
    std::filesystem::temp_directory_path() / "config_cache_test.conf";
const char* TEST_CONFIG_FILE = kTestConfigFile.c_str();

}  // namespace

namespace testing {

/* Test to basic btif_config_cache set up
 * 1. when received Local device sections information, the sections can be put
 * into btif config cache
 * 2. the device sections and key-value will be set to Btif config cache when
 * receiving different device sections
 * 3. limit the capacity of unpacire devices cache to 3, test the oldest device
 * section will be ruled out when receiveing 4 different device sections.
 */
TEST(BtifConfigCacheTest, test_setup_btif_config_cache) {
  BtifConfigCache test_btif_config_cache(kCapacity);
  // Info section
  test_btif_config_cache.SetString(kBtInfo, "FileSource", "");
  test_btif_config_cache.SetString(kBtInfo, "TimeCreated",
                                   "2020-06-05 12:12:12");
  // Metrics section
  test_btif_config_cache.SetString(kBtMetrics, "Salt256Bit",
                                   "92a331174d20f2bb");
  // Adapter Section
  test_btif_config_cache.SetString(kBtAdapter, "Address", kBtLocalAddr);
  EXPECT_TRUE(test_btif_config_cache.HasSection(kBtAdapter));

  // bt_device_1
  test_btif_config_cache.SetString(kBtAddr1, "Name", "Headset_1");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Name"));

  test_btif_config_cache.SetInt(kBtAddr1, "Property_Int", 1);
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Property_Int"));

  // bt_device_2
  test_btif_config_cache.SetString(kBtAddr2, "Name", "Headset_2");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr2, "Name"));

  // bt_device_3
  test_btif_config_cache.SetString(kBtAddr3, "Name", "Headset_3");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr3, "Name"));

  // bt_device_4
  test_btif_config_cache.SetString(kBtAddr4, "Name", "Headset_4");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr4, "Name"));

  // out out the capacty of unpair devices cache, the bt_device_1 be ruled out
  EXPECT_FALSE(test_btif_config_cache.HasSection(kBtAddr1));
  EXPECT_TRUE(test_btif_config_cache.HasSection(kBtAddr2));
  EXPECT_TRUE(test_btif_config_cache.HasSection(kBtAddr3));
  EXPECT_TRUE(test_btif_config_cache.HasSection(kBtAddr4));
}

/* Test to set up btif_config_cache with invalid bt address or section name
 * when received Invalid bt address or section, it's not allowed to put invalid
 * section to paired devices list section
 */
TEST(BtifConfigCacheTest, test_set_up_config_cache_with_invalid_section) {
  BtifConfigCache test_btif_config_cache(kCapacity);

  // kBtAddrInvalid1
  test_btif_config_cache.SetString(kBtAddrInvalid1, "Name", "Headset_1");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddrInvalid1, "Name"));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddrInvalid1));
  // get the LinkKey
  test_btif_config_cache.SetString(kBtAddrInvalid1, "LinkKey",
                                   "1122334455667788");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddrInvalid1, "LinkKey"));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddrInvalid1));
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddrInvalid1));

  // kBtAddrInvalid2
  test_btif_config_cache.SetString(kBtAddrInvalid2, "Name", "Headset_1");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddrInvalid2, "Name"));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddrInvalid2));
  // get the LinkKey
  test_btif_config_cache.SetString(kBtAddrInvalid2, "LinkKey",
                                   "1122334455667788");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddrInvalid2, "LinkKey"));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddrInvalid2));
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddrInvalid2));

  // kBtAddrInvalid3
  test_btif_config_cache.SetString(kBtAddrInvalid3, "Name", "Headset_1");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddrInvalid3, "Name"));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddrInvalid3));
  // get the LinkKey
  test_btif_config_cache.SetString(kBtAddrInvalid3, "LinkKey",
                                   "1122334455667788");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddrInvalid3, "LinkKey"));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddrInvalid3));
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddrInvalid3));

  // kBtAddrInvalid4
  test_btif_config_cache.SetString(kBtAddrInvalid4, "Name", "Headset_1");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddrInvalid4, "Name"));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddrInvalid4));
  // get the LinkKey
  test_btif_config_cache.SetString(kBtAddrInvalid4, "LinkKey",
                                   "1122334455667788");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddrInvalid4, "LinkKey"));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddrInvalid4));
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddrInvalid4));

  // kBtSectionInvalid1
  test_btif_config_cache.SetString(kBtSectionInvalid1, "Name", "Headset_1");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtSectionInvalid1, "Name"));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtSectionInvalid1));
  // get the LinkKey
  test_btif_config_cache.SetString(kBtSectionInvalid1, "LinkKey",
                                   "1122334455667788");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtSectionInvalid1, "LinkKey"));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtSectionInvalid1));
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtSectionInvalid1));
}

/* Stress test to set and get key values
 * 1. stress test to set different type key value to unpaired device cache and
 * get the different type key values in the unpaired cache section to check if
 * we get the key-values the same with we set in unpaired device cache.
 * 2. stress test to set different type key value to paired device section and
 * get the different type key values in the paired cache section to check if we
 * get the key-values the same with we set in paired device cache.
 */
TEST(BtifConfigCacheTest, test_get_set_key_value_test) {
  BtifConfigCache test_btif_config_cache(kCapacity);
  // test in unpaired cache
  test_btif_config_cache.SetString(kBtAddr1, "Name", "Headset_1");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Name"));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "Name"),
              Optional(StrEq("Headset_1")));

  test_btif_config_cache.SetInt(kBtAddr1, "Property_Int", 65536);
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Property_Int"));
  EXPECT_THAT(test_btif_config_cache.GetInt(kBtAddr1, "Property_Int"),
              Optional(Eq(65536)));

  test_btif_config_cache.SetUint64(kBtAddr1, "Property_64", 4294967296);
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Property_64"));
  EXPECT_THAT(test_btif_config_cache.GetUint64(kBtAddr1, "Property_64"),
              Optional(Eq(uint64_t(4294967296))));

  test_btif_config_cache.SetBool(kBtAddr1, "Property_Bool", true);
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Property_Bool"));
  EXPECT_THAT(test_btif_config_cache.GetBool(kBtAddr1, "Property_Bool"),
              Optional(IsTrue()));

  // empty value
  test_btif_config_cache.SetString(kBtAddr1, "Name", "");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Name"));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "Name"),
              Optional(StrEq("")));

  // get the LinkKey
  test_btif_config_cache.SetString(kBtAddr1, "LinkKey", "1122334455667788");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "LinkKey"));
  EXPECT_FALSE(test_btif_config_cache.HasUnpairedSection(kBtAddr1));
  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr1));

  // test in unpaired cache
  test_btif_config_cache.SetString(kBtAddr1, "Name", "Headset_1");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Name"));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "Name"),
              Optional(StrEq("Headset_1")));

  test_btif_config_cache.SetInt(kBtAddr1, "Property_Int", 65536);
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Property_Int"));
  EXPECT_THAT(test_btif_config_cache.GetInt(kBtAddr1, "Property_Int"),
              Optional(Eq(65536)));

  test_btif_config_cache.SetUint64(kBtAddr1, "Property_64", 4294967296);
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Property_64"));
  EXPECT_THAT(test_btif_config_cache.GetUint64(kBtAddr1, "Property_64"),
              Optional(Eq(uint64_t(4294967296))));

  test_btif_config_cache.SetBool(kBtAddr1, "Property_Bool", true);
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Property_Bool"));
  EXPECT_THAT(test_btif_config_cache.GetBool(kBtAddr1, "Property_Bool"),
              Optional(IsTrue()));

  // empty section is disallowed
  EXPECT_DEATH({ test_btif_config_cache.SetString("", "name", "Headset_1"); },
               "Empty section not allowed");
  // empty key is disallowed
  EXPECT_DEATH({ test_btif_config_cache.SetString(kBtAddr1, "", "Headset_1"); },
               "Empty key not allowed");
  EXPECT_FALSE(test_btif_config_cache.HasKey(kBtAddr1, ""));
  // empty value is allowed
  test_btif_config_cache.SetString(kBtAddr1, "Name", "");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "Name"));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "Name"),
              Optional(StrEq("")));
}

/* Test to set values in the same key
 * Receiving the same key with different values in a section, the new incoming
 * value will be updated but the key will not be added repeatedly. test this
 * feature in both unpaired devic cache and paired device list cache
 */
TEST(BtifConfigCacheTest, test_set_values_in_the_same_key) {
  BtifConfigCache test_btif_config_cache(kCapacity);
  // add new a key "Name"
  test_btif_config_cache.SetString(kBtAddr1, "Name", "Headset_1");
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "Name"),
              Optional(StrEq("Headset_1")));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddr1));

  // add the same key "Name" with different value
  test_btif_config_cache.SetString(kBtAddr1, "Name", "Headset_1A");
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "Name"),
              Optional(StrEq("Headset_1A")));

  // add the same key "Name" with different value
  test_btif_config_cache.SetString(kBtAddr1, "Name", "Headset_2A");
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "Name"),
              Optional(StrEq("Headset_2A")));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddr1));

  // add new a key "Property_Int"
  test_btif_config_cache.SetInt(kBtAddr1, "Property_Int", 65536);
  EXPECT_THAT(test_btif_config_cache.GetInt(kBtAddr1, "Property_Int"),
              Optional(Eq(65536)));

  // add the same key "Property_Int" with different value
  test_btif_config_cache.SetInt(kBtAddr1, "Property_Int", 256);
  EXPECT_THAT(test_btif_config_cache.GetInt(kBtAddr1, "Property_Int"),
              Optional(Eq(256)));

  test_btif_config_cache.SetUint64(kBtAddr1, "Property_64", 4294967296);
  EXPECT_THAT(test_btif_config_cache.GetUint64(kBtAddr1, "Property_64"),
              Optional(Eq(uint64_t(4294967296))));

  // get the LinkKey and set values in the same key in paired device list
  test_btif_config_cache.SetString(kBtAddr1, "LinkKey", "1122334455667788");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "LinkKey"));
  EXPECT_FALSE(test_btif_config_cache.HasUnpairedSection(kBtAddr1));
  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr1));

  // add the same key "Name" with the different value
  test_btif_config_cache.SetString(kBtAddr1, "Name", "Headset_1A");
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "Name"),
              Optional(StrEq("Headset_1A")));

  // add the same key "Name" with the value different
  test_btif_config_cache.SetString(kBtAddr1, "Name", "Headset_2A");
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "Name"),
              Optional(StrEq("Headset_2A")));

  test_btif_config_cache.SetInt(kBtAddr1, "Property_Int", 64);
  EXPECT_THAT(test_btif_config_cache.GetInt(kBtAddr1, "Property_Int"),
              Optional(Eq(64)));

  test_btif_config_cache.SetUint64(kBtAddr1, "Property_64", 65537);
  EXPECT_THAT(test_btif_config_cache.GetUint64(kBtAddr1, "Property_64"),
              Optional(Eq(uint64_t(65537))));

  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr1));
}

/* Stress test to pair with device then unpair device
 * 1. paired with device by adding a "LinKey" to device and check the device be
 * moved into paired devices list
 * 2. unpaired with the device by removing the "LinkKey" and check the device be
 * moved back to unpaired devices cache
 * 3. loop for 30 times
 */
TEST(BtifConfigCacheTest, test_pair_unpair_device_stress_test) {
  BtifConfigCache test_btif_config_cache(kCapacity);

  // pair with Headset_1 11:22:33:44:55:66
  test_btif_config_cache.SetString(kBtAddr1, "Name", "Headset_1");
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddr1));
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddr1));

  for (int i = 0; i < kTestRepeatCount; ++i) {
    // get the LinkKey, the device will be moved from the unpaired cache to
    // paired cache
    test_btif_config_cache.SetString(kBtAddr1, "LinkKey", "1122334455667788");
    EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "LinkKey"));
    EXPECT_FALSE(test_btif_config_cache.HasUnpairedSection(kBtAddr1));
    EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr1));

    // remove the LinkKey, the device will be moved from the paired cache to
    // unpaired cache
    test_btif_config_cache.RemoveKey(kBtAddr1, "LinkKey");
    EXPECT_FALSE(test_btif_config_cache.HasKey(kBtAddr1, "LinkKey"));
    EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddr1));
    EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddr1));
  }
}

/* Stress test to pair with multi-devices and unpair with multi-devices
 * 1. Pired with 4 devices with Link-Key type key in order, to check these 4
 * devices are in the paired devices list cache
 * 2. unpair with these 4 devices by removed Link-Key type key in order, to
 * check the fisrt device was ruled-out from unpaired devices cache due to
 * capacity limitation, and other 3 devices are be moved to unpaired device
 * cache.
 */
TEST(BtifConfigCacheTest, test_multi_pair_unpair_with_devices) {
  BtifConfigCache test_btif_config_cache(kCapacity);
  // pair with 4 bt address devices by add different type linkkey.
  test_btif_config_cache.SetString(kBtAddr1, "name", "kBtAddr1");
  test_btif_config_cache.SetString(kBtAddr1, "LinkKey", "1122334455667788");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "LinkKey"));

  test_btif_config_cache.SetString(kBtAddr2, "name", "kBtAddr2");
  test_btif_config_cache.SetString(kBtAddr2, "LE_KEY_PENC", "aabbccddeeff9900");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr2, "LE_KEY_PENC"));

  test_btif_config_cache.SetString(kBtAddr3, "name", "kBtAddr3");
  test_btif_config_cache.SetString(kBtAddr3, "LE_KEY_PID", "a1b2c3d4e5feeeee");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr3, "LE_KEY_PID"));

  test_btif_config_cache.SetString(kBtAddr4, "LE_KEY_PCSRK",
                                   "aaaabbbbccccdddd");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr4, "LE_KEY_PCSRK"));

  test_btif_config_cache.SetString(kBtAddr5, "name", "kBtAddr5");
  test_btif_config_cache.SetString(kBtAddr5, "LE_KEY_LENC", "jilkjlkjlkn");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr5, "LE_KEY_LENC"));

  // checking these 4 devices are in paired list cache and the content are
  // correct.
  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr1));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "LinkKey"),
              Optional(StrEq("1122334455667788")));
  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr2));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr2, "LE_KEY_PENC"),
              Optional(StrEq("aabbccddeeff9900")));
  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr3));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr3, "LE_KEY_PID"),
              Optional(StrEq("a1b2c3d4e5feeeee")));
  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr4));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr4, "LE_KEY_PCSRK"),
              Optional(StrEq("aaaabbbbccccdddd")));
  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr5));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr5, "LE_KEY_LENC"),
              Optional(StrEq("jilkjlkjlkn")));

  // unpair with these 4 bt address devices by removed the linkkey.
  // unpair kBtAddr1 11:22:33:44:55:66
  test_btif_config_cache.RemoveKey(kBtAddr1, "LinkKey");
  EXPECT_FALSE(test_btif_config_cache.HasKey(kBtAddr1, "LinkKey"));
  // no empty section is moved to unpaired
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddr1));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddr1));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "name"),
              Optional(StrEq("kBtAddr1")));

  // unpair with kBtAddr2 aa:bb:cc:dd:ee:ff
  test_btif_config_cache.RemoveKey(kBtAddr2, "LE_KEY_PENC");
  EXPECT_FALSE(test_btif_config_cache.HasKey(kBtAddr2, "LE_KEY_PENC"));
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddr2));
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddr2));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr2, "name"),
              Optional(StrEq("kBtAddr2")));

  // unpair with kBtAddr3 AB:CD:EF:12:34:56
  test_btif_config_cache.RemoveKey(kBtAddr3, "LE_KEY_PID");
  EXPECT_FALSE(test_btif_config_cache.HasKey(kBtAddr3, "LE_KEY_PID"));
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddr3));
  // no empty section is moved to unpaired
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddr3));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr3, "name"),
              Optional(StrEq("kBtAddr3")));

  // unpair with kBtAddr4 11:AA:22:BB:33:CC
  test_btif_config_cache.RemoveKey(kBtAddr4, "LE_KEY_PCSRK");
  EXPECT_FALSE(test_btif_config_cache.HasKey(kBtAddr4, "LE_KEY_PCSRK"));
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddr4));
  // empty section is removed
  EXPECT_FALSE(test_btif_config_cache.HasUnpairedSection(kBtAddr4));

  // unpair with kBtAddr5 11:AA:22:BB:33:CD
  test_btif_config_cache.RemoveKey(kBtAddr5, "LE_KEY_LENC");
  EXPECT_FALSE(test_btif_config_cache.HasKey(kBtAddr5, "LE_KEY_LENC"));
  EXPECT_FALSE(test_btif_config_cache.HasPersistentSection(kBtAddr5));
  // no empty section is moved to unpaired
  EXPECT_TRUE(test_btif_config_cache.HasUnpairedSection(kBtAddr5));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr5, "name"),
              Optional(StrEq("kBtAddr5")));

  // checking the oldest unpaired device kBtAddr1 was ruled out from cache due
  // to capacity limitation (3) in unpaired cache.
  EXPECT_FALSE(test_btif_config_cache.HasUnpairedSection(kBtAddr1));
}

/* Test to remove sections with the specific key
 * paired with sections with the specific "Restricted" key and then removed the
 * "Restricted" key, check if the sections with the specific "Restricted" key
 * are removed.
 */
TEST(BtifConfigCacheTest, test_remove_sections_with_key) {
  BtifConfigCache test_btif_config_cache(kCapacity);
  // pair with Headset_1 (kBtAddr1), Headset_2 (kBtAddr1), Heasdet_3 (kBtAddr3)
  // , and Headset_1 (kBtAddr1), Headset_3 (kBtAddr3) have sepcific "Restricted"
  // key
  test_btif_config_cache.SetString(kBtAddr1, "Name", "Headset_1");
  test_btif_config_cache.SetString(kBtAddr1, "Restricted", "1");
  test_btif_config_cache.SetString(kBtAddr1, "LinkKey", "1122334455667788");
  test_btif_config_cache.SetString(kBtAddr2, "Name", "Headset_2");
  test_btif_config_cache.SetString(kBtAddr2, "LinkKey", "aabbccddeeff9900");
  test_btif_config_cache.SetString(kBtAddr3, "Name", "Headset_3");
  test_btif_config_cache.SetString(kBtAddr3, "LinkKey", "a1b2c3d4e5feeeee");
  test_btif_config_cache.SetString(kBtAddr3, "Restricted", "1");

  // remove sections with "Restricted" key
  test_btif_config_cache.RemovePersistentSectionsWithKey("Restricted");

  // checking the kBtAddr1 and kBtAddr3 can not be found in config cache, only
  // keep kBtAddr2 in config cache.
  EXPECT_FALSE(test_btif_config_cache.HasSection(kBtAddr1));
  EXPECT_TRUE(test_btif_config_cache.HasSection(kBtAddr2));
  EXPECT_FALSE(test_btif_config_cache.HasSection(kBtAddr3));
}

/* Test PersistentSectionCopy and Init */
TEST(BtifConfigCacheTest, test_PersistentSectionCopy_Init) {
  BtifConfigCache test_btif_config_cache(kCapacity);
  config_t config_paired = {};
  // pair with 3 bt devices, kBtAddr1, kBtAddr2, kBtAddr3
  test_btif_config_cache.SetString(kBtAddr1, "LinkKey", "1122334455667788");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr1, "LinkKey"));
  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr1));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr1, "LinkKey"),
              Optional(StrEq("1122334455667788")));

  test_btif_config_cache.SetString(kBtAddr2, "LE_KEY_PENC", "aabbccddeeff9900");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr2, "LE_KEY_PENC"));
  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr2));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr2, "LE_KEY_PENC"),
              Optional(StrEq("aabbccddeeff9900")));

  test_btif_config_cache.SetString(kBtAddr3, "LE_KEY_PID", "a1b2c3d4e5feeeee");
  EXPECT_TRUE(test_btif_config_cache.HasKey(kBtAddr3, "LE_KEY_PID"));
  EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(kBtAddr3));
  EXPECT_THAT(test_btif_config_cache.GetString(kBtAddr3, "LE_KEY_PID"),
              Optional(StrEq("a1b2c3d4e5feeeee")));

  // check GetPersistentSections
  int num_of_paired_devices = 0;
  for (const section_t& sec : test_btif_config_cache.GetPersistentSections()) {
    EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(sec.name));
    num_of_paired_devices++;
  }
  EXPECT_EQ(num_of_paired_devices, 3);

  // copy persistent sections
  int num_of_copy_paired_devices = 0;
  config_paired = test_btif_config_cache.PersistentSectionCopy();
  for (const section_t& sec : config_paired.sections) {
    EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(sec.name));
    num_of_copy_paired_devices++;
  }
  EXPECT_EQ(num_of_copy_paired_devices, 3);

  // write persistent sections to temp test config file
  EXPECT_TRUE(config_save(config_paired, TEST_CONFIG_FILE));
  // get persistent sections from temp test config file
  int num_of_save_paired_devices = 0;
  std::unique_ptr<config_t> config_source = config_new(TEST_CONFIG_FILE);
  for (const section_t& sec : config_paired.sections) {
    EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(sec.name));
    num_of_save_paired_devices++;
  }
  EXPECT_EQ(num_of_save_paired_devices, 3);

  // Clear all btif config cache sections
  test_btif_config_cache.Clear();

  // move the persistent sections to btif config paired list
  int num_of_init_paired_devices = 0;
  test_btif_config_cache.Init(std::move(config_source));
  for (const section_t& sec : config_paired.sections) {
    EXPECT_TRUE(test_btif_config_cache.HasPersistentSection(sec.name));
    num_of_init_paired_devices++;
  }
  EXPECT_EQ(num_of_init_paired_devices, 3);

  EXPECT_TRUE(std::filesystem::remove(kTestConfigFile));
}

}  // namespace testing
