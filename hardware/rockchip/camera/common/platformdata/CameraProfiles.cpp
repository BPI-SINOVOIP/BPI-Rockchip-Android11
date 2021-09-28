/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#define LOG_TAG "Profiles"

#include <expat.h>
#include <limits.h>
#include <system/camera_metadata.h>

#include "LogHelper.h"
#include "CameraProfiles.h"
#include "Metadata.h"
#include "CameraMetadataHelper.h"
#include "IPSLConfParser.h"
#include "Utils.h"

#ifdef CAMERA_RKISP2_SUPPORT
#include "RKISP2PSLConfParser.h"
#else
#include "PSLConfParser.h"
#endif

/**
 * Platform specific implementation
 */
#include "ChromeCameraProfiles.h"

#define STATIC_ENTRY_CAP 256
#define STATIC_DATA_CAP 6688 // TODO: we may need to increase it again if more metadata is added
#define MAX_METADATA_NAME_LENTGTH 128
#define MAX_METADATA_ATTRIBUTE_NAME_LENTGTH 128
#define MAX_METADATA_ATTRIBUTE_VALUE_LENTGTH 6144

NAMESPACE_DECLARATION {
CameraProfiles::CameraProfiles(CameraHWInfo *cameraHWInfo) :
    mCurrentDataField(FIELD_INVALID),
    mMetadataCache(nullptr),
    mSensorIndex(-1),
    mXmlSensorIndex(-1),
    mItemsCount(-1),
    mCameraCommon(cameraHWInfo),
    mUseEntry(true)
{
    CLEAR(mProfileEnd);
}

status_t CameraProfiles::init()
{
    LOGI("@%s", __FUNCTION__);

    if (!mCameraCommon) {
        LOGE("CameraHWInfo is nullptr");
        return BAD_VALUE;
    }
#ifdef CAMERA_RKISP2_SUPPORT
    mCameraCommon->init(rkisp2::RKISP2PSLConfParser::getSensorMediaDevicePath());
#else
    mCameraCommon->init(PSLConfParser::getSensorMediaDevicePath());
#endif
    // Assumption: Driver enumeration order will match the CameraId
    // CameraId in camera_profiles.xml. Main camera is always at
    // index 0, front camera at index 1.
    if (mCameraCommon->mSensorInfo.empty()) {
        LOGE("No sensor Info available, exit parsing");
        return UNKNOWN_ERROR;
    }

    mSensorNames = mCameraCommon->mSensorInfo;

    mCameraInfoPool.init(MAX_CAMERAS);
    for (int i = 0;i < MAX_CAMERAS; i++)
         mCharacteristicsKeys[i].clear();

    return OK;
}

void CameraProfiles::createConfParser()
{
    // Now parse PSL sections for found cameras
    for (const auto &cameraIdToCameraInfo : mCameraIdToCameraInfo) {
        // get psl parser
        CameraInfo *info = cameraIdToCameraInfo.second;
#ifdef CAMERA_RKISP2_SUPPORT
        info->parser = rkisp2::RKISP2PSLConfParser::getInstance(mXmlConfigName, mSensorNames);
#else
        info->parser = PSLConfParser::getInstance(mXmlConfigName, mSensorNames);
#endif
    }
}

void CameraProfiles::destroyConfParser()
{
#ifdef CAMERA_RKISP2_SUPPORT
    rkisp2::RKISP2PSLConfParser::deleteInstance();
#else
    PSLConfParser::deleteInstance();
#endif
}

int CameraProfiles::getXmlCameraId(int cameraId) const
{
    LOGI("@%s", __FUNCTION__);
    std::map<int, CameraInfo*>::const_iterator it =
                            mCameraIdToCameraInfo.find(cameraId);
    if (it == mCameraIdToCameraInfo.end()) {
        return NAME_NOT_FOUND;
    }
    return it->second->xmlCameraId;
}

CameraProfiles::~CameraProfiles()
{
    LOGI("@%s", __FUNCTION__);
    for (unsigned int i = 0; i < mStaticMeta.size(); i++) {
        if (mStaticMeta[i])
            free_camera_metadata(mStaticMeta[i]);
    }

    mStaticMeta.clear();

    for (const auto &cameraIdToCameraInfo : mCameraIdToCameraInfo) {
        CameraInfo *info = cameraIdToCameraInfo.second;
        mCameraInfoPool.releaseItem(info);
    }

    mCameraIdToCameraInfo.clear();

    mSensorNames.clear();
}

const CameraCapInfo *CameraProfiles::getCameraCapInfo(int cameraId)
{
    // get the psl parser instance for cameraid
    IPSLConfParser *parserInstance;
    std::map<int, CameraInfo*>::iterator it =
                            mCameraIdToCameraInfo.find(cameraId);
    if (it == mCameraIdToCameraInfo.end()) {
        LOGE("Camera id: %d not found. Sensor might not be live", cameraId);
        return nullptr;
    }
    CameraInfo *info = it->second;
    parserInstance = info->parser;

    if (parserInstance == nullptr) {
        LOGE("Failed to get PSL parser instance");
        return nullptr;
    }

    return parserInstance->getCameraCapInfo(cameraId);
}

const CameraCapInfo *CameraProfiles::getCameraCapInfoForXmlCameraId(int xmlCameraId)
{
    // get the psl parser instance for cameraid
    IPSLConfParser *parserInstance;

    int cameraId = -1;
    for (const auto &cameraIdToCameraInfo : mCameraIdToCameraInfo) {
        if (cameraIdToCameraInfo.second->xmlCameraId == xmlCameraId) {
            cameraId = cameraIdToCameraInfo.first;
            break;
        }
    }
    if (cameraId == -1)
        return nullptr;

    CameraInfo *info = mCameraIdToCameraInfo[cameraId];
    parserInstance = info->parser;

    if (parserInstance == nullptr) {
        LOGE("Failed to get PSL parser instance");
        return nullptr;
    }

    return parserInstance->getCameraCapInfo(cameraId);
}

camera_metadata_t *CameraProfiles::constructDefaultMetadata(int cameraId, int requestTemplate)
{
    // get the psl parser instance for cameraid
    IPSLConfParser *parserInstance;
    std::map<int, CameraInfo*>::iterator it =
                            mCameraIdToCameraInfo.find(cameraId);
    if (it == mCameraIdToCameraInfo.end()) {
        LOGE("Failed to get camera info for camera:%d", cameraId);
        return nullptr;
    }

    CameraInfo *info = it->second;
    parserInstance = info->parser;

    if (parserInstance == nullptr) {
        LOGE("Failed to get PSL parser instance");
        return nullptr;
    }

    return parserInstance->constructDefaultMetadata(cameraId, requestTemplate);

}

status_t CameraProfiles::addCamera(int cameraId)
{
    LOGI("%s: for camera %d", __FUNCTION__, cameraId);

    camera_metadata_t * meta = allocate_camera_metadata(STATIC_ENTRY_CAP, STATIC_DATA_CAP);
    if (!meta) {
        LOGE("No memory for camera metadata!");
        return NO_MEMORY;
    }
    LOGI("Add cameraId: %d to mStaticMeta", cameraId);
    mStaticMeta[cameraId] = meta;

    return NO_ERROR;
}

/**
 * convertEnum
 * Converts from the string provided as src to an enum value.
 * It uses a table to convert from the string to an enum value (usually BYTE)
 * \param [IN] dest: data buffer where to store the result
 * \param [IN] src: pointer to the string to parse
 * \param [IN] table: table to convert from string to value
 * \param [IN] tableNum: size of the enum table
 * \param [IN] type: data type to be parsed (byte, int32, int64 etc..)
 * \param [OUT] newDest: pointer to the new write location
 */
int CameraProfiles::convertEnum(void *dest, const char *src, int type,
                                const metadata_value_t *table, int tableLen,
                                void **newDest)
{
    int ret = 0;
    union {
        uint8_t * u8;
        int32_t * i32;
        int64_t * i64;
    } data;

    *newDest = dest;
    data.u8 = (uint8_t *)dest;

    /* ignore any spaces in front of the string */
    size_t blanks = strspn(src," ");
    src = src + blanks;

    for (int i = 0; i < tableLen; i++ ) {
        if (!strncasecmp(src, table[i].name, STRLEN_S(table[i].name))
            && (STRLEN_S(src) == STRLEN_S(table[i].name))) {
            if (type == TYPE_BYTE) {
                data.u8[0] = table[i].value;
                LOGI("byte    - %s: %d -", table[i].name, data.u8[0]);
                *newDest = (void*) &data.u8[1];
            } else if (type == TYPE_INT32) {
                data.i32[0] = table[i].value;
                LOGI("int    - %s: %d -", table[i].name, data.i32[0]);
                *newDest = (void*) &data.i32[1];
            } else if (type == TYPE_INT64) {
                data.i64[0] = table[i].value;
                LOGI("int64    - %s: %" PRId64 " -", table[i].name, data.i64[0]);
                *newDest = (void*) &data.i64[1];
            }
            ret = 1;
            break;
        }
    }

    return ret;
}


/**
 * parseEnum
 * parses an enumeration type or a list of enumeration types it stores the data
 * in the member buffer mMetadataCache that is of size mMetadataCacheSize.
 *
 * \param src: string to be parsed
 * \param tagInfo: structure with the description of the static metadata
 * \param metadataCacheSize:the upper limited size of the dest buffer
 * \param metadataCache: the dest buffer to store the medatada after persed
 * \return number of elements parsed
 */
int CameraProfiles::parseEnum(const char *src,
                              const metadata_tag_t *tagInfo,
                              int metadataCacheSize,
                              int64_t* metadataCache)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int count = 0;
    int maxCount = metadataCacheSize / camera_metadata_type_size[tagInfo->type];
    char *endPtr = nullptr;

    /**
     * pointer to the metadata cache buffer
     */
    void *storeBuf = metadataCache;
    void *next;

    do {
        endPtr = (char*) strchr(src, ',');
        if (endPtr)
            *endPtr = 0;

        count += convertEnum(storeBuf, src,tagInfo->type,
                             tagInfo->enumTable, tagInfo->tableLength, &next);
        if (endPtr) {
            src = endPtr + 1;
            storeBuf = next;
        }
    } while (count < maxCount && endPtr);

    return count;
}

