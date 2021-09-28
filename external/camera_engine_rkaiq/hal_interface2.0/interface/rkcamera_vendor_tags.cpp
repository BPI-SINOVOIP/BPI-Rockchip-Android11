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

#define LOG_TAG "rkcamera3VendorTags"

// Camera dependencies
#include "rkcamera_vendor_tags.h"
#include <base/xcam_log.h>

uint32_t rkcamera3_ext3_section_bounds[RKCAMERA3_EXT_SECTION_COUNT][2] = {
    { (uint32_t) RKCAMERA3_PRIVATEDATA_START,       (uint32_t) RKCAMERA3_PRIVATEDATA_END },
    { (uint32_t) RK_NR_FEATURE_START,               (uint32_t) RK_NR_FEATURE_END },
    { (uint32_t) RK_CONTROL_AIQ_START,               (uint32_t) RK_CONTROL_AIQ_END },
    { (uint32_t) RK_MEANLUMA_START,               (uint32_t) RK_MEANLUMA_END}
};

typedef struct vendor_tag_info {
    const char *tag_name;
    uint8_t     tag_type;
} vendor_tag_info_t;

const char *rkcamera3_ext_section_names[RKCAMERA3_EXT_SECTION_COUNT] = {
    "org.codeaurora.rkcamera3.privatedata",
    "com.rockchip.nrfeature",
    "com.rockchip.control.aiq",
    "com.rockchip.luma",
};

vendor_tag_info_t rkcamera3_privatedata[RKCAMERA3_PRIVATEDATA_END - RKCAMERA3_PRIVATEDATA_START] = {
    { "privatedata_effective_driver_frame_id", TYPE_INT64 },
    { "privatedata_frame_sof_timestamp", TYPE_INT64 },
    { "privatedata_stillcap_sync_needed", TYPE_BYTE },
    { "privatedata_stillcap_sync_cmd", TYPE_BYTE },
    { "privatedata_stillcap_isp_param", TYPE_BYTE }
};

vendor_tag_info_t rk_nr_feature_3dnr[RK_NR_FEATURE_END -
        RK_NR_FEATURE_START] = {
    { "3dnrmode",   TYPE_BYTE }
};
vendor_tag_info_t rk_control_aiq[RK_CONTROL_AIQ_END -
        RK_CONTROL_AIQ_START] = {
    { "brightness",   TYPE_BYTE },
    { "contrast",   TYPE_BYTE },
    { "saturation",   TYPE_BYTE }
};

vendor_tag_info_t rk_meanluma[RK_MEANLUMA_END -
        RK_MEANLUMA_START] = {
    { "meanluma",   TYPE_FLOAT }
};

vendor_tag_info_t *rkcamera3_tag_info[RKCAMERA3_EXT_SECTION_COUNT] = {
    rkcamera3_privatedata,
    rk_nr_feature_3dnr,
    rk_control_aiq,
    rk_meanluma
};

uint32_t rkcamera3_all_tags[] = {
    // rkcamera3_PRIVATEDATA
    (uint32_t)RKCAMERA3_PRIVATEDATA_EFFECTIVE_DRIVER_FRAME_ID,
    (uint32_t)RKCAMERA3_PRIVATEDATA_FRAME_SOF_TIMESTAMP,
    (uint32_t)RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_NEEDED,
    (uint32_t)RKCAMERA3_PRIVATEDATA_STILLCAP_SYNC_CMD,
    (uint32_t)RKCAMERA3_PRIVATEDATA_STILLCAP_ISP_PARAM
};

const vendor_tag_ops_t* RkCamera3VendorTags::Ops = NULL;

/*===========================================================================
 * FUNCTION   : get_vendor_tag_ops
 *
 * DESCRIPTION: Get the metadata vendor tag function pointers
 *
 * PARAMETERS :
 *    @ops   : function pointer table to be filled by HAL
 *
 *
 * RETURN     : NONE
 *==========================================================================*/
void RkCamera3VendorTags::get_vendor_tag_ops(
                                vendor_tag_ops_t* ops)
{
    LOGD("E");

    Ops = ops;

    ops->get_tag_count = get_tag_count;
    ops->get_all_tags = get_all_tags;
    ops->get_section_name = get_section_name;
    ops->get_tag_name = get_tag_name;
    ops->get_tag_type = get_tag_type;
    ops->reserved[0] = NULL;

    LOGD("X");
    return;
}

/*===========================================================================
 * FUNCTION   : get_tag_count
 *
 * DESCRIPTION: Get number of vendor tags supported
 *
 * PARAMETERS :
 *    @ops   :  Vendor tag ops data structure
 *
 *
 * RETURN     : Number of vendor tags supported
 *==========================================================================*/

