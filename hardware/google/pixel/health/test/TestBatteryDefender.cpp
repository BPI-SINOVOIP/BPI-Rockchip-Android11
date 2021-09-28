/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "BatteryDefender.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utils/Timers.h>

#include <android-base/file.h>
#include <android-base/properties.h>

#define MIN_TIME_BETWEEN_FILE_UPDATES (30 + 1)

class HealthInterface {
  public:
    virtual ~HealthInterface() {}
    virtual bool ReadFileToString(const std::string &path, std::string *content,
                                  bool follow_symlinks);
    virtual int GetIntProperty(const std::string &key, int default_value, int min, int max);
    virtual bool GetBoolProperty(const std::string &key, bool default_value);
    virtual bool SetProperty(const std::string &key, const std::string &value);
    virtual bool WriteStringToFile(const std::string &content, const std::string &path,
                                   bool follow_symlinks);
};

class HealthInterfaceMock : public HealthInterface {
  public:
    virtual ~HealthInterfaceMock() {}

    MOCK_METHOD3(ReadFileToString,
                 bool(const std::string &path, std::string *content, bool follow_symlinks));
    MOCK_METHOD4(GetIntProperty, int(const std::string &key, int default_value, int min, int max));
    MOCK_METHOD2(GetBoolProperty, bool(const std::string &key, bool default_value));
    MOCK_METHOD2(SetProperty, bool(const std::string &key, const std::string &value));
    MOCK_METHOD3(WriteStringToFile,
                 bool(const std::string &content, const std::string &path, bool follow_symlinks));
};

HealthInterfaceMock *mock;

namespace android {
namespace base {

bool ReadFileToString(const std::string &path, std::string *content, bool follow_symlinks) {
    return mock->ReadFileToString(path, content, follow_symlinks);
}

bool WriteStringToFile(const std::string &content, const std::string &path, bool follow_symlinks) {
    return mock->WriteStringToFile(content, path, follow_symlinks);
}

template <typename T>
T GetIntProperty(const std::string &key, T default_value, T min, T max) {
    return (T)(mock->GetIntProperty(key, default_value, min, max));
}

bool GetBoolProperty(const std::string &key, bool default_value) {
    return mock->GetBoolProperty(key, default_value);
}

template int8_t GetIntProperty(const std::string &, int8_t, int8_t, int8_t);
template int16_t GetIntProperty(const std::string &, int16_t, int16_t, int16_t);
template int32_t GetIntProperty(const std::string &, int32_t, int32_t, int32_t);
template int64_t GetIntProperty(const std::string &, int64_t, int64_t, int64_t);

bool SetProperty(const std::string &key, const std::string &value) {
    return mock->SetProperty(key, value);
}

}  // namespace base
}  // namespace android

nsecs_t testvar_systemTimeSecs = 0;
nsecs_t systemTime(int clock) {
    UNUSED(clock);
    return seconds_to_nanoseconds(testvar_systemTimeSecs);
}

namespace hardware {
namespace google {
namespace pixel {
namespace health {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;

struct android::BatteryProperties props;

class BatteryDefenderTest : public ::testing::Test {
  public:
    BatteryDefenderTest() {}

    void SetUp() {
        mock = &mockFixture;

        props = {};

        EXPECT_CALL(*mock, SetProperty(_, _)).Times(AnyNumber());
        EXPECT_CALL(*mock, ReadFileToString(_, _, _)).Times(AnyNumber());
        EXPECT_CALL(*mock, GetIntProperty(_, _, _, _)).Times(AnyNumber());
        EXPECT_CALL(*mock, GetBoolProperty(_, _)).Times(AnyNumber());
        EXPECT_CALL(*mock, WriteStringToFile(_, _, _)).Times(AnyNumber());

        ON_CALL(*mock, ReadFileToString(_, _, _))
                .WillByDefault(DoAll(SetArgPointee<1>(std::string("0")), Return(true)));

        ON_CALL(*mock, WriteStringToFile(_, _, _)).WillByDefault(Return(true));
    }

    void TearDown() {}