/**
 * parseEnumAndNumbers
 * parses an enumeration type or a list of enumeration types or tries to convert string to a number
 * it stores the data in the member buffer mMetadataCache that is of size mMetadataCacheSize.
 *
 * \param src: string to be parsed
 * \param tagInfo: structure with the description of the static metadata
  * \param metadataCacheSize:the upper limited size of the dest buffer
 * \param metadataCache: the dest buffer to store the medatada after persed
 * \return number of elements parsed
 */
int CameraProfiles::parseEnumAndNumbers(const char *src,
                                        const metadata_tag_t *tagInfo,
                                        int metadataCacheSize,
                                        int64_t* metadataCache)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int count = 0;
    int maxCount = metadataCacheSize / camera_metadata_type_size[tagInfo->type];
    char * endPtr = nullptr;

    /**
     * pointer to the metadata cache buffer
     */
    void *storeBuf = metadataCache;
    void *next;

    do {
        endPtr = (char *) strchr(src, ',');
        if (endPtr)
            *endPtr = 0;

        count += convertEnum(storeBuf, src,tagInfo->type,
                             tagInfo->enumTable, tagInfo->tableLength, &next);
        /* Try to convert value to number */
        if (count == 0) {
            long int *number = reinterpret_cast<long int*>(storeBuf);
            *number = strtol(src, &endPtr, 10);
            if (*number == LONG_MAX || *number == LONG_MIN)
                LOGW("You might have invalid value in the camera profiles: %s", src);
            count++;
        }

        if (endPtr) {
            src = endPtr + 1;
            storeBuf = next;
        }
    } while (count < maxCount && endPtr);

    return count;
}

