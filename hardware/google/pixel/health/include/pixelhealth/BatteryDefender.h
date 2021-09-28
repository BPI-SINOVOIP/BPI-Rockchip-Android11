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

#ifndef HARDWARE_GOOGLE_PIXEL_HEALTH_BATTERYDEFENDER_H
#define HARDWARE_GOOGLE_PIXEL_HEALTH_BATTERYDEFENDER_H

#include <batteryservice/BatteryService.h>

#include <stdbool.h>
#include <time.h>
#include <string>

namespace hardware {
namespace google {
namespace pixel {
namespace health {

const uint32_t ONE_MIN_IN_SECONDS = 60;
const uint32_t ONE_HOUR_IN_MINUTES = 60;
const uint32_t ONE_DAY_IN_HOURS = 24;
const uint32_t ONE_DAY_IN_SECONDS = ONE_DAY_IN_HOURS * ONE_HOUR_IN_MINUTES * ONE_MIN_IN_SECONDS;

const uint32_t DEFAULT_TIME_TO_ACTIVATE_SECONDS = (4 * ONE_DAY_IN_SECONDS);
const uint32_t DEFAULT_TIME_TO_CLEAR_SECONDS = (5 * ONE_MIN_IN_SECONDS);
const int DEFAULT_CHARGE_LEVEL_START = 0;
const int DEFAULT_CHARGE_LEVEL_STOP = 100;
const int DEFAULT_CHARGE_LEVEL_DEFENDER_START = 70;
const int DEFAULT_CHARGE_LEVEL_DEFENDER_STOP = 80;
const int DEFAULT_CAPACITY_LEVEL = 100;

class BatteryDefender {
  public:
    // Set default google charger paths - can be overridden for other devices
    BatteryDefender(const char *pathWirelessPresent = "/sys/class/power_supply/wireless/present",
                    const char *pathChargeLevelStart =
                            "/sys/devices/platform/soc/soc:google,charger/charge_start_level",
                    const char *pathChargeLevelStop =
                            "/sys/devices/platform/soc/soc:google,charger/charge_stop_level",
                    const int32_t timeToActivateSecs = DEFAULT_TIME_TO_ACTIVATE_SECONDS,
                    const int32_t timeToClearTimerSecs = DEFAULT_TIME_TO_CLEAR_SECONDS);

    // This function shall be called periodically in HealthService
    void update(struct android::BatteryProperties *props);

  private:
    enum state_E {
        STATE_INIT,
        STATE_DISABLED,
        STATE_DISCONNECTED,
        STATE_CONNECTED,
        STATE_ACTIVE,
        STATE_COUNT,
    };
    const char *const stateStringMap[STATE_COUNT] = {
            [STATE_INIT] = "INIT",
            [STATE_DISABLED] = "DISABLED",
            [STATE_DISCONNECTED] = "DISCONNECTED",
            [STATE_CONNECTED] = "CONNECTED",
            [STATE_ACTIVE] = "ACTIVE",
    };

    const char *const kPathWirelessPresent;
    const char *const kPathChargeLevelStart;
    const char *const kPathChargeLevelStop;
    const int32_t kTimeToActivateSecs;
    const int32_t kTimeToClearTimerSecs;

    // Sysfs
    const char *const kPathUSBChargerPresent = "/sys/class/power_supply/usb/present";
    const char *const kPathPersistChargerPresentTime =
            "/mnt/vendor/persist/battery/defender_charger_time";
    const char *const kPathPersistDefenderActiveTime =
            "/mnt/vendor/persist/battery/defender_active_time";