  private:
    HealthInterfaceMock mockFixture;
};

const char *kPathWiredChargerPresent = "/sys/class/power_supply/usb/present";
const char *kPathWirelessChargerPresent = "/sys/class/power_supply/wireless/present";
const char *kPathPersistChargerPresentTime = "/mnt/vendor/persist/battery/defender_charger_time";
const char *kPathPersistDefenderActiveTime = "/mnt/vendor/persist/battery/defender_active_time";
const char *kPathStartLevel = "/sys/devices/platform/soc/soc:google,charger/charge_start_level";
const char *kPathStopLevel = "/sys/devices/platform/soc/soc:google,charger/charge_stop_level";

const char *kPropChargeLevelVendorStart = "persist.vendor.charge.start.level";
const char *kPropChargeLevelVendorStop = "persist.vendor.charge.stop.level";
const char *kPropBatteryDefenderState = "vendor.battery.defender.state";
const char *kPropBatteryDefenderDisable = "vendor.battery.defender.disable";
const char *kPropBatteryDefenderThreshold = "vendor.battery.defender.threshold";

const char *kPropBatteryDefenderCtrlEnable = "vendor.battery.defender.ctrl.enable";
const char *kPropBatteryDefenderCtrlActivateTime = "vendor.battery.defender.ctrl.trigger_time";
const char *kPropBatteryDefenderCtrlResumeTime = "vendor.battery.defender.ctrl.resume_time";
const char *kPropBatteryDefenderCtrlStartSOC = "vendor.battery.defender.ctrl.recharge_soc_start";
const char *kPropBatteryDefenderCtrlStopSOC = "vendor.battery.defender.ctrl.recharge_soc_stop";
const char *kPropBatteryDefenderCtrlTriggerSOC = "vendor.battery.defender.ctrl.trigger_soc";

static void enableDefender(void) {
    ON_CALL(*mock, GetIntProperty(kPropChargeLevelVendorStart, _, _, _)).WillByDefault(Return(0));
    ON_CALL(*mock, GetIntProperty(kPropChargeLevelVendorStop, _, _, _)).WillByDefault(Return(100));
    ON_CALL(*mock, GetBoolProperty(kPropBatteryDefenderDisable, _)).WillByDefault(Return(false));

    ON_CALL(*mock, GetBoolProperty(kPropBatteryDefenderCtrlEnable, _)).WillByDefault(Return(true));
}

static void usbPresent(void) {
    ON_CALL(*mock, ReadFileToString(kPathWiredChargerPresent, _, _))
            .WillByDefault(DoAll(SetArgPointee<1>(std::string("1")), Return(true)));
}

static void wirelessPresent(void) {
    ON_CALL(*mock, ReadFileToString(kPathWirelessChargerPresent, _, _))
            .WillByDefault(DoAll(SetArgPointee<1>(std::string("1")), Return(true)));
}

static void wirelessNotPresent(void) {
    ON_CALL(*mock, ReadFileToString(kPathWirelessChargerPresent, _, _))
            .WillByDefault(DoAll(SetArgPointee<1>(std::string("0")), Return(true)));
}

static void powerAvailable(void) {
    wirelessPresent();
    usbPresent();
}

static void defaultThresholds(void) {
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderThreshold, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_ACTIVATE_SECONDS));

    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlActivateTime, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_ACTIVATE_SECONDS));
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlResumeTime, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_CLEAR_SECONDS));

    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlStartSOC, _, _, _))
            .WillByDefault(Return(70));
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlStopSOC, _, _, _))
            .WillByDefault(Return(80));

    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlTriggerSOC, _, _, _))
            .WillByDefault(Return(100));
}

static void capacityReached(void) {
    props.batteryLevel = 100;
}

static void initToConnectedCapacityReached(void) {
    ON_CALL(*mock, ReadFileToString(kPathPersistChargerPresentTime, _, _))
            .WillByDefault(DoAll(SetArgPointee<1>(std::to_string(1000)), Return(true)));
}

static void initToActive(void) {
    ON_CALL(*mock, ReadFileToString(kPathPersistChargerPresentTime, _, _))
            .WillByDefault(
                    DoAll(SetArgPointee<1>(std::to_string(DEFAULT_TIME_TO_ACTIVATE_SECONDS + 1)),
                          Return(true)));
}