/**
 * parseData
 * parses a generic array type. It stores the data in the member buffer
 * mMetadataCache that is of size mMetadataCacheSize.
 *
 * \param src: string to be parsed
 * \param tagInfo: structure with the description of the static metadata
 * \param metadataCacheSize:the upper limited size of the dest buffer
 * \param metadataCache: the dest buffer to store the medatada after persed
 * \return number of elements parsed
 */
int CameraProfiles::parseData(const char *src,
                              const metadata_tag_t *tagInfo,
                              int metadataCacheSize,
                              int64_t* metadataCache)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int index = 0;
    int maxIndex = metadataCacheSize/sizeof(double); // worst case

    char * endPtr = nullptr;
    union {
        uint8_t * u8;
        int32_t * i32;
        int64_t * i64;
        float * f;
        double * d;
    } data;
    data.u8 = (uint8_t *)metadataCache;

    do {
        switch (tagInfo->type) {
        case TYPE_BYTE:
            data.u8[index] = (char)strtol(src, &endPtr, 10);
            LOGI("    - %d -", data.u8[index]);
            break;
        case TYPE_INT32:
        case TYPE_RATIONAL:
            data.i32[index] = strtol(src, &endPtr, 10);
            LOGI("    - %d -", data.i32[index]);
            break;
        case TYPE_INT64:
            data.i64[index] = strtol(src, &endPtr, 10);
            LOGI("    - %" PRId64 " -", data.i64[index]);
            break;
        case TYPE_FLOAT:
            data.f[index] = strtof(src, &endPtr);
            LOGI("    - %8.3f -", data.f[index]);
            break;
        case TYPE_DOUBLE:
            data.d[index] = strtof(src, &endPtr);
            LOGI("    - %8.3f -", data.d[index]);
            break;
        }
        index++;
        if (endPtr != nullptr) {
            if (*endPtr == ',' || *endPtr == 'x')
                src = endPtr + 1;
            else if (*endPtr == ')')
                src = endPtr + 3;
            else if (*endPtr == 0)
                break;
        }
    } while (index < maxIndex);

    if (tagInfo->type == TYPE_RATIONAL) {
        if (index % 2) {
            LOGW("Invalid number of entries to define rational (%d) in tag %s."
                            " It should be even", index, tagInfo->name);
            // lets make it even
            index -= 1;
        }
        index = index / 2;
        // we divide by 2 because one rational is made of 2 ints
    }

    return index;
}


const char *CameraProfiles::skipWhiteSpace(const char *src)
{
    /* Skip whitespace. (space, tab, newline, vertical tab, feed, carriage return) */
    while( *src == '\n' || *src == '\t' || *src == ' ' || *src == '\v' || *src == '\r' || *src == '\f'  ) {
        src++;
    }
    return src;
}

/**
 * Parses the string with the supported stream configurations
 * a stream configuration is made of 3 elements
 * - Format
 * - Resolution
 * - Direction (input or output)
 * we parse the string in 3 steps
 * example of valid stream configuration is:
 * RAW16,4208x3120,INPUT
 * \param src: string to be parsed
 * \param tagInfo: descriptor of the static metadata. this is the entry from the
 * table defined in the autogenerated code
 * \param metadataCacheSize:the upper limited size of the dest buffer
 * \param metadataCache: the dest buffer to store the medatada after persed
 *
 * \return number of int32 entries to be stored (i.e. 4 per configuration found)
 */
int CameraProfiles::parseStreamConfig(const char *src,
        const metadata_tag_t *tagInfo,
        std::vector<MetaValueRefTable> refTables,
        int metadataCacheSize,
        int64_t* metadataCache)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    int count = 0;  // entry count
    int maxCount = metadataCacheSize/sizeof(int32_t);
    int ret;
    char * endPtr = nullptr;
    int parseStep = 1;
    int32_t *i32;

    const metadata_value_t * activeTable;
    int activeTableLen = 0;

    void *storeBuf = metadataCache;
    void *next;

    if (refTables.size() < 2) {
        LOGE("incomplete reference table :%zu", refTables.size());
        return count;
    }

    do {
        endPtr = (char *) strchr(src, ',');
        if (endPtr)
            *endPtr = 0;

        if (parseStep == 1) {
            activeTable = refTables[0].table;
            activeTableLen = refTables[0].tableSize;
        } else if (parseStep == 3) {
            activeTable = refTables[1].table;
            activeTableLen = refTables[1].tableSize;
        }

        if (parseStep == 1 || parseStep == 3) {
            ret = convertEnum(storeBuf, src, tagInfo->type, activeTable,
                              activeTableLen, &next);
            if (ret == 1) {
                count++;
                storeBuf = next;
            } else {
                LOGE("Malformed enum in stream configuration %s", src);
                goto parseError;
            }

        } else {  // Step 2: Parse the resolution
            i32 = reinterpret_cast<int32_t*>(storeBuf);
            i32[0] = strtol(src, &endPtr, 10);
            if (endPtr == nullptr || *endPtr != 'x') {
                LOGE("Malformed resolution in stream configuration");
                goto parseError;
            }
            src = endPtr + 1;
            i32[1] = strtol(src, &endPtr, 10);
            storeBuf = reinterpret_cast<void*>(&i32[2]);
            count += 2;
            LOGI("  - %dx%d -", i32[0], i32[1]);
        }

        if (endPtr) {
            src = endPtr + 1;
            src = skipWhiteSpace(src);
            parseStep++;
            /* parsing steps go from 1 to 3 */
            if (parseStep == 4) {
                parseStep = 1;
                LOGI("Stream Configuration found");
            }
        } else {
            break;
        }
    } while (count < maxCount);

    if (endPtr != nullptr) {
        LOGW("Stream configuration stream too long for parser");
    }
    /**
     * Total number of entries per stream configuration is 4
     * - one for the format
     * - two for the resolution
     * - one for the direction
     * The total entry count should be multiple of 4
     */
    if (count % 4) {
        LOGE("Malformed string for stream configuration."
             " ignoring last %d entries", count % 4);
        count -= count % 4;
    }
    return count;

