/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THERMAL_THERMAL_HELPER_H__
#define THERMAL_THERMAL_HELPER_H__

#include <array>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <android/hardware/thermal/2.0/IThermal.h>
#include "ThermalWatcher.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::hidl_vec;
using ::android::hardware::thermal::V1_0::CpuUsage;
using ::android::hardware::thermal::V2_0::CoolingType;
using ::android::hardware::thermal::V2_0::IThermal;
using CoolingDevice_1_0 = ::android::hardware::thermal::V1_0::CoolingDevice;
using CoolingDevice_2_0 = ::android::hardware::thermal::V2_0::CoolingDevice;
using Temperature_1_0 = ::android::hardware::thermal::V1_0::Temperature;
using Temperature_2_0 = ::android::hardware::thermal::V2_0::Temperature;
using TemperatureType_1_0 = ::android::hardware::thermal::V1_0::TemperatureType;
using TemperatureType_2_0 = ::android::hardware::thermal::V2_0::TemperatureType;
using ::android::hardware::thermal::V2_0::TemperatureThreshold;
using ::android::hardware::thermal::V2_0::ThrottlingSeverity;

using NotificationCallback = std::function<void(const std::vector<Temperature_2_0> &temps)>;
using NotificationTime = std::chrono::time_point<std::chrono::steady_clock>;

struct SensorStatus {
	ThrottlingSeverity severity;
	ThrottlingSeverity prev_hot_severity;
	ThrottlingSeverity prev_cold_severity;
};

class ThermalImpl {
  public:
	ThermalImpl(const NotificationCallback &cb);
	~ThermalImpl() = default;
	// Dissallow copy and assign.
	ThermalImpl(const ThermalImpl &) = delete;
	void operator=(const ThermalImpl &) = delete;
	bool isInitializedOk() const { return is_initialized_; }
	ThrottlingSeverity getSeverityFromThresholds(float value, TemperatureType_2_0 type);
	bool fillCpuUsages(hidl_vec<CpuUsage> *cpu_usages);
	bool fill_temperatures_1_0(hidl_vec<Temperature_1_0> *temperatures);
	bool fill_temperatures(bool filterType, hidl_vec<Temperature_2_0> *temperatures, TemperatureType_2_0 type);
	bool fill_thresholds(bool filterType, hidl_vec<TemperatureThreshold> *Threshold, TemperatureType_2_0 type);
	bool fill_cooling_devices(bool filterType, std::vector<CoolingDevice_2_0> *CoolingDevice, CoolingType type);
	void init_tz_path(void);
	bool init_cl_path(void);
	bool read_temperature(int type, Temperature_1_0 *ret_temp);
	bool read_temperature(int type, Temperature_2_0 *ret_temp);
	int get_tz_num() const { return thermal_zone_num; };
	int get_cooling_num() const { return cooling_device_num; }
	bool is_tz_path_valided(int type);
	bool is_cooling_path_valided();
  private:
	// For thermal_watcher_'s polling thread
	bool thermalWatcherCallbackFunc(const std::set<std::string> &uevent_sensors);
	sp<ThermalWatcher> thermal_watcher_;
	bool is_initialized_;
	const NotificationCallback cb_;

	mutable std::shared_mutex sensor_status_map_mutex_;
	std::map<std::string, SensorStatus> sensor_status_map_;
	int thermal_zone_num;
	int cooling_device_num;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android

#endif  // THERMAL_THERMAL_HELPER_H__