TEST_F(BatteryDefenderTest, EnableAndDisconnected) {
    BatteryDefender battDefender;

    enableDefender();
    // No power

    // Enable Battery Defender
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "DISCONNECTED"));
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, DisableNonDefaultLevels) {
    BatteryDefender battDefender;

    // Enable Battery Defender
    EXPECT_CALL(*mock, GetIntProperty(kPropChargeLevelVendorStart, _, _, _)).WillOnce(Return(30));
    EXPECT_CALL(*mock, GetIntProperty(kPropChargeLevelVendorStop, _, _, _)).WillOnce(Return(35));

    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "DISABLED"));
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, DisableExplicit) {
    BatteryDefender battDefender;

    // Enable Battery Defender
    EXPECT_CALL(*mock, GetBoolProperty(kPropBatteryDefenderDisable, _)).WillOnce(Return(true));

    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "DISABLED"));
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, InitActive) {
    BatteryDefender battDefender;

    enableDefender();
    powerAvailable();
    defaultThresholds();

    EXPECT_CALL(*mock, ReadFileToString(kPathPersistChargerPresentTime, _, _))
            .WillOnce(DoAll(SetArgPointee<1>(std::to_string(DEFAULT_TIME_TO_ACTIVATE_SECONDS + 1)),
                            Return(true)));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE"));
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, InitConnectedCapacityReached) {
    BatteryDefender battDefender;

    enableDefender();
    powerAvailable();
    defaultThresholds();

    InSequence s;

    int time_expected = DEFAULT_TIME_TO_ACTIVATE_SECONDS - 1;
    EXPECT_CALL(*mock, ReadFileToString(kPathPersistChargerPresentTime, _, _))
            .WillOnce(DoAll(SetArgPointee<1>(std::to_string(time_expected)), Return(true)));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    battDefender.update(&props);

    testvar_systemTimeSecs += MIN_TIME_BETWEEN_FILE_UPDATES;
    time_expected += MIN_TIME_BETWEEN_FILE_UPDATES;
    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(time_expected),
                                         kPathPersistChargerPresentTime, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE"));
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, InitConnected) {
    BatteryDefender battDefender;

    enableDefender();
    powerAvailable();
    defaultThresholds();

    InSequence s;

    EXPECT_CALL(*mock, ReadFileToString(kPathPersistChargerPresentTime, _, _))
            .WillOnce(DoAll(SetArgPointee<1>(std::to_string(0)), Return(true)));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    battDefender.update(&props);

    // mHasReachedHighCapacityLevel shall be false
    testvar_systemTimeSecs += DEFAULT_TIME_TO_ACTIVATE_SECONDS + 1;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    battDefender.update(&props);

    // Would be active if mHasReachedHighCapacityLevel was true
    testvar_systemTimeSecs += DEFAULT_TIME_TO_ACTIVATE_SECONDS + 1;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, TriggerTime) {
    BatteryDefender battDefender;

    enableDefender();
    powerAvailable();
    defaultThresholds();

    InSequence s;

    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += MIN_TIME_BETWEEN_FILE_UPDATES;
    battDefender.update(&props);

    // Reached 100% capacity at least once
    capacityReached();
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += MIN_TIME_BETWEEN_FILE_UPDATES;
    battDefender.update(&props);

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(DEFAULT_TIME_TO_ACTIVATE_SECONDS),
                                         kPathPersistChargerPresentTime, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += DEFAULT_TIME_TO_ACTIVATE_SECONDS;
    battDefender.update(&props);

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(DEFAULT_TIME_TO_ACTIVATE_SECONDS +
                                                        MIN_TIME_BETWEEN_FILE_UPDATES),
                                         kPathPersistChargerPresentTime, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE"));
    testvar_systemTimeSecs += MIN_TIME_BETWEEN_FILE_UPDATES;
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, ChargeLevels) {
    BatteryDefender battDefender;

    enableDefender();
    powerAvailable();
    defaultThresholds();
    initToConnectedCapacityReached();

    InSequence s;

    // No expectations needed; default values already set
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += 0;
    battDefender.update(&props);

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(70), kPathStartLevel, _));
    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(80), kPathStopLevel, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE"));
    testvar_systemTimeSecs += DEFAULT_TIME_TO_ACTIVATE_SECONDS + 1;
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, ActiveTime) {
    BatteryDefender battDefender;

    enableDefender();
    powerAvailable();
    defaultThresholds();
    initToActive();

    InSequence s;

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(70), kPathStartLevel, _));
    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(80), kPathStopLevel, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE"));
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, ActiveTime_NonDefaultLevels) {
    BatteryDefender battDefender;

    enableDefender();
    powerAvailable();
    initToActive();
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderThreshold, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_ACTIVATE_SECONDS));
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlActivateTime, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_ACTIVATE_SECONDS));
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlResumeTime, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_CLEAR_SECONDS));

    // Non-default
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlStartSOC, _, _, _))
            .WillByDefault(Return(50));
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlStopSOC, _, _, _))
            .WillByDefault(Return(60));

    InSequence s;

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(50), kPathStartLevel, _));
    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(60), kPathStopLevel, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE"));
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, ActiveTime_NonDefaultLevels_invalid) {
    BatteryDefender battDefender;

    enableDefender();
    powerAvailable();
    initToActive();
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderThreshold, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_ACTIVATE_SECONDS));
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlActivateTime, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_ACTIVATE_SECONDS));
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlResumeTime, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_CLEAR_SECONDS));

    // Non-default
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlStartSOC, _, _, _))
            .WillByDefault(Return(30));
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlStopSOC, _, _, _))
            .WillByDefault(Return(10));

    InSequence s;

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(70), kPathStartLevel, _));
    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(80), kPathStopLevel, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE"));
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, ConnectDisconnectCycle) {
    BatteryDefender battDefender;

    enableDefender();
    defaultThresholds();
    initToConnectedCapacityReached();

    InSequence s;

    // Power ON
    wirelessPresent();

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(1000), kPathPersistChargerPresentTime, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += 60;
    battDefender.update(&props);

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(1060), kPathPersistChargerPresentTime, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += 60;
    battDefender.update(&props);

    // Power OFF
    wirelessNotPresent();

    // Maintain kPathPersistChargerPresentTime = 1060
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += 60;
    battDefender.update(&props);

    // Maintain kPathPersistChargerPresentTime = 1060
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += 60 * 4 - 1;
    battDefender.update(&props);

    testvar_systemTimeSecs += 1;
    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(0), kPathPersistChargerPresentTime, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "DISCONNECTED"));
    testvar_systemTimeSecs += MIN_TIME_BETWEEN_FILE_UPDATES;
    battDefender.update(&props);

    // Power ON
    wirelessPresent();

    // Maintain kPathPersistChargerPresentTime = 0
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += 60;
    battDefender.update(&props);

    capacityReached();
    // Maintain kPathPersistChargerPresentTime = 0
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += 60;
    battDefender.update(&props);

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(60), kPathPersistChargerPresentTime, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += 60;
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, ConnectDisconnectResumeTimeThreshold0) {
    BatteryDefender battDefender;

    enableDefender();
    initToConnectedCapacityReached();
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderThreshold, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_ACTIVATE_SECONDS));
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlActivateTime, _, _, _))
            .WillByDefault(Return(DEFAULT_TIME_TO_ACTIVATE_SECONDS));

    // Non-default thresholds
    ON_CALL(*mock, GetIntProperty(kPropBatteryDefenderCtrlResumeTime, _, _, _))
            .WillByDefault(Return(0));

    InSequence s;

    // Power ON
    wirelessPresent();

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(1000), kPathPersistChargerPresentTime, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += 60;
    battDefender.update(&props);

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(1060), kPathPersistChargerPresentTime, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    testvar_systemTimeSecs += 60;
    battDefender.update(&props);

    // Power OFF
    wirelessNotPresent();

    EXPECT_CALL(*mock, WriteStringToFile(std::to_string(0), kPathPersistChargerPresentTime, _));
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "DISCONNECTED"));
    testvar_systemTimeSecs += MIN_TIME_BETWEEN_FILE_UPDATES;
    battDefender.update(&props);
}