parseError:
    LOGE("Error parsing stream configuration ");
    return 0;
}
/**
 * parseAvailableKeys
 * This method is used to parse the following two static metadata tags:
 * android.request.availableRequestKeys
 * android.request.availableResultKeys
 *
 * It uses the auto-generated table metadataNames to look for all the non
 * static tags.
 */
int CameraProfiles::parseAvailableKeys(const char *src,
                                       const metadata_tag_t *tagInfo,
                                       int metadataCacheSize,
                                       int64_t* metadataCache)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int count = 0;  // entry count
    int maxCount = metadataCacheSize/camera_metadata_type_size[tagInfo->type];
    size_t tableSize = ELEMENT(metadataNames);
    size_t blanks, tokenSize;
    const char *token;
    const char *cleanToken;
    const char delim = ',';
    int32_t* storeBuf = (int32_t*)metadataCache;
    std::vector<std::string> tokens;
    getTokens(src, delim, tokens);

    for (size_t i = 0; i < tokens.size(); i++) {
        token = tokens.at(i).c_str();
        /* ignore any spaces in front of the string */
        blanks = strspn(token," ");
        cleanToken = token + blanks;
        /**
         * Parse the token without blanks.
         * TODO: Add support for simple wildcard to allow things like
         * android.request.*
         */
        tokenSize = STRLEN_S(cleanToken);
        for (unsigned int i = 0; i< tableSize; i++) {
            if (strncmp(cleanToken, metadataNames[i].name, tokenSize) == 0) {
                storeBuf[count] = metadataNames[i].value;
                count++;
            }
        }
        if (count >= maxCount) {
            LOGW("Too many keys found (%d)- ignoring the rest", count);
            /* if this happens then we should increase the size of the
             * mMetadataCache
             */
            break;
        }
    }
    return count;
}

/**
 * Parses the string with the avaialble input-output formats map
 * a format map is made of 3 elements
 * - Input Format
 * - Number output formats it can be converted in to
 * - List of the output formats.
 * we parse the string in 3 steps
 * example of valid input-output formats map is:
 * RAW_OPAQUE,3,BLOB,IMPLEMENTATION_DEFINED,YCbCr_420_888
 *
 * \param src: string to be parsed
 * \param tagInfo: descriptor of the static metadata. this is the entry from the
 * table defined in the autogenerated code
 * \param metadataCacheSize:the upper limit size of the dest buffer
 * \param metadataCache: the dest buffer to store the medatada after persed
 *
 * \return number of int32 entries to be stored
 */
int CameraProfiles::parseAvailableInputOutputFormatsMap(const char *src,
        const metadata_tag_t *tagInfo,
        std::vector<MetaValueRefTable> refTables,
        int metadataCacheSize,
        int64_t* metadataCache)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    int count = 0;  // entry count
    int maxCount = metadataCacheSize/camera_metadata_type_size[tagInfo->type];
    int ret;
    char * endPtr = nullptr;
    int parseStep = 1;
    int32_t *i32;
    int numOutputFormats = 0;

    const metadata_value_t * activeTable;
    int activeTableLen = 0;

    void *storeBuf = metadataCache;
    void *next;

    if (refTables.size() < 1) {
        LOGE("incomplete reference table :%zu", refTables.size());
        return count;
    }

    do {
        endPtr = (char *) strchr(src, ',');
        if (endPtr)
            *endPtr = 0;

        if (parseStep == 1) {  // Step 1 parse the input format
            if (STRLEN_S(src) == 0) break;
            // detect empty string. It means we are done, so get out of the loop
            activeTable = refTables[0].table;
            activeTableLen = refTables[0].tableSize;
            ret = convertEnum(storeBuf, src, tagInfo->type, activeTable,
                              activeTableLen, &next);
            if (ret == 1) {
                count++;
                storeBuf = next;
            } else {
                LOGE("Malformed enum in format map %s", src);
                break;
            }
        } else if (parseStep == 2) {  // Step 2: Parse the num of output formats
            i32 = reinterpret_cast<int32_t*>(storeBuf);
            i32[0] = strtol(src, &endPtr, 10);
            numOutputFormats = i32[0];
            count += 1;
            storeBuf = reinterpret_cast<void*>(&i32[1]);
            LOGD("Num of output formats = %d", i32[0]);

        } else {  // Step3 parse the output formats
            activeTable = refTables[0].table;
            activeTableLen =  refTables[0].tableSize;
            for (int i = 0; i < numOutputFormats; i++) {
                ret = convertEnum(storeBuf, src, tagInfo->type, activeTable,
                                  activeTableLen, &next);
                if (ret == 1) {
                    count += 1;
                    if (endPtr == nullptr) return count;
                    src = endPtr + 1;
                    storeBuf = next;
                } else {
                    LOGE("Malformed enum in format map %s", src);
                    break;
                }
                if (i < numOutputFormats - 1) {
                    endPtr = (char *) strchr(src, ',');
                    if (endPtr)
                        *endPtr = 0;
                }
            }
        }

        if (endPtr) {
            src = endPtr + 1;
            src = skipWhiteSpace(src);
            parseStep++;
            /* parsing steps go from 1 to 3 */
            if (parseStep == 4) {
                parseStep = 1;
            }
        }
    } while (count < maxCount && endPtr);

    if (endPtr != nullptr) {
        LOGW("Formats Map string too long for parser");
    }

    return count;
}


