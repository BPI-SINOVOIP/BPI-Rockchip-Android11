/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <iterator>
#include <set>
#include <sstream>
#include <thread>
#include <vector>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>


#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <hidl/HidlTransportSupport.h>

#include "ThermalImpl.h"
#include "thermal_map_table_type.h"
#include "thermal_map_table.h"


#define TH_LOG_TAG "thermal_hal"
#define TH_DLOG(_priority_, _fmt_, args...)  /*LOG_PRI(_priority_, TH_LOG_TAG, _fmt_, ##args)*/
#define TH_LOG(_priority_, _fmt_, args...)  LOG_PRI(_priority_, TH_LOG_TAG, _fmt_, ##args)

namespace android {
namespace hardware {
namespace thermal {
namespace V2_0 {
namespace implementation {

using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V1_0::ThermalStatusCode;
using ::android::hardware::thermal::V2_0::implementation::kRockchipTempThreshold;
using ::android::hardware::thermal::V2_0::implementation::tz_data;
using ::android::hardware::thermal::V2_0::implementation::cdata;



ThermalImpl::ThermalImpl(const NotificationCallback &cb)
    : thermal_watcher_(new ThermalWatcher(
              std::bind(&ThermalImpl::thermalWatcherCallbackFunc, this, std::placeholders::_1))),
      cb_(cb) {
    thermal_zone_num = 0;
    cooling_device_num = 0;
    thermal_watcher_->initThermalWatcher();
    // Need start watching after status map initialized
    is_initialized_ = thermal_watcher_->startThermalWatcher();
    if (!is_initialized_) {
        LOG(FATAL) << "ThermalHAL could not start watching thread properly.";
    }
}

ThrottlingSeverity ThermalImpl::getSeverityFromThresholds(float value, TemperatureType_2_0 type) {
	ThrottlingSeverity ret_hot = ThrottlingSeverity::NONE;
	int typetoint = static_cast<int>(type);

	if (typetoint < 0)
		return ret_hot;

	for (size_t i = static_cast<size_t>(ThrottlingSeverity::SHUTDOWN);
		i > static_cast<size_t>(ThrottlingSeverity::NONE); --i) {
		if (!std::isnan(kRockchipTempThreshold[typetoint].hotThrottlingThresholds[i]) && kRockchipTempThreshold[typetoint].hotThrottlingThresholds[i] <= value &&
			ret_hot == ThrottlingSeverity::NONE) {
			ret_hot = static_cast<ThrottlingSeverity>(i);
		}
	}

	return ret_hot;
}

bool ThermalImpl::read_temperature(int type, Temperature_1_0 *ret_temp) {

	FILE *file;
	float temp;
	bool ret = false;
	char temp_path[TZPATH_LENGTH];

	if (type < 0 || type > TT_SKIN) {
		return ret;
	}
	snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/temp", tz_data[type].tz_idx);
	file = fopen(temp_path, "r");
	if (file == NULL) {
		ALOGW("%s: failed to open type %d path %s", __func__, type, temp_path);
		return ret;
	} else {
		if (fscanf(file, "%f", &temp) > 0){
			ret_temp->name = tz_data[type].label;
			ret_temp->type = static_cast<TemperatureType_1_0>(type);
			ret_temp->currentValue = temp * 0.001;
			ret_temp->throttlingThreshold = kRockchipTempThreshold[type].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::SEVERE)];
			ret_temp->shutdownThreshold = kRockchipTempThreshold[type].hotThrottlingThresholds[static_cast<size_t>(ThrottlingSeverity::SHUTDOWN)];
			ret_temp->vrThrottlingThreshold = kRockchipTempThreshold[type].vrThrottlingThreshold;
			ret = true;
		}
		else
			ALOGW("%s: failed to fscanf %s", __func__, temp_path);
		}

		fclose(file);

