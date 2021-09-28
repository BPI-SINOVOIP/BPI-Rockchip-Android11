/*
 * Copyright (C) 2017 Intel Corporation
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

#define LOG_TAG "ChromeProfiles"

#include <string>
#include <sstream>
#include <sys/stat.h>
#include <vector>
#include <expat.h>
#include <sys/stat.h>
#include "Utils.h"
#include "LogHelper.h"
#include "PlatformData.h"
#include "ChromeCameraProfiles.h"
#include <system/camera_metadata.h>
#include "Metadata.h"
#include "CameraMetadataHelper.h"
#include "IPSLConfParser.h"

NAMESPACE_DECLARATION {
#if defined(ANDROID_VERSION_ABOVE_8_X)
static const char *sDefaultXmlFileName = "/vendor/etc/camera/camera3_profiles.xml";
#else
static const char *sDefaultXmlFileName = "/etc/camera/camera3_profiles.xml";
#endif
ChromeCameraProfiles::ChromeCameraProfiles(CameraHWInfo *cameraHWInfo) :
    CameraProfiles(cameraHWInfo)
{
}

/**
 * getXMLConfigName
 *
 * retrieve the name of the XML file used for configuration and store it in
 * the member variable: mXmlConfigName
 * By default the value is sDefaultXmlFileName
 *
 */
void ChromeCameraProfiles::getXmlConfigName(void)
{
    struct stat sb;
    int PathExists = stat(sDefaultXmlFileName, &sb);
    if (PathExists != 0) {
        LOGE("Error, could not find camera3_profiles.xml!!");
    }
    mXmlConfigName = sDefaultXmlFileName;
}

status_t ChromeCameraProfiles::init()
{
    status_t status = OK;

    LOGI("@%s", __FUNCTION__);

    // determine the xml file name
    getXmlConfigName();

    status = CameraProfiles::init();
    if (status) {
        LOGE("CameraProfiles base init error:%d", status);
        return status;
    }

    // Parse common sections
    getDataFromXmlFile();

    createConfParser();

    return OK;
}

ChromeCameraProfiles::~ChromeCameraProfiles()
{
    LOGI("@%s", __FUNCTION__);
    destroyConfParser();
}

/**
 * This function will handle all the android static metadata related elements of sensor.
 *
 * It will be called in the function startElement
 * This method parses the input from the XML file, that can be manipulated.
 * So extra care is applied in the validation of strings
 *
 * \param name: the element's name.
 * \param atts: the element's attribute.
 */
void ChromeCameraProfiles::handleAndroidStaticMetadata(const char *name, const char **atts)
{
    if (!validateStaticMetadata(name, atts))
        return;

    // Find tag
    const metadata_tag_t *tagInfo = findTagInfo(name, android_static_tags_table, STATIC_TAGS_TABLE_SIZE);
    if (tagInfo == nullptr)
        return;

    int count = 0;
    camera_metadata_t * currentMeta = nullptr;
    MetaValueRefTable mvrt;
    std::vector<MetaValueRefTable> refTables;
    LOGI("@%s: Parsing static tag %s: value %s", __FUNCTION__, tagInfo->name,  atts[1]);

    /**
     * Complex parsing types done manually (exceptions)
     * scene overrides uses different tables for each entry one from ae/awb/af
     * mode
     */
    if (tagInfo->value == ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP) {
        mvrt.table      = android_scaler_availableFormats_values;
        mvrt.tableSize  = ELEMENT(android_scaler_availableFormats_values);
        refTables.push_back(mvrt);
        count = parseAvailableInputOutputFormatsMap(atts[1], tagInfo, refTables,
                METADATASIZE, mMetadataCache);
    } else if ((tagInfo->value == ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS) ||
               (tagInfo->value == ANDROID_REQUEST_AVAILABLE_RESULT_KEYS)) {
        count = parseAvailableKeys(atts[1], tagInfo, METADATASIZE, mMetadataCache);
    } else if (tagInfo->value == ANDROID_SYNC_MAX_LATENCY) {
        count = parseEnumAndNumbers(atts[1], tagInfo, METADATASIZE, mMetadataCache);
    } else { /* Parsing of generic types */
        if (tagInfo->arrayTypedef == STREAM_CONFIGURATION) {
            mvrt.table      = android_scaler_availableFormats_values;
            mvrt.tableSize  = ELEMENT(android_scaler_availableFormats_values);
            refTables.push_back(mvrt);
            mvrt.table      = android_scaler_availableStreamConfigurations_values;
            mvrt.tableSize  = ELEMENT(android_scaler_availableStreamConfigurations_values);
            refTables.push_back(mvrt);
            count = parseStreamConfig(atts[1], tagInfo, refTables, METADATASIZE, mMetadataCache);
        } else if (tagInfo->arrayTypedef == STREAM_CONFIGURATION_DURATION) {
            mvrt.table      = android_scaler_availableFormats_values;
            mvrt.tableSize  = ELEMENT(android_scaler_availableFormats_values);
            refTables.push_back(mvrt);
            count = parseStreamConfigDuration(atts[1], tagInfo, refTables, METADATASIZE,
                    mMetadataCache);
        } else {
            count = parseGenericTypes(atts[1], tagInfo, METADATASIZE, mMetadataCache);
        }
    }

    if (count == 0) {
        LOGW("Error parsing static tag %s. ignoring", tagInfo->name);
        return;
    }

    LOGI("@%s: writing static tag %s: count %d", __FUNCTION__,
                                                 tagInfo->name,
                                                 count);

    if (mStaticMeta.size() > 0) {
        currentMeta = mStaticMeta.at(mSensorIndex);
    } else {
        LOGE("Camera isn't added, unable to get the static metadata");
        return;
    }
    if (add_camera_metadata_entry(currentMeta, tagInfo->value, mMetadataCache, count)) {
        LOGE("call add_camera_metadata_entry fail for tag:%s", get_camera_metadata_tag_name(tagInfo->value));
    }
    else
        // save the key to mCharacteristicsKeys used to update the
        // REQUEST_AVAILABLE_CHARACTERISTICS_KEYS
        mCharacteristicsKeys[mSensorIndex].push_back(tagInfo->value);
}

} NAMESPACE_DECLARATION_END