int CameraProfiles::parseSizes(const char *src,
                               const metadata_tag_t *tagInfo,
                               int metadataCacheSize,
                               int64_t* metadataCache)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int entriesFound = 0;

    entriesFound = parseData(src, tagInfo, metadataCacheSize, metadataCache);

    if (entriesFound % 2) {
        LOGE("Odd number of entries (%d), resolutions should have an even "
              "number of entries", entriesFound);
        entriesFound -= 1; //make it even Ignore the last one
    }
    return entriesFound;
}

int CameraProfiles::parseImageFormats(const char *src,
                                      const metadata_tag_t *tagInfo,
                                      int metadataCacheSize,
                                      int64_t *metadataCache)
{
    /**
     * DEPRECATED in V 3.2: TODO: add warning and extra checks
     */
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int entriesFound = 0;

    entriesFound = parseEnum(src, tagInfo, metadataCacheSize, metadataCache);

    return entriesFound;
}

int CameraProfiles::parseRectangle(const char *src,
                                   const metadata_tag_t *tagInfo,
                                   int metadataCacheSize,
                                   int64_t *metadataCache)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int entriesFound = 0;
    entriesFound = parseData(src, tagInfo, metadataCacheSize, metadataCache);

    if (entriesFound % 4) {
        LOGE("incorrect number of entries (%d), rectangles have 4 values",
                    entriesFound);
        entriesFound -= entriesFound % 4; //round to multiple of 4
    }

    return entriesFound;
}
int CameraProfiles::parseBlackLevelPattern(const char *src,
                                           const metadata_tag_t *tagInfo,
                                           int metadataCacheSize,
                                           int64_t *metadataCache)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int entriesFound = 0;
    entriesFound = parseData(src, tagInfo, metadataCacheSize, metadataCache);
    if (entriesFound % 4) {
        LOGE("incorrect number of entries (%d), black level pattern have 4 values",
                    entriesFound);
        entriesFound -= entriesFound % 4; //round to multiple of 4
    }
    return entriesFound;
}

int CameraProfiles::parseStreamConfigDuration(const char *src,
        const metadata_tag_t *tagInfo,
        std::vector<MetaValueRefTable> refTables,
        int metadataCacheSize,
        int64_t *metadataCache)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int count = 0;  // entry count
    int maxCount = metadataCacheSize/camera_metadata_type_size[tagInfo->type];
    int ret;
    char * endPtr = nullptr;
    int parseStep = 1;
    int64_t *i64;

    const metadata_value_t * activeTable;
    int activeTableLen = 0;

    void *storeBuf = metadataCache;
    void *next;

    if (refTables.size() < 1) {
        LOGE("incomplete reference table :%zu", refTables.size());
        return count;
    }

    do {
        endPtr = (char *) strchr(src, ',');
        if (endPtr)
            *endPtr = 0;

        if (parseStep == 1) {  // Step 1 parse the format
            if (STRLEN_S(src) == 0) break;
            // detect empty string. It means we are done, so get out of the loop
            activeTable = refTables[0].table;
            activeTableLen = refTables[0].tableSize;
            ret = convertEnum(storeBuf, src, tagInfo->type, activeTable,
                              activeTableLen, &next);
            if (ret == 1) {
                count++;
                storeBuf = next;
            } else {
                LOGE("Malformed enum in stream configuration duration %s", src);
                break;
            }

        } else if (parseStep == 2) {  // Step 2: Parse the resolution
            i64 = reinterpret_cast<int64_t*>(storeBuf);
            i64[0] = strtol(src, &endPtr, 10);
            if (endPtr == nullptr || *endPtr != 'x') {
                LOGE("Malformed resolution in stream duration configuration");
                break;
            }
            src = endPtr + 1;
            i64[1] = strtol(src, &endPtr, 10);
            storeBuf = reinterpret_cast<void*>(&i64[2]);
            count += 2;
            LOGI("  - %" PRId64 "x%" PRId64 " -", i64[0], i64[1]);

        } else {  // Step3 parse the duration

            i64 = reinterpret_cast<int64_t*>(storeBuf);
            if (endPtr)
                i64[0] = strtol(src, &endPtr, 10);
            else
                i64[0] = strtol(src, nullptr, 10); // Do not update endPtr
            storeBuf = reinterpret_cast<void*>(&i64[1]);
            count += 1;
            LOGI("  - %" PRId64 " ns -", i64[0]);
        }

        if (endPtr) {
            src = endPtr + 1;
            src = skipWhiteSpace(src);
            parseStep++;
            /* parsing steps go from 1 to 3 */
            if (parseStep == 4) {
                parseStep = 1;
                LOGI("Stream Configuration Duration found");
            }
        }
    } while (count < maxCount && endPtr);

    if (endPtr != nullptr) {
        LOGW("Stream configuration duration string too long for parser");
    }
    /**
     * Total number of entries per stream configuration is 4
     * - one for the format
     * - two for the resolution
     * - one for the duration
     * The total entry count should be multiple of 4
     */
    if (count % 4) {
        LOGE("Malformed string for stream config duration."
                " ignoring last %d entries", count % 4);
        count -= count % 4;
    }
    return count;
}
/**
 *
 * Checks whether the sensor named in a profile is present in the list of
 * runtime detected sensors.
 * The result of this method helps to decide whether to use a particular profile
 * from the XML file.
 *
 * \param[in] detectedSensors vector with the description of the runtime
 *                            detected sensors.
 * \param[in] profileName name of the sensor present in the XML config file.
 * \param[in] cameraId camera Id for the sensor name in the XML.
 * \param[in] moduleId module Id for the sensor name in the XML.
 *
 * \return true if the sensor named in the profile is available in HW.
 */
