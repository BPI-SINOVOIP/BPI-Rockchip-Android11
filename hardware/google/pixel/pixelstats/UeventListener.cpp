/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "pixelstats-uevent"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <android/frameworks/stats/1.0/IStats.h>
#include <cutils/uevent.h>
#include <hardware/google/pixel/pixelstats/pixelatoms.pb.h>
#include <log/log.h>
#include <pixelstats/UeventListener.h>
#include <pixelstats/WlcReporter.h>
#include <unistd.h>
#include <utils/StrongPointer.h>

#include <string>
#include <thread>

using android::sp;
using android::base::ReadFileToString;
using android::base::WriteStringToFile;
using android::frameworks::stats::V1_0::HardwareFailed;
using android::frameworks::stats::V1_0::IStats;
using android::frameworks::stats::V1_0::UsbPortOverheatEvent;
using android::frameworks::stats::V1_0::VendorAtom;
using android::hardware::google::pixel::WlcReporter;
using android::hardware::google::pixel::PixelAtoms::ChargeStats;
using android::hardware::google::pixel::PixelAtoms::VoltageTierStats;
using android::hardware::google::pixel::PixelAtoms::PdVidPid;

namespace android {
namespace hardware {
namespace google {
namespace pixel {

constexpr int32_t UEVENT_MSG_LEN = 2048;  // it's 2048 in all other users.
constexpr int32_t PRODUCT_TYPE_OFFSET = 23;
constexpr int32_t PRODUCT_TYPE_MASK = 7;
constexpr int32_t PRODUCT_TYPE_CHARGER = 3;
constexpr int32_t VID_MASK = 0xffff;
constexpr int32_t VID_GOOGLE = 0x18d1;
constexpr int32_t PID_OFFSET = 2;
constexpr int32_t PID_LENGTH = 4;

bool UeventListener::ReadFileToInt(const std::string &path, int *val) {
    return ReadFileToInt(path.c_str(), val);
}

bool UeventListener::ReadFileToInt(const char *const path, int *val) {
    std::string file_contents;

    if (!ReadFileToString(path, &file_contents)) {
        ALOGE("Unable to read %s - %s", path, strerror(errno));
        return false;
    } else if (sscanf(file_contents.c_str(), "%d", val) != 1) {
        ALOGE("Unable to convert %s to int - %s", path, strerror(errno));
        return false;
    }
    return true;
}

void UeventListener::ReportMicBrokenOrDegraded(const int mic, const bool isbroken) {
    sp<IStats> stats_client = IStats::tryGetService();

    if (stats_client) {
        HardwareFailed failure = {
                .hardwareType = HardwareFailed::HardwareType::MICROPHONE,
                .hardwareLocation = mic,
                .errorCode = isbroken ? HardwareFailed::HardwareErrorCode::COMPLETE
                                      : HardwareFailed::HardwareErrorCode::DEGRADE};
        Return<void> ret = stats_client->reportHardwareFailed(failure);
        if (!ret.isOk())
            ALOGE("Unable to report physical drop to Stats service");
    } else {
        ALOGE("Unable to connect to Stats service");
    }
}

void UeventListener::ReportMicStatusUevents(const char *devpath, const char *mic_status) {
    if (!devpath || !mic_status)
        return;
    if (!strcmp(devpath, ("DEVPATH=" + kAudioUevent).c_str())) {
        std::vector<std::string> value = android::base::Split(mic_status, "=");
        bool isbroken;

        if (value.size() == 2) {
            if (!value[0].compare("MIC_BREAK_STATUS"))
                isbroken = true;
            else if (!value[0].compare("MIC_DEGRADE_STATUS"))
                isbroken = false;
            else
                return;

            if (!value[1].compare("true")) {
                ReportMicBrokenOrDegraded(0, isbroken);
            } else {
                int mic_status = atoi(value[1].c_str());

                if (mic_status > 0 && mic_status <= 7) {
                    for (int mic_bit = 0; mic_bit < 3; mic_bit++)
                        if (mic_status & (0x1 << mic_bit))
                            ReportMicBrokenOrDegraded(mic_bit, isbroken);
                } else if (mic_status == 0) {
                    // mic is ok
                    return;
                } else {
                    // should not enter here
                    ALOGE("invalid mic status");
                    return;
                }
            }
        }
    }
}

void UeventListener::ReportUsbPortOverheatEvent(const char *driver) {
    UsbPortOverheatEvent event = {};
    std::string file_contents;

    if (!driver || strcmp(driver, "DRIVER=google,overheat_mitigation")) {
        return;
    }

    ReadFileToInt((kUsbPortOverheatPath + "/plug_temp"), &event.plugTemperatureDeciC);
    ReadFileToInt((kUsbPortOverheatPath + "/max_temp"), &event.maxTemperatureDeciC);
    ReadFileToInt((kUsbPortOverheatPath + "/trip_time"), &event.timeToOverheat);
    ReadFileToInt((kUsbPortOverheatPath + "/hysteresis_time"), &event.timeToHysteresis);
    ReadFileToInt((kUsbPortOverheatPath + "/cleared_time"), &event.timeToInactive);

    sp<IStats> stats_client = IStats::tryGetService();

    if (stats_client) {
        stats_client->reportUsbPortOverheatEvent(event);
    }
}

void UeventListener::ReportChargeStats(const sp<IStats> &stats_client, const char *line) {
    std::vector<int> charge_stats_fields = {
            ChargeStats::kAdapterTypeFieldNumber,     ChargeStats::kAdapterVoltageFieldNumber,
            ChargeStats::kAdapterAmperageFieldNumber, ChargeStats::kSsocInFieldNumber,
            ChargeStats::kVoltageInFieldNumber,       ChargeStats::kSsocOutFieldNumber,
            ChargeStats::kVoltageOutFieldNumber};
    std::vector<VendorAtom::Value> values(charge_stats_fields.size());
    VendorAtom::Value val;
    int32_t i = 0, tmp[7] = {0};

    ALOGD("ChargeStats: processing %s", line);
    if (sscanf(line, "%d,%d,%d, %d,%d,%d,%d", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5],
               &tmp[6]) != 7) {
        ALOGE("Couldn't process %s", line);
        return;
    }
    for (i = 0; i < charge_stats_fields.size(); i++) {
        val.intValue(tmp[i]);
        values[charge_stats_fields[i] - kVendorAtomOffset] = val;
    }