TEST_F(BatteryDefenderTest, PropsOverride_InitActive_allOnlineFalse) {
    BatteryDefender battDefender;

    enableDefender();
    usbPresent();
    defaultThresholds();
    initToActive();

    InSequence s;

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE")).Times(2);
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, false);
    ASSERT_EQ(props.chargerUsbOnline, true);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, false);
    ASSERT_EQ(props.chargerUsbOnline, true);
}

TEST_F(BatteryDefenderTest, PropsOverride_InitActive_usbOnline) {
    BatteryDefender battDefender;

    enableDefender();
    usbPresent();
    defaultThresholds();
    initToActive();

    InSequence s;

    props.chargerAcOnline = false;
    props.chargerUsbOnline = true;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE")).Times(2);
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, false);
    ASSERT_EQ(props.chargerUsbOnline, true);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, false);
    ASSERT_EQ(props.chargerUsbOnline, true);
}

TEST_F(BatteryDefenderTest, PropsOverride_InitActive_acOnline) {
    BatteryDefender battDefender;

    enableDefender();
    usbPresent();
    defaultThresholds();
    initToActive();

    InSequence s;

    props.chargerAcOnline = true;
    props.chargerUsbOnline = false;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE")).Times(2);
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, true);
    ASSERT_EQ(props.chargerUsbOnline, false);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, true);
    ASSERT_EQ(props.chargerUsbOnline, false);
}

TEST_F(BatteryDefenderTest, PropsOverride_InitActive_allOnline) {
    BatteryDefender battDefender;

    enableDefender();
    usbPresent();
    defaultThresholds();
    initToActive();

    InSequence s;

    props.chargerAcOnline = true;
    props.chargerUsbOnline = true;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE")).Times(2);
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, true);
    ASSERT_EQ(props.chargerUsbOnline, true);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, true);
    ASSERT_EQ(props.chargerUsbOnline, true);
}