bool CameraProfiles::isSensorPresent(std::vector<SensorDriverDescriptor> &detectedSensors,
                                     const char *profileName, int cameraId,
                                     const char *moduleId) const
{
    for (unsigned int i = 0; i < detectedSensors.size(); i++) {
#if 0
        /*
         * Logic for legacy platforms with only 1-2 sensors.
         */
        if ((detectedSensors[i].mIspPort == PRIMARY && cameraId == 0) ||
            (detectedSensors[i].mIspPort == SECONDARY && cameraId == 1) ||
            (detectedSensors[i].mIspPort == UNKNOWN_PORT)) {
            if (detectedSensors[i].mSensorName == profileName) {
                LOGI("@%s: mUseEntry is true, mSensorIndex = %d, name = %s",
                        __FUNCTION__, cameraId, profileName);
                return true;
            }
        }
#endif
        /*
         * Logic for new platforms that support more than 2 sensors.
         * To uniquely match an XML profile to a sensor present in HW we will
         * use 2 pieces of information:
         * - sensor name
         * - module id
         */
        if (detectedSensors[i].mSensorDevType == SENSOR_DEVICE_MC) {
           if (detectedSensors[i].mSensorName == profileName &&
               strcmp(detectedSensors[i].mModuleIndexStr.c_str(), moduleId) == 0) {
               LOGI("@%s: mUseEntry is true, mSensorIndex = %d, name = %s module_id = %s",
                       __FUNCTION__, cameraId, profileName, moduleId);
               return true;
           }
       }
    }

    return false;
}
/**
 * This function will check which field that the parser parses to.
 *
 * The field is set to 5 types.
 * FIELD_INVALID FIELD_SENSOR_COMMON FIELD_SENSOR_ANDROID_METADATA FIELD_SENSOR_VENDOR_METADATA and FIELD_COMMON
 *
 * \param name: the element's name.
 * \param atts: the element's attribute.
 */
void CameraProfiles::checkField(CameraProfiles *profiles,
                                const char *name,
                                const char **atts)
{
    if (!strcmp(name, "Profiles")) {
        mXmlSensorIndex = atoi(atts[1]);
        if (mXmlSensorIndex > MAX_CAMERAS) {
            LOGE("ERROR: bad camera id %d!", mSensorIndex);
            return;
        }

        profiles->mUseEntry = false;
        int attIndex = 2;
        if (atts[attIndex]) {
            if (strcmp(atts[attIndex], "name") == 0) {
                profiles->mUseEntry = isSensorPresent(profiles->mSensorNames,
                                                      atts[attIndex + 1],
                                                      mSensorIndex + 1,
                                                      atts[attIndex + 3]);
                if (profiles->mUseEntry) {
                    mSensorIndex++;
                    LOGI("@%s: mSensorIndex = %d, name = %s moduleId = %s, mSensorNames.size():%zu",
                         __FUNCTION__, mSensorIndex, atts[attIndex + 1],
                         atts[attIndex + 3], profiles->mSensorNames.size());
                    mCameraIdToSensorName.insert(make_pair(mSensorIndex, std::string(atts[attIndex + 1])));
                }
            } else {
                LOGE("unknown attribute atts[%d] = %s", attIndex, atts[attIndex]);
            }
        } else {
            // for platforms that don't have the name field in the camera profiles.
            profiles->mUseEntry = true;
            mSensorIndex++;
        }

        if (profiles->mUseEntry
                && mSensorIndex >= mStaticMeta.size()
                && mStaticMeta.size() < profiles->mSensorNames.size())
            addCamera(mSensorIndex);
    } else if (strcmp(name, "Supported_hardware") == 0) {
        mCurrentDataField = FIELD_SUPPORTED_HARDWARE;
        mItemsCount = -1;
    } else if (strcmp(name, "Android_metadata") == 0) {
        mCurrentDataField = FIELD_ANDROID_STATIC_METADATA;
        mItemsCount = -1;
    } else if (strcmp(name, "Common") == 0) {
        mCurrentDataField = FIELD_COMMON;
        mItemsCount = -1;
    }

    LOGI("@%s: name:%s, field %d", __FUNCTION__, name, mCurrentDataField);
    return;
}

void CameraProfiles::handleSupportedHardware(const char *name, const char **atts)
{
    LOGI("@%s, type:%s", __FUNCTION__, name);
    if (strcmp(atts[0], "value") != 0) {
        LOGE("name:%s, atts[0]:%s, xml format wrong", name, atts[0]);
        return;
    }

    if (strcmp(name, "hwType") == 0) {
        CameraInfo *info = nullptr;
        mCameraInfoPool.acquireItem(&info);
        if (info != nullptr) {
            info->parser = nullptr;
            info->hwType = atts[1];
            info->xmlCameraId = mXmlSensorIndex;
            mCameraIdToCameraInfo.insert(std::make_pair(mSensorIndex, info));
            LOGI("Add sensor: %s to mCameraIdToCameraInfo with key: %d", name, mSensorIndex);
        } else {
            LOGE("Failed to get camera info for sensor index %d", mSensorIndex);
        }
    } else {
        LOGE("Unhandled xml attribute in Supported_hardware");
    }
}