    VendorAtom event = {.reverseDomainName = PixelAtoms::ReverseDomainNames().pixel(),
                        .atomId = PixelAtoms::Ids::CHARGE_STATS,
                        .values = values};
    Return<void> ret = stats_client->reportVendorAtom(event);
    if (!ret.isOk())
        ALOGE("Unable to report ChargeStats to Stats service");
}

void UeventListener::ReportVoltageTierStats(const sp<IStats> &stats_client, const char *line) {
    std::vector<int> voltage_tier_stats_fields = {VoltageTierStats::kVoltageTierFieldNumber,
                                                  VoltageTierStats::kSocInFieldNumber,
                                                  VoltageTierStats::kCcInFieldNumber,
                                                  VoltageTierStats::kTempInFieldNumber,
                                                  VoltageTierStats::kTimeFastSecsFieldNumber,
                                                  VoltageTierStats::kTimeTaperSecsFieldNumber,
                                                  VoltageTierStats::kTimeOtherSecsFieldNumber,
                                                  VoltageTierStats::kTempMinFieldNumber,
                                                  VoltageTierStats::kTempAvgFieldNumber,
                                                  VoltageTierStats::kTempMaxFieldNumber,
                                                  VoltageTierStats::kIbattMinFieldNumber,
                                                  VoltageTierStats::kIbattAvgFieldNumber,
                                                  VoltageTierStats::kIbattMaxFieldNumber,
                                                  VoltageTierStats::kIclMinFieldNumber,
                                                  VoltageTierStats::kIclAvgFieldNumber,
                                                  VoltageTierStats::kIclMaxFieldNumber};
    std::vector<VendorAtom::Value> values(voltage_tier_stats_fields.size());
    VendorAtom::Value val;
    float ssoc_tmp;
    int32_t i = 0, tmp[15] = {0};

    if (sscanf(line, "%d, %f,%d,%d, %d,%d,%d, %d,%d,%d, %d,%d,%d, %d,%d,%d", &tmp[0], &ssoc_tmp,
               &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5], &tmp[6], &tmp[7], &tmp[8], &tmp[9],
               &tmp[10], &tmp[11], &tmp[12], &tmp[13], &tmp[14]) != 16) {
        /* If format isn't as expected, then ignore line on purpose */
        return;
    }
    ALOGD("VoltageTierStats: processed %s", line);
    val.intValue(tmp[0]);
    values[voltage_tier_stats_fields[0] - kVendorAtomOffset] = val;
    val.floatValue(ssoc_tmp);
    values[voltage_tier_stats_fields[1] - kVendorAtomOffset] = val;
    for (i = 2; i < voltage_tier_stats_fields.size(); i++) {
        val.intValue(tmp[i - 1]);
        values[voltage_tier_stats_fields[i] - kVendorAtomOffset] = val;
    }