TEST_F(BatteryDefenderTest, PropsOverride_InitConnected_allOnlineFalse) {
    BatteryDefender battDefender;

    enableDefender();
    usbPresent();
    defaultThresholds();
    initToConnectedCapacityReached();

    InSequence s;

    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    battDefender.update(&props);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    testvar_systemTimeSecs += DEFAULT_TIME_TO_ACTIVATE_SECONDS + 1;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE")).Times(2);
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, false);
    ASSERT_EQ(props.chargerUsbOnline, true);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, false);
    ASSERT_EQ(props.chargerUsbOnline, true);
}

TEST_F(BatteryDefenderTest, PropsOverride_InitConnected_usbOnline) {
    BatteryDefender battDefender;

    enableDefender();
    usbPresent();
    defaultThresholds();
    initToConnectedCapacityReached();

    InSequence s;

    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    battDefender.update(&props);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = true;
    testvar_systemTimeSecs += DEFAULT_TIME_TO_ACTIVATE_SECONDS + 1;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE")).Times(2);
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, false);
    ASSERT_EQ(props.chargerUsbOnline, true);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, false);
    ASSERT_EQ(props.chargerUsbOnline, true);
}

TEST_F(BatteryDefenderTest, PropsOverride_InitConnected_acOnline) {
    BatteryDefender battDefender;

    enableDefender();
    usbPresent();
    defaultThresholds();
    initToConnectedCapacityReached();

    InSequence s;

    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    battDefender.update(&props);

    props.chargerAcOnline = true;
    props.chargerUsbOnline = false;
    testvar_systemTimeSecs += DEFAULT_TIME_TO_ACTIVATE_SECONDS + 1;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE")).Times(2);
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, true);
    ASSERT_EQ(props.chargerUsbOnline, false);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, true);
    ASSERT_EQ(props.chargerUsbOnline, false);
}

TEST_F(BatteryDefenderTest, PropsOverride_InitConnected_allOnline) {
    BatteryDefender battDefender;

    enableDefender();
    usbPresent();
    defaultThresholds();
    initToConnectedCapacityReached();

    InSequence s;

    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    battDefender.update(&props);

    props.chargerAcOnline = true;
    props.chargerUsbOnline = true;
    testvar_systemTimeSecs += DEFAULT_TIME_TO_ACTIVATE_SECONDS + 1;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE")).Times(2);
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, true);
    ASSERT_EQ(props.chargerUsbOnline, true);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, true);
    ASSERT_EQ(props.chargerUsbOnline, true);
}

TEST_F(BatteryDefenderTest, PropsOverride_InitConnected_overrideHealth) {
    BatteryDefender battDefender;

    enableDefender();
    usbPresent();
    defaultThresholds();
    initToConnectedCapacityReached();

    InSequence s;

    props.batteryHealth = android::BATTERY_HEALTH_UNKNOWN;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED"));
    battDefender.update(&props);
    ASSERT_EQ(props.batteryHealth, android::BATTERY_HEALTH_UNKNOWN);

    props.batteryHealth = android::BATTERY_HEALTH_UNKNOWN;
    testvar_systemTimeSecs += DEFAULT_TIME_TO_ACTIVATE_SECONDS + 1;
    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "ACTIVE"));
    battDefender.update(&props);
    ASSERT_EQ(props.batteryHealth, android::BATTERY_HEALTH_OVERHEAT);
}

TEST_F(BatteryDefenderTest, PropsOverride_InitConnected_kernelDefend) {
    BatteryDefender battDefender;

    enableDefender();
    usbPresent();
    defaultThresholds();
    initToConnectedCapacityReached();

    InSequence s;

    EXPECT_CALL(*mock, SetProperty(kPropBatteryDefenderState, "CONNECTED")).Times(3);
    battDefender.update(&props);

    props.chargerAcOnline = true;
    props.chargerUsbOnline = true;
    props.batteryHealth = android::BATTERY_HEALTH_OVERHEAT;
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, true);
    ASSERT_EQ(props.chargerUsbOnline, true);

    props.chargerAcOnline = false;
    props.chargerUsbOnline = false;
    battDefender.update(&props);
    ASSERT_EQ(props.chargerAcOnline, true);
    ASSERT_EQ(props.chargerUsbOnline, true);
}

}  // namespace health
}  // namespace pixel
}  // namespace google
}  // namespace hardware