/**
 * This function will handle all the common related elements.
 *
 * It will be called in the function startElement
 *
 * \param name: the element's name.
 * \param atts: the element's attribute.
 */
void CameraProfiles::handleCommon(const char *name, const char **atts)
{
    LOGI("@%s, name:%s, atts[0]:%s", __FUNCTION__, name, atts[0]);

    if (strcmp(atts[0], "value") != 0) {
        LOGE("name:%s, atts[0]:%s, xml format wrong", name, atts[0]);
        return;
    }
}

bool CameraProfiles::validateStaticMetadata(const char *name, const char **atts)
{
    /**
     * string validation
     */
    size_t nameSize = strnlen(name, MAX_METADATA_NAME_LENTGTH);
    size_t attrNameSize = strnlen(atts[0], MAX_METADATA_ATTRIBUTE_NAME_LENTGTH);
    size_t attrValueSize = strnlen(atts[1], MAX_METADATA_ATTRIBUTE_VALUE_LENTGTH);
    if ((attrValueSize == MAX_METADATA_ATTRIBUTE_VALUE_LENTGTH) ||
        (attrNameSize == MAX_METADATA_ATTRIBUTE_NAME_LENTGTH) ||
        (nameSize == MAX_METADATA_NAME_LENTGTH)) {
        LOGW("Warning XML strings too long ignoring this tag %s", name);
        return false;
    }

    if ((strncmp(atts[0], "value", attrNameSize) != 0) ||
            (attrValueSize == 0)) {
        LOGE("Check atts failed! name: %s, atts[0]: \"%s\", atts[1]: \"%s\", the format of xml is wrong!", name, atts[0], atts[1]);
        return false;
    }

    return true;
}

const metadata_tag_t *CameraProfiles::findTagInfo(const char *name,
                                                  const metadata_tag_t *tagsTable,
                                                  unsigned int size)
{
    size_t nameSize = strnlen(name, MAX_METADATA_NAME_LENTGTH);
    unsigned int index = 0;
    const metadata_tag_t *tagInfo = nullptr;

    for (index = 0; index < size; index++) {
        if (!strncmp(name, tagsTable[index].name, nameSize)) {
            tagInfo = &tagsTable[index];
            break;
        }
    }
    if (index >= size) {
        LOGW("Parser does not support tag %s! - ignoring", name);
    }

    return tagInfo;
}

int CameraProfiles::parseGenericTypes(const char *src,
                                      const metadata_tag_t *tagInfo,
                                      int metadataCacheSize,
                                      int64_t *metadataCache)
{
    int count = 0;
    switch (tagInfo->arrayTypedef) {
            case BOOLEAN:
            case ENUM_LIST:
                count = parseEnum(src, tagInfo, metadataCacheSize, metadataCache);
                break;
            case RANGE_INT:
            case RANGE_LONG:
                count = parseData(src, tagInfo, metadataCacheSize, metadataCache);
                break;
            case SIZE_F:
            case SIZE:
                count = parseSizes(src, tagInfo, metadataCacheSize, metadataCache);
                break;
            case RECTANGLE:
                count = parseRectangle(src, tagInfo, metadataCacheSize, metadataCache);
                break;
            case IMAGE_FORMAT:
                count = parseImageFormats(src, tagInfo, metadataCacheSize, metadataCache);
                break;
            case BLACK_LEVEL_PATTERN:
                count = parseBlackLevelPattern(src, tagInfo, metadataCacheSize, metadataCache);
                break;
            case TYPEDEF_NONE: /* Single values*/
                if (tagInfo->enumTable) {
                    count = parseEnum(src, tagInfo, metadataCacheSize, metadataCache);
                } else {
                    count = parseData(src, tagInfo, metadataCacheSize, metadataCache);
                }
                break;
            default:
                LOGW("Unsupported typedef %s", tagInfo->name);
                break;
            }

    return count;
}

/**
 * the callback function of the libexpat for handling of one element start
 *
 * When it comes to the start of one element. This function will be called.
 *
 * \param userData: the pointer we set by the function XML_SetUserData.
 * \param name: the element's name.
 */
void CameraProfiles::startElement(void *userData, const char *name, const char **atts)
{
    CameraProfiles *profiles = (CameraProfiles *)userData;

    if (profiles->mCurrentDataField == FIELD_INVALID) {
        profiles->checkField(profiles, name, atts);
        return;
    }

    if (profiles->mUseEntry)
        LOGD("@%s: name:%s, for sensor %d", __FUNCTION__, name, profiles->mSensorIndex);

    profiles->mItemsCount++;

    switch (profiles->mCurrentDataField) {
        case FIELD_SUPPORTED_HARDWARE:
            if (profiles->mUseEntry)
                profiles->handleSupportedHardware(name, atts);
            break;
        case FIELD_ANDROID_STATIC_METADATA:
            if (profiles->mUseEntry)
                profiles->handleAndroidStaticMetadata(name, atts);

            break;
        case FIELD_COMMON:
            if (profiles->mStaticMeta.size() > 0)
                profiles->handleCommon(name, atts);

            break;
        default:
            LOGE("line:%d, go to default handling", __LINE__);
            break;
    }
}

/**
 * the callback function of the libexpat for handling of one element end
 *
 * When it comes to the end of one element. This function will be called.
 *
 * \param userData: the pointer we set by the function XML_SetUserData.
 * \param name: the element's name.
 */
void CameraProfiles::endElement(void *userData, const char *name)
{
    CameraProfiles *profiles = (CameraProfiles *)userData;
    if (!strcmp(name, "Profiles")) {
        profiles->mCurrentDataField = FIELD_INVALID;
        if (profiles->mUseEntry)
            profiles->mProfileEnd[profiles->mSensorIndex] = true;
    } else if (!strcmp(name, "Supported_hardware")
             || !strcmp(name, "Android_metadata")
             || !strcmp(name, "Common")) {
        profiles->mCurrentDataField = FIELD_INVALID;
        profiles->mItemsCount = -1;
    }
    return;
}