int RkCamera3VendorTags::get_tag_count(
                const vendor_tag_ops_t * ops)
{
    int section;
    unsigned int start, end;
    int count = 0;

    if (ops != Ops) return -1;

    for (section = 0; section < RKCAMERA3_EXT_SECTION_COUNT; section++) {
        start = rkcamera3_ext3_section_bounds[section][0];
        end = rkcamera3_ext3_section_bounds[section][1];
        count += end - start;
        LOGD("section:%d,count:%d",section,end - start);
    }

    LOGD("count is %d", count);
    return (int)count;
}

/*===========================================================================
 * FUNCTION   : get_all_tags
 *
 * DESCRIPTION: Fill array with all supported vendor tags
 *
 * PARAMETERS :
 *    @ops      :  Vendor tag ops data structure
 *    @tag_array:  array of metadata tags
 *
 * RETURN     : Success: the section name of the specific tag
 *              Failure: NULL
 *==========================================================================*/
void RkCamera3VendorTags::get_all_tags(
                const vendor_tag_ops_t * ops,
                uint32_t *g_array)
{
    int section;
    unsigned int start, end, tag;

    if (ops != Ops || g_array == NULL) return;
    for (section = 0; section < RKCAMERA3_EXT_SECTION_COUNT; section++) {
        start = rkcamera3_ext3_section_bounds[section][0];
        end = rkcamera3_ext3_section_bounds[section][1];
        LOGD("section:%d,start:%d,end:%d",section,start,end);
        for (tag = start; tag < end; tag++) {
            *g_array++ = tag;
            LOGD("g_array[%d] is %d",tag, g_array[tag]);
        }
    }
}

/*===========================================================================
 * FUNCTION   : get_section_name
 *
 * DESCRIPTION: Get section name for vendor tag
 *
 * PARAMETERS :
 *    @ops   :  Vendor tag ops structure
 *    @tag   :  Vendor specific tag
 *
 *
 * RETURN     : Success: the section name of the specific tag
 *              Failure: NULL
 *==========================================================================*/

const char* RkCamera3VendorTags::get_section_name(
                const vendor_tag_ops_t * ops,
                uint32_t tag)
{
    LOGD("E");
    if (ops != Ops)
        return NULL;

    const char *ret;
    int tag_section = (tag >> 16) - VENDOR_SECTION;
    if (tag_section < 0 || tag_section >= RKCAMERA3_EXT_SECTION_COUNT)
        ret = NULL;
    else
        ret = rkcamera3_ext_section_names[tag_section];

    if (ret)
        LOGD("section_name[%d] is %s", tag, ret);
    LOGD("X");
    return ret;
}

/*===========================================================================
 * FUNCTION   : get_tag_name
 *
 * DESCRIPTION: Get name of a vendor specific tag
 *
 * PARAMETERS :
 *    @tag   :  Vendor specific tag
 *
 *
 * RETURN     : Success: the name of the specific tag
 *              Failure: NULL
 *==========================================================================*/
const char* RkCamera3VendorTags::get_tag_name(
                const vendor_tag_ops_t * ops,
                uint32_t tag)
{
    LOGD("E");
    const char *ret;
    int tag_index = tag & 0xFFFF;
    int tag_section = (tag >> 16) - VENDOR_SECTION;
    if (ops != Ops) {
        ret = NULL;
        goto done;
    }
    if (tag_section < 0
            || tag_section >= RKCAMERA3_EXT_SECTION_COUNT
            || tag >= rkcamera3_ext3_section_bounds[tag_section][1])
        ret = NULL;
    else
        ret = rkcamera3_tag_info[tag_section][tag_index].tag_name;

    if (ret)
        LOGD("tag name for tag %d is %s", tag, ret);

done:
    LOGD("X");
    return ret;
}

/*===========================================================================
 * FUNCTION   : get_tag_type
 *
 * DESCRIPTION: Get type of a vendor specific tag
 *
 * PARAMETERS :
 *    @tag   :  Vendor specific tag
 *
 *
 * RETURN     : Success: the type of the specific tag
 *              Failure: -1
 *==========================================================================*/
int RkCamera3VendorTags::get_tag_type(
                const vendor_tag_ops_t *ops,
                uint32_t tag)
{
    //LOGD("E");
    int ret;
    int tag_section = (tag >> 16) - VENDOR_SECTION;
    int tag_index = tag & 0xFFFF;

    if (ops != Ops) {
        ret = -1;
        goto done;
    }

    if (tag_section < 0
            || tag_section >= RKCAMERA3_EXT_SECTION_COUNT
            || tag >= rkcamera3_ext3_section_bounds[tag_section][1])
        ret = -1;
    else
        ret = rkcamera3_tag_info[tag_section][tag_index].tag_type;

    LOGD("tag type for tag %u is %d", tag, ret);
    //LOGD("X");
done:
    return ret;
}