	return ret;
}

bool ThermalImpl::read_temperature(int type, Temperature_2_0 *ret_temp) {

	FILE *file;
	float temp;
	bool ret = false;
	char temp_path[TZPATH_LENGTH];

	if (type < 0 || type >= TT_MAX) {
		return ret;
	}

	snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/temp", tz_data[type].tz_idx);
	file = fopen(temp_path, "r");
	if (file == NULL) {
		ALOGW("%s: failed to open type %d path %s", __func__, type, temp_path);
		return ret;
	} else {
		if (fscanf(file, "%f", &temp) > 0){
			ret_temp->name = tz_data[type].label;
			ret_temp->type = static_cast<TemperatureType_2_0>(type);
			ret_temp->value = temp * 0.001;
			ret_temp->throttlingStatus = getSeverityFromThresholds(ret_temp->value, ret_temp->type);
			ret = true;
		}
		else
			ALOGW("%s: failed to fscanf %s", __func__, temp_path);
		}

		fclose(file);

	return ret;
}

bool ThermalImpl::fillCpuUsages(hidl_vec<CpuUsage> *cpu_usages) {
	int vals, ret;
	ssize_t read;
	uint64_t user, nice, system, idle, active, total;
	char *line = NULL;
	size_t len = 0;
	int size = 0;
	FILE *file = NULL;
	unsigned int max_core_num, cpu_array;
	unsigned int cpu_num = 0;
	FILE *core_num_file = NULL;
	std::vector<CpuUsage> ret_cpu_usages;
	int i;

	/*======get device max core num=======*/
	core_num_file = fopen(CORENUM_PATH, "r");
	if (core_num_file == NULL) {
		ALOGW("thermal_hal: %s: failed to open:CORENUM_PATH %s", __func__, strerror(errno));
		return false;
	}

	if (fscanf(core_num_file, "%*d-%d", &max_core_num) != 1) {
		ALOGW("thermal_hal: %s: unable to parse CORENUM_PATH", __func__);
		ret = fclose(core_num_file);
		if (ret) {
			ALOGW("%s: fclose fail: %d", __func__, ret);
		}
		return false;
	}
	ret = fclose(core_num_file);
	if (ret) {
		ALOGW("%s: fclose fail: %d", __func__, ret);
	}

	cpu_array = sizeof(CPU_ALL_LABEL) / sizeof(CPU_ALL_LABEL[0]);

	if (((max_core_num + 1) > cpu_array) || ((max_core_num + 1) <= 0)) {
		ALOGW("thermal_hal: %s: max_core_num = %d, cpu_array = %d", __func__, max_core_num, cpu_array);
		return false;
	}
	max_core_num += 1;

	ALOGW("%s: max_core_num=%d", __func__, max_core_num);
	/*======get device max core num=======*/


	file = fopen(CPU_USAGE_FILE, "r");
	if (file == NULL) {
		ALOGW("thermal_hal: %s: failed to open: CPU_USAGE_FILE: %s", __func__, strerror(errno));
		return false;
	}

	while ((read = getline(&line, &len, file)) != -1) {
		CpuUsage cpu_usage;

		// Skip non "cpu[0-9]" lines.
		if (strnlen(line, read) < 4 || strncmp(line, "cpu", 3) != 0 || !isdigit(line[3])) {
			free(line);
			line = NULL;
			len = 0;
			continue;
		}


		vals = sscanf(line, "cpu%d %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64, &cpu_num, &user,
                        &nice, &system, &idle);

		free(line);
		line = NULL;
		len = 0;


		if (vals != 5) {
			ALOGW("thermal_hal: %s: failed to read CPU information from file: %s", __func__, strerror(errno));
			ret = fclose(file);
			if (ret) {
				ALOGW("%s: fclose fail: %d", __func__, ret);
			}
			return false;
		}

		active = user + nice + system;
		total = active + idle;

		if (cpu_num < max_core_num) {
			cpu_usage.name = CPU_ALL_LABEL[cpu_num];
			cpu_usage.active = active;
			cpu_usage.total = total;
			cpu_usage.isOnline = 1;
			ret_cpu_usages.emplace_back(std::move(cpu_usage));
		} else {
			ALOGW("thermal_hal: %s: cpu_num %d > max_core_num %d", __func__, cpu_num, max_core_num);
			ret = fclose(file);
			if (ret) {
				ALOGW("%s: fclose fail: %d", __func__, ret);
			}
			return false;
		}

		size++;
	}

    /*if there are hotplug off CPUs, set cpu_usage.total = 0*/
    for (i = size; i < max_core_num; i++) {
		CpuUsage cpu_usage;
		cpu_usage.name = CPU_ALL_LABEL[i];
		cpu_usage.active = 0;
		cpu_usage.total = 0;
		cpu_usage.isOnline = 0;
		ret_cpu_usages.emplace_back(std::move(cpu_usage));
    }

	ALOGW("%s end loop, size %d, cpu_num = %d, max_core_num = %d", __func__, size, cpu_num, max_core_num);

	ret = fclose(file);
	if (ret) {
		ALOGW("%s: fclose fail: %d", __func__, ret);
	}
	*cpu_usages = ret_cpu_usages;
	return true;

}

bool ThermalImpl::fill_temperatures_1_0(hidl_vec<Temperature_1_0> *temperatures) {

	bool ret = false;
	std::vector<Temperature_1_0> ret_temps;
	int current_index = 0;

	for (int i = 0; i <= TT_SKIN; i++) {
		Temperature_1_0 ret_temp;
		if (!is_tz_path_valided(i))
			init_tz_path();

		if (tz_data[i].tz_idx == -1) {
			continue;
		}
		if (read_temperature(i, &ret_temp))	{
			LOG(INFO) << "fill_temperatures_1_0 "
					<< " name: " << ret_temp.name
					<< " throttlingStatus: " << ret_temp.throttlingThreshold
					<< " value: " << ret_temp.currentValue;
			ret_temps.emplace_back(std::move(ret_temp));
			ret = true;
		} else {
			ALOGW("%s: read temp fail type:%d", __func__, i);
			return false;
		}
		++current_index;
	}
	*temperatures = ret_temps;
	return current_index > 0;
}

bool ThermalImpl::fill_temperatures(bool filterType, hidl_vec<Temperature_2_0> *temperatures, TemperatureType_2_0 type) {

	bool ret = false;
	std::vector<Temperature_2_0> ret_temps;
	int typetoint = static_cast<int>(type);

	if (!is_tz_path_valided(typetoint))
		init_tz_path();

	for (int i = 0; i < TT_MAX; i++) {
		Temperature_2_0 ret_temp;
		if ((filterType && i != typetoint) || (tz_data[i].tz_idx == -1)) {
			continue;
		}
		if (read_temperature(i, &ret_temp))	{
			LOG(INFO) << "fill_temperatures "
					<< "filterType" << filterType
					<< " name: " << ret_temp.name
					<< " type: " << android::hardware::thermal::V2_0::toString(ret_temp.type)
					<< " throttlingStatus: " << android::hardware::thermal::V2_0::toString(ret_temp.throttlingStatus)
					<< " value: " << ret_temp.value
					<< " ret_temps size " << ret_temps.size();
			ret_temps.emplace_back(std::move(ret_temp));
			ret = true;
		} else {
			ALOGW("%s: read temp fail type:%d", __func__, i);
			return false;
		}
	}

	*temperatures = ret_temps;

	return ret;
}
bool ThermalImpl::fill_thresholds(bool filterType, hidl_vec<TemperatureThreshold> *Threshold, TemperatureType_2_0 type) {

	FILE *file;
	bool ret = false;
	std::vector<TemperatureThreshold> ret_thresholds;
	int typetoint = static_cast<int>(type);
	char temp_path[TZPATH_LENGTH];

	for (int i = 0; i < TT_MAX; i++) {
		TemperatureThreshold ret_threshold;
		if (filterType && i != typetoint) {
			continue;
		}
		snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", tz_data[i].tz_idx);
		file = fopen(temp_path, "r");
		if (file) {
			ret_threshold = {kRockchipTempThreshold[i]};
			LOG(INFO) << "fill_thresholds "
					<< "filterType" << filterType
					<< " name: " << ret_threshold.name
					<< " type: " << android::hardware::thermal::V2_0::toString(ret_threshold.type)
					<< " vrThrottlingThreshold: " << ret_threshold.vrThrottlingThreshold
					<< " ret_thresholds size " << ret_thresholds.size();
			ret_thresholds.emplace_back(std::move(ret_threshold));
			ret = true;
			fclose(file);
		}
		else {
			ALOGW("%s: %s not support", __func__, kRockchipTempThreshold[i].name.c_str());
			}
	}

	*Threshold = ret_thresholds;
	return ret;
}

bool ThermalImpl::fill_cooling_devices(bool filterType, std::vector<CoolingDevice_2_0> *CoolingDevice, CoolingType type) {

	std::vector<CoolingDevice_2_0> ret_coolings;
	bool ret = false;

	if (!is_cooling_path_valided())
		init_cl_path();

	for (int i = 0; i < MAX_COOLING; i++) {
		if (filterType && type != cdata[i].cl_2_0.type) {
			continue;
		}
		if (cdata[i].cl_idx != -1) {
			CoolingDevice_2_0 coolingdevice;
			coolingdevice.name = cdata[i].cl_2_0.name;
			coolingdevice.type = cdata[i].cl_2_0.type;
			coolingdevice.value = cdata[i].cl_2_0.value;
			LOG(INFO) << "fill_cooling_devices "
						<< "filterType" << filterType
						<< " name: " << coolingdevice.name
						<< " type: " << android::hardware::thermal::V2_0::toString(coolingdevice.type)
						<< " value: " << coolingdevice.value
						<< " ret_coolings size " << ret_coolings.size();
			ret_coolings.emplace_back(std::move(coolingdevice));
			ret = true;
		}
	}
	*CoolingDevice = ret_coolings;
	return ret;
}



bool ThermalImpl::init_cl_path() {

	char temp_path[CDPATH_LENGTH];
	char temp_value_path[CDPATH_LENGTH];
	char buf[CDNAME_SZ];
	int fd = -1;
	int fd_value = -1;
	int read_len = 0;
	int i = 0;
	bool ret = true;
	/*initial cdata*/
	for (int j = 0; j < MAX_COOLING; ++j) {
		cdata[j].cl_2_0.value = 0;
		cdata[j].cl_idx = -1;
	}
	cooling_device_num = 0;
	while(1) {
		snprintf(temp_path, CDPATH_LENGTH, CDPATH_PREFIX"%d/type", i);
		fd = open(temp_path, O_RDONLY);
		if (fd == -1) {
			ALOGW("%s:find out cooling path", __func__);
			cooling_device_num = i;
			break;
		} else {
			CoolingDevice_2_0 coolingdevice;
			read_len = read(fd, buf, CDNAME_SZ);
			for (int j = 0; j < MAX_COOLING; ++j) {
				if (std::strncmp(buf, cdata[j].cl_2_0.name.c_str(), std::strlen(cdata[j].cl_2_0.name.c_str())) == 0) {
					cdata[j].cl_idx = i;
					snprintf(temp_value_path, CDPATH_LENGTH, CDPATH_PREFIX"%d/cur_state", i);
					fd_value = open(temp_value_path, O_RDONLY);
					if (fd_value == -1) {
						ALOGW("%s:get value fail", __func__);
						ret = false;
						break;
					} else {
						read_len = read(fd_value, buf, CDNAME_SZ);
						cdata[j].cl_2_0.value = std::atoi(buf);
						LOG(INFO) << "init_cl_path"
									<< "cl_idx" << cdata[j].cl_idx
									<< " name: " << cdata[j].cl_2_0.name
									<< " value: " << cdata[j].cl_2_0.value;
					}
					close(fd_value);
				}
			}

		}
		i++;
		close(fd);
	}
	return ret;
}

bool ThermalImpl::is_cooling_path_valided() {
	char temp_path[CDPATH_LENGTH];
	char buf[CDNAME_SZ];
	int fd = -1;
	int read_len = 0;
	bool ret = true;

	/*check if cooling device number are changed*/
	snprintf(temp_path, CDPATH_LENGTH, CDPATH_PREFIX"%d/type", (cooling_device_num - 1));
	fd = open(temp_path, O_RDONLY);
	if (fd == -1) {
		LOG(INFO) << "cl_num are changed" << cooling_device_num;
		return false;
	} else {
		close(fd);
	}

	snprintf(temp_path, CDPATH_LENGTH, CDPATH_PREFIX"%d/type", cooling_device_num);
	fd = open(temp_path, O_RDONLY);
	if (fd != -1) {
		close(fd);
		LOG(INFO) << "cl_num are increased" << cooling_device_num;
		return false;
	}


	for (int i = 0; i < MAX_COOLING; i++) {
		if (cdata[i].cl_idx != -1) {
			snprintf(temp_path, CDPATH_LENGTH, CDPATH_PREFIX"%d/type", cdata[i].cl_idx);
			fd = open(temp_path, O_RDONLY);
			if (fd == -1) {
				ALOGW("%s:cl path error %d %s" , __func__, i, temp_path);
					ret = false;
				break;
			} else {
				read_len = read(fd, buf, CDNAME_SZ);
				if (std::strncmp(buf, cdata[i].cl_2_0.name.c_str(), std::strlen(cdata[i].cl_2_0.name.c_str())) != 0) {
					ret = false;
					close(fd);
					LOG(INFO) << " cl name mismatch "<< i << cdata[i].cl_2_0.name;
					break;
				}
			close(fd);
			}
		}
	}

	return ret;
}

bool ThermalImpl::is_tz_path_valided(int type) {
	char temp_path[TZPATH_LENGTH];
	char buf[TZNAME_SZ];
	int fd = -1;
	int read_len = 0;
	bool ret = true;

	if (type < 0 || type >= TT_MAX) {
		return false;
	}
	/*check if thermal zone number are changed*/
	snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", (thermal_zone_num - 1));
	fd = open(temp_path, O_RDONLY);
	if (fd == -1) {
		LOG(INFO) << "thermal_zone_num are changed" << thermal_zone_num;
		return false;
	} else {
		close(fd);
	}

	snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", thermal_zone_num);
	fd = open(temp_path, O_RDONLY);
	if (fd != -1) {
		close(fd);
		LOG(INFO) << "thermal_zone_num are increased" << thermal_zone_num;
		return false;
	}
	if (tz_data[type].tz_idx != -1) {
		snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", tz_data[type].tz_idx);
		fd = open(temp_path, O_RDONLY);
		if (fd == -1) {
			ALOGW("%s:tz path error %d %s" , __func__, type, temp_path);
			ret = false;
		} else {
			read_len = read(fd, buf, TZNAME_SZ);
			/*/sys/class/thermal/thermal_zone{$tz_idx}/type should equal tzName*/
			if (std::strncmp(buf, tz_data[type].tzName, strlen(tz_data[type].tzName)) != 0) {
				ret = false;
				LOG(INFO) << " tz name mismatch "<< type << tz_data[type].tzName;
			}
			close(fd);
		}
	}

	return ret;
}


void ThermalImpl::init_tz_path() {
	char temp_path[TZPATH_LENGTH];
	char buf[TZNAME_SZ];
	int fd = -1;
	int read_len = 0;
	int i = 0;

	/*initial tz_data*/
	for (int j = 0; j < TT_MAX; ++j) {
		tz_data[j].tz_idx = -1;
	}
	thermal_zone_num = 0;

	while(1) {
		snprintf(temp_path, TZPATH_LENGTH, TZPATH_PREFIX"%d/type", i);
		fd = open(temp_path, O_RDONLY);
		if (fd == -1) {
			ALOGW("%s:find out tz path", __func__);
			thermal_zone_num = i;
			break;
		} else {
			read_len = read(fd, buf, TZNAME_SZ);
			for (int j = 0; j < TT_MAX; ++j) {
				if (std::strncmp(buf, tz_data[j].tzName, strlen(tz_data[j].tzName)) == 0) {
					tz_data[j].tz_idx = i;
					ALOGW("tz_data[%d].tz_idx:%d",j,i);
				}
			}
			i++;
			close(fd);
		}
	}

}


// This is called in the different thread context and will update sensor_status
// uevent_sensors is the set of sensors which trigger uevent from thermal core driver.
bool ThermalImpl::thermalWatcherCallbackFunc(const std::set<std::string> &uevent_sensors) {
	std::vector<Temperature_2_0> temps;
	bool thermal_triggered = false;
	Temperature_2_0 temp;

	if (uevent_sensors.size() != 0) {
	// writer lock
	std::unique_lock<std::shared_mutex> _lock(sensor_status_map_mutex_);
		for (const auto &name : uevent_sensors) {
			for (int i = 0; i < TT_MAX; i++) {
				if (strncmp(name.c_str(), tz_data[i].tzName, strlen(tz_data[i].tzName)) == 0) {
					if (!is_tz_path_valided(i))
						init_tz_path();
					if (read_temperature(i,&temp))
						temps.push_back(temp);
				}
			}
		}
		thermal_triggered = true;
	}
	if (!temps.empty() && cb_) {
		cb_(temps);
	}

	return thermal_triggered;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace thermal
}  // namespace hardware
}  // namespace android