    VendorAtom event = {.reverseDomainName = PixelAtoms::ReverseDomainNames().pixel(),
                        .atomId = PixelAtoms::Ids::VOLTAGE_TIER_STATS,
                        .values = values};
    Return<void> ret = stats_client->reportVendorAtom(event);
    if (!ret.isOk())
        ALOGE("Unable to report VoltageTierStats to Stats service");
}

void UeventListener::ReportChargeMetricsEvent(const char *driver) {
    if (!driver || strcmp(driver, "DRIVER=google,battery")) {
        return;
    }

    std::string file_contents, line;
    std::istringstream ss;

    if (!ReadFileToString(kChargeMetricsPath.c_str(), &file_contents)) {
        ALOGE("Unable to read %s - %s", kChargeMetricsPath.c_str(), strerror(errno));
        return;
    }

    ss.str(file_contents);

    if (!std::getline(ss, line)) {
        ALOGE("Unable to read first line");
        return;
    }

    if (!WriteStringToFile(std::to_string(0), kChargeMetricsPath.c_str())) {
        ALOGE("Couldn't clear %s - %s", kChargeMetricsPath.c_str(), strerror(errno));
    }

    sp<IStats> stats_client = IStats::tryGetService();
    if (!stats_client) {
        ALOGE("Couldn't connect to IStats service");
        return;
    }

    ReportChargeStats(stats_client, line.c_str());

    while (std::getline(ss, line)) {
        ReportVoltageTierStats(stats_client, line.c_str());
    }
}

/* ReportWlc
 * Report wireless relate  metrics when wireless charging start
 */
void UeventListener::ReportWlc(const bool pow_wireless, const bool online, const char *ptmc) {
    if (!pow_wireless) {
        return;
    }

    wlc_reporter_.checkAndReport(online, ptmc);
}
/**
 * Report raw battery capacity, system battery capacity and associated
 * battery capacity curves. This data is collected to verify the filter
 * applied on the battery capacity. This will allow debugging of issues
 * ranging from incorrect fuel gauge hardware calculations to issues
 * with the software reported battery capacity.
 *
 * The data is retrieved by parsing the battery power supply's ssoc_details.
 *
 * This atom logs data in 5 potential events:
 *      1. When a device is connected
 *      2. When a device is disconnected
 *      3. When a device has reached a full charge (from the UI's perspective)
 *      4. When there is a >= 2 percent skip in the UI reported SOC
 *      5. When there is a difference of >= 4 percent between the raw hardware
 *          battery capacity and the system reported battery capacity.
 */
void UeventListener::ReportBatteryCapacityFGEvent(const char *subsystem) {
    if (!subsystem || strcmp(subsystem, "SUBSYSTEM=power_supply")) {
        return;
    }

    // Indicates an implicit disable of the battery capacity reporting
    if (kBatterySSOCPath.empty()) {
        return;
    }

    battery_capacity_reporter_.checkAndReport(kBatterySSOCPath);
}

void UeventListener::ReportTypeCPartnerId() {
    std::string file_contents_vid, file_contents_pid;
    uint32_t pid, vid;

    if (!ReadFileToString(kTypeCPartnerVidPath.c_str(), &file_contents_vid)) {
        ALOGE("Unable to read %s - %s", kTypeCPartnerVidPath.c_str(), strerror(errno));
        return;
    }

    if (sscanf(file_contents_vid.c_str(), "%x", &vid) != 1) {
        ALOGE("Unable to parse vid %s from file %s to int.",
                file_contents_vid.c_str(), kTypeCPartnerVidPath.c_str());
        return;
    }

    if (!ReadFileToString(kTypeCPartnerPidPath.c_str(), &file_contents_pid)) {
        ALOGE("Unable to read %s - %s", kTypeCPartnerPidPath.c_str(), strerror(errno));
        return;
    }

    if (sscanf(file_contents_pid.substr(PID_OFFSET, PID_LENGTH).c_str(), "%x", &pid) != 1) {
        ALOGE("Unable to parse pid %s from file %s to int.",
              file_contents_pid.substr(PID_OFFSET, PID_LENGTH).c_str(),
              kTypeCPartnerPidPath.c_str());
        return;
    }

    // Upload data only for chargers
    if (((vid >> PRODUCT_TYPE_OFFSET) & PRODUCT_TYPE_MASK) != PRODUCT_TYPE_CHARGER) {
        return;
    }

    // Upload data only for Google VID
    if ((VID_MASK & vid) != VID_GOOGLE) {
        return;
    }

    std::vector<VendorAtom::Value> values(2);
    VendorAtom::Value tmp;

    tmp.intValue(vid & VID_MASK);
    values[PdVidPid::kVidFieldNumber - kVendorAtomOffset] = tmp;
    tmp.intValue(pid);
    values[PdVidPid::kPidFieldNumber - kVendorAtomOffset] = tmp;
    sp<IStats> stats_client = IStats::tryGetService();
    if (!stats_client) {
        ALOGE("PD PID/VID Couldn't connect to IStats service");
        return;
    }

    // Send vendor atom to IStats HAL
    VendorAtom event = {.reverseDomainName = PixelAtoms::ReverseDomainNames().pixel(),
             .atomId = PixelAtoms::Ids::PD_VID_PID,
             .values = values};
    Return<void> ret = stats_client->reportVendorAtom(event);
    if (!ret.isOk()) {
        ALOGE("Unable to report PD VID/PID to Stats service");
    }
}

bool UeventListener::ProcessUevent() {
    char msg[UEVENT_MSG_LEN + 2];
    char *cp;
    const char *driver, *product, *subsystem;
    const char *mic_break_status, *mic_degrade_status;
    const char *devpath;
    bool collect_partner_id = false;
    bool pow_online;
    bool pow_wireless;
    const char *pow_ptmc;
    int n;

    if (uevent_fd_ < 0) {
        uevent_fd_ = uevent_open_socket(64 * 1024, true);
        if (uevent_fd_ < 0) {
            ALOGE("uevent_init: uevent_open_socket failed\n");
            return false;
        }
    }

    n = uevent_kernel_multicast_recv(uevent_fd_, msg, UEVENT_MSG_LEN);
    if (n <= 0 || n >= UEVENT_MSG_LEN)
        return false;

    // Ensure double-null termination of msg.
    msg[n] = '\0';
    msg[n + 1] = '\0';

    driver = product = subsystem = NULL;
    mic_break_status = mic_degrade_status = devpath = pow_ptmc = NULL;
    pow_online = pow_wireless = false;

    /**
     * msg is a sequence of null-terminated strings.
     * Iterate through and record positions of string/value pairs of interest.
     * Double null indicates end of the message. (enforced above).
     */
    cp = msg;
    while (*cp) {
        if (!strncmp(cp, "DRIVER=", strlen("DRIVER="))) {
            driver = cp;
        } else if (!strncmp(cp, "PRODUCT=", strlen("PRODUCT="))) {
            product = cp;
        } else if (!strncmp(cp, "MIC_BREAK_STATUS=", strlen("MIC_BREAK_STATUS="))) {
            mic_break_status = cp;
        } else if (!strncmp(cp, "MIC_DEGRADE_STATUS=", strlen("MIC_DEGRADE_STATUS="))) {
            mic_degrade_status = cp;
        } else if (!strncmp(cp, "DEVPATH=", strlen("DEVPATH="))) {
            devpath = cp;
        } else if (!strncmp(cp, "SUBSYSTEM=", strlen("SUBSYSTEM="))) {
            subsystem = cp;
        } else if (!strncmp(cp, "DEVTYPE=typec_partner", strlen("DEVTYPE=typec_partner"))) {
            collect_partner_id = true;
        } else if (!strncmp(cp, "POWER_SUPPLY_NAME=wireless",
                            strlen("POWER_SUPPLY_NAME=wireless"))) {
            pow_wireless = true;
        } else if (!strncmp(cp, "POWER_SUPPLY_ONLINE=1", strlen("POWER_SUPPLY_ONLINE=1"))) {
            pow_online = true;
        } else if (!strncmp(cp, "POWER_SUPPLY_PTMC_ID=", strlen("POWER_SUPPLY_PTMC_ID="))) {
            pow_ptmc = cp;
        }
        /* advance to after the next \0 */
        while (*cp++) {
        }
    }

    /* Process the strings recorded. */
    ReportMicStatusUevents(devpath, mic_break_status);
    ReportMicStatusUevents(devpath, mic_degrade_status);
    ReportUsbPortOverheatEvent(driver);
    ReportChargeMetricsEvent(driver);
    ReportWlc(pow_wireless, pow_online, pow_ptmc);
    ReportBatteryCapacityFGEvent(subsystem);
    if (collect_partner_id) {
        ReportTypeCPartnerId();
    }

    return true;
}

UeventListener::UeventListener(const std::string audio_uevent, const std::string ssoc_details_path,
                               const std::string overheat_path,
                               const std::string charge_metrics_path,
                               const std::string typec_partner_vid_path,
                               const std::string typec_partner_pid_path)
    : kAudioUevent(audio_uevent),
      kBatterySSOCPath(ssoc_details_path),
      kUsbPortOverheatPath(overheat_path),
      kChargeMetricsPath(charge_metrics_path),
      kTypeCPartnerVidPath(typec_partner_vid_path),
      kTypeCPartnerPidPath(typec_partner_pid_path),
      uevent_fd_(-1) {}

/* Thread function to continuously monitor uevents.
 * Exit after kMaxConsecutiveErrors to prevent spinning. */
void UeventListener::ListenForever() {
    constexpr int kMaxConsecutiveErrors = 10;
    int consecutive_errors = 0;

    while (1) {
        if (ProcessUevent()) {
            consecutive_errors = 0;
        } else {
            if (++consecutive_errors >= kMaxConsecutiveErrors) {
                ALOGE("Too many ProcessUevent errors; exiting UeventListener.");
                return;
            }
        }
    }
}

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android
