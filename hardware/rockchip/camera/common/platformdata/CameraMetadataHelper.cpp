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

#define LOG_TAG "MetadataHelper"
#include "CameraMetadataHelper.h"
#include "LogHelper.h"
#include <string>
#include <sstream>

NAMESPACE_DECLARATION {
namespace MetadataHelper {

#define CHECK_AND_GET(TYPE, UNION_FIELD) \
    camera_metadata_ro_entry entry = metadata.find(tag); \
    if (entry.count == 0) {  \
        LOGI("tag %s.%s is not set.", \
            get_camera_metadata_section_name(tag), \
            get_camera_metadata_tag_name(tag)); \
        return false; \
    } else if (count > 0 && entry.count != (size_t)count) { \
        LOGE("Bad count %d for tag %s.%s! Should be %zu", \
            count, \
            get_camera_metadata_section_name(tag), \
            get_camera_metadata_tag_name(tag), \
            entry.count); \
        return false; \
    } else if (entry.type != (TYPE)) { \
        LOGE("Bad type %d for tag %s.%s! Should be %d", \
            TYPE, \
            get_camera_metadata_section_name(tag), \
            get_camera_metadata_tag_name(tag), \
            entry.type); \
        return false; \
    }\
    value = *entry.data.UNION_FIELD; \
    return true;

bool getMetadataValue(const CameraMetadata &metadata, uint32_t tag, uint8_t & value, int count)
{
    CHECK_AND_GET(TYPE_BYTE, u8);
}
bool getMetadataValue(const CameraMetadata &metadata, uint32_t tag, int32_t & value, int count)
{
    CHECK_AND_GET(TYPE_INT32, i32);
}
bool getMetadataValue(const CameraMetadata &metadata, uint32_t tag, int64_t & value, int count)
{
    CHECK_AND_GET(TYPE_INT64, i64);
}
bool getMetadataValue(const CameraMetadata &metadata, uint32_t tag, float & value, int count)
{
    CHECK_AND_GET(TYPE_FLOAT, f);
}
bool getMetadataValue(const CameraMetadata &metadata, uint32_t tag, double & value, int count)
{
    CHECK_AND_GET(TYPE_DOUBLE, d);
}

const void * getMetadataValues(const CameraMetadata &metadata, uint32_t tag, int type, int * count)
{
    camera_metadata_ro_entry entry = metadata.find(tag);

    if (count)
        *count = entry.count;

    if (entry.count == 0) {
        LOGI("Tag %s.%s is not set.",
            get_camera_metadata_section_name(tag),
            get_camera_metadata_tag_name(tag));
        return nullptr;
    }

    if (entry.type != type) {
        LOGE("Bad type %d for tag %s.%s! Should be %d",
            type,
            get_camera_metadata_section_name(tag),
            get_camera_metadata_tag_name(tag),
            entry.type);
        return nullptr;
    }

    return entry.data.u8;
}

/**
 * Convenience getter for an entry. Difference to the framework version is that
 * the tag is always written to the entry, even if no entry is found.
 *
 * \param[in] metadata The camera metadata to get tag info for
 * \param[in] tag Tag of the queried metadata
 * \param[in] printError If set to \e true, Prints an error if no metadata
 *  for given tag found. Default: true.
 *
 * \return The metadata entry corresponding to \e tag.
 */
camera_metadata_ro_entry_t getMetadataEntry(const camera_metadata_t *metadata, uint32_t tag, bool printError)
{
    camera_metadata_ro_entry entry;
    CLEAR(entry);
    entry.tag = tag;
    find_camera_metadata_ro_entry(metadata, tag, &entry);

    if (printError && entry.count <= 0)
        LOGW("Metadata error, check camera3_profile. Tag %s", get_camera_metadata_tag_name(tag));

    return entry;
}

void getValueByType(const camera_metadata_ro_entry_t& setting, int idx, uint8_t* value)
{
    *value = setting.data.u8[idx];
}

void getValueByType(const camera_metadata_ro_entry_t& setting, int idx, int32_t* value)
{
    *value = setting.data.i32[idx];
}

void getValueByType(const camera_metadata_ro_entry_t& setting, int idx, float* value)
{
    *value = setting.data.f[idx];
}

void getValueByType(const camera_metadata_ro_entry_t& setting, int idx, int64_t* value)
{
    *value = setting.data.i64[idx];
}

void getValueByType(const camera_metadata_ro_entry_t& setting, int idx, double* value)
{
    *value = setting.data.d[idx];
}

const void * getMetadataValues(const camera_metadata_t * metadata, uint32_t tag, int type, int * count)
{
    status_t res = NO_ERROR;
    camera_metadata_ro_entry entry;
    res = find_camera_metadata_ro_entry(metadata, tag, &entry);
    if (res != OK) {
        LOGE("Failed to find %s.%s",
            get_camera_metadata_section_name(tag),
            get_camera_metadata_tag_name(tag));
        return nullptr;
    }
    if (entry.type != type) {
        LOGE("Bad type %d for tag %s.%s! Should be %d",
            type,
            get_camera_metadata_section_name(tag),
            get_camera_metadata_tag_name(tag),
            entry.type);
        return nullptr;
    }
    if (count)
        * count = entry.count;

    return entry.count ? entry.data.u8 : nullptr;
}

status_t updateMetadata(camera_metadata_t * metadata, uint32_t tag, const void* data, size_t data_count)
{
    status_t res = NO_ERROR;
    camera_metadata_entry_t entry;
    res = find_camera_metadata_entry(metadata, tag, &entry);
    if (res == NAME_NOT_FOUND) {
        res = add_camera_metadata_entry(metadata,
              tag, data, data_count);
    } else if (res == OK) {
        res = update_camera_metadata_entry(metadata,
            entry.index, data, data_count, nullptr);
    }
    return res;
}

void dumpMetadata(const camera_metadata_t * meta)
{
    if (!meta)
        return;

    int entryCount = get_camera_metadata_entry_count(meta);

    for (int i = 0; i < entryCount; i++) {
        camera_metadata_entry_t entry;
        if (get_camera_metadata_entry((camera_metadata_t *)meta, i, &entry)) {
            continue;
        }

        // Print tag & type
        const char *tagName, *tagSection;
        tagSection = get_camera_metadata_section_name(entry.tag);
        if (tagSection == nullptr) {
            tagSection = "unknownSection";
        }
        tagName = get_camera_metadata_tag_name(entry.tag);
        if (tagName == nullptr) {
            tagName = "unknownTag";
        }
        const char *typeName;
        if (entry.type >= NUM_TYPES) {
            typeName = "unknown";
        } else {
            typeName = camera_metadata_type_names[entry.type];
        }
        LOGD("(%d)%s.%s (%05x): %s[%zu], type: %d",
             i,
             tagSection,
             tagName,
             entry.tag,
             typeName,
             entry.count,
             entry.type);

        // Print data
        size_t j;
        const uint8_t *u8;
        const int32_t *i32;
        const float   *f;
        const int64_t *i64;
        const double  *d;
        const camera_metadata_rational_t *r;
        std::ostringstream stringStream;
        stringStream << "[";

        switch (entry.type) {
        case TYPE_BYTE:
            u8 = entry.data.u8;
            for (j = 0; j < entry.count; j++)
                stringStream << (int32_t)u8[j] << " ";
            break;
        case TYPE_INT32:
            i32 = entry.data.i32;
            for (j = 0; j < entry.count; j++)
                stringStream << " " << i32[j] << " ";
            break;
        case TYPE_FLOAT:
            f = entry.data.f;
            for (j = 0; j < entry.count; j++)
                stringStream << " " << f[j] << " ";
            break;
        case TYPE_INT64:
            i64 = entry.data.i64;
            for (j = 0; j < entry.count; j++)
                stringStream << " " << i64[j] << " ";
            break;
        case TYPE_DOUBLE:
            d = entry.data.d;
            for (j = 0; j < entry.count; j++)
                stringStream << " " << d[j] << " ";
            break;
        case TYPE_RATIONAL:
            r = entry.data.r;
            for (j = 0; j < entry.count; j++)
                stringStream << " (" << r[j].numerator << ", " << r[j].denominator << ") ";
            break;
        }
        stringStream << "]";
        std::string str = stringStream.str();
        LOGD("%s", str.c_str());
    }

}

}
} NAMESPACE_DECLARATION_END