    // Properties
    const char *const kPropChargeLevelVendorStart = "persist.vendor.charge.start.level";
    const char *const kPropChargeLevelVendorStop = "persist.vendor.charge.stop.level";
    const char *const kPropBatteryDefenderState = "vendor.battery.defender.state";
    const char *const kPropBatteryDefenderDisable = "vendor.battery.defender.disable";
    const char *const kPropBatteryDefenderThreshold = "vendor.battery.defender.threshold";
    const char *const kPropBootmode = "ro.bootmode";
    const char *const kPropBatteryDefenderCtrlEnable = "vendor.battery.defender.ctrl.enable";
    const char *const kPropBatteryDefenderCtrlActivateTime =
            "vendor.battery.defender.ctrl.trigger_time";
    const char *const kPropBatteryDefenderCtrlResumeTime =
            "vendor.battery.defender.ctrl.resume_time";
    const char *const kPropBatteryDefenderCtrlStartSOC =
            "vendor.battery.defender.ctrl.recharge_soc_start";
    const char *const kPropBatteryDefenderCtrlStopSOC =
            "vendor.battery.defender.ctrl.recharge_soc_stop";
    const char *const kPropBatteryDefenderCtrlTriggerSOC =
            "vendor.battery.defender.ctrl.trigger_soc";

    // Default thresholds
    const bool kDefaultEnable = true;
    const int kChargeLevelDefaultStart = DEFAULT_CHARGE_LEVEL_START;
    const int kChargeLevelDefaultStop = DEFAULT_CHARGE_LEVEL_STOP;
    const int kChargeLevelDefenderStart = DEFAULT_CHARGE_LEVEL_DEFENDER_START;
    const int kChargeLevelDefenderStop = DEFAULT_CHARGE_LEVEL_DEFENDER_STOP;
    const int kChargeHighCapacityLevel = DEFAULT_CAPACITY_LEVEL;

    // Inputs
    int64_t mTimeBetweenUpdateCalls = 0;
    int64_t mTimePreviousSecs;
    bool mIsUsbPresent = false;
    bool mIsWirelessPresent = false;
    bool mIsPowerAvailable = false;
    bool mIsDefenderDisabled = false;
    int32_t mTimeToActivateSecsModified;

    // State
    state_E mCurrentState = STATE_INIT;
    int64_t mTimeChargerPresentSecs = 0;
    int64_t mTimeChargerPresentSecsPrevious = -1;
    int64_t mTimeChargerNotPresentSecs = 0;
    int64_t mTimeActiveSecs = 0;
    int64_t mTimeActiveSecsPrevious = -1;
    int mChargeLevelStartPrevious = DEFAULT_CHARGE_LEVEL_START;
    int mChargeLevelStopPrevious = DEFAULT_CHARGE_LEVEL_STOP;
    bool mHasReachedHighCapacityLevel = false;
    bool mWasAcOnline = false;
    bool mWasUsbOnline = true; /* Default; in case neither AC/USB online becomes 1 */
    bool mIgnoreWirelessFileError = false;

    // Process state actions
    void stateMachine_runAction(const state_E state,
                                const struct android::BatteryProperties *props);

    // Check state transitions
    state_E stateMachine_getNextState(const state_E state);

    // Process state entry actions
    void stateMachine_firstAction(const state_E state);

    void updateDefenderProperties(struct android::BatteryProperties *props);
    void clearStateData(void);
    void loadPersistentStorage(void);
    int64_t getTime(void);
    int64_t getDeltaTimeSeconds(int64_t *timeStartSecs);
    int32_t getTimeToActivate(void);
    void removeLineEndings(std::string *str);
    int readFileToInt(const char *path, const bool optionalFile = false);
    bool writeIntToFile(const char *path, const int value);
    void writeTimeToFile(const char *path, const int value, int64_t *previous);
    void writeChargeLevelsToFile(const int vendorStart, const int vendorStop);
    bool isChargePowerAvailable(void);
    bool isDefaultChargeLevel(const int start, const int stop);
    bool isBatteryDefenderDisabled(const int vendorStart, const int vendorStop);
    void addTimeToChargeTimers(void);
};

}  // namespace health
}  // namespace pixel
}  // namespace google
}  // namespace hardware

#endif /* HARDWARE_GOOGLE_PIXEL_HEALTH_BATTERYDEFENDER_H */
