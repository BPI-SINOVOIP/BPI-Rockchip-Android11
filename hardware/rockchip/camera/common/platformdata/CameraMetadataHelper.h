/*
 * Copyright (C) 2014-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef _CAMERA3_HAL_METADATAHELPER_H_
#define _CAMERA3_HAL_METADATAHELPER_H_

#include <CameraMetadata.h>
#include "LogHelper.h"

#define RK_GRALLOC_USAGE_SPECIFY_STRIDE 1ULL << 30

/* ********************************************************************
 * Camera metadata auxiliary API
 */
USING_METADATA_NAMESPACE;
using namespace android;

NAMESPACE_DECLARATION {
namespace MetadataHelper {

void dumpMetadata(const camera_metadata_t * meta);

bool getMetadataValue(const CameraMetadata &metadata, uint32_t tag, uint8_t & value, int count = -1);
bool getMetadataValue(const CameraMetadata &metadata, uint32_t tag, int32_t & value, int count = -1);
bool getMetadataValue(const CameraMetadata &metadata, uint32_t tag, int64_t & value, int count = -1);
bool getMetadataValue(const CameraMetadata &metadata, uint32_t tag, float & value, int count = -1);
bool getMetadataValue(const CameraMetadata &metadata, uint32_t tag, double & value, int count = -1);

const void * getMetadataValues(const CameraMetadata &metadata, uint32_t tag, int type, int * count = nullptr);
const void * getMetadataValues(const camera_metadata_t * metadata, uint32_t tag, int type, int * count = nullptr);

camera_metadata_ro_entry getMetadataEntry(const camera_metadata_t *metadata, uint32_t tag, bool printError = true);

void getValueByType(const camera_metadata_ro_entry_t& setting, int idx, uint8_t* value);
void getValueByType(const camera_metadata_ro_entry_t& setting, int idx, int32_t* value);
void getValueByType(const camera_metadata_ro_entry_t& setting, int idx, float* value);
void getValueByType(const camera_metadata_ro_entry_t& setting, int idx, int64_t* value);
void getValueByType(const camera_metadata_ro_entry_t& setting, int idx, double* value);
/**
 * Convenience function to get a setting from a list of supported
 * Note that the "supported" entry MUST have the tag in it!
 * supported: it's a supported list
 * setting: it should be one of supported
 * value:
 * if the setting is one of the supported, return the value in the setting.
 * if the setting is not one of the supported, return the default one in supported.
 */
template <typename T>
bool getSetting(const camera_metadata_ro_entry_t& supported,
                  const camera_metadata_ro_entry_t& setting,
                  T* value)
{
    if (supported.count == 0) {
        LOGE("no supported option in xml for tag \"%s.%s\"",
             get_camera_metadata_section_name(supported.tag),
             get_camera_metadata_tag_name(supported.tag));
        return false;
    }

    T supportedVal = 0;
    if (setting.count == 1) {
        T settingVal = 0;
        getValueByType(setting, 0, &settingVal);
        for (size_t i = 0; i < supported.count; i++) {
            getValueByType(supported, i, &supportedVal);
            if (supportedVal == settingVal) {
                *value = settingVal;
                return true;
            }
        }
        LOGE("trying to use unsupported value %d for tag \"%s.%s\"",
             settingVal,
             get_camera_metadata_section_name(setting.tag),
             get_camera_metadata_tag_name(setting.tag));
    } else {
        LOGE("count for settings isn't one, can't check it, count:%zu", setting.count);
    }

    getValueByType(supported, 0, &supportedVal);
    LOGE("using default value %d instead of the setting", supportedVal);
    *value = supportedVal;
    return false;
}

status_t updateMetadata(camera_metadata_t * metadata, uint32_t tag, const void* data, size_t data_count);

};

} NAMESPACE_DECLARATION_END
#endif // _CAMERA3_HAL_METADATAHELPER_H_