/**
 * Get camera configuration from xml file
 *
 * The function will read the xml configuration file firstly.
 * Then it will parse out the camera settings.
 * The camera setting is stored inside this CameraProfiles class.
 *
 */
void CameraProfiles::getDataFromXmlFile(void)
{
    int done;
    void *pBuf = nullptr;
    FILE *fp = nullptr;
    camera_metadata_t *currentMeta = nullptr;
    status_t res;
    int tag = ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS;

    fp = ::fopen(mXmlConfigName.c_str(), "r");
    if (nullptr == fp) {
        LOGE("line:%d, fp is nullptr", __LINE__);
        return;
    }

    XML_Parser parser = ::XML_ParserCreate(nullptr);
    if (nullptr == parser) {
        LOGE("line:%d, parser is nullptr", __LINE__);
        goto exit;
    }
    ::XML_SetUserData(parser, this);
    ::XML_SetElementHandler(parser, startElement, endElement);

    pBuf = ::operator new(BUFFERSIZE);
    mMetadataCache = new int64_t[METADATASIZE];

    do {
        int len = (int)::fread(pBuf, 1, BUFFERSIZE, fp);
        if (!len) {
            if (ferror(fp)) {
                clearerr(fp);
                goto exit;
            }
        }
        done = len < BUFFERSIZE;
        if (XML_Parse(parser, (const char *)pBuf, len, done) == XML_STATUS_ERROR) {
            LOGE("line:%d, XML_Parse error", __LINE__);
            goto exit;
        }
    } while (!done);

    if (mStaticMeta.size() > 0) {
        for (unsigned int i = 0; i < mStaticMeta.size(); i++) {
            sort_camera_metadata(mStaticMeta.at(i));
            currentMeta = mStaticMeta.at(i);
            if (currentMeta == nullptr) {
                LOGE("can't get the static metadata");
                goto exit;
            }
            // update REQUEST_AVAILABLE_CHARACTERISTICS_KEYS
            int *keys = mCharacteristicsKeys[i].data();
            res = MetadataHelper::updateMetadata(currentMeta, tag, keys, mCharacteristicsKeys[i].size());
            if (res != OK)
                LOGE("call add/update_camera_metadata_entry fail for request.availableCharacteristicsKeys");
        }
    }
exit:
    if (parser)
        ::XML_ParserFree(parser);

    ::operator delete(pBuf);

    if (mMetadataCache) {
        delete [] mMetadataCache;
        mMetadataCache = nullptr;
    }
    if (fp)
        ::fclose(fp);
}

CameraHwType CameraProfiles::getCameraHwforId(int cameraId)
{
    LOGI("@%s cameraId: %d", __FUNCTION__, cameraId);

    std::map<int, CameraInfo*>::iterator it =
                            mCameraIdToCameraInfo.find(cameraId);
    if (it == mCameraIdToCameraInfo.end()) {
        LOGE("Camera id not found, BUG, this should not happen!!mSensorIndex = %d", cameraId);
        return SUPPORTED_HW_UNKNOWN;
    }

    CameraInfo *info = it->second;
    std::string hwType = info->hwType;

    if (hwType == "SUPPORTED_HW_RKISP1") {
        return SUPPORTED_HW_RKISP1;
    } else if (hwType == "SUPPORTED_HW_RKISP2") {
        return SUPPORTED_HW_RKISP2;
    } else
        LOGE("ERROR: Camera HW type wrong in xml");
        return SUPPORTED_HW_UNKNOWN;
}

void CameraProfiles::dumpSupportedHWSection(int cameraId) {
    LOGD("@%s", __FUNCTION__);
    std::map<int, CameraInfo*>::iterator it =
                            mCameraIdToCameraInfo.find(cameraId);
    if (it == mCameraIdToCameraInfo.end()) {
        LOGE("Camera id not found, BUG, this should not happen!!mSensorIndex = %d", cameraId);
        return;
    }

    CameraInfo *info = it->second;
    LOGD("element name hwType element value = %s", info->hwType.c_str());
}

void CameraProfiles::dumpStaticMetadataSection(int cameraId)
{
    LOGD("@%s", __FUNCTION__);
    if (mStaticMeta.size() > 0)
        MetadataHelper::dumpMetadata(mStaticMeta[cameraId]);
    else {
        LOGE("Camera isn't added, unable to get the static metadata");
    }
}

void CameraProfiles::dumpCommonSection()
{
    LOGD("@%s", __FUNCTION__);
    LOGD("element name: boardName, element value = %s", mCameraCommon->mBoardName.c_str());
    LOGD("element name: productName, element value = %s", mCameraCommon->mProductName.c_str());
    LOGD("element name: manufacturerName, element value = %s", mCameraCommon->mManufacturerName.c_str());
    LOGD("element name: mSupportDualVideo, element value = %d", mCameraCommon-> mSupportDualVideo);
    LOGD("element name: supportExtendedMakernote, element value = %d", mCameraCommon->mSupportExtendedMakernote);
}

// To be modified when new elements or sections are added
// Use LOGD for traces to be visible
void CameraProfiles::dump()
{
    LOGD("===========================@%s======================", __FUNCTION__);
    for (unsigned int i = 0; i <= mSensorIndex; i++) {
        dumpSupportedHWSection(i);
        dumpStaticMetadataSection(i);
    }
    dumpCommonSection();
    LOGD("===========================end======================");
}

} NAMESPACE_DECLARATION_END
