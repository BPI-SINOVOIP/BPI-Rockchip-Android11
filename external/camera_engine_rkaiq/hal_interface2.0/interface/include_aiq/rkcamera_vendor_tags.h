/*
 * Copyright (c) 2018, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef __RKCAMERA_VENDOR_TAGS_H__
#define __RKCAMERA_VENDOR_TAGS_H__ 

// Camera dependencies
#include <system/camera_metadata.h>
#include <system/camera_vendor_tags.h>

enum rkcamera3_ext_section {
    RKCAMERA3_PRIVATEDATA = VENDOR_SECTION,
    RK_NR_FEATURE,
    RK_CONTROL_AIQ,
    RK_MEANLUMA,
    RKCAMERA3_EXT_SECTION_END
};

const int RKCAMERA3_EXT_SECTION_COUNT = RKCAMERA3_EXT_SECTION_END - VENDOR_SECTION;

enum rkcamera3_ext_section_ranges {
    RKCAMERA3_PRIVATEDATA_START = RKCAMERA3_PRIVATEDATA << 16,
    RK_NR_FEATURE_START         = RK_NR_FEATURE << 16,
    RK_CONTROL_AIQ_START        = RK_CONTROL_AIQ << 16,
    RK_MEANLUMA_START           = RK_MEANLUMA << 16
};

enum rkcamera3_ext_tags {
    RKCAMERA3_PRIVATEDATA_EFFECTIVE_DRIVER_FRAME_ID = RKCAMERA3_PRIVATEDATA_START,
    RKCAMERA3_PRIVATEDATA_FRAME_SOF_TIMESTAMP,
    RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_NEEDED,
    RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD,
    RKCAMERA3_PRIVATEDATA_STILLCAP_ISP_PARAM,
    RKCAMERA3_PRIVATEDATA_END,
    RK_NR_FEATURE_3DNR_MODE                         = RK_NR_FEATURE_START,
    RK_NR_FEATURE_END,
    RK_CONTROL_AIQ_BRIGHTNESS                       = RK_CONTROL_AIQ_START,
    RK_CONTROL_AIQ_CONTRAST,
    RK_CONTROL_AIQ_SATURATION,
    RK_CONTROL_AIQ_END,
    RK_MEANLUMA_VALUE                              = RK_MEANLUMA_START,
    RK_MEANLUMA_END,
};

// RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD
typedef enum rkcamera3_privatemeta_enum_stillcap_sync_cmd {
    RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD_SYNCSTART = 1,
    RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD_SYNCDONE,
    RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD_SYNCEND,
} rkcamera3_privatemeta_enum_stillcap_sync_cmd_t;

// RK 3DNR
typedef enum rk_camera_metadata_enum_3dnr_mode {
    RK_NR_FEATURE_3DNR_MODE_OFF = 0,
    RK_NR_FEATURE_3DNR_MODE_ON,
} rk_camera_metadata_enum_3dnr_mode_t;

#ifdef __cplusplus
class RkCamera3VendorTags {
    public:
        static void get_vendor_tag_ops(vendor_tag_ops_t* ops);
        static int get_tag_count(const vendor_tag_ops_t *ops);
        static void get_all_tags(const vendor_tag_ops_t *ops, uint32_t *tag_array);
        static const char* get_section_name(const vendor_tag_ops_t *ops, uint32_t tag);
        static const char* get_tag_name(const vendor_tag_ops_t *ops, uint32_t tag);
        static int get_tag_type(const vendor_tag_ops_t *ops, uint32_t tag);
        static const vendor_tag_ops_t *Ops;
};
#endif

#endif /* __RKCAMERA_VENDOR_TAGS_H__ */
