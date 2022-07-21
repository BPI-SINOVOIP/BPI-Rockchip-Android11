/*
 *  Copyright (c) 2019 Rockchip Corporation
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
 *
 */

#include "CamHwIsp20.h"
#include "Isp20Evts.h"
#include "rk_isp20_hw.h"
#include "Isp20_module_dbg.h"
#include "mediactl/mediactl-priv.h"
#include <linux/v4l2-subdev.h>
#include <sys/mman.h>
#ifdef ANDROID_OS
#include <cutils/properties.h>
#endif
#include "SPStreamProcUnit.h"
#include "PdafStreamProcUnit.h"
#include "CaptureRawData.h"
#include "code_to_pixel_format.h"
#include "RkAiqCalibDbV2.h"
#include "IspParamsSplitter.h"

namespace RkCam {
std::map<std::string, SmartPtr<rk_aiq_static_info_t>> CamHwIsp20::mCamHwInfos;
std::map<std::string, SmartPtr<rk_sensor_full_info_t>> CamHwIsp20::mSensorHwInfos;
rk_aiq_isp_hw_info_t CamHwIsp20::mIspHwInfos;
rk_aiq_cif_hw_info_t CamHwIsp20::mCifHwInfos;
bool CamHwIsp20::mIsMultiIspMode = false;
uint16_t CamHwIsp20::mMultiIspExtendedPixel = 0;

CamHwIsp20::CamHwIsp20()
    : _is_exit(false)
    , _state(CAM_HW_STATE_INVALID)
    , _hdr_mode(0)
    , _ispp_module_init_ens(0)
    , _sharp_fbc_rotation(RK_AIQ_ROTATION_0)
    , _linked_to_isp(false)
{
    mNoReadBack = false;
#ifndef ANDROID_OS
    char* valueStr = getenv("normal_no_read_back");
    if (valueStr) {
        mNoReadBack = atoi(valueStr) > 0 ? true : false;
    }
#else
    char property_value[PROPERTY_VALUE_MAX] = {'\0'};

    property_get("persist.vendor.rkisp_no_read_back", property_value, "-1");
    int val = atoi(property_value);
    if (val != -1)
        mNoReadBack = atoi(property_value) > 0 ? true : false;
#endif
    xcam_mem_clear(_fec_drv_mem_ctx);
    xcam_mem_clear(_ldch_drv_mem_ctx);
    xcam_mem_clear(_cac_drv_mem_ctx);
    _fec_drv_mem_ctx.type = MEM_TYPE_FEC;
    _fec_drv_mem_ctx.ops_ctx = this;
    _fec_drv_mem_ctx.mem_info = (void*)(fec_mem_info_array);

    _ldch_drv_mem_ctx.type = MEM_TYPE_LDCH;
    _ldch_drv_mem_ctx.ops_ctx = this;
    _ldch_drv_mem_ctx.mem_info = (void*)(ldch_mem_info_array);

    _cac_drv_mem_ctx.type = MEM_TYPE_CAC;
    _cac_drv_mem_ctx.ops_ctx = this;
    _cac_drv_mem_ctx.mem_info = (void*)(cac_mem_info_array);

    xcam_mem_clear(_crop_rect);
    mParamsAssembler = new IspParamsAssembler("ISP_PARAMS_ASSEMBLER");
    mVicapIspPhyLinkSupported = false;
    mIspStremEvtTh = NULL;
    mIsGroupMode = false;
    mIsMain = false;
    _isp_stream_status = ISP_STREAM_STATUS_INVALID;
}

CamHwIsp20::~CamHwIsp20()
{}

static XCamReturn get_isp_ver(rk_aiq_isp_hw_info_t *hw_info) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    struct v4l2_capability cap;
    V4l2Device vdev(hw_info->isp_info[0].stats_path);


    ret = vdev.open();
    if (ret != XCAM_RETURN_NO_ERROR) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "failed to open dev (%s)", hw_info->isp_info[0].stats_path);
        return XCAM_RETURN_ERROR_FAILED;
    }

    if (vdev.query_cap(cap) == XCAM_RETURN_NO_ERROR) {
        char *p;
        p = strrchr((char*)cap.driver, '_');
        if (p == NULL) {
            ret = XCAM_RETURN_ERROR_FAILED;
            goto out;
        }

        if (*(p + 1) != 'v') {
            ret = XCAM_RETURN_ERROR_FAILED;
            goto out;
        }

        hw_info->hw_ver_info.isp_ver = atoi(p + 2);
        //awb/aec version?
        vdev.close();
        return XCAM_RETURN_NO_ERROR;
    } else {
        ret = XCAM_RETURN_ERROR_FAILED;
        goto out;
    }

out:
    vdev.close();
    LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get isp version failed !");
    return ret;
}

static XCamReturn get_sensor_caps(rk_sensor_full_info_t *sensor_info) {
    struct v4l2_subdev_frame_size_enum fsize_enum;
    struct v4l2_subdev_mbus_code_enum  code_enum;
    std::vector<uint32_t> formats;
    rk_frame_fmt_t frameSize;
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    V4l2SubDevice vdev(sensor_info->device_name.c_str());
    ret = vdev.open();
    if (ret != XCAM_RETURN_NO_ERROR) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "failed to open dev (%s)", sensor_info->device_name.c_str());
        return XCAM_RETURN_ERROR_FAILED;
    }
    //get module info
    struct rkmodule_inf *minfo = &(sensor_info->mod_info);
    if(vdev.io_control(RKMODULE_GET_MODULE_INFO, minfo) < 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "@%s %s: Get sensor module info failed", __FUNCTION__, sensor_info->device_name.c_str());
        return XCAM_RETURN_ERROR_FAILED;
    }
    sensor_info->len_name = std::string(minfo->base.lens);

#if 0
    memset(&code_enum, 0, sizeof(code_enum));
    while (vdev.io_control(VIDIOC_SUBDEV_ENUM_MBUS_CODE, &code_enum) == 0) {
        formats.push_back(code_enum.code);
        code_enum.index++;
    };

    //TODO: sensor driver not supported now
    for (auto it = formats.begin(); it != formats.end(); ++it) {
        fsize_enum.pad = 0;
        fsize_enum.index = 0;
        fsize_enum.code = *it;
        while (vdev.io_control(VIDIOC_SUBDEV_ENUM_FRAME_SIZE, &fsize_enum) == 0) {
            frameSize.format = (rk_aiq_format_t)fsize_enum.code;
            frameSize.width = fsize_enum.max_width;
            frameSize.height = fsize_enum.max_height;
            sensor_info->frame_size.push_back(frameSize);
            fsize_enum.index++;
        };
    }
    if(!formats.size() || !sensor_info->frame_size.size()) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "@%s %s: Enum sensor frame size failed", __FUNCTION__, sensor_info->device_name.c_str());
        ret = XCAM_RETURN_ERROR_FAILED;
    }
#endif

    struct v4l2_subdev_frame_interval_enum fie;
    memset(&fie, 0, sizeof(fie));
    while(vdev.io_control(VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fie) == 0) {
        frameSize.format = (rk_aiq_format_t)fie.code;
        frameSize.width = fie.width;
        frameSize.height = fie.height;
        frameSize.fps = fie.interval.denominator / fie.interval.numerator;
        frameSize.hdr_mode = fie.reserved[0];
        sensor_info->frame_size.push_back(frameSize);
        fie.index++;
    }
    if (fie.index == 0)
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "@%s %s: Enum sensor frame interval failed", __FUNCTION__, sensor_info->device_name.c_str());
    vdev.close();

    return ret;
}

static XCamReturn
parse_module_info(rk_sensor_full_info_t *sensor_info)
{
    // sensor entity name format SHOULD be like this:
    // m00_b_ov13850 1-0010
    std::string entity_name(sensor_info->sensor_name);

    if (entity_name.empty())
        return XCAM_RETURN_ERROR_SENSOR;

    int parse_index = 0;

    if (entity_name.at(parse_index) != 'm') {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "%d:parse sensor entity name %s error at %d, please check sensor driver !",
                        __LINE__, entity_name.c_str(), parse_index);
        return XCAM_RETURN_ERROR_SENSOR;
    }

    std::string index_str = entity_name.substr (parse_index, 3);
    sensor_info->module_index_str = index_str;

    parse_index += 3;

    if (entity_name.at(parse_index) != '_') {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "%d:parse sensor entity name %s error at %d, please check sensor driver !",
                        __LINE__, entity_name.c_str(), parse_index);
        return XCAM_RETURN_ERROR_SENSOR;
    }

    parse_index++;

    if (entity_name.at(parse_index) != 'b' &&
            entity_name.at(parse_index) != 'f') {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "%d:parse sensor entity name %s error at %d, please check sensor driver !",
                        __LINE__, entity_name.c_str(), parse_index);
        return XCAM_RETURN_ERROR_SENSOR;
    }
    sensor_info->phy_module_orient = entity_name.at(parse_index);

    parse_index++;

    if (entity_name.at(parse_index) != '_') {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "%d:parse sensor entity name %s error at %d, please check sensor driver !",
                        __LINE__, entity_name.c_str(), parse_index);
        return XCAM_RETURN_ERROR_SENSOR;
    }

    parse_index++;

    std::size_t real_name_end = std::string::npos;
    if ((real_name_end = entity_name.find(' ')) == std::string::npos) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "%d:parse sensor entity name %s error at %d, please check sensor driver !",
                        __LINE__, entity_name.c_str(), parse_index);
        return XCAM_RETURN_ERROR_SENSOR;
    }

    std::string real_name_str = entity_name.substr(parse_index, real_name_end - parse_index);
    sensor_info->module_real_sensor_name = real_name_str;

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "%s:%d, real sensor name %s, module ori %c, module id %s",
                    __FUNCTION__, __LINE__, sensor_info->module_real_sensor_name.c_str(),
                    sensor_info->phy_module_orient, sensor_info->module_index_str.c_str());

    return XCAM_RETURN_NO_ERROR;
}

static rk_aiq_ispp_t*
get_ispp_subdevs(struct media_device *device, const char *devpath, rk_aiq_ispp_t* ispp_info)
{
    media_entity *entity = NULL;
    const char *entity_name = NULL;
    int index = 0;

    if(!device || !ispp_info || !devpath)
        return NULL;

    for(index = 0; index < MAX_CAM_NUM; index++) {
        if (0 == strlen(ispp_info[index].media_dev_path))
            break;
        if (0 == strncmp(ispp_info[index].media_dev_path, devpath, sizeof(ispp_info[index].media_dev_path))) {
            LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp info of path %s exists!", devpath);
            return &ispp_info[index];
        }
    }

    if (index >= MAX_CAM_NUM)
        return NULL;
#if defined(ISP_HW_V30)
    // parse driver pattern: soc:rkisp0-vir0
    int model_idx = -1;
    char* rkispp = strstr(device->info.driver, "rkispp");
    if (rkispp) {
        int ispp_idx = atoi(rkispp + strlen("rkispp"));
        char* vir = strstr(device->info.driver, "vir");
        if (vir) {
            int vir_idx = atoi(vir + strlen("vir"));
            model_idx = ispp_idx * 4 + vir_idx;
        }
    }

    if (model_idx == -1) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "wrong ispp media driver info: %s", device->info.driver);
        return NULL;
    }

    ispp_info[index].model_idx = model_idx;
#else

    if (strcmp(device->info.model, "rkispp0") == 0 ||
            strcmp(device->info.model, "rkispp") == 0)
        ispp_info[index].model_idx = 0;
    else if (strcmp(device->info.model, "rkispp1") == 0)
        ispp_info[index].model_idx = 1;
    else if (strcmp(device->info.model, "rkispp2") == 0)
        ispp_info[index].model_idx = 2;
    else if (strcmp(device->info.model, "rkispp3") == 0)
        ispp_info[index].model_idx = 3;
    else
        ispp_info[index].model_idx = -1;
#endif
    strncpy(ispp_info[index].media_dev_path, devpath, sizeof(ispp_info[index].media_dev_path));

    entity = media_get_entity_by_name(device, "rkispp_input_image", strlen("rkispp_input_image"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_input_image_path, entity_name, sizeof(ispp_info[index].pp_input_image_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkispp_m_bypass", strlen("rkispp_m_bypass"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_m_bypass_path, entity_name, sizeof(ispp_info[index].pp_m_bypass_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkispp_scale0", strlen("rkispp_scale0"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_scale0_path, entity_name, sizeof(ispp_info[index].pp_scale0_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkispp_scale1", strlen("rkispp_scale1"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_scale1_path, entity_name, sizeof(ispp_info[index].pp_scale1_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkispp_scale2", strlen("rkispp_scale2"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_scale2_path, entity_name, sizeof(ispp_info[index].pp_scale2_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkispp_tnr_params", strlen("rkispp_tnr_params"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_tnr_params_path, entity_name, sizeof(ispp_info[index].pp_tnr_params_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkispp_tnr_stats", strlen("rkispp_tnr_stats"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_tnr_stats_path, entity_name, sizeof(ispp_info[index].pp_tnr_stats_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkispp_nr_params", strlen("rkispp_nr_params"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_nr_params_path, entity_name, sizeof(ispp_info[index].pp_nr_params_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkispp_nr_stats", strlen("rkispp_nr_stats"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_nr_stats_path, entity_name, sizeof(ispp_info[index].pp_nr_stats_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkispp_fec_params", strlen("rkispp_fec_params"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_fec_params_path, entity_name, sizeof(ispp_info[index].pp_fec_params_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkispp-subdev", strlen("rkispp-subdev"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(ispp_info[index].pp_dev_path, entity_name, sizeof(ispp_info[index].pp_dev_path));
        }
    }

    LOGI_CAMHW_SUBM(ISP20HW_SUBM, "model(%s): ispp_info(%d): ispp-subdev entity name: %s\n",
                    device->info.model, index,
                    ispp_info[index].pp_dev_path);

    return &ispp_info[index];
}

static rk_aiq_isp_t*
get_isp_subdevs(struct media_device *device, const char *devpath, rk_aiq_isp_t* isp_info)
{
    media_entity *entity = NULL;
    const char *entity_name = NULL;
    int index = 0;
    if(!device || !isp_info || !devpath)
        return NULL;

    for(index = 0; index < MAX_CAM_NUM; index++) {
        if (0 == strlen(isp_info[index].media_dev_path)) {
            isp_info[index].logic_id = index;
            break;
        }
        if (0 == strncmp(isp_info[index].media_dev_path, devpath, sizeof(isp_info[index].media_dev_path))) {
            LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp info of path %s exists!", devpath);
            return &isp_info[index];
        }
    }
    if (index >= MAX_CAM_NUM)
        return NULL;

#if defined(ISP_HW_V30)
    // parse driver pattern: soc:rkisp0-vir0
    int model_idx = -1;
    char* rkisp = strstr(device->info.driver, "rkisp");
    if (rkisp) {
        char* str_unite = NULL;
        str_unite = strstr(device->info.driver, "unite");
        if (str_unite) {
            model_idx = 0;
        } else {
            int isp_idx = atoi(rkisp + strlen("rkisp"));
            char* vir = strstr(device->info.driver, "vir");
            if (vir) {
                int vir_idx = atoi(vir + strlen("vir"));
                model_idx = isp_idx * 4 + vir_idx;
                isp_info[index].phy_id = isp_idx;
            }
        }
    }

    if (model_idx == -1) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "wrong isp media driver info: %s", device->info.driver);
        return NULL;
    }

    isp_info[index].model_idx = model_idx;
#else
    if (strcmp(device->info.model, "rkisp0") == 0 ||
            strcmp(device->info.model, "rkisp") == 0)
        isp_info[index].model_idx = 0;
    else if (strcmp(device->info.model, "rkisp1") == 0)
        isp_info[index].model_idx = 1;
    else if (strcmp(device->info.model, "rkisp2") == 0)
        isp_info[index].model_idx = 2;
    else if (strcmp(device->info.model, "rkisp3") == 0)
        isp_info[index].model_idx = 3;
    else
        isp_info[index].model_idx = -1;
#endif

    strncpy(isp_info[index].media_dev_path, devpath, sizeof(isp_info[index].media_dev_path));

    entity = media_get_entity_by_name(device, "rkisp-isp-subdev", strlen("rkisp-isp-subdev"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].isp_dev_path, entity_name, sizeof(isp_info[index].isp_dev_path));
        }
    }

    entity = media_get_entity_by_name(device, "rkisp-csi-subdev", strlen("rkisp-csi-subdev"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].csi_dev_path, entity_name, sizeof(isp_info[index].csi_dev_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp-mpfbc-subdev", strlen("rkisp-mpfbc-subdev"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].mpfbc_dev_path, entity_name, sizeof(isp_info[index].mpfbc_dev_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp_mainpath", strlen("rkisp_mainpath"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].main_path, entity_name, sizeof(isp_info[index].main_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp_selfpath", strlen("rkisp_selfpath"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].self_path, entity_name, sizeof(isp_info[index].self_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp_rawwr0", strlen("rkisp_rawwr0"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].rawwr0_path, entity_name, sizeof(isp_info[index].rawwr0_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp_rawwr1", strlen("rkisp_rawwr1"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].rawwr1_path, entity_name, sizeof(isp_info[index].rawwr1_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp_rawwr2", strlen("rkisp_rawwr2"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].rawwr2_path, entity_name, sizeof(isp_info[index].rawwr2_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp_rawwr3", strlen("rkisp_rawwr3"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].rawwr3_path, entity_name, sizeof(isp_info[index].rawwr3_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp_dmapath", strlen("rkisp_dmapath"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].dma_path, entity_name, sizeof(isp_info[index].dma_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp_rawrd0_m", strlen("rkisp_rawrd0_m"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].rawrd0_m_path, entity_name, sizeof(isp_info[index].rawrd0_m_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp_rawrd1_l", strlen("rkisp_rawrd1_l"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].rawrd1_l_path, entity_name, sizeof(isp_info[index].rawrd1_l_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp_rawrd2_s", strlen("rkisp_rawrd2_s"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].rawrd2_s_path, entity_name, sizeof(isp_info[index].rawrd2_s_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp-statistics", strlen("rkisp-statistics"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].stats_path, entity_name, sizeof(isp_info[index].stats_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp-input-params", strlen("rkisp-input-params"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].input_params_path, entity_name, sizeof(isp_info[index].input_params_path));
        }
    }
    entity = media_get_entity_by_name(device, "rkisp-mipi-luma", strlen("rkisp-mipi-luma"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].mipi_luma_path, entity_name, sizeof(isp_info[index].mipi_luma_path));
        }
    }
    entity = media_get_entity_by_name(device, "rockchip-mipi-dphy-rx", strlen("rockchip-mipi-dphy-rx"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(isp_info[index].mipi_dphy_rx_path, entity_name, sizeof(isp_info[index].mipi_dphy_rx_path));
        }
    } else {
        entity = media_get_entity_by_name(device, "rockchip-csi2-dphy0", strlen("rockchip-csi2-dphy0"));
        if(entity) {
            entity_name = media_entity_get_devname (entity);
            if(entity_name) {
                strncpy(isp_info[index].mipi_dphy_rx_path, entity_name, sizeof(isp_info[index].mipi_dphy_rx_path));
            }
        }
    }

    entity = media_get_entity_by_name(device, "rkcif_dvp", strlen("rkcif_dvp"));
    if(entity)
        isp_info[index].linked_dvp = true;
    else
        isp_info[index].linked_dvp = false;

    const char* linked_entity_name_strs[] = {
        "rkcif_dvp",
        "rkcif_lite_mipi_lvds",
        "rkcif_mipi_lvds",
        "rkcif_mipi_lvds1",
        "rkcif_mipi_lvds2",
        "rkcif_mipi_lvds3",
        "rkcif_mipi_lvds4",
        "rkcif_mipi_lvds5",
        "rkcif-mipi-lvds",
        "rkcif-mipi-lvds1",
        "rkcif-mipi-lvds2",
        "rkcif-mipi-lvds3",
        "rkcif-mipi-lvds4",
        "rkcif-mipi-lvds5",
        NULL
    };

    int vicap_idx = 0;
    for (int i = 0; linked_entity_name_strs[i] != NULL; i++) {
        entity = media_get_entity_by_name(device, linked_entity_name_strs[i], strlen(linked_entity_name_strs[i]));
        if (entity) {
            strncpy(isp_info[index].linked_vicap[vicap_idx], entity->info.name, sizeof(isp_info[index].linked_vicap[vicap_idx]));
            isp_info[index].linked_sensor = true;
            if (vicap_idx++ >= MAX_ISP_LINKED_VICAP_CNT) {
                break;
            }
        }
    }

    LOGI_CAMHW_SUBM(ISP20HW_SUBM, "model(%s): isp_info(%d): ispp-subdev entity name: %s\n",
                    device->info.model, index,
                    isp_info[index].isp_dev_path);

    return &isp_info[index];
}

static rk_aiq_cif_info_t*
get_cif_subdevs(struct media_device *device, const char *devpath, rk_aiq_cif_info_t* cif_info)
{
    media_entity *entity = NULL;
    const char *entity_name = NULL;
    int index = 0;
    if(!device || !devpath || !cif_info)
        return NULL;

    for(index = 0; index < MAX_CAM_NUM; index++) {
        if (0 == strlen(cif_info[index].media_dev_path))
            break;
        if (0 == strncmp(cif_info[index].media_dev_path, devpath, sizeof(cif_info[index].media_dev_path))) {
            LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp info of path %s exists!", devpath);
            return &cif_info[index];
        }
    }
    if (index >= MAX_CAM_NUM)
        return NULL;

    cif_info[index].model_idx = index;

    strncpy(cif_info[index].media_dev_path, devpath, sizeof(cif_info[index].media_dev_path) - 1);

    entity = media_get_entity_by_name(device, "stream_cif_mipi_id0", strlen("stream_cif_mipi_id0"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_id0, entity_name, sizeof(cif_info[index].mipi_id0) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif_mipi_id1", strlen("stream_cif_mipi_id1"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_id1, entity_name, sizeof(cif_info[index].mipi_id1) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif_mipi_id2", strlen("stream_cif_mipi_id2"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_id2, entity_name, sizeof(cif_info[index].mipi_id2) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif_mipi_id3", strlen("stream_cif_mipi_id3"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_id3, entity_name, sizeof(cif_info[index].mipi_id3) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "rkcif_scale_ch0", strlen("rkcif_scale_ch0"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_scl0, entity_name, sizeof(cif_info[index].mipi_scl0) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "rkcif_scale_ch1", strlen("rkcif_scale_ch1"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_scl1, entity_name, sizeof(cif_info[index].mipi_scl1) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "rkcif_scale_ch2", strlen("rkcif_scale_ch2"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_scl2, entity_name, sizeof(cif_info[index].mipi_scl2) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "rkcif_scale_ch3", strlen("rkcif_scale_ch3"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_scl3, entity_name, sizeof(cif_info[index].mipi_scl3) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif_dvp_id0", strlen("stream_cif_dvp_id0"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].dvp_id0, entity_name, sizeof(cif_info[index].dvp_id0) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif_dvp_id1", strlen("stream_cif_dvp_id1"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].dvp_id1, entity_name, sizeof(cif_info[index].dvp_id1) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif_dvp_id2", strlen("stream_cif_dvp_id2"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].dvp_id2, entity_name, sizeof(cif_info[index].dvp_id2) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif_dvp_id3", strlen("stream_cif_dvp_id3"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].dvp_id3, entity_name, sizeof(cif_info[index].dvp_id3) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "rkcif-mipi-luma", strlen("rkisp-mipi-luma"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_luma_path, entity_name, sizeof(cif_info[index].mipi_luma_path) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "rockchip-mipi-csi2", strlen("rockchip-mipi-csi2"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_csi2_sd_path, entity_name, sizeof(cif_info[index].mipi_csi2_sd_path) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "rkcif-lvds-subdev", strlen("rkcif-lvds-subdev"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].lvds_sd_path, entity_name, sizeof(cif_info[index].lvds_sd_path) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "rkcif-lite-lvds-subdev", strlen("rkcif-lite-lvds-subdev"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].lvds_sd_path, entity_name, sizeof(cif_info[index].lvds_sd_path) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "rockchip-mipi-dphy-rx", strlen("rockchip-mipi-dphy-rx"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].mipi_dphy_rx_path, entity_name, sizeof(cif_info[index].mipi_dphy_rx_path) - 1);
        }
    } else {
        entity = media_get_entity_by_name(device, "rockchip-csi2-dphy0", strlen("rockchip-csi2-dphy0"));
        if(entity) {
            entity_name = media_entity_get_devname (entity);
            if(entity_name) {
                strncpy(cif_info[index].mipi_dphy_rx_path, entity_name, sizeof(cif_info[index].mipi_dphy_rx_path) - 1);
            }
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif", strlen("stream_cif"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].stream_cif_path, entity_name, sizeof(cif_info[index].stream_cif_path) - 1);
        }
    }

    entity = media_get_entity_by_name(device, "rkcif-dvp-sof", strlen("rkcif-dvp-sof"));
    if(entity) {
        entity_name = media_entity_get_devname (entity);
        if(entity_name) {
            strncpy(cif_info[index].dvp_sof_sd_path, entity_name, sizeof(cif_info[index].dvp_sof_sd_path) - 1);
        }
    }

    return &cif_info[index];
}


static
XCamReturn
SensorInfoCopy(rk_sensor_full_info_t *finfo, rk_aiq_static_info_t *info) {
    int fs_num, i = 0;
    rk_aiq_sensor_info_t *sinfo = NULL;

    //info->media_node_index = finfo->media_node_index;
    strncpy(info->lens_info.len_name, finfo->len_name.c_str(), sizeof(info->lens_info.len_name));
    sinfo = &info->sensor_info;
    strncpy(sinfo->sensor_name, finfo->sensor_name.c_str(), sizeof(sinfo->sensor_name));
    fs_num = finfo->frame_size.size();
    if (fs_num) {
        for (auto iter = finfo->frame_size.begin(); iter != finfo->frame_size.end() && i < 10; ++iter, i++) {
            sinfo->support_fmt[i].width = (*iter).width;
            sinfo->support_fmt[i].height = (*iter).height;
            sinfo->support_fmt[i].format = (*iter).format;
            sinfo->support_fmt[i].fps = (*iter).fps;
            sinfo->support_fmt[i].hdr_mode = (*iter).hdr_mode;
        }
        sinfo->num = i;
    }

    if (finfo->module_index_str.size()) {
        sinfo->phyId = atoi(finfo->module_index_str.c_str() + 1);
    } else {
        sinfo->phyId = -1;
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CamHwIsp20::selectIqFile(const char* sns_ent_name, char* iqfile_name)
{
    if (!sns_ent_name || !iqfile_name)
        return XCAM_RETURN_ERROR_SENSOR;
    std::map<std::string, SmartPtr<rk_sensor_full_info_t>>::iterator it;
    const struct rkmodule_base_inf* base_inf = NULL;
    const char *sensor_name, *module_name, *lens_name;
    char sensor_name_full[32];
    std::string str(sns_ent_name);

    if ((it = CamHwIsp20::mSensorHwInfos.find(str)) == CamHwIsp20::mSensorHwInfos.end()) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "can't find sensor %s", sns_ent_name);
        return XCAM_RETURN_ERROR_SENSOR;
    }
    base_inf = &(it->second.ptr()->mod_info.base);
    if (!strlen(base_inf->module) || !strlen(base_inf->sensor) ||
            !strlen(base_inf->lens)) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "no camera module info, check the drv !");
        return XCAM_RETURN_ERROR_SENSOR;
    }
    sensor_name = base_inf->sensor;
    strncpy(sensor_name_full, sensor_name, 32);

    module_name = base_inf->module;
    lens_name = base_inf->lens;
    if (strlen(module_name) && strlen(lens_name)) {
        sprintf(iqfile_name, "%s_%s_%s.xml",
                sensor_name_full, module_name, lens_name);
    } else {
        sprintf(iqfile_name, "%s.xml", sensor_name_full);
    }

    return XCAM_RETURN_NO_ERROR;
}

rk_aiq_static_info_t*
CamHwIsp20::getStaticCamHwInfo(const char* sns_ent_name, uint16_t index)
{
    std::map<std::string, SmartPtr<rk_aiq_static_info_t>>::iterator it;

    if (sns_ent_name) {
        std::string str(sns_ent_name);

        it = mCamHwInfos.find(str);
        if (it != mCamHwInfos.end()) {
            LOGD_CAMHW_SUBM(ISP20HW_SUBM, "find camerainfo of %s!", sns_ent_name);
            return it->second.ptr();
        } else {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "camerainfo of %s not fount!", sns_ent_name);
        }
    } else {
        if (index >= 0 && index < mCamHwInfos.size()) {
            int i = 0;
            for (it = mCamHwInfos.begin(); it != mCamHwInfos.end(); it++, i++) {
                if (i == index)
                    return it->second.ptr();
            }
        }
    }

    return NULL;
}

XCamReturn
CamHwIsp20::clearStaticCamHwInfo()
{
    /* std::map<std::string, SmartPtr<rk_aiq_static_info_t>>::iterator it1; */
    /* std::map<std::string, SmartPtr<rk_sensor_full_info_t>>::iterator it2; */

    /* for (it1 = mCamHwInfos.begin(); it1 != mCamHwInfos.end(); it1++) { */
    /*     rk_aiq_static_info_t *ptr = it1->second.ptr(); */
    /*     delete ptr; */
    /* } */
    /* for (it2 = mSensorHwInfos.begin(); it2 != mSensorHwInfos.end(); it2++) { */
    /*     rk_sensor_full_info_t *ptr = it2->second.ptr(); */
    /*     delete ptr; */
    /* } */
    mCamHwInfos.clear();
    mSensorHwInfos.clear();

    return XCAM_RETURN_NO_ERROR;
}

void
CamHwIsp20::findAttachedSubdevs(struct media_device *device, uint32_t count, rk_sensor_full_info_t *s_info)
{
    const struct media_entity_desc *entity_info = NULL;
    struct media_entity *entity = NULL;
    uint32_t k;

    for (k = 0; k < count; ++k) {
        entity = media_get_entity (device, k);
        entity_info = media_entity_get_info(entity);
        if ((NULL != entity_info) && (entity_info->type == MEDIA_ENT_T_V4L2_SUBDEV_LENS)) {
            if ((entity_info->name[0] == 'm') &&
                    (strncmp(entity_info->name, s_info->module_index_str.c_str(), 3) == 0)) {
                if (entity_info->flags == 1)
                    s_info->module_ircut_dev_name = std::string(media_entity_get_devname(entity));
                else//vcm
                    s_info->module_lens_dev_name = std::string(media_entity_get_devname(entity));
            }
        } else if ((NULL != entity_info) && (entity_info->type == MEDIA_ENT_T_V4L2_SUBDEV_FLASH)) {
            if ((entity_info->name[0] == 'm') &&
                    (strncmp(entity_info->name, s_info->module_index_str.c_str(), 3) == 0)) {

                /* check if entity name has the format string mxx_x_xxx-irxxx */
                if (strstr(entity_info->name, "-ir") != NULL) {
                    s_info->module_flash_ir_dev_name[s_info->flash_ir_num++] =
                        std::string(media_entity_get_devname(entity));
                } else
                    s_info->module_flash_dev_name[s_info->flash_num++] =
                        std::string(media_entity_get_devname(entity));
            }
        }
    }
    // query flash infos
    if (s_info->flash_num) {
        SmartPtr<FlashLightHw> fl = new FlashLightHw(s_info->module_flash_dev_name, s_info->flash_num);
        fl->init(1);
        s_info->fl_strth_adj_sup = fl->isStrengthAdj();
        fl->deinit();
    }
    if (s_info->flash_ir_num) {
        SmartPtr<FlashLightHw> fl_ir = new FlashLightHw(s_info->module_flash_ir_dev_name, s_info->flash_ir_num);
        fl_ir->init(1);
        s_info->fl_ir_strth_adj_sup = fl_ir->isStrengthAdj();
        fl_ir->deinit();
    }
}

XCamReturn
CamHwIsp20::initCamHwInfos()
{
    char sys_path[64], devpath[32];
    FILE *fp = NULL;
    struct media_device *device = NULL;
    int nents, j = 0, i = 0, node_index = 0;
    const struct media_entity_desc *entity_info = NULL;
    struct media_entity *entity = NULL;

    xcam_mem_clear (CamHwIsp20::mIspHwInfos);
    xcam_mem_clear (CamHwIsp20::mCifHwInfos);

    while (i < MAX_MEDIA_INDEX) {
        node_index = i;
        snprintf (sys_path, 64, "/dev/media%d", i++);
        fp = fopen (sys_path, "r");
        if (!fp)
            continue;
        fclose (fp);
        device = media_device_new (sys_path);
        if (!device) {
            continue;
        }

        /* Enumerate entities, pads and links. */
        media_device_enumerate (device);

        rk_aiq_isp_t* isp_info = NULL;
        rk_aiq_cif_info_t* cif_info = NULL;
        bool dvp_itf = false;
        if (strcmp(device->info.model, "rkispp0") == 0 ||
                strcmp(device->info.model, "rkispp1") == 0 ||
                strcmp(device->info.model, "rkispp2") == 0 ||
                strcmp(device->info.model, "rkispp3") == 0 ||
                strcmp(device->info.model, "rkispp") == 0) {
            rk_aiq_ispp_t* ispp_info = get_ispp_subdevs(device, sys_path, CamHwIsp20::mIspHwInfos.ispp_info);
            if (ispp_info)
                ispp_info->valid = true;
            goto media_unref;
        } else if (strcmp(device->info.model, "rkisp0") == 0 ||
                   strcmp(device->info.model, "rkisp1") == 0 ||
                   strcmp(device->info.model, "rkisp2") == 0 ||
                   strcmp(device->info.model, "rkisp3") == 0 ||
                   strcmp(device->info.model, "rkisp") == 0) {
            isp_info = get_isp_subdevs(device, sys_path, CamHwIsp20::mIspHwInfos.isp_info);
            if (strcmp(device->info.driver, "rkisp-unite") == 0) {
                isp_info->is_multi_isp_mode = true;
                mIsMultiIspMode = true;
                mMultiIspExtendedPixel = RKMOUDLE_UNITE_EXTEND_PIXEL;
            } else {
                isp_info->is_multi_isp_mode = false;
                mIsMultiIspMode = false;
                mMultiIspExtendedPixel = 0;
            }
            isp_info->valid = true;
        } else if (strcmp(device->info.model, "rkcif") == 0 ||
                   strcmp(device->info.model, "rkcif_dvp") == 0 ||
                   strstr(device->info.model, "rkcif_mipi_lvds") ||
                   strstr(device->info.model, "rkcif-mipi-lvds") ||
                   strcmp(device->info.model, "rkcif_lite_mipi_lvds") == 0) {
            cif_info = get_cif_subdevs(device, sys_path, CamHwIsp20::mCifHwInfos.cif_info);
            strncpy(cif_info->model_str, device->info.model, sizeof(cif_info->model_str));

            if (strcmp(device->info.model, "rkcif_dvp") == 0)
                dvp_itf = true;
        } else {
            goto media_unref;
        }

        nents = media_get_entities_count (device);
        for (j = 0; j < nents; ++j) {
            entity = media_get_entity (device, j);
            entity_info = media_entity_get_info(entity);
            if ((NULL != entity_info) && (entity_info->type == MEDIA_ENT_T_V4L2_SUBDEV_SENSOR)) {
                rk_aiq_static_info_t *info = new rk_aiq_static_info_t();
                rk_sensor_full_info_t *s_full_info = new rk_sensor_full_info_t();
                s_full_info->media_node_index = node_index;
                strncpy(devpath, media_entity_get_devname(entity), sizeof(devpath));
                s_full_info->device_name = std::string(devpath);
                s_full_info->sensor_name = std::string(entity_info->name);
                s_full_info->parent_media_dev = std::string(sys_path);
                parse_module_info(s_full_info);
                get_sensor_caps(s_full_info);

                if (cif_info) {
                    s_full_info->linked_to_isp = false;
                    s_full_info->cif_info = cif_info;
                    s_full_info->isp_info = NULL;
                    s_full_info->dvp_itf = dvp_itf;
                } else if (isp_info) {
                    s_full_info->linked_to_isp = true;
                    isp_info->linked_sensor = true;
                    isp_info->isMultiplex = false;
                    s_full_info->isp_info = isp_info;
                } else {
                    LOGE_CAMHW_SUBM(ISP20HW_SUBM, "sensor device mount error!\n");
                }

                findAttachedSubdevs(device, nents, s_full_info);
                SensorInfoCopy(s_full_info, info);
                info->has_lens_vcm = s_full_info->module_lens_dev_name.empty() ? false : true;
                info->has_fl = s_full_info->flash_num > 0 ? true : false;
                info->has_irc = s_full_info->module_ircut_dev_name.empty() ? false : true;
                info->fl_strth_adj_sup = s_full_info->fl_ir_strth_adj_sup;
                info->fl_ir_strth_adj_sup = s_full_info->fl_ir_strth_adj_sup;
                if (s_full_info->isp_info)
                    info->is_multi_isp_mode = s_full_info->isp_info->is_multi_isp_mode;
                info->multi_isp_extended_pixel = mMultiIspExtendedPixel;
                LOGD_CAMHW_SUBM(ISP20HW_SUBM, "Init sensor %s with Multi-ISP Mode:%d Extended Pixels:%d ",
                                s_full_info->sensor_name.c_str(), info->is_multi_isp_mode,
                                info->multi_isp_extended_pixel);
                CamHwIsp20::mSensorHwInfos[s_full_info->sensor_name] = s_full_info;
                CamHwIsp20::mCamHwInfos[s_full_info->sensor_name] = info;
            }
        }

media_unref:
        media_device_unref (device);
    }

    // judge isp if multiplex by multiple cams
    rk_aiq_isp_t* isp_info = NULL;
    for (i = 0; i < MAX_CAM_NUM; i++) {
        isp_info = &CamHwIsp20::mIspHwInfos.isp_info[i];
        if (isp_info->valid) {
            for (j = i - 1; j >= 0; j--) {
                if (isp_info->phy_id == CamHwIsp20::mIspHwInfos.isp_info[j].phy_id) {
                    isp_info->isMultiplex = true;
                    CamHwIsp20::mIspHwInfos.isp_info[j].isMultiplex = true;
                }
            }
        }
    }

    std::map<std::string, SmartPtr<rk_sensor_full_info_t>>::iterator iter;
    for(iter = CamHwIsp20::mSensorHwInfos.begin(); \
            iter != CamHwIsp20::mSensorHwInfos.end(); iter++) {
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "match the sensor_name(%s) media link\n", (iter->first).c_str());
        SmartPtr<rk_sensor_full_info_t> s_full_info = iter->second;

        /*
         * The ISP and ISPP match links through the media device model
         */
        if (s_full_info->linked_to_isp) {
            for (i = 0; i < MAX_CAM_NUM; i++) {
                LOGI_CAMHW_SUBM(ISP20HW_SUBM, "isp model_idx: %d, ispp(%d) model_idx: %d\n",
                                s_full_info->isp_info->model_idx,
                                i,
                                CamHwIsp20::mIspHwInfos.ispp_info[i].model_idx);

                if (CamHwIsp20::mIspHwInfos.ispp_info[i].valid &&
                        (s_full_info->isp_info->model_idx == CamHwIsp20::mIspHwInfos.ispp_info[i].model_idx)) {
                    s_full_info->ispp_info = &CamHwIsp20::mIspHwInfos.ispp_info[i];
                    LOGI_CAMHW_SUBM(ISP20HW_SUBM, "isp(%d) link to ispp(%d)\n",
                                    s_full_info->isp_info->model_idx,
                                    CamHwIsp20::mIspHwInfos.ispp_info[i].model_idx);
                    CamHwIsp20::mCamHwInfos[s_full_info->sensor_name]->sensor_info.binded_strm_media_idx =
                        atoi(s_full_info->ispp_info->media_dev_path + strlen("/dev/media"));
                    LOGI_CAMHW_SUBM(ISP20HW_SUBM, "sensor %s adapted to pp media %d:%s\n",
                                    s_full_info->sensor_name.c_str(),
                                    CamHwIsp20::mCamHwInfos[s_full_info->sensor_name]->sensor_info.binded_strm_media_idx,
                                    s_full_info->ispp_info->media_dev_path);
                    break;
                }
            }
        } else {
            /*
             * Determine which isp that vipCap is linked
             */
            for (i = 0; i < MAX_CAM_NUM; i++) {
                rk_aiq_isp_t* isp_info = &CamHwIsp20::mIspHwInfos.isp_info[i];

                for (int vicap_idx = 0; vicap_idx < MAX_ISP_LINKED_VICAP_CNT; vicap_idx++) {
                    LOGI_CAMHW_SUBM(ISP20HW_SUBM, "vicap %s, linked_vicap %s",
                                    s_full_info->cif_info->model_str, isp_info->linked_vicap[vicap_idx]);
                    if (strcmp(s_full_info->cif_info->model_str, isp_info->linked_vicap[vicap_idx]) == 0) {
                        s_full_info->isp_info = &CamHwIsp20::mIspHwInfos.isp_info[i];
                        CamHwIsp20::mCamHwInfos[s_full_info->sensor_name]->is_multi_isp_mode =
                            s_full_info->isp_info->is_multi_isp_mode;
                        CamHwIsp20::mCamHwInfos[s_full_info->sensor_name]
                        ->multi_isp_extended_pixel = mMultiIspExtendedPixel;
                        if (CamHwIsp20::mIspHwInfos.ispp_info[i].valid)
                            s_full_info->ispp_info = &CamHwIsp20::mIspHwInfos.ispp_info[i];
                        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "vicap link to isp(%d) to ispp(%d)\n",
                                        s_full_info->isp_info->model_idx,
                                        s_full_info->ispp_info ? s_full_info->ispp_info->model_idx : -1);
                        CamHwIsp20::mCamHwInfos[s_full_info->sensor_name]->sensor_info.binded_strm_media_idx =
                            s_full_info->ispp_info ? atoi(s_full_info->ispp_info->media_dev_path + strlen("/dev/media")) :
                            -1;
                        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "sensor %s adapted to pp media %d:%s\n",
                                        s_full_info->sensor_name.c_str(),
                                        CamHwIsp20::mCamHwInfos[s_full_info->sensor_name]->sensor_info.binded_strm_media_idx,
                                        s_full_info->ispp_info ? s_full_info->ispp_info->media_dev_path : "null");
                        CamHwIsp20::mIspHwInfos.isp_info[i].linked_sensor = true;
                    }
                }
            }
        }

        if (!s_full_info->isp_info/* || !s_full_info->ispp_info*/) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get isp or ispp info fail, something gos wrong!");
        } else {
            //CamHwIsp20::mCamHwInfos[s_full_info->sensor_name]->linked_isp_info = *s_full_info->isp_info;
            //CamHwIsp20::mCamHwInfos[s_full_info->sensor_name]->linked_ispp_info = *s_full_info->ispp_info;
        }
    }

    /* Look for free isp&ispp link to fake camera */
    for (i = 0; i < MAX_CAM_NUM; i++) {
        if (CamHwIsp20::mIspHwInfos.isp_info[i].valid &&
                !CamHwIsp20::mIspHwInfos.isp_info[i].linked_sensor) {
            rk_aiq_static_info_t *hwinfo = new rk_aiq_static_info_t();
            rk_sensor_full_info_t *fullinfo = new rk_sensor_full_info_t();

            fullinfo->isp_info = &CamHwIsp20::mIspHwInfos.isp_info[i];
            if (CamHwIsp20::mIspHwInfos.ispp_info[i].valid) {
                fullinfo->ispp_info = &CamHwIsp20::mIspHwInfos.ispp_info[i];
                hwinfo->sensor_info.binded_strm_media_idx =
                    atoi(fullinfo->ispp_info->media_dev_path + strlen("/dev/media"));
            }
            fullinfo->media_node_index = -1;
            fullinfo->device_name = std::string("/dev/null");
            fullinfo->sensor_name = std::string("FakeCamera");
            fullinfo->sensor_name += std::to_string(i);
            fullinfo->parent_media_dev = std::string("/dev/null");
            fullinfo->linked_to_isp = true;

            hwinfo->sensor_info.support_fmt[0].hdr_mode = NO_HDR;
            hwinfo->sensor_info.support_fmt[1].hdr_mode = HDR_X2;
            hwinfo->sensor_info.support_fmt[2].hdr_mode = HDR_X3;
            hwinfo->sensor_info.num = 3;
            CamHwIsp20::mIspHwInfos.isp_info[i].linked_sensor = true;

            SensorInfoCopy(fullinfo, hwinfo);
            hwinfo->has_lens_vcm = false;
            hwinfo->has_fl = false;
            hwinfo->has_irc = false;
            hwinfo->fl_strth_adj_sup = 0;
            hwinfo->fl_ir_strth_adj_sup = 0;
            hwinfo->is_multi_isp_mode        = fullinfo->isp_info->is_multi_isp_mode;
            hwinfo->multi_isp_extended_pixel = mMultiIspExtendedPixel;
            CamHwIsp20::mSensorHwInfos[fullinfo->sensor_name] = fullinfo;
            CamHwIsp20::mCamHwInfos[fullinfo->sensor_name] = hwinfo;
            LOGI_CAMHW_SUBM(ISP20HW_SUBM, "fake camera %d link to isp(%d) to ispp(%d)\n",
                            i,
                            fullinfo->isp_info->model_idx,
                            fullinfo->ispp_info ? fullinfo->ispp_info->model_idx : -1);
            LOGI_CAMHW_SUBM(ISP20HW_SUBM, "sensor %s adapted to pp media %d:%s\n",
                            fullinfo->sensor_name.c_str(),
                            CamHwIsp20::mCamHwInfos[fullinfo->sensor_name]->sensor_info.binded_strm_media_idx,
                            fullinfo->ispp_info ? fullinfo->ispp_info->media_dev_path : "null");
        }
    }

    get_isp_ver(&CamHwIsp20::mIspHwInfos);
    for (auto &item : mCamHwInfos)
        item.second->isp_hw_ver = mIspHwInfos.hw_ver_info.isp_ver;
    return XCAM_RETURN_NO_ERROR;
}

const char*
CamHwIsp20::getBindedSnsEntNmByVd(const char* vd)
{
    if (!vd)
        return NULL;

    std::map<std::string, SmartPtr<rk_sensor_full_info_t>>::iterator iter;
    for(iter = CamHwIsp20::mSensorHwInfos.begin(); \
            iter != CamHwIsp20::mSensorHwInfos.end(); iter++) {
        SmartPtr<rk_sensor_full_info_t> s_full_info = iter->second;
        if (!s_full_info->isp_info)
            continue;

        bool stream_vd = false;
        if (s_full_info->ispp_info) {
            if (strstr(s_full_info->ispp_info->pp_m_bypass_path, vd) ||
                    strstr(s_full_info->ispp_info->pp_scale0_path, vd) ||
                    strstr(s_full_info->ispp_info->pp_scale1_path, vd) ||
                    strstr(s_full_info->ispp_info->pp_scale2_path, vd))
                stream_vd = true;
        } else {
            if (strstr(s_full_info->isp_info->main_path, vd) ||
                    strstr(s_full_info->isp_info->self_path, vd))
                stream_vd = true;
        }

        if (stream_vd) {
            // check linked
            if (strstr(s_full_info->sensor_name.c_str(), "FakeCamera") == NULL) {
                FILE *fp = NULL;
                struct media_device *device = NULL;
                uint32_t nents, j = 0, i = 0;
                const struct media_entity_desc *entity_info = NULL;
                struct media_entity *entity = NULL;
                media_pad *src_pad_s = NULL;
                char sys_path[64], devpath[32];

                snprintf (sys_path, 64, "/dev/media%d", s_full_info->media_node_index);
                if (0 != access(sys_path, F_OK))
                    continue;

                device = media_device_new (sys_path);
                if (!device)
                    return nullptr;

                /* Enumerate entities, pads and links. */
                media_device_enumerate (device);
                entity = media_get_entity_by_name(device,
                                                  s_full_info->sensor_name.c_str(),
                                                  s_full_info->sensor_name.size());
                entity_info = media_entity_get_info(entity);
                if (entity && entity->num_links > 0) {
                    if (entity->links[0].flags == MEDIA_LNK_FL_ENABLED) {
                        media_device_unref (device);
                        return  s_full_info->sensor_name.c_str();
                    }
                }
                media_device_unref (device);
            } else
                return  s_full_info->sensor_name.c_str();
        }
    }

    return NULL;
}

XCamReturn
CamHwIsp20::init_pp(rk_sensor_full_info_t *s_info)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<PollThread> isp20IsppPollthread;

    if (!s_info->ispp_info)
        return ret;

    if (!strlen(s_info->ispp_info->media_dev_path))
        return ret;
    _ispp_sd = new V4l2SubDevice(s_info->ispp_info->pp_dev_path);
    _ispp_sd ->open();
    LOGI_CAMHW_SUBM(ISP20HW_SUBM, "pp_dev_path: %s\n", s_info->ispp_info->pp_dev_path);

    mTnrStreamProcUnit = new TnrStreamProcUnit(s_info);
    mTnrStreamProcUnit->set_devices(this, _ispp_sd);
    mNrStreamProcUnit = new NrStreamProcUnit(s_info);
    mNrStreamProcUnit->set_devices(this, _ispp_sd);
    mFecParamStream = new FecParamStream(s_info);
    mFecParamStream->set_devices(this, _ispp_sd);

    return ret;
}

XCamReturn
CamHwIsp20::init(const char* sns_ent_name)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<BaseSensorHw> sensorHw;
    SmartPtr<LensHw> lensHw;
    SmartPtr<V4l2Device> mipi_tx_devs[3];
    SmartPtr<V4l2Device> mipi_rx_devs[3];
    std::string sensor_name(sns_ent_name);

    ENTER_CAMHW_FUNCTION();


    std::map<std::string, SmartPtr<rk_sensor_full_info_t>>::iterator it;
    if ((it = mSensorHwInfos.find(sensor_name)) == mSensorHwInfos.end()) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "can't find sensor %s", sns_ent_name);
        return XCAM_RETURN_ERROR_SENSOR;
    }
    rk_sensor_full_info_t *s_info = it->second.ptr();
    sensorHw = new SensorHw(s_info->device_name.c_str());
    sensorHw->setCamPhyId(mCamPhyId);
    mSensorDev = sensorHw;
    mSensorDev->open();

    Isp20Params::setCamPhyId(mCamPhyId);

    strncpy(sns_name, sns_ent_name, sizeof(sns_name));

    if (s_info->linked_to_isp)
        _linked_to_isp = true;

    mIspCoreDev = new V4l2SubDevice(s_info->isp_info->isp_dev_path);
    mIspCoreDev->open();

    if (strlen(s_info->isp_info->mipi_luma_path)) {
        if (_linked_to_isp) {
            mIspLumaDev = new V4l2Device(s_info->isp_info->mipi_luma_path);
        } else
            mIspLumaDev = new V4l2Device(s_info->cif_info->mipi_luma_path);
        mIspLumaDev->open();
    }

    mIspStatsDev = new V4l2Device (s_info->isp_info->stats_path);
    mIspStatsDev->open();
    mIspParamsDev = new V4l2Device (s_info->isp_info->input_params_path);
    mIspParamsDev->open();

    if(!s_info->module_lens_dev_name.empty()) {
        lensHw = new LensHw(s_info->module_lens_dev_name.c_str());
        mLensDev = lensHw;
        mLensDev->open();
    }

    if(!s_info->module_ircut_dev_name.empty()) {
        mIrcutDev = new V4l2SubDevice(s_info->module_ircut_dev_name.c_str());
        mIrcutDev->open();
    }

    if (!_linked_to_isp) {
        if (strlen(s_info->cif_info->mipi_csi2_sd_path) > 0)
            _cif_csi2_sd = new V4l2SubDevice (s_info->cif_info->mipi_csi2_sd_path);
        else if (strlen(s_info->cif_info->lvds_sd_path) > 0)
            _cif_csi2_sd = new V4l2SubDevice (s_info->cif_info->lvds_sd_path);
        else if (strlen(s_info->cif_info->dvp_sof_sd_path) > 0)
            _cif_csi2_sd = new V4l2SubDevice (s_info->cif_info->dvp_sof_sd_path);
        else
            LOGW_CAMHW_SUBM(ISP20HW_SUBM, "_cif_csi2_sd is null! \n");
        _cif_csi2_sd->open();
    }

    init_pp(s_info);

    mIspSpDev = new V4l2Device (s_info->isp_info->self_path);//rkisp_selfpath
    mIspSpDev->open();
    mSpStreamUnit = new SPStreamProcUnit(mIspSpDev, ISP_POLL_SP, mIspHwInfos.hw_ver_info.isp_ver);
    mSpStreamUnit->set_devices(this, mIspCoreDev, _ispp_sd, mLensDev);

    mPdafStreamUnit = new PdafStreamProcUnit(ISP_POLL_PDAF_STATS);
    mPdafStreamUnit->set_devices(this);

    mRawCapUnit = new RawStreamCapUnit(s_info, _linked_to_isp);
    mRawProcUnit = new RawStreamProcUnit(s_info, _linked_to_isp);
    mRawProcUnit->set_devices(mIspCoreDev, this);
    mRawCapUnit->set_devices(mIspCoreDev, this, mRawProcUnit.ptr());
    mRawProcUnit->setCamPhyId(mCamPhyId);
    mRawCapUnit->setCamPhyId(mCamPhyId);
    //isp stats
    mIspStatsStream = new RKStatsStream(mIspStatsDev, ISP_POLL_3A_STATS);
    mIspStatsStream->setPollCallback (this);
    mIspStatsStream->set_event_handle_dev(sensorHw);
    if(lensHw.ptr()) {
        mIspStatsStream->set_focus_handle_dev(lensHw);
    }
    mIspStatsStream->set_rx_handle_dev(this);
    mIspStatsStream->setCamPhyId(mCamPhyId);
    //luma
    if (mIspLumaDev.ptr()) {
        mLumaStream = new RKStream(mIspLumaDev, ISP_POLL_LUMA);
        mLumaStream->setPollCallback (this);
    }
    //isp params
    mIspParamStream = new RKStream(mIspParamsDev, ISP_POLL_PARAMS);
    mIspParamStream->setCamPhyId(mCamPhyId);

    if (s_info->flash_num) {
        mFlashLight = new FlashLightHw(s_info->module_flash_dev_name, s_info->flash_num);
        mFlashLight->init(s_info->flash_num);
    }
    if (s_info->flash_ir_num) {
        mFlashLightIr = new FlashLightHw(s_info->module_flash_ir_dev_name, s_info->flash_ir_num);
        mFlashLightIr->init(s_info->flash_ir_num);
    }

    xcam_mem_clear (_full_active_isp_params);
    xcam_mem_clear (_full_active_ispp_params);

    _state = CAM_HW_STATE_INITED;

    EXIT_CAMHW_FUNCTION();

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CamHwIsp20::deInit()
{
    if (mFlashLight.ptr())
        mFlashLight->deinit();
    if (mFlashLightIr.ptr())
        mFlashLightIr->deinit();

    std::map<std::string, SmartPtr<rk_sensor_full_info_t>>::iterator it;
    if (strlen(sns_name) == 0 || (it = mSensorHwInfos.find(sns_name)) == mSensorHwInfos.end()) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "can't find sensor %s", strlen(sns_name) ? sns_name : "");
        return XCAM_RETURN_ERROR_SENSOR;
    }

    rk_sensor_full_info_t *s_info = it->second.ptr();
    int isp_index = s_info->isp_info->logic_id;
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "sensor_name(%s) is linked to isp_index(%d)",
                    sns_name, isp_index);
    if (!mNoReadBack) {
        setupHdrLink(RK_AIQ_WORKING_MODE_ISP_HDR3, isp_index, false);
        setupHdrLink_vidcap(_hdr_mode, isp_index, false);
    }

    _state = CAM_HW_STATE_INVALID;
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CamHwIsp20::poll_buffer_ready (SmartPtr<VideoBuffer> &buf)
{
    if (buf->_buf_type == ISP_POLL_3A_STATS) {
        // stats is comming, means that next params should be ready
        if (mNoReadBack)
            mParamsAssembler->forceReady(buf->get_sequence() + 1);
    }
    return CamHwBase::poll_buffer_ready(buf);
}

XCamReturn
CamHwIsp20::setupPipelineFmtCif(struct v4l2_subdev_selection& sns_sd_sel,
                                struct v4l2_subdev_format& sns_sd_fmt,
                                __u32 sns_v4l_pix_fmt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    // TODO: set cif crop according to sensor crop bounds

    mRawCapUnit->set_tx_format(sns_sd_sel, sns_v4l_pix_fmt);
    mRawProcUnit->set_rx_format(sns_sd_sel, sns_v4l_pix_fmt);

    // set isp sink fmt, same as sensor bounds - crop
    struct v4l2_subdev_format isp_sink_fmt;

    memset(&isp_sink_fmt, 0, sizeof(isp_sink_fmt));
    isp_sink_fmt.pad = 0;
    isp_sink_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    ret = mIspCoreDev->getFormat(isp_sink_fmt);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set mIspCoreDev fmt failed !\n");
        return ret;
    }
    isp_sink_fmt.format.width = sns_sd_sel.r.width;
    isp_sink_fmt.format.height = sns_sd_sel.r.height;
    isp_sink_fmt.format.code = sns_sd_fmt.format.code;

    ret = mIspCoreDev->setFormat(isp_sink_fmt);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set mIspCoreDev fmt failed !\n");
        return ret;
    }

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp sink fmt info: fmt 0x%x, %dx%d !",
                    isp_sink_fmt.format.code, isp_sink_fmt.format.width, isp_sink_fmt.format.height);

    // set selection, isp needn't do the crop
    struct v4l2_subdev_selection aSelection;
    memset(&aSelection, 0, sizeof(aSelection));

    aSelection.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    aSelection.pad = 0;
    aSelection.flags = 0;
    aSelection.target = V4L2_SEL_TGT_CROP;
    aSelection.r.width = sns_sd_sel.r.width;
    aSelection.r.height = sns_sd_sel.r.height;
    aSelection.r.left = 0;
    aSelection.r.top = 0;
    ret = mIspCoreDev->set_selection (aSelection);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set mIspCoreDev crop failed !\n");
        return ret;
    }

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp sink crop info: %dx%d@%d,%d !",
                    aSelection.r.width, aSelection.r.height,
                    aSelection.r.left, aSelection.r.top);

    // set isp rkisp-isp-subdev src crop
    aSelection.pad = 2;
#if 1 // isp src has no crop
    ret = mIspCoreDev->set_selection (aSelection);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set mIspCoreDev source crop failed !\n");
        return ret;
    }
#endif
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp src crop info: %dx%d@%d,%d !",
                    aSelection.r.width, aSelection.r.height,
                    aSelection.r.left, aSelection.r.top);

    // set isp rkisp-isp-subdev src pad fmt
    struct v4l2_subdev_format isp_src_fmt;

    memset(&isp_src_fmt, 0, sizeof(isp_src_fmt));
    isp_src_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    isp_src_fmt.pad = 2;
    ret = mIspCoreDev->getFormat(isp_src_fmt);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get mIspCoreDev src fmt failed !\n");
        return ret;
    }

    isp_src_fmt.format.width = aSelection.r.width;
    isp_src_fmt.format.height = aSelection.r.height;
    ret = mIspCoreDev->setFormat(isp_src_fmt);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set mIspCoreDev src fmt failed !\n");
        return ret;
    }

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp src fmt info: fmt 0x%x, %dx%d !",
                    isp_src_fmt.format.code, isp_src_fmt.format.width, isp_src_fmt.format.height);

    return ret;

}

XCamReturn
CamHwIsp20::setupPipelineFmtIsp(struct v4l2_subdev_selection& sns_sd_sel,
                                struct v4l2_subdev_format& sns_sd_fmt,
                                __u32 sns_v4l_pix_fmt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    mRawCapUnit->set_tx_format(sns_sd_fmt, sns_v4l_pix_fmt);
    mRawProcUnit->set_rx_format(sns_sd_fmt, sns_v4l_pix_fmt);
#ifndef ANDROID_OS // Android camera hal will set pipeline itself
    // set isp sink fmt, same as sensor fmt
    struct v4l2_subdev_format isp_sink_fmt;

    memset(&isp_sink_fmt, 0, sizeof(isp_sink_fmt));
    isp_sink_fmt.pad = 0;
    isp_sink_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    ret = mIspCoreDev->getFormat(isp_sink_fmt);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set mIspCoreDev fmt failed !\n");
        return ret;
    }

    isp_sink_fmt.format.width = sns_sd_fmt.format.width;
    isp_sink_fmt.format.height = sns_sd_fmt.format.height;
    isp_sink_fmt.format.code = sns_sd_fmt.format.code;

    ret = mIspCoreDev->setFormat(isp_sink_fmt);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set mIspCoreDev fmt failed !\n");
        return ret;
    }

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp sink fmt info: fmt 0x%x, %dx%d !",
                    isp_sink_fmt.format.code, isp_sink_fmt.format.width, isp_sink_fmt.format.height);

    // set selection, isp do the crop
    struct v4l2_subdev_selection aSelection;
    memset(&aSelection, 0, sizeof(aSelection));

    aSelection.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    aSelection.pad = 0;
    aSelection.flags = 0;
    aSelection.target = V4L2_SEL_TGT_CROP;
    aSelection.r.width = sns_sd_sel.r.width;
    aSelection.r.height = sns_sd_sel.r.height;
    aSelection.r.left = sns_sd_sel.r.left;
    aSelection.r.top = sns_sd_sel.r.top;
    ret = mIspCoreDev->set_selection (aSelection);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set mIspCoreDev crop failed !\n");
        return ret;
    }

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp sink crop info: %dx%d@%d,%d !",
                    aSelection.r.width, aSelection.r.height,
                    aSelection.r.left, aSelection.r.top);

    // set isp rkisp-isp-subdev src crop
    aSelection.pad = 2;
    aSelection.target = V4L2_SEL_TGT_CROP;
    aSelection.r.left = 0;
    aSelection.r.top = 0;
    aSelection.r.width = sns_sd_sel.r.width;
    aSelection.r.height = sns_sd_sel.r.height;
#if 1 // isp src has no crop
    ret = mIspCoreDev->set_selection (aSelection);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set mIspCoreDev source crop failed !\n");
        return ret;
    }
#endif
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp src crop info: %dx%d@%d,%d !",
                    aSelection.r.width, aSelection.r.height,
                    aSelection.r.left, aSelection.r.top);

    // set isp rkisp-isp-subdev src pad fmt
    struct v4l2_subdev_format isp_src_fmt;

    isp_src_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    isp_src_fmt.pad = 2;
    ret = mIspCoreDev->getFormat(isp_src_fmt);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get mIspCoreDev src fmt failed !\n");
        return ret;
    }

    isp_src_fmt.format.width = aSelection.r.width;
    isp_src_fmt.format.height = aSelection.r.height;
    ret = mIspCoreDev->setFormat(isp_src_fmt);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set mIspCoreDev src fmt failed !\n");
        return ret;
    }

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp src fmt info: fmt 0x%x, %dx%d !",
                    isp_src_fmt.format.code, isp_src_fmt.format.width, isp_src_fmt.format.height);
#endif
    return ret;
}

XCamReturn
CamHwIsp20::setupPipelineFmt()
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    // get sensor v4l2 pixfmt
    SmartPtr<BaseSensorHw> mSensorSubdev = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();
    rk_aiq_exposure_sensor_descriptor sns_des;
    if (mSensorSubdev->get_format(&sns_des)) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "getSensorModeData failed \n");
        return XCAM_RETURN_ERROR_UNKNOWN;
    }
    __u32 sns_v4l_pix_fmt = sns_des.sensor_pixelformat;

    struct v4l2_subdev_format sns_sd_fmt;

    // get sensor real outupt size
    memset(&sns_sd_fmt, 0, sizeof(sns_sd_fmt));
    sns_sd_fmt.pad = 0;
    sns_sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    ret = mSensorDev->getFormat(sns_sd_fmt);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get sensor fmt failed !\n");
        return ret;
    }

    // get sensor crop bounds
    struct v4l2_subdev_selection sns_sd_sel;
    memset(&sns_sd_sel, 0, sizeof(sns_sd_sel));

    ret = mSensorDev->get_selection(0, V4L2_SEL_TGT_CROP_BOUNDS, sns_sd_sel);
    if (ret) {
        LOGW_CAMHW_SUBM(ISP20HW_SUBM, "get_selection failed !\n");
        // TODO, some sensor driver has not implemented this
        // ioctl now
        sns_sd_sel.r.width = sns_sd_fmt.format.width;
        sns_sd_sel.r.height = sns_sd_fmt.format.height;
        ret = XCAM_RETURN_NO_ERROR;
    }

    if (!_linked_to_isp && _crop_rect.width && _crop_rect.height) {
        struct v4l2_format mipi_tx_fmt;
        memset(&mipi_tx_fmt, 0, sizeof(mipi_tx_fmt));
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "vicap get_crop %dx%d@%d,%d\n",
                        _crop_rect.width, _crop_rect.height, _crop_rect.left, _crop_rect.top);
        ret = mRawCapUnit->get_tx_device(0)->get_format(mipi_tx_fmt);
        mipi_tx_fmt.fmt.pix.width = _crop_rect.width;
        mipi_tx_fmt.fmt.pix.height = _crop_rect.height;
        ret = mRawCapUnit->get_tx_device(0)->set_format(mipi_tx_fmt);
        sns_sd_sel.r.width = _crop_rect.width;
        sns_sd_sel.r.height = _crop_rect.height;
        sns_sd_fmt.format.width = _crop_rect.width;
        sns_sd_fmt.format.height = _crop_rect.height;
        ret = XCAM_RETURN_NO_ERROR;
    }

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "sensor fmt info: bounds %dx%d, crop %dx%d@%d,%d !",
                    sns_sd_sel.r.width, sns_sd_sel.r.height,
                    sns_sd_fmt.format.width, sns_sd_fmt.format.height,
                    sns_sd_sel.r.left, sns_sd_sel.r.top);

    if (_linked_to_isp)
        ret = setupPipelineFmtIsp(sns_sd_sel, sns_sd_fmt, sns_v4l_pix_fmt);
    else
        ret = setupPipelineFmtCif(sns_sd_sel, sns_sd_fmt, sns_v4l_pix_fmt);

    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set ispcore fmt failed !\n");
        return ret;
    }

    if (!_ispp_sd.ptr())
        return ret;

    struct v4l2_subdev_format isp_src_fmt;

    isp_src_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    isp_src_fmt.pad = 2;
    ret = mIspCoreDev->getFormat(isp_src_fmt);

    // set ispp format, same as isp_src_fmt
    isp_src_fmt.pad = 0;
    ret = _ispp_sd->setFormat(isp_src_fmt);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set _ispp_sd sink fmt failed !\n");
        return ret;
    }
#if 0//not use
    struct v4l2_subdev_selection aSelection;
    memset(&aSelection, 0, sizeof(aSelection));

    aSelection.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    aSelection.pad = 0;
    aSelection.target = V4L2_SEL_TGT_CROP_BOUNDS;
    aSelection.flags = 0;
    aSelection.r.left = 0;
    aSelection.r.top = 0;
    aSelection.r.width = isp_src_fmt.format.width;
    aSelection.r.height = isp_src_fmt.format.height;
#if 0
    ret = _ispp_sd->set_selection (aSelection);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set _ispp_sd crop bound failed !\n");
        return ret;
    }
#endif
    aSelection.target = V4L2_SEL_TGT_CROP;
    ret = _ispp_sd->set_selection (aSelection);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set _ispp_sd crop failed !\n");
        return ret;
    }
#endif
    // set sp format to NV12 as default

    if (mIspSpDev.ptr()) {
        struct v4l2_selection selection;
        memset(&selection, 0, sizeof(selection));

        selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        selection.target = V4L2_SEL_TGT_CROP;
        selection.flags = 0;
        selection.r.left = 0;
        selection.r.top = 0;
        selection.r.width = isp_src_fmt.format.width;
        selection.r.height = isp_src_fmt.format.height;

        ret = mIspSpDev->set_selection (selection);

        struct v4l2_format fmt;
        ret = mIspSpDev->get_format (fmt);
        if (ret) {
            LOGW_CAMHW_SUBM(ISP20HW_SUBM, "get mIspSpDev fmt failed !\n");
            //return;
        }
        if (V4L2_PIX_FMT_FBCG == fmt.fmt.pix.pixelformat) {
            mIspSpDev->set_format(/*isp_src_fmt.format.width*/1920,
                    /*isp_src_fmt.format.height*/1080,
                    V4L2_PIX_FMT_NV12,
                    V4L2_FIELD_NONE, 0);
        }
    }
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "ispp sd fmt info: %dx%d",
                    isp_src_fmt.format.width, isp_src_fmt.format.height);

    return ret;
}

XCamReturn
CamHwIsp20::setupHdrLink_vidcap(int hdr_mode, int cif_index, bool enable)
{
    media_device *device = NULL;
    media_entity *entity = NULL;
    media_pad *src_pad_s = NULL, *src_pad_m = NULL, *src_pad_l = NULL, *sink_pad = NULL;

    // TODO: have some bugs now
    return XCAM_RETURN_NO_ERROR;

    if (_linked_to_isp)
        return XCAM_RETURN_NO_ERROR;

    // TODO: normal mode
    device = media_device_new (mCifHwInfos.cif_info[cif_index].media_dev_path);

    /* Enumerate entities, pads and links. */
    media_device_enumerate (device);
    entity = media_get_entity_by_name(device, "rockchip-mipi-csi2", strlen("rockchip-mipi-csi2"));
    if(entity) {
        src_pad_s = (media_pad *)media_entity_get_pad(entity, 1);
        if (!src_pad_s) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get rockchip-mipi-csi2 source pad0 failed !\n");
            goto FAIL;
        }
    } else {
        entity = media_get_entity_by_name(device, "rkcif-lvds-subdev", strlen("rkcif-lvds-subdev"));
        if(entity) {
            src_pad_s = (media_pad *)media_entity_get_pad(entity, 1);
            if (!src_pad_s) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get rkcif-lvds-subdev source pad0 failed !\n");
                goto FAIL;
            }
        } else {
            entity = media_get_entity_by_name(device, "rkcif-lite-lvds-subdev", strlen("rkcif-lite-lvds-subdev"));
            if(entity) {
                src_pad_s = (media_pad *)media_entity_get_pad(entity, 1);
                if (!src_pad_s) {
                    LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get rkcif-lite-lvds-subdev source pad0 failed !\n");
                    goto FAIL;
                }
            }
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif_mipi_id0", strlen("stream_cif_mipi_id0"));
    if(entity) {
        sink_pad = (media_pad *)media_entity_get_pad(entity, 0);
        if (!sink_pad) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get HDR pad s failed!\n");
            goto FAIL;
        }
    }
    if (enable)
        media_setup_link(device, src_pad_s, sink_pad, MEDIA_LNK_FL_ENABLED);
    else
        media_setup_link(device, src_pad_s, sink_pad, 0);

    entity = media_get_entity_by_name(device, "rockchip-mipi-csi2", strlen("rockchip-mipi-csi2"));
    if(entity) {
        src_pad_m = (media_pad *)media_entity_get_pad(entity, 2);
        if (!src_pad_m) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get rockchip-mipi-csi2 source pad0 failed !\n");
            goto FAIL;
        }
    } else {
        entity = media_get_entity_by_name(device, "rkcif-lvds-subdev", strlen("rkcif-lvds-subdev"));
        if(entity) {
            src_pad_m = (media_pad *)media_entity_get_pad(entity, 2);
            if (!src_pad_m) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get rkcif-lvds-subdev source pad0 failed !\n");
                goto FAIL;
            }
        } else {
            entity = media_get_entity_by_name(device, "rkcif-lite-lvds-subdev", strlen("rkcif-lite-lvds-subdev"));
            if(entity) {
                src_pad_m = (media_pad *)media_entity_get_pad(entity, 2);
                if (!src_pad_m) {
                    LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get rkcif-lite-lvds-subdev source pad0 failed !\n");
                    goto FAIL;
                }
            }
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif_mipi_id1", strlen("stream_cif_mipi_id1"));
    if(entity) {
        sink_pad = (media_pad *)media_entity_get_pad(entity, 0);
        if (!sink_pad) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get HDR pad s failed!\n");
            goto FAIL;
        }
    }
    if (enable)
        media_setup_link(device, src_pad_m, sink_pad, MEDIA_LNK_FL_ENABLED);
    else
        media_setup_link(device, src_pad_m, sink_pad, 0);

#if 0
    entity = media_get_entity_by_name(device, "rockchip-mipi-csi2", strlen("rockchip-mipi-csi2"));
    if(entity) {
        src_pad_l = (media_pad *)media_entity_get_pad(entity, 3);
        if (!src_pad_l) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get rockchip-mipi-csi2 source pad0 failed !\n");
            goto FAIL;
        }
    }

    entity = media_get_entity_by_name(device, "stream_cif_mipi_id2", strlen("stream_cif_mipi_id2"));
    if(entity) {
        sink_pad = (media_pad *)media_entity_get_pad(entity, 0);
        if (!sink_pad) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get HDR pad s failed!\n");
            goto FAIL;
        }
    }

    if (RK_AIQ_HDR_GET_WORKING_MODE(hdr_mode) == RK_AIQ_WORKING_MODE_ISP_HDR3) {
        if (enable)
            media_setup_link(device, src_pad_l, sink_pad, MEDIA_LNK_FL_ENABLED);
        else
            media_setup_link(device, src_pad_l, sink_pad, 0);
    } else
        media_setup_link(device, src_pad_l, sink_pad, 0);
#endif
    media_device_unref (device);
    return XCAM_RETURN_NO_ERROR;
FAIL:
    media_device_unref (device);
    return XCAM_RETURN_ERROR_FAILED;
}

XCamReturn
CamHwIsp20::setupHdrLink(int hdr_mode, int isp_index, bool enable)
{
    media_device *device = NULL;
    media_entity *entity = NULL;
    media_pad *src_pad_s = NULL, *src_pad_m = NULL, *src_pad_l = NULL, *sink_pad = NULL;

    device = media_device_new (mIspHwInfos.isp_info[isp_index].media_dev_path);
    if (!device)
        return XCAM_RETURN_ERROR_FAILED;

    /* Enumerate entities, pads and links. */
    media_device_enumerate (device);
    entity = media_get_entity_by_name(device, "rkisp-isp-subdev", strlen("rkisp-isp-subdev"));
    if(entity) {
        sink_pad = (media_pad *)media_entity_get_pad(entity, 0);
        if (!sink_pad) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get HDR sink pad failed!\n");
            goto FAIL;
        }
    }

#if 0
    entity = media_get_entity_by_name(device, mIspHwInfos.isp_info[isp_index].linked_vicap,
                                      strlen(mIspHwInfos.isp_info[isp_index].linked_vicap));
    if (entity) {
        media_pad* linked_vicap_pad = (media_pad *)media_entity_get_pad(entity, 0);
        if (linked_vicap_pad) {
            if (enable) {
                media_setup_link(device, linked_vicap_pad, sink_pad, 0);
            } else {
                media_setup_link(device, linked_vicap_pad, sink_pad, MEDIA_LNK_FL_ENABLED);
            }
        }
    }
#endif
    entity = media_get_entity_by_name(device, "rkisp_rawrd2_s", strlen("rkisp_rawrd2_s"));
    if(entity) {
        src_pad_s = (media_pad *)media_entity_get_pad(entity, 0);
        if (!src_pad_s) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get HDR source pad s failed!\n");
            goto FAIL;
        }
    }
    if (src_pad_s && sink_pad) {
        if (enable)
            media_setup_link(device, src_pad_s, sink_pad, MEDIA_LNK_FL_ENABLED);
        else
            media_setup_link(device, src_pad_s, sink_pad, 0);
    }

    entity = media_get_entity_by_name(device, "rkisp_rawrd0_m", strlen("rkisp_rawrd0_m"));
    if(entity) {
        src_pad_m = (media_pad *)media_entity_get_pad(entity, 0);
        if (!src_pad_m) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get HDR source pad m failed!\n");
            goto FAIL;
        }
    }

    if (src_pad_m && sink_pad) {
        if (RK_AIQ_HDR_GET_WORKING_MODE(hdr_mode) >= RK_AIQ_WORKING_MODE_ISP_HDR2 && enable) {
            media_setup_link(device, src_pad_m, sink_pad, MEDIA_LNK_FL_ENABLED);
        } else
            media_setup_link(device, src_pad_m, sink_pad, 0);
    }

    entity = media_get_entity_by_name(device, "rkisp_rawrd1_l", strlen("rkisp_rawrd1_l"));
    if(entity) {
        src_pad_l = (media_pad *)media_entity_get_pad(entity, 0);
        if (!src_pad_l) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get HDR source pad l failed!\n");
            goto FAIL;
        }
    }

    if (src_pad_l && sink_pad) {
        if (RK_AIQ_HDR_GET_WORKING_MODE(hdr_mode) == RK_AIQ_WORKING_MODE_ISP_HDR3 && enable) {
            media_setup_link(device, src_pad_l, sink_pad, MEDIA_LNK_FL_ENABLED);
        } else
            media_setup_link(device, src_pad_l, sink_pad, 0);
    }
    media_device_unref (device);
    return XCAM_RETURN_NO_ERROR;
FAIL:
    media_device_unref (device);
    return XCAM_RETURN_ERROR_FAILED;
}

XCamReturn
CamHwIsp20::setExpDelayInfo(int mode)
{
    ENTER_CAMHW_FUNCTION();
    SmartPtr<BaseSensorHw> sensorHw;
    sensorHw = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();

    if(mode != RK_AIQ_WORKING_MODE_NORMAL) {
        sensorHw->set_exp_delay_info(_cur_calib_infos.sensor.CISExpUpdate.Hdr.time_update,
                                     _cur_calib_infos.sensor.CISExpUpdate.Hdr.gain_update,
                                     _cur_calib_infos.sensor.CISDcgSet.Hdr.support_en ? \
                                     _cur_calib_infos.sensor.CISExpUpdate.Hdr.dcg_update : -1);

        sint32_t timeDelay = _cur_calib_infos.sensor.CISExpUpdate.Hdr.time_update;
        sint32_t gainDelay = _cur_calib_infos.sensor.CISExpUpdate.Hdr.gain_update;
        _exp_delay = timeDelay > gainDelay ? timeDelay : gainDelay;
    } else {
        sensorHw->set_exp_delay_info(_cur_calib_infos.sensor.CISExpUpdate.Linear.time_update,
                                     _cur_calib_infos.sensor.CISExpUpdate.Linear.gain_update,
                                     _cur_calib_infos.sensor.CISDcgSet.Linear.support_en ? \
                                     _cur_calib_infos.sensor.CISExpUpdate.Linear.dcg_update : -1);

        sint32_t timeDelay = _cur_calib_infos.sensor.CISExpUpdate.Linear.time_update;
        sint32_t gainDelay = _cur_calib_infos.sensor.CISExpUpdate.Linear.gain_update;
        _exp_delay = timeDelay > gainDelay ? timeDelay : gainDelay;
    }

    EXIT_CAMHW_FUNCTION();
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CamHwIsp20::setLensVcmCfg(struct rkmodule_inf& mod_info)
{
    ENTER_CAMHW_FUNCTION();
    SmartPtr<LensHw> lensHw = mLensDev.dynamic_cast_ptr<LensHw>();
    rk_aiq_lens_vcmcfg old_cfg, new_cfg;
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    if (lensHw.ptr()) {
        ret = lensHw->getLensVcmCfg(old_cfg);
        if (ret != XCAM_RETURN_NO_ERROR)
            return ret;

        CalibDbV2_Af_VcmCfg_t* vcmcfg = &_cur_calib_infos.af.vcmcfg;
        float posture_diff = vcmcfg->posture_diff;

        new_cfg = old_cfg;
        if (vcmcfg->start_current != -1) {
            new_cfg.start_ma = vcmcfg->start_current;
        }
        if (vcmcfg->rated_current != -1) {
            new_cfg.rated_ma = vcmcfg->rated_current;
        }
        if (vcmcfg->step_mode != -1) {
            new_cfg.step_mode = vcmcfg->step_mode;
        }

        if (vcmcfg->start_current == -1 && vcmcfg->rated_current == -1 && vcmcfg->step_mode == -1) {
            if (mod_info.af.flag) {
                new_cfg.start_ma = mod_info.af.af_otp[0].vcm_start;
                new_cfg.rated_ma = mod_info.af.af_otp[0].vcm_end;

                if (posture_diff != 0) {
                    int range = new_cfg.rated_ma - new_cfg.start_ma;
                    int start_ma = new_cfg.start_ma;
                    int rated_ma = new_cfg.rated_ma;

                    new_cfg.start_ma = start_ma - (int)(range * posture_diff);
                    new_cfg.rated_ma = rated_ma + (int)(range * posture_diff);

                    LOGD_AF("posture_diff %f, start_ma %d -> %d, rated_ma %d -> %d",
                            posture_diff, start_ma, new_cfg.start_ma, rated_ma, new_cfg.rated_ma);
                }
            }
        }

        if (memcmp(&new_cfg, &old_cfg, sizeof(new_cfg)) != 0) {
            ret = lensHw->setLensVcmCfg(new_cfg);
        }
    }
    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::get_sensor_pdafinfo(rk_sensor_full_info_t *sensor_info,
                                rk_sensor_pdaf_info_t *pdaf_info) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    struct rkmodule_channel_info channel;
    memset(&channel, 0, sizeof(struct rkmodule_channel_info));

    V4l2SubDevice vdev(sensor_info->device_name.c_str());
    ret = vdev.open();
    if (ret != XCAM_RETURN_NO_ERROR) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "failed to open dev (%s)", sensor_info->device_name.c_str());
        return XCAM_RETURN_ERROR_FAILED;
    }

    pdaf_info->pdaf_support = false;
    for (int i = 0; i < 4; i++) {
        channel.index = i;
        if (vdev.io_control(RKMODULE_GET_CHANNEL_INFO, &channel) == 0) {
            if (channel.bus_fmt == MEDIA_BUS_FMT_SPD_2X8) {
                pdaf_info->pdaf_support = true;
                pdaf_info->pdaf_vc = i;
                pdaf_info->pdaf_code = channel.bus_fmt;
                pdaf_info->pdaf_width = channel.width;
                pdaf_info->pdaf_height = channel.height;
                if (channel.data_bit == 10)
                    pdaf_info->pdaf_pixelformat = V4L2_PIX_FMT_SRGGB10;
                else if (channel.data_bit == 12)
                    pdaf_info->pdaf_pixelformat = V4L2_PIX_FMT_SRGGB12;
                else if (channel.data_bit == 8)
                    pdaf_info->pdaf_pixelformat = V4L2_PIX_FMT_SRGGB8;
                else
                    pdaf_info->pdaf_pixelformat = V4L2_PIX_FMT_SRGGB16;
                LOGI_CAMHW_SUBM(ISP20HW_SUBM, "channel.bus_fmt 0x%x, pdaf_width %d, pdaf_height %d",
                    channel.bus_fmt, pdaf_info->pdaf_width, pdaf_info->pdaf_height);
                break;
            }
        }
    }

    if (pdaf_info->pdaf_support) {
        if (sensor_info->linked_to_isp) {
            switch (pdaf_info->pdaf_vc) {
            case 0:
            strcpy(pdaf_info->pdaf_vdev, sensor_info->isp_info->rawwr0_path);
            break;
            case 1:
            strcpy(pdaf_info->pdaf_vdev, sensor_info->isp_info->rawwr1_path);
            break;
            case 2:
            strcpy(pdaf_info->pdaf_vdev, sensor_info->isp_info->rawwr2_path);
            break;
            case 3:
            default:
            strcpy(pdaf_info->pdaf_vdev, sensor_info->isp_info->rawwr3_path);
            break;
            }
        } else {
            switch (pdaf_info->pdaf_vc) {
            case 0:
            strcpy(pdaf_info->pdaf_vdev, sensor_info->cif_info->mipi_id0);
            break;
            case 1:
            strcpy(pdaf_info->pdaf_vdev, sensor_info->cif_info->mipi_id1);
            break;
            case 2:
            strcpy(pdaf_info->pdaf_vdev, sensor_info->cif_info->mipi_id2);
            break;
            case 3:
            default:
            strcpy(pdaf_info->pdaf_vdev, sensor_info->cif_info->mipi_id3);
            break;
            }
        }
    }
    LOGI_CAMHW_SUBM(ISP20HW_SUBM, "%s: pdaf_vdev %s", __func__, pdaf_info->pdaf_vdev);

    vdev.close();
    return ret;
}

bool CamHwIsp20::isOnlineByWorkingMode()
{
    return true;
}

void
CamHwIsp20::setCalib(const CamCalibDbV2Context_t* calibv2)
{
    mCalibDbV2 = calibv2;
    CalibDbV2_MFNR_t* mfnr =
        (CalibDbV2_MFNR_t*)CALIBDBV2_GET_MODULE_PTR((void*)(mCalibDbV2), mfnr_v1);
    if (mfnr) {
        _cur_calib_infos.mfnr.enable = mfnr->TuningPara.enable;
        _cur_calib_infos.mfnr.motion_detect_en = mfnr->TuningPara.motion_detect_en;
    } else {
        _cur_calib_infos.mfnr.enable = false;
        _cur_calib_infos.mfnr.motion_detect_en = false;
    }

    CalibDb_Aec_ParaV2_t* aec =
        (CalibDb_Aec_ParaV2_t*)CALIBDBV2_GET_MODULE_PTR((void*)mCalibDbV2, ae_calib);
    if (aec) {
        _cur_calib_infos.aec.IrisType = aec->IrisCtrl.IrisType;
    } else {
        _cur_calib_infos.aec.IrisType = IRISV2_DC_TYPE;
    }

    if (CHECK_ISP_HW_V30()) {
        CalibDbV2_AFV30_t *af_v30 =
            (CalibDbV2_AFV30_t*)(CALIBDBV2_GET_MODULE_PTR((void*)mCalibDbV2, af_v30));
        if (af_v30) {
            _cur_calib_infos.af.vcmcfg = af_v30->TuningPara.vcmcfg;
        } else {
            memset(&_cur_calib_infos.af.vcmcfg, 0, sizeof(CalibDbV2_Af_VcmCfg_t));
        }
        memset(&_cur_calib_infos.af.ldg_param, 0, sizeof(CalibDbV2_Af_LdgParam_t));
    } else {
        CalibDbV2_AF_t *af =
            (CalibDbV2_AF_t*)CALIBDBV2_GET_MODULE_PTR((void*)mCalibDbV2, af);
        if (af) {
            _cur_calib_infos.af.vcmcfg = af->TuningPara.vcmcfg;
            _cur_calib_infos.af.ldg_param = af->TuningPara.ldg_param;
            _cur_calib_infos.af.highlight = af->TuningPara.highlight;
        } else {
            memset(&_cur_calib_infos.af.vcmcfg, 0, sizeof(CalibDbV2_Af_VcmCfg_t));
            memset(&_cur_calib_infos.af.ldg_param, 0, sizeof(CalibDbV2_Af_LdgParam_t));
        }
    }

    CalibDb_Sensor_ParaV2_t* sensor_calib =
        (CalibDb_Sensor_ParaV2_t*)(CALIBDBV2_GET_MODULE_PTR((void*)mCalibDbV2, sensor_calib));
    if (sensor_calib) {
        _cur_calib_infos.sensor.CISDcgSet = sensor_calib->CISDcgSet;
        _cur_calib_infos.sensor.CISExpUpdate = sensor_calib->CISExpUpdate;
    } else {
        memset(&_cur_calib_infos.sensor, 0,
               sizeof(_cur_calib_infos.sensor));
    }

    // update infos to sensor hw
    setExpDelayInfo(_hdr_mode);
}

XCamReturn
CamHwIsp20::prepare(uint32_t width, uint32_t height, int mode, int t_delay, int g_delay)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<BaseSensorHw> sensorHw;
    SmartPtr<LensHw> lensHw = mLensDev.dynamic_cast_ptr<LensHw>();

    ENTER_CAMHW_FUNCTION();

    XCAM_ASSERT (mCalibDbV2);

    _hdr_mode = mode;

    Isp20Params::set_working_mode(_hdr_mode);

    std::map<std::string, SmartPtr<rk_sensor_full_info_t>>::iterator it;
    if ((it = mSensorHwInfos.find(sns_name)) == mSensorHwInfos.end()) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "can't find sensor %s", sns_name);
        return XCAM_RETURN_ERROR_SENSOR;
    }

    rk_sensor_full_info_t *s_info = it->second.ptr();
    int isp_index = s_info->isp_info->logic_id;
    LOGI_CAMHW_SUBM(ISP20HW_SUBM, "sensor_name(%s) is linked to isp_index(%d)",
                    sns_name, isp_index);

    if ((_hdr_mode > 0 && isOnlineByWorkingMode()) ||
            (!_linked_to_isp && !mVicapIspPhyLinkSupported)) {
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "use read back mode!");
        mNoReadBack = false;
    }

    // multimplex mode should be using readback mode
    if (s_info->isp_info->isMultiplex)
        mNoReadBack = false;

    LOGI_CAMHW_SUBM(ISP20HW_SUBM, "isp hw working mode: %s !", mNoReadBack ? "online" : "readback");

    //sof event
    if (!mIspSofStream.ptr()) {
        if (mNoReadBack)
            mIspSofStream = new RKSofEventStream(mIspCoreDev, ISP_POLL_SOF);
        else {
            if (_linked_to_isp)
                mIspSofStream = new RKSofEventStream(mIspCoreDev, ISP_POLL_SOF);
            else
                mIspSofStream = new RKSofEventStream(_cif_csi2_sd, ISP_POLL_SOF);
        }
        mIspSofStream->setPollCallback (this);
    }

    _isp_stream_status = ISP_STREAM_STATUS_INVALID;
    if (/*mIsGroupMode*/true) {
        mIspStremEvtTh = new RkStreamEventPollThread("StreamEvt",
                new V4l2Device (s_info->isp_info->input_params_path),
                this);
    }

    if (!mNoReadBack) {
        setupHdrLink(RK_AIQ_HDR_GET_WORKING_MODE(_hdr_mode), isp_index, true);
        if (!_linked_to_isp) {
            int cif_index = s_info->cif_info->model_idx;
            setupHdrLink_vidcap(_hdr_mode, cif_index, true);
        }
    } else
        setupHdrLink(RK_AIQ_WORKING_MODE_ISP_HDR3, isp_index, false);

    sensorHw = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();
    ret = sensorHw->set_working_mode(mode);
    if (ret) {
        LOGW_CAMHW_SUBM(ISP20HW_SUBM, "set sensor mode error !");
        return ret;
    }

    if (mIsGroupMode) {
        ret = sensorHw->set_sync_mode(mIsMain ? INTERNAL_MASTER_MODE : EXTERNAL_MASTER_MODE);
        if (ret) {
            LOGW_CAMHW_SUBM(ISP20HW_SUBM, "set sensor group mode error !\n");
        }
    } else {
        sensorHw->set_sync_mode(NO_SYNC_MODE);
    }

    mRawCapUnit->set_working_mode(mode);
    mRawProcUnit->set_working_mode(mode);
    setExpDelayInfo(mode);
    setLensVcmCfg(s_info->mod_info);
    xcam_mem_clear(_lens_des);
    if (lensHw.ptr())
        lensHw->getLensModeData(_lens_des);

    _ispp_module_init_ens = 0;

    ret = setupPipelineFmt();
    if (ret < 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "setupPipelineFmt err: %d\n", ret);
    }

    struct v4l2_subdev_format isp_src_fmt;
    memset(&isp_src_fmt, 0, sizeof(isp_src_fmt));
    isp_src_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    isp_src_fmt.pad = 2;
    ret = mIspCoreDev->getFormat(isp_src_fmt);
    if (ret == XCAM_RETURN_NO_ERROR && s_info->isp_info->is_multi_isp_mode) {
        uint16_t extended_pixel = mMultiIspExtendedPixel;
        uint32_t width = isp_src_fmt.format.width;
        uint32_t height = isp_src_fmt.format.height;
        mParamsSplitter        = new IspParamsSplitter();
        mParamsSplitter->SetPicInfo({0, 0, width, height})
        .SetLeftIspRect({0, 0, width / 2 + extended_pixel, height})
        .SetRightIspRect({width / 2 - extended_pixel, 0, width / 2 + extended_pixel, height});
        IspParamsSplitter::Rectangle f = mParamsSplitter->GetPicInfo();
        IspParamsSplitter::Rectangle l = mParamsSplitter->GetLeftIspRect();
        IspParamsSplitter::Rectangle r = mParamsSplitter->GetRightIspRect();
        LOGD_ANALYZER(
            "Set Multi-ISP Mode ParamSplitter:\n"
            " Extended Pixel%d\n"
            " F : { %u, %u, %u, %u }\n"
            " L : { %u, %u, %u, %u }\n"
            " R : { %u, %u, %u, %u }\n",
            extended_pixel,
            f.x, f.y, f.w, f.h,
            l.x, l.y, l.w, l.h,
            r.x, r.y, r.w, r.h);
    }


    if (!_linked_to_isp && !mNoReadBack)
        mRawCapUnit->prepare_cif_mipi();

    if ((_cur_calib_infos.mfnr.enable && _cur_calib_infos.mfnr.motion_detect_en) || _cur_calib_infos.af.ldg_param.enable) {
        mSpStreamUnit->prepare(&_cur_calib_infos.af.ldg_param, &_cur_calib_infos.af.highlight);
    }

    CalibDbV2_Af_Pdaf_t *pdaf;
    if (CHECK_ISP_HW_V30()) {
        CalibDbV2_AFV30_t *af_v30 =
            (CalibDbV2_AFV30_t*)(CALIBDBV2_GET_MODULE_PTR((void*)mCalibDbV2, af_v30));
        pdaf = &af_v30->TuningPara.pdaf;
    } else {
        CalibDbV2_AF_t *af =
            (CalibDbV2_AF_t*)CALIBDBV2_GET_MODULE_PTR((void*)mCalibDbV2, af);
        pdaf = &af->TuningPara.pdaf;
    }

    get_sensor_pdafinfo(s_info, &mPdafInfo);
    if (mPdafInfo.pdaf_support && pdaf->enable) {
        mPdafStreamUnit->prepare(pdaf, &mPdafInfo);
    } else {
        mPdafInfo.pdaf_support = false;
    }

    _state = CAM_HW_STATE_PREPARED;
    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::start()
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<BaseSensorHw> sensorHw;
    SmartPtr<LensHw> lensHw;

    ENTER_CAMHW_FUNCTION();
    sensorHw = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();
    lensHw = mLensDev.dynamic_cast_ptr<LensHw>();

    if (_state != CAM_HW_STATE_PREPARED &&
            _state != CAM_HW_STATE_STOPPED) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "camhw state err: %d\n", ret);
        return XCAM_RETURN_ERROR_FAILED;
    }

    // set inital params
    if (mParamsAssembler.ptr()) {
        mParamsAssembler->setCamPhyId(mCamPhyId);
        ret = mParamsAssembler->start();
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "params assembler start err: %d\n", ret);
        }

        if (mParamsAssembler->ready())
            setIspConfig();
    }

    if (mLumaStream.ptr())
        mLumaStream->start();
    if (mIspSofStream.ptr()) {
        mIspSofStream->setCamPhyId(mCamPhyId);
        mIspSofStream->start();
    }

    if (_linked_to_isp)
        mIspCoreDev->subscribe_event(V4L2_EVENT_FRAME_SYNC);

    if (mIspStremEvtTh.ptr()) {
        ret = mIspStremEvtTh->start();
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "start isp stream event failed: %d\n", ret);
        }
    } else {
        ret = hdr_mipi_start_mode(_hdr_mode);
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "hdr mipi start err: %d\n", ret);
        }
    }

    ret = mIspCoreDev->start();
    if (ret < 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "start isp core dev err: %d\n", ret);
    }
    if (mIspStatsStream.ptr())
        mIspStatsStream->start();

    if (mFlashLight.ptr()) {
        ret = mFlashLight->start();
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "start flashlight err: %d\n", ret);
        }
    }
    if (mFlashLightIr.ptr()) {
        ret = mFlashLightIr->start();
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "start flashlight ir err: %d\n", ret);
        }
    }
    if ((_cur_calib_infos.mfnr.enable && _cur_calib_infos.mfnr.motion_detect_en) || _cur_calib_infos.af.ldg_param.enable) {
        mSpStreamUnit->start();
    }

    if (mPdafInfo.pdaf_support) {
        mPdafStreamUnit->start();
    }

    if (mIspParamStream.ptr())
        mIspParamStream->startThreadOnly();

    if (mNrStreamProcUnit.ptr())
        mNrStreamProcUnit->start();

    if (mTnrStreamProcUnit.ptr())
        mTnrStreamProcUnit->start();

    if (mFecParamStream.ptr())
        mFecParamStream->start();

    sensorHw->start();
    if (lensHw.ptr())
        lensHw->start();
    _is_exit = false;
    _state = CAM_HW_STATE_STARTED;

    EXIT_CAMHW_FUNCTION();
    return ret;
}


XCamReturn
CamHwIsp20::hdr_mipi_prepare_mode(int mode)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    int new_mode = RK_AIQ_HDR_GET_WORKING_MODE(mode);

    if (!mNoReadBack) {
        if (new_mode == RK_AIQ_WORKING_MODE_NORMAL) {
            ret = mRawCapUnit->prepare(MIPI_STREAM_IDX_0);
            ret = mRawProcUnit->prepare(MIPI_STREAM_IDX_0);
        } else if (new_mode == RK_AIQ_WORKING_MODE_ISP_HDR2) {
            ret = mRawCapUnit->prepare(MIPI_STREAM_IDX_0 | MIPI_STREAM_IDX_1);
            ret = mRawProcUnit->prepare(MIPI_STREAM_IDX_0 | MIPI_STREAM_IDX_1);
        } else {
            ret = mRawCapUnit->prepare(MIPI_STREAM_IDX_ALL);
            ret = mRawProcUnit->prepare(MIPI_STREAM_IDX_ALL);
        }
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "hdr mipi start err: %d\n", ret);
        }
    }

    return ret;
}

XCamReturn
CamHwIsp20::hdr_mipi_start_mode(int mode)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "%s enter", __FUNCTION__);
    if (!mNoReadBack) {
        mRawCapUnit->start(mode);
        mRawProcUnit->start(mode);
    }
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "%s exit", __FUNCTION__);
    return ret;
}

XCamReturn
CamHwIsp20::hdr_mipi_stop()
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    mRawProcUnit->stop();
    mRawCapUnit->stop();
    return ret;
}

XCamReturn CamHwIsp20::stop()
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<BaseSensorHw> sensorHw;
    SmartPtr<LensHw> lensHw;

    ENTER_CAMHW_FUNCTION();

    if (_state == CAM_HW_STATE_STOPPED)
        return ret;

    if (mIspStatsStream.ptr())
        mIspStatsStream->stop();
    if (mLumaStream.ptr())
        mLumaStream->stop();
    if (mIspSofStream.ptr())
        mIspSofStream->stop();

    if ((_cur_calib_infos.mfnr.enable && _cur_calib_infos.mfnr.motion_detect_en) || _cur_calib_infos.af.ldg_param.enable) {
        mSpStreamUnit->stop();
    }

    if (mPdafInfo.pdaf_support) {
        mPdafStreamUnit->stop();
    }

    // stop after pollthread, ensure that no new events
    // come into snesorHw
    sensorHw = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();
    sensorHw->stop();

    lensHw = mLensDev.dynamic_cast_ptr<LensHw>();
    if (lensHw.ptr())
        lensHw->stop();

    if (_linked_to_isp)
        mIspCoreDev->unsubscribe_event(V4L2_EVENT_FRAME_SYNC);
    ret = mIspCoreDev->stop();
    if (ret < 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "stop isp core dev err: %d\n", ret);
    }

    if (mIspStremEvtTh.ptr()) {
        if (_isp_stream_status != ISP_STREAM_STATUS_STREAM_OFF) {
            LOGW_CAMHW_SUBM(ISP20HW_SUBM, "wait isp stream stop failed");
            if (mIspParamStream.ptr())
                mIspParamStream->stop();
            hdr_mipi_stop();
            _isp_stream_status = ISP_STREAM_STATUS_INVALID;
        }

        mIspStremEvtTh->stop();
    } else {
        if (!mNoReadBack) {
            ret = hdr_mipi_stop();
            if (ret < 0) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "hdr mipi stop err: %d\n", ret);
            }
        }
    }

    if (mTnrStreamProcUnit.ptr())
        mTnrStreamProcUnit->stop();

    if (mNrStreamProcUnit.ptr())
        mNrStreamProcUnit->stop();

    if (mFecParamStream.ptr())
        mFecParamStream->stop();

    if (mParamsAssembler.ptr())
        mParamsAssembler->stop();

    if (mIspParamStream.ptr())
        mIspParamStream->stop();

    if (mFlashLight.ptr()) {
        ret = mFlashLight->stop();
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "stop flashlight err: %d\n", ret);
        }
    }
    if (mFlashLightIr.ptr()) {
        mFlashLightIr->keep_status(mKpHwSt);
        ret = mFlashLightIr->stop();
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "stop flashlight ir err: %d\n", ret);
        }
    }

    if (!mKpHwSt)
        setIrcutParams(false);

    {
        SmartLock locker (_isp_params_cfg_mutex);
        _camIsp3aResult.clear();
        _effecting_ispparam_map.clear();
    }
    _state = CAM_HW_STATE_STOPPED;

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn CamHwIsp20::pause()
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<BaseSensorHw> sensorHw;

    if (mIspStatsStream.ptr())
        mIspStatsStream->stop();
    if (mIspSofStream.ptr())
        mIspSofStream->stop();
    if (mLumaStream.ptr())
        mLumaStream->stop();
    if (!mNoReadBack)
        hdr_mipi_stop();

    sensorHw = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();
    sensorHw->stop();
    if (mIspParamStream.ptr())
        mIspParamStream->stop();

    if (mTnrStreamProcUnit.ptr())
        mTnrStreamProcUnit->start();

    if (mNrStreamProcUnit.ptr())
        mNrStreamProcUnit->stop();

    if (mFecParamStream.ptr())
        mFecParamStream->stop();

    if (mParamsAssembler.ptr())
        mParamsAssembler->stop();

    if (mPdafStreamUnit.ptr())
        mPdafStreamUnit->stop();

    {
        SmartLock locker (_isp_params_cfg_mutex);
        _camIsp3aResult.clear();
        _effecting_ispparam_map.clear();
    }

    _state = CAM_HW_STATE_PAUSED;
    return ret;
}

XCamReturn CamHwIsp20::swWorkingModeDyn(int mode)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<BaseSensorHw> sensorHw;

    if (_linked_to_isp || mNoReadBack) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "sensor linked to isp, not supported now!");
        return XCAM_RETURN_ERROR_FAILED;
    }

    sensorHw = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();

    ret = sensorHw->set_working_mode(mode);
    if (ret) {
        LOGW_CAMHW_SUBM(ISP20HW_SUBM, "set sensor mode error !");
        return ret;
    }

    setExpDelayInfo(mode);

    Isp20Params::set_working_mode(mode);

#if 0 // for quick switch, not used now
    int old_mode = RK_AIQ_HDR_GET_WORKING_MODE(_hdr_mode);
    int new_mode = RK_AIQ_HDR_GET_WORKING_MODE(mode);
    //set hdr mode to drv
    if (mIspCoreDev.ptr()) {
        int drv_mode = ISP2X_HDR_MODE_NOMAL;
        if (new_mode == RK_AIQ_WORKING_MODE_ISP_HDR3)
            drv_mode = ISP2X_HDR_MODE_X3;
        else if (new_mode == RK_AIQ_WORKING_MODE_ISP_HDR2)
            drv_mode = ISP2X_HDR_MODE_X2;

        if (mIspCoreDev->io_control (RKISP_CMD_SW_HDR_MODE_QUICK, &drv_mode) < 0) {
            ret = XCAM_RETURN_ERROR_FAILED;
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set RKISP_CMD_SW_HDR_MODE_QUICK failed");
            return ret;
        }
    }
    // reconfig tx,rx stream
    if (!old_mode && new_mode) {
        // normal to hdr
        if (new_mode == RK_AIQ_WORKING_MODE_ISP_HDR3)
            hdr_mipi_start(MIPI_STREAM_IDX_1 | MIPI_STREAM_IDX_2);
        else
            hdr_mipi_start(MIPI_STREAM_IDX_1);
    } else if (old_mode && !new_mode) {
        // hdr to normal
        if (new_mode == RK_AIQ_WORKING_MODE_ISP_HDR3)
            hdr_mipi_stop(MIPI_STREAM_IDX_1 | MIPI_STREAM_IDX_2);
        else
            hdr_mipi_stop(MIPI_STREAM_IDX_1);
    } else if (old_mode == RK_AIQ_WORKING_MODE_ISP_HDR3 &&
               new_mode == RK_AIQ_WORKING_MODE_ISP_HDR2) {
        // hdr3 to hdr2
        hdr_mipi_stop(MIPI_STREAM_IDX_1);
    } else if (old_mode == RK_AIQ_WORKING_MODE_ISP_HDR2 &&
               new_mode == RK_AIQ_WORKING_MODE_ISP_HDR3) {
        // hdr3 to hdr2
        hdr_mipi_start(MIPI_STREAM_IDX_2);
    } else {
        // do nothing
        LOGW_CAMHW_SUBM(ISP20HW_SUBM, "do nothing, old mode %d, new mode %d\n",
                        _hdr_mode, mode);
    }
#endif
    _hdr_mode = mode;
    mRawCapUnit->set_working_mode(mode);
    mRawProcUnit->set_working_mode(mode);
    // remap _mipi_tx_devs for cif
    if (!_linked_to_isp && !mNoReadBack)
        mRawCapUnit->prepare_cif_mipi();

    return ret;
}

XCamReturn CamHwIsp20::resume()
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<BaseSensorHw> sensorHw = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();

    // set inital params
    ret = mParamsAssembler->start();
    if (ret < 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "params assembler start err: %d\n", ret);
    }

    if (mParamsAssembler->ready())
        setIspConfig();

    ret = hdr_mipi_start_mode(_hdr_mode);
    if (ret < 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "hdr mipi start err: %d\n", ret);
    }
    sensorHw->start();
    if (mIspSofStream.ptr())
        mIspSofStream->start();
    if (mIspParamStream.ptr())
        mIspParamStream->startThreadOnly();
    if (mLumaStream.ptr())
        mLumaStream->start();
    if (mIspStatsStream.ptr())
        mIspStatsStream->start();

    if (mTnrStreamProcUnit.ptr())
        mTnrStreamProcUnit->start();

    if (mNrStreamProcUnit.ptr())
        mNrStreamProcUnit->start();

    if (mFecParamStream.ptr())
        mFecParamStream->start();

    if (mPdafStreamUnit.ptr())
        mPdafStreamUnit->start();

    _state = CAM_HW_STATE_STARTED;
    return ret;
}

/*
 * some module(HDR/TNR) parameters are related to the next frame exposure
 * and can only be easily obtained at the hwi layer,
 * so these parameters are calculated at hwi and the result is overwritten.
 */
XCamReturn
CamHwIsp20::overrideExpRatioToAiqResults(const sint32_t frameId,
        int module_id,
        cam3aResultList &results,
        int hdr_mode)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<RkAiqExpParamsProxy> curFrameExpParam;
    SmartPtr<RkAiqExpParamsProxy> nextFrameExpParam;
    SmartPtr<BaseSensorHw> mSensorSubdev = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();

    if (mSensorSubdev.ptr()) {
        if (mSensorSubdev->getEffectiveExpParams(curFrameExpParam, frameId) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "exp-sync: module_id: 0x%x, rx id: %d\n",
                            module_id,
                            frameId);
            return ret;
        }

        if (mSensorSubdev->getEffectiveExpParams(nextFrameExpParam, frameId + 1) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "exp-sync: module_id: 0x%x, rx id: %d\n",
                            module_id,
                            frameId + 1);
            return ret;
        }
    }

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "exp-sync: module_id: 0x%x, rx id: %d\n"
                    "curFrame(%d): lexp: %f-%f, mexp: %f-%f, sexp: %f-%f\n"
                    "nextFrame(%d): lexp: %f-%f, mexp: %f-%f, sexp: %f-%f\n",
                    module_id,
                    frameId,
                    frameId,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[2].exp_real_params.analog_gain,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[2].exp_real_params.integration_time,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[1].exp_real_params.analog_gain,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[1].exp_real_params.integration_time,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[0].exp_real_params.analog_gain,
                    curFrameExpParam->data()->aecExpInfo.HdrExp[0].exp_real_params.integration_time,
                    frameId + 1,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[2].exp_real_params.analog_gain,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[2].exp_real_params.integration_time,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[1].exp_real_params.analog_gain,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[1].exp_real_params.integration_time,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[0].exp_real_params.analog_gain,
                    nextFrameExpParam->data()->aecExpInfo.HdrExp[0].exp_real_params.integration_time);

    rk_aiq_luma_params_t curFrameLumaParam, nextFrameLumaParam;

    //get expo info for merge and tmo
    float expo[6];
    int FrameCnt = 0;
    if(hdr_mode == RK_AIQ_WORKING_MODE_NORMAL)
        FrameCnt = 1;
    else if(hdr_mode >= RK_AIQ_WORKING_MODE_ISP_HDR2 && hdr_mode < RK_AIQ_WORKING_MODE_ISP_HDR3)
        FrameCnt = 2;
    else if(hdr_mode >= RK_AIQ_WORKING_MODE_ISP_HDR3)
        FrameCnt = 3;
    else {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get HDR mode failed!\n");
        return ret;
    }

    hdrtmoGetAeInfo(&nextFrameExpParam->data()->aecExpInfo,
                    &curFrameExpParam->data()->aecExpInfo, FrameCnt, expo);

    float curSExpo = expo[0];
    float curMExpo = expo[1];
    float curLExpo = expo[2];
    float nextSExpo = expo[3];
    float nextMExpo = expo[4];
    float nextLExpo = expo[5];
    float nextRatioLS = 0;
    float nextRatioLM = 0;
    float curRatioLS = 0;
    if(FrameCnt == 1) {
        nextRatioLS = 1;
        nextRatioLM = 1;
        curRatioLS = 1;
    } else if(FrameCnt == 2) {
        nextRatioLS = nextLExpo / nextSExpo;
        nextRatioLM = 1;
        curRatioLS = curLExpo / curSExpo;
    } else if(FrameCnt == 3) {
        nextRatioLS = nextLExpo / nextSExpo;
        nextRatioLM = nextLExpo / nextMExpo;
        curRatioLS = curLExpo / curSExpo;
    }

    float nextLgmax = 12 + log(nextRatioLS) / log(2);
    float curLgmax = 12 + log(curRatioLS) / log(2);


    switch (module_id) {
    case RK_ISP2X_HDRTMO_ID:
    {
        float nextSLuma[16] ;
        float curSLuma[16] ;
        float nextMLuma[16] ;
        float curMLuma[16] ;
        float nextLLuma[16];
        float curLLuma[16];
        float lgmin = 0;

        SmartPtr<cam3aResult> res = get_3a_module_result(results, RESULT_TYPE_TMO_PARAM);
        SmartPtr<RkAiqIspTmoParamsProxy> tmoParams;
        if (res.ptr()) {
            tmoParams = res.dynamic_cast_ptr<RkAiqIspTmoParamsProxy>();
        } else {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get tmo params from 3a result failed!\n");
            return ret;
        }
        rk_aiq_isp_tmo_t &tmo_proc_res = tmoParams->data()->result;

        if(!(tmo_proc_res.bTmoEn))
            break;

        if(tmo_proc_res.LongFrameMode) {
            nextRatioLS = 1;
            nextRatioLM = 1;
            curRatioLS = 1;
        }

        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "nextRatioLS:%f nextRatioLM:%f curRatioLS:%f\n", nextRatioLS, nextRatioLM, curRatioLS);
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "nextLgmax:%f curLgmax:%f \n", nextLgmax, curLgmax);

        // shadow resgister,needs to set a frame before, for ctrl_cfg/lg_scl reg
        tmo_proc_res.Res.sw_hdrtmo_expl_lgratio = \
                (int)(2048 * (log(curLExpo / nextLExpo) / log(2)));
        if( tmo_proc_res.LongFrameMode || tmo_proc_res.isLinearTmo)
            tmo_proc_res.Res.sw_hdrtmo_lgscl_ratio = 128;
        else
            tmo_proc_res.Res.sw_hdrtmo_lgscl_ratio = \
                    (int)(128 * (log(nextRatioLS) / log(curRatioLS)));
        tmo_proc_res.Res.sw_hdrtmo_lgscl = (int)(4096 * 16 / nextLgmax);
        tmo_proc_res.Res.sw_hdrtmo_lgscl_inv = (int)(4096 * nextLgmax / 16);

        // not shadow resgister
        tmo_proc_res.Res.sw_hdrtmo_lgmax = (int)(2048 * nextLgmax);
        tmo_proc_res.Res.sw_hdrtmo_set_lgmax = \
                                               tmo_proc_res.Res.sw_hdrtmo_lgmax;

        //sw_hdrtmo_set_lgrange0
        float value = 0.0;
        float clipratio0 = tmo_proc_res.Res.sw_hdrtmo_clipratio0 / 256.0;
        float clipgap0 = tmo_proc_res.Res.sw_hdrtmo_clipgap0 / 4.0;
        float Lgmax = tmo_proc_res.Res.sw_hdrtmo_set_lgmax / 2048.0;
        value = lgmin * (1 - clipratio0) + Lgmax * clipratio0;
        value = MIN(value, (lgmin + clipgap0));
        tmo_proc_res.Res.sw_hdrtmo_set_lgrange0 = (int)(2048 * value);

        //sw_hdrtmo_set_lgrange1
        value = 0.0;
        float clipratio1 = tmo_proc_res.Res.sw_hdrtmo_clipratio1 / 256.0;
        float clipgap1 = tmo_proc_res.Res.sw_hdrtmo_clipgap1 / 4.0;
        value = lgmin * (1 - clipratio1) + Lgmax * clipratio1;
        value = MAX(value, (Lgmax - clipgap1));
        tmo_proc_res.Res.sw_hdrtmo_set_lgrange1 = (int)(2048 * value);

        LOGD_CAMHW_SUBM(ISP20HW_SUBM,
                        "sw_hdrtmo_expl_lgratio:%d sw_hdrtmo_lgscl_ratio:%d "
                        "sw_hdrtmo_lgmax:%d sw_hdrtmo_set_lgmax:%d sw_hdrtmo_lgscl:%d sw_hdrtmo_lgscl_inv:%d\n",
                        tmo_proc_res.Res.sw_hdrtmo_expl_lgratio,
                        tmo_proc_res.Res.sw_hdrtmo_lgscl_ratio,
                        tmo_proc_res.Res.sw_hdrtmo_lgmax,
                        tmo_proc_res.Res.sw_hdrtmo_set_lgmax,
                        tmo_proc_res.Res.sw_hdrtmo_lgscl,
                        tmo_proc_res.Res.sw_hdrtmo_lgscl_inv);
        LOGD_CAMHW_SUBM(ISP20HW_SUBM,
                        "sw_hdrtmo_set_lgrange0:%d sw_hdrtmo_set_lgrange1:%d\n",
                        tmo_proc_res.Res.sw_hdrtmo_set_lgrange0,
                        tmo_proc_res.Res.sw_hdrtmo_set_lgrange1);

        //predict
        SmartPtr<cam3aResult> blc_res = get_3a_module_result(results, RESULT_TYPE_BLC_PARAM);
        SmartPtr<RkAiqIspBlcParamsProxy> blcParams;
        if (blc_res.ptr()) {
            blcParams = blc_res.dynamic_cast_ptr<RkAiqIspBlcParamsProxy>();
        } else {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get blc params from 3a result failed!\n");
            return ret;
        }

        rk_aiq_isp_blc_t &blc = blcParams->data()->result;

        float blc_result = (blc.blc_r + blc.blc_gr
                            + blc.blc_gb + blc.blc_b) / (16.0 * 4.0);
        int cols = tmo_proc_res.TmoFlicker.width;
        int rows = tmo_proc_res.TmoFlicker.height;
        int PixelNum = cols * rows;
        int PixelNumBlock = PixelNum / ISP2X_MIPI_LUMA_MEAN_MAX;

        //get luma info
        float luma[96];
        hdrtmoGetLumaInfo(&nextFrameLumaParam, &curFrameLumaParam, FrameCnt,
                          PixelNumBlock, blc_result, luma);

        //get predict para
        bool SceneStable = hdrtmoSceneStable(frameId, tmo_proc_res.TmoFlicker.iirmax,
                                             tmo_proc_res.TmoFlicker.iir,
                                             tmo_proc_res.Res.sw_hdrtmo_set_weightkey,
                                             FrameCnt + 1,
                                             tmo_proc_res.TmoFlicker.LumaDeviation,
                                             tmo_proc_res.TmoFlicker.StableThr);
        int PredicPara = 0;//hdrtmoPredictK(luma, expo,
        float GlobalTmoStrength = tmo_proc_res.TmoFlicker.GlobalTmoStrength;
        tmo_proc_res.Predict.Scenestable = SceneStable;
        tmo_proc_res.Predict.K_Rolgmean = PredicPara;
        tmo_proc_res.Predict.cnt_mode = tmo_proc_res.TmoFlicker.cnt_mode;
        tmo_proc_res.Predict.cnt_vsize = tmo_proc_res.TmoFlicker.cnt_vsize;
        tmo_proc_res.Predict.iir_max = tmo_proc_res.TmoFlicker.iirmax;
        tmo_proc_res.Predict.iir = tmo_proc_res.TmoFlicker.iir;
        tmo_proc_res.Predict.global_tmo_strength = 2048 * log(GlobalTmoStrength) / log(2);
        if (tmo_proc_res.TmoFlicker.GlobalTmoStrengthDown)
            tmo_proc_res.Predict.global_tmo_strength *= -1;

        LOGD_CAMHW_SUBM(ISP20HW_SUBM,
                        "SceneStable:%d K_Rolgmean:%d iir:%d iir_max:%d global_tmo_strength:%d\n",
                        tmo_proc_res.Predict.Scenestable,
                        tmo_proc_res.Predict.K_Rolgmean, tmo_proc_res.Predict.iir,
                        tmo_proc_res.Predict.iir_max, tmo_proc_res.Predict.global_tmo_strength);

        break;
    }
    case RK_ISP2X_HDRMGE_ID:
    {
        if(FrameCnt == 1)
            break;

        SmartPtr<cam3aResult> res = get_3a_module_result(results, RESULT_TYPE_MERGE_PARAM);
        SmartPtr<RkAiqIspMergeParamsProxy> mergeParams;
        if (res.ptr()) {
            mergeParams = res.dynamic_cast_ptr<RkAiqIspMergeParamsProxy>();
        } else {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get merge params from 3a result failed!\n");
            return ret;
        }

        rk_aiq_isp_merge_t &merge_proc_res = mergeParams->data()->result;

        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "nextRatioLS:%f nextRatioLM:%f curRatioLS:%f\n", nextRatioLS, nextRatioLM, curRatioLS);
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "nextLgmax:%f curLgmax:%f \n", nextLgmax, curLgmax);

        // shadow resgister,needs to set a frame before, for gain0/1/2 reg
        merge_proc_res.Merge_v20.sw_hdrmge_gain0 = (int)(64 * nextRatioLS);
        if(nextRatioLS == 1)
            merge_proc_res.Merge_v20.sw_hdrmge_gain0_inv = (int)(4096 * (1 / nextRatioLS) - 1);
        else
            merge_proc_res.Merge_v20.sw_hdrmge_gain0_inv = (int)(4096 * (1 / nextRatioLS));
        merge_proc_res.Merge_v20.sw_hdrmge_gain1 = (int)(64 * nextRatioLM);
        if(nextRatioLM == 1)
            merge_proc_res.Merge_v20.sw_hdrmge_gain1_inv = (int)(4096 * (1 / nextRatioLM) - 1);
        else
            merge_proc_res.Merge_v20.sw_hdrmge_gain1_inv = (int)(4096 * (1 / nextRatioLM));

        LOGD_CAMHW_SUBM(ISP20HW_SUBM,
                        "sw_hdrmge_gain0:%d sw_hdrmge_gain0_inv:%d sw_hdrmge_gain1:%d sw_hdrmge_gain1_inv:%d\n",
                        merge_proc_res.Merge_v20.sw_hdrmge_gain0,
                        merge_proc_res.Merge_v20.sw_hdrmge_gain0_inv,
                        merge_proc_res.Merge_v20.sw_hdrmge_gain1,
                        merge_proc_res.Merge_v20.sw_hdrmge_gain1_inv);
        break;
    }
    case RK_ISP2X_PP_TNR_ID:
        break;
    default:
        LOGW_CAMHW_SUBM(ISP20HW_SUBM, "unkown module id: 0x%x!\n", module_id);
        break;
    }

    return ret;
}

void
CamHwIsp20::gen_full_ispp_params(const struct rkispp_params_cfg* update_params,
                                 struct rkispp_params_cfg* full_params)
{
    XCAM_ASSERT (update_params);
    XCAM_ASSERT (full_params);

    int end = RK_ISP2X_PP_MAX_ID - RK_ISP2X_PP_TNR_ID;

    ENTER_CAMHW_FUNCTION();
    for (int i = 0; i < end; i++)
        if (update_params->module_en_update & (1 << i)) {
            full_params->module_en_update |= 1 << i;
            // clear old bit value
            full_params->module_ens &= ~(1 << i);
            // set new bit value
            full_params->module_ens |= update_params->module_ens & (1LL << i);
        }

    for (int i = 0; i < end; i++)
        if (update_params->module_cfg_update & (1 << i)) {
            full_params->module_cfg_update |= 1 << i;
        }

    EXIT_CAMHW_FUNCTION();
}

void
CamHwIsp20::gen_full_isp_params(const struct isp2x_isp_params_cfg* update_params,
                                struct isp2x_isp_params_cfg* full_params,
                                uint64_t* module_en_update_partial,
                                uint64_t* module_cfg_update_partial)
{
    XCAM_ASSERT (update_params);
    XCAM_ASSERT (full_params);
    int i = 0;

    ENTER_CAMHW_FUNCTION();
    for (; i <= RK_ISP2X_MAX_ID; i++)
        if (update_params->module_en_update & (1LL << i)) {
            if ((full_params->module_ens & (1LL << i)) !=
                    (update_params->module_ens & (1LL << i)))
                *module_en_update_partial |= 1LL << i;
            full_params->module_en_update |= 1LL << i;
            // clear old bit value
            full_params->module_ens &= ~(1LL << i);
            // set new bit value
            full_params->module_ens |= update_params->module_ens & (1LL << i);
        }

    for (i = 0; i <= RK_ISP2X_MAX_ID; i++) {
        if (update_params->module_cfg_update & (1LL << i)) {

#define CHECK_UPDATE_PARAMS(dst, src) \
            if (memcmp(&dst, &src, sizeof(dst)) == 0 && \
                       full_params->frame_id > ISP_PARAMS_EFFECT_DELAY_CNT) \
                continue; \
            *module_cfg_update_partial |= 1LL << i; \
            dst = src; \

            full_params->module_cfg_update |= 1LL << i;
            switch (i) {
            case RK_ISP2X_RAWAE3_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawae3, update_params->meas.rawae3);
                break;
            case RK_ISP2X_RAWAE1_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawae1, update_params->meas.rawae1);
                break;
            case RK_ISP2X_RAWAE2_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawae2, update_params->meas.rawae2);
                break;
            case RK_ISP2X_RAWAE0_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawae0, update_params->meas.rawae0);
                break;
            case RK_ISP2X_RAWHIST3_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawhist3, update_params->meas.rawhist3);
                break;
            case RK_ISP2X_RAWHIST1_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawhist1, update_params->meas.rawhist1);
                break;
            case RK_ISP2X_RAWHIST2_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawhist2, update_params->meas.rawhist2);
                break;
            case RK_ISP2X_RAWHIST0_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawhist0, update_params->meas.rawhist0);
                break;
            case RK_ISP2X_YUVAE_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.yuvae, update_params->meas.yuvae);
                break;
            case RK_ISP2X_SIHST_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.sihst, update_params->meas.sihst);
                break;
            case RK_ISP2X_SIAWB_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.siawb, update_params->meas.siawb);
                break;
            case RK_ISP2X_RAWAWB_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawawb, update_params->meas.rawawb);
                break;
            case RK_ISP2X_AWB_GAIN_ID:
                CHECK_UPDATE_PARAMS(full_params->others.awb_gain_cfg, update_params->others.awb_gain_cfg);
                break;
            case RK_ISP2X_RAWAF_ID:
                CHECK_UPDATE_PARAMS(full_params->meas.rawaf, update_params->meas.rawaf);
                break;
            case RK_ISP2X_HDRMGE_ID:
                CHECK_UPDATE_PARAMS(full_params->others.hdrmge_cfg, update_params->others.hdrmge_cfg);
                break;
            case RK_ISP2X_HDRTMO_ID:
                CHECK_UPDATE_PARAMS(full_params->others.hdrtmo_cfg, update_params->others.hdrtmo_cfg);
                break;
            case RK_ISP2X_CTK_ID:
                CHECK_UPDATE_PARAMS(full_params->others.ccm_cfg, update_params->others.ccm_cfg);
                break;
            case RK_ISP2X_LSC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.lsc_cfg, update_params->others.lsc_cfg);
                break;
            case RK_ISP2X_GOC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.gammaout_cfg, update_params->others.gammaout_cfg);
                break;
            case RK_ISP2X_3DLUT_ID:
                CHECK_UPDATE_PARAMS(full_params->others.isp3dlut_cfg, update_params->others.isp3dlut_cfg);
                break;
            case RK_ISP2X_DPCC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.dpcc_cfg, update_params->others.dpcc_cfg);
                break;
            case RK_ISP2X_BLS_ID:
                CHECK_UPDATE_PARAMS(full_params->others.bls_cfg, update_params->others.bls_cfg);
                break;
            case RK_ISP2X_DEBAYER_ID:
                CHECK_UPDATE_PARAMS(full_params->others.debayer_cfg, update_params->others.debayer_cfg);
                break;
            case RK_ISP2X_DHAZ_ID:
                CHECK_UPDATE_PARAMS(full_params->others.dhaz_cfg, update_params->others.dhaz_cfg);
                break;
            case RK_ISP2X_RAWNR_ID:
                CHECK_UPDATE_PARAMS(full_params->others.rawnr_cfg, update_params->others.rawnr_cfg);
                break;
            case RK_ISP2X_GAIN_ID:
                CHECK_UPDATE_PARAMS(full_params->others.gain_cfg, update_params->others.gain_cfg);
                break;
            case RK_ISP2X_LDCH_ID:
                CHECK_UPDATE_PARAMS(full_params->others.ldch_cfg, update_params->others.ldch_cfg);
                break;
            case RK_ISP2X_GIC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.gic_cfg, update_params->others.gic_cfg);
                break;
            case RK_ISP2X_CPROC_ID:
                CHECK_UPDATE_PARAMS(full_params->others.cproc_cfg, update_params->others.cproc_cfg);
                break;
            case RK_ISP2X_SDG_ID:
                CHECK_UPDATE_PARAMS(full_params->others.sdg_cfg, update_params->others.sdg_cfg);
                break;
            default:
                break;
            }
        }
    }
    EXIT_CAMHW_FUNCTION();
}

#if 0
void CamHwIsp20::dump_isp_config(struct isp2x_isp_params_cfg* isp_params,
                                 SmartPtr<RkAiqIspParamsProxy> aiq_results,
                                 SmartPtr<RkAiqIspParamsProxy> aiq_other_results)
{
    rk_aiq_isp_meas_params_v20_t* isp20_meas_result =
        static_cast<rk_aiq_isp_meas_params_v20_t*>(aiq_results->data().ptr());
    rk_aiq_isp_other_params_v20_t* isp20_other_result =
        static_cast<rk_aiq_isp_other_params_v20_t*>(aiq_other_results->data().ptr());

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp_params mask: 0x%llx-0x%llx-0x%llx\n",
                    isp_params->module_en_update,
                    isp_params->module_ens,
                    isp_params->module_cfg_update);

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "aiq_results: ae-lite.winnum=%d; ae-big2.winnum=%d, sub[1].size: [%dx%d]\n",
                    isp20_meas_result->aec_meas.rawae0.wnd_num,
                    isp20_meas_result->aec_meas.rawae1.wnd_num,
                    isp20_meas_result->aec_meas.rawae1.subwin[1].h_size,
                    isp20_meas_result->aec_meas.rawae1.subwin[1].v_size);

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp_params: ae-lite.winnum=%d; ae-big2.winnum=%d, sub[1].size: [%dx%d]\n",
                    isp_params->meas.rawae0.wnd_num,
                    isp_params->meas.rawae1.wnd_num,
                    isp_params->meas.rawae1.subwin[1].h_size,
                    isp_params->meas.rawae1.subwin[1].v_size);

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp_params: win size: [%dx%d]-[%dx%d]-[%dx%d]-[%dx%d]\n",
                    isp_params->meas.rawae0.win.h_size,
                    isp_params->meas.rawae0.win.v_size,
                    isp_params->meas.rawae3.win.h_size,
                    isp_params->meas.rawae3.win.v_size,
                    isp_params->meas.rawae1.win.h_size,
                    isp_params->meas.rawae1.win.v_size,
                    isp_params->meas.rawae2.win.h_size,
                    isp_params->meas.rawae2.win.v_size);

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp_params: debayer:");
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "clip_en:%d, dist_scale:%d, filter_c_en:%d, filter_g_en:%d",
                    isp_params->others.debayer_cfg.clip_en,
                    isp_params->others.debayer_cfg.dist_scale,
                    isp_params->others.debayer_cfg.filter_c_en,
                    isp_params->others.debayer_cfg.filter_g_en);
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "gain_offset:%d,hf_offset:%d,max_ratio:%d,offset:%d,order_max:%d",
                    isp_params->others.debayer_cfg.gain_offset,
                    isp_params->others.debayer_cfg.hf_offset,
                    isp_params->others.debayer_cfg.max_ratio,
                    isp_params->others.debayer_cfg.offset,
                    isp_params->others.debayer_cfg.order_max);
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "order_min:%d, shift_num:%d, thed0:%d, thed1:%d",
                    isp_params->others.debayer_cfg.order_min,
                    isp_params->others.debayer_cfg.shift_num,
                    isp_params->others.debayer_cfg.thed0,
                    isp_params->others.debayer_cfg.thed1);
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "filter1:[%d, %d, %d, %d, %d]",
                    isp_params->others.debayer_cfg.filter1_coe1,
                    isp_params->others.debayer_cfg.filter1_coe2,
                    isp_params->others.debayer_cfg.filter1_coe3,
                    isp_params->others.debayer_cfg.filter1_coe4,
                    isp_params->others.debayer_cfg.filter1_coe5);
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "filter2:[%d, %d, %d, %d, %d]",
                    isp_params->others.debayer_cfg.filter2_coe1,
                    isp_params->others.debayer_cfg.filter2_coe2,
                    isp_params->others.debayer_cfg.filter2_coe3,
                    isp_params->others.debayer_cfg.filter2_coe4,
                    isp_params->others.debayer_cfg.filter2_coe5);

    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "isp_params: gic: \n"
                    "edge_open:%d, regmingradthrdark2:%d, regmingradthrdark1:%d, regminbusythre:%d\n"
                    "regdarkthre:%d,regmaxcorvboth:%d,regdarktthrehi:%d,regkgrad2dark:%d,regkgrad1dark:%d\n"
                    "regstrengthglobal_fix:%d, regdarkthrestep:%d, regkgrad2:%d, regkgrad1:%d\n"
                    "reggbthre:%d, regmaxcorv:%d, regmingradthr2:%d, regmingradthr1:%d, gr_ratio:%d\n"
                    "dnloscale:%d, dnhiscale:%d, reglumapointsstep:%d, gvaluelimitlo:%d, gvaluelimithi:%d\n"
                    "fratiohilimt1:%d, strength_fix:%d, noise_cuten:%d, noise_coe_a:%d, noise_coe_b:%d, diff_clip:%d\n",
                    isp_params->others.gic_cfg.edge_open,
                    isp_params->others.gic_cfg.regmingradthrdark2,
                    isp_params->others.gic_cfg.regmingradthrdark1,
                    isp_params->others.gic_cfg.regminbusythre,
                    isp_params->others.gic_cfg.regdarkthre,
                    isp_params->others.gic_cfg.regmaxcorvboth,
                    isp_params->others.gic_cfg.regdarktthrehi,
                    isp_params->others.gic_cfg.regkgrad2dark,
                    isp_params->others.gic_cfg.regkgrad1dark,
                    isp_params->others.gic_cfg.regstrengthglobal_fix,
                    isp_params->others.gic_cfg.regdarkthrestep,
                    isp_params->others.gic_cfg.regkgrad2,
                    isp_params->others.gic_cfg.regkgrad1,
                    isp_params->others.gic_cfg.reggbthre,
                    isp_params->others.gic_cfg.regmaxcorv,
                    isp_params->others.gic_cfg.regmingradthr2,
                    isp_params->others.gic_cfg.regmingradthr1,
                    isp_params->others.gic_cfg.gr_ratio,
                    isp_params->others.gic_cfg.dnloscale,
                    isp_params->others.gic_cfg.dnhiscale,
                    isp_params->others.gic_cfg.reglumapointsstep,
                    isp_params->others.gic_cfg.gvaluelimitlo,
                    isp_params->others.gic_cfg.gvaluelimithi,
                    isp_params->others.gic_cfg.fusionratiohilimt1,
                    isp_params->others.gic_cfg.regstrengthglobal_fix,
                    isp_params->others.gic_cfg.noise_cut_en,
                    isp_params->others.gic_cfg.noise_coe_a,
                    isp_params->others.gic_cfg.noise_coe_b,
                    isp_params->others.gic_cfg.diff_clip);
    for(int i = 0; i < ISP2X_GIC_SIGMA_Y_NUM; i++) {
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "sigma_y[%d]=%d\n", i, isp_params->others.gic_cfg.sigma_y[i]);
    }
#if 0 //TODO Merge
    LOGD_CAMHW_SUBM(ISP20HW_SUBM, "aiq_results: gic: dnloscale=%f, dnhiscale=%f,gvaluelimitlo=%f,gvaluelimithi=%f,fusionratiohilimt1=%f"
                    "textureStrength=%f,globalStrength=%f,noiseCurve_0=%f,noiseCurve_1=%f",
                    isp20_other_result->gic.dnloscale, isp20_other_result->gic.dnhiscale,
                    isp20_other_result->gic.gvaluelimitlo, isp20_other_result->gic.gvaluelimithi,
                    isp20_other_result->gic.fusionratiohilimt1, isp20_other_result->gic.textureStrength,
                    isp20_other_result->gic.globalStrength, isp20_other_result->gic.noiseCurve_0,
                    isp20_other_result->gic.noiseCurve_1);
    for(int i = 0; i < ISP2X_GIC_SIGMA_Y_NUM; i++) {
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "sigma[%d]=%f\n", i, isp20_other_result->gic.sigma_y[i]);
    }
#endif
}
#endif

XCamReturn
CamHwIsp20::setIsppSharpFbcRot(struct rkispp_sharp_config* shp_cfg)
{
    // check if sharp enable
    // check if fec disable
    if ((_ispp_module_init_ens & ISPP_MODULE_SHP) &&
            !(_ispp_module_init_ens & ISPP_MODULE_FEC)) {
        switch (_sharp_fbc_rotation) {
        case RK_AIQ_ROTATION_0 :
            shp_cfg->rotation = 0;
            break;
        case RK_AIQ_ROTATION_90 :
            shp_cfg->rotation = 1;
            break;
        case RK_AIQ_ROTATION_270 :
            shp_cfg->rotation = 3;
            break;
        default:
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "wrong rotation %d\n", _sharp_fbc_rotation);
            return XCAM_RETURN_ERROR_PARAM;
        }
    } else {
        if (_sharp_fbc_rotation != RK_AIQ_ROTATION_0) {
            shp_cfg->rotation = 0;
            _sharp_fbc_rotation = RK_AIQ_ROTATION_0;
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "can't set sharp config, check fec & sharp config\n");
            return XCAM_RETURN_ERROR_PARAM;
        }
    }

    LOGD("sharp rotation %d", _sharp_fbc_rotation);

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CamHwIsp20::showOtpPdafData(struct rkmodule_pdaf_inf *otp_pdaf)
{
    unsigned int gainmap_w, gainmap_h;
    unsigned int dccmap_w, dccmap_h;
    char print_buf[256];
    unsigned int i, j;

    if (otp_pdaf->flag) {
        gainmap_w = otp_pdaf->gainmap_width;
        gainmap_h = otp_pdaf->gainmap_height;
        dccmap_w = otp_pdaf->dccmap_width;
        dccmap_h = otp_pdaf->dccmap_height;
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "[RKPDAFOTPParam]");
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "flag=%d;", otp_pdaf->flag);
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "gainmap_width=%d;", gainmap_w);
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "gainmap_height=%d;", gainmap_h);
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "gainmap_table=");
        for (i = 0; i < gainmap_h; i++) {
            memset(print_buf, 0, sizeof(print_buf));
            for (j = 0; j < gainmap_w; j++) {
                sprintf(print_buf + strlen(print_buf), "%d ", otp_pdaf->gainmap[i * gainmap_w + j]);
            }
            LOGI_CAMHW_SUBM(ISP20HW_SUBM, "%s", print_buf);
        }

        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "dcc_mode=%d;", otp_pdaf->dcc_mode);
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "dcc_dir=%d;", otp_pdaf->dcc_dir);
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "dccmap_width=%d;", otp_pdaf->dccmap_width);
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "dccmap_height=%d;", otp_pdaf->dccmap_height);
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "dccmap_table=");
        for (i = 0; i < dccmap_h; i++) {
            memset(print_buf, 0, sizeof(print_buf));
            for (j = 0; j < dccmap_w; j++) {
                sprintf(print_buf + strlen(print_buf), "%d ", otp_pdaf->dccmap[i * dccmap_w + j]);
            }
            LOGI_CAMHW_SUBM(ISP20HW_SUBM, "%s", print_buf);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CamHwIsp20::showOtpAfData(struct rkmodule_af_inf *af_inf)
{
    unsigned int i;

    if (af_inf->flag) {
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "[RKAFOTPParam]");
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "flag=%d;", af_inf->flag);
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "dir_cnt=%d;", af_inf->dir_cnt);

        for (i = 0; i < af_inf->dir_cnt; i++) {
            LOGI_CAMHW_SUBM(ISP20HW_SUBM, "af_inf=%d;", af_inf->af_otp[i].vcm_dir);
            LOGI_CAMHW_SUBM(ISP20HW_SUBM, "af_macro=%d;", af_inf->af_otp[i].vcm_start);
            LOGI_CAMHW_SUBM(ISP20HW_SUBM, "af_macro=%d;", af_inf->af_otp[i].vcm_end);
        }
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CamHwIsp20::getSensorModeData(const char* sns_ent_name,
                              rk_aiq_exposure_sensor_descriptor& sns_des)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    const SmartPtr<BaseSensorHw> mSensorSubdev = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();
    const SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();
    struct v4l2_subdev_selection select;

    ret = mSensorSubdev->getSensorModeData(sns_ent_name, sns_des);
    if (ret) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "getSensorModeData failed \n");
        return ret;
    }

    xcam_mem_clear (select);
    ret = mIspCoreDev->get_selection(0, V4L2_SEL_TGT_CROP, select);
    if (ret == XCAM_RETURN_NO_ERROR) {
        sns_des.isp_acq_width = select.r.width;
        sns_des.isp_acq_height = select.r.height;
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "get isp acq,w: %d, h: %d\n",
                        sns_des.isp_acq_width,
                        sns_des.isp_acq_height);
    } else {
        LOGW_CAMHW_SUBM(ISP20HW_SUBM, "get selecttion error \n");
        sns_des.isp_acq_width = sns_des.sensor_output_width;
        sns_des.isp_acq_height = sns_des.sensor_output_height;
        ret = XCAM_RETURN_NO_ERROR;
    }

    xcam_mem_clear (sns_des.lens_des);
    if (mLensSubdev.ptr())
        mLensSubdev->getLensModeData(sns_des.lens_des);

    std::map<std::string, SmartPtr<rk_sensor_full_info_t>>::iterator iter_sns_info;
    if ((iter_sns_info = mSensorHwInfos.find(sns_name)) == mSensorHwInfos.end()) {
        LOGW_CAMHW_SUBM(ISP20HW_SUBM, "can't find sensor %s", sns_name);
    } else {
        struct rkmodule_inf *minfo = &(iter_sns_info->second->mod_info);
        if (minfo->awb.flag)
            memcpy(&sns_des.otp_awb, &minfo->awb, sizeof(minfo->awb));
        else
            minfo->awb.flag = 0;

        if (minfo->lsc.flag)
            sns_des.otp_lsc = &minfo->lsc;
        else
            sns_des.otp_lsc = nullptr;

        if (minfo->af.flag) {
            sns_des.otp_af = &minfo->af;
            showOtpAfData(sns_des.otp_af);
        } else {
            sns_des.otp_af = nullptr;
        }

        if (minfo->pdaf.flag) {
            sns_des.otp_pdaf = &minfo->pdaf;
            showOtpPdafData(sns_des.otp_pdaf);
        } else {
            sns_des.otp_pdaf = nullptr;
        }
    }

    return ret;
}

XCamReturn
CamHwIsp20::setExposureParams(SmartPtr<RkAiqExpParamsProxy>& expPar)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();
    SmartPtr<BaseSensorHw> mSensorSubdev = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();
    SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();

    //exp
    ret = mSensorSubdev->setExposureParams(expPar);
    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::setIrisParams(SmartPtr<RkAiqIrisParamsProxy>& irisPar, CalibDb_IrisTypeV2_t irisType)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();
    SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();

    if(irisType == IRISV2_P_TYPE) {   //P-iris
        int step = irisPar->data()->PIris.step;
        bool update = irisPar->data()->PIris.update;

        if (mLensSubdev.ptr() && update) {
            LOGE("|||set P-Iris step: %d", step);
            if (mLensSubdev->setPIrisParams(step) < 0) {
                LOGE("set P-Iris step failed to device");
                return XCAM_RETURN_ERROR_IOCTL;
            }
        }
    } else if(irisType == IRISV2_DC_TYPE) {
        //DC-iris
        int PwmDuty = irisPar->data()->DCIris.pwmDuty;
        bool update = irisPar->data()->DCIris.update;

        if (mLensSubdev.ptr() && update) {
            LOGE("|||set DC-Iris PwmDuty: %d", PwmDuty);
            if (mLensSubdev->setDCIrisParams(PwmDuty) < 0) {
                LOGE("set DC-Iris PwmDuty failed to device");
                return XCAM_RETURN_ERROR_IOCTL;
            }
        }
    }
    EXIT_CAMHW_FUNCTION();
    return ret;
}


XCamReturn
CamHwIsp20::setFocusParams(SmartPtr<RkAiqFocusParamsProxy>& focus_params)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();
    SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();
    rk_aiq_focus_params_t* p_focus = &focus_params->data()->result;
    bool focus_valid = p_focus->lens_pos_valid;
    bool zoom_valid = p_focus->zoom_pos_valid;
    bool focus_correction = p_focus->focus_correction;
    bool zoom_correction = p_focus->zoom_correction;
    bool zoomfocus_modifypos = p_focus->zoomfocus_modifypos;
    bool end_zoom_chg = p_focus->end_zoom_chg;
    bool vcm_config_valid = p_focus->vcm_config_valid;

    if (!mLensSubdev.ptr())
        goto OUT;

    if (zoomfocus_modifypos)
        mLensSubdev->ZoomFocusModifyPosition(focus_params);
    if (focus_correction)
        mLensSubdev->FocusCorrection();
    if (zoom_correction)
        mLensSubdev->ZoomCorrection();

    if (focus_valid && !zoom_valid) {
        if (mLensSubdev->setFocusParams(focus_params) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set focus result failed to device");
            return XCAM_RETURN_ERROR_IOCTL;
        }
    } else if ((focus_valid && zoom_valid) || end_zoom_chg) {
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "|||setZoomFocusParams");
        if (mLensSubdev->setZoomFocusParams(focus_params) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set setZoomFocusParams failed to device");
            return XCAM_RETURN_ERROR_IOCTL;
        }
    }

    rk_aiq_lens_vcmcfg lens_cfg;
    if (mLensSubdev.ptr() && vcm_config_valid) {
        mLensSubdev->getLensVcmCfg(lens_cfg);
        lens_cfg.start_ma = p_focus->vcm_start_ma;
        lens_cfg.rated_ma = p_focus->vcm_end_ma;
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "|||set vcm config: %d, %d", lens_cfg.start_ma, lens_cfg.rated_ma);
        if (mLensSubdev->setLensVcmCfg(lens_cfg) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set vcm config failed to device");
            return XCAM_RETURN_ERROR_IOCTL;
        }
    }

OUT:
    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::getZoomPosition(int& position)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();
    SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();

    if (mLensSubdev.ptr()) {
        if (mLensSubdev->getZoomParams(&position) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get zoom result failed to device");
            return XCAM_RETURN_ERROR_IOCTL;
        }
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "|||get zoom result: %d", position);
    }

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::setLensVcmCfg(rk_aiq_lens_vcmcfg& lens_cfg)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();
    SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();

    if (mLensSubdev.ptr()) {
        if (mLensSubdev->setLensVcmCfg(lens_cfg) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set vcm config failed");
            return XCAM_RETURN_ERROR_IOCTL;
        }
    }

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::FocusCorrection()
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();
    SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();

    if (mLensSubdev.ptr()) {
        if (mLensSubdev->FocusCorrection() < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "focus correction failed");
            return XCAM_RETURN_ERROR_IOCTL;
        }
    }

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::ZoomCorrection()
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();
    SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();

    if (mLensSubdev.ptr()) {
        if (mLensSubdev->ZoomCorrection() < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "zoom correction failed");
            return XCAM_RETURN_ERROR_IOCTL;
        }
    }

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::getLensVcmCfg(rk_aiq_lens_vcmcfg& lens_cfg)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();
    SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();

    if (mLensSubdev.ptr()) {
        if (mLensSubdev->getLensVcmCfg(lens_cfg) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "get vcm config failed");
            return XCAM_RETURN_ERROR_IOCTL;
        }
    }

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::setAngleZ(float angleZ)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();
    SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();

    if (mLensSubdev.ptr()) {
        if (mLensSubdev->setAngleZ(angleZ) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "setAngleZ failed");
            return XCAM_RETURN_ERROR_IOCTL;
        }
    }

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::setCpslParams(SmartPtr<RkAiqCpslParamsProxy>& cpsl_params)
{
    ENTER_CAMHW_FUNCTION();

    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    RKAiqCpslInfoWrapper_t* cpsl_setting = cpsl_params->data().ptr();

    if (cpsl_setting->update_fl) {
        rk_aiq_flash_setting_t* fl_setting = &cpsl_setting->fl;
        if (mFlashLight.ptr()) {
            ret = mFlashLight->set_params(*fl_setting);
            if (ret < 0) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set flashlight params err: %d\n", ret);
            }
        }
    }

    if (cpsl_setting->update_ir) {
        rk_aiq_ir_setting_t* ir_setting = &cpsl_setting->ir;
        ret = setIrcutParams(ir_setting->irc_on);
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set ir params err: %d\n", ret);
        }

        rk_aiq_flash_setting_t* fl_ir_setting = &cpsl_setting->fl_ir;

        if (mFlashLightIr.ptr()) {
            ret = mFlashLightIr->set_params(*fl_ir_setting);
            if (ret < 0) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set flashlight ir params err: %d\n", ret);
            }
        }
    }

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::setHdrProcessCount(rk_aiq_luma_params_t luma_params)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    ENTER_CAMHW_FUNCTION();
    mRawProcUnit->set_hdr_frame_readback_infos(luma_params.frame_id, luma_params.hdrProcessCnt);

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::getEffectiveIspParams(rkisp_effect_params_v20& ispParams, int frame_id)
{
    ENTER_CAMHW_FUNCTION();
    std::map<int, rkisp_effect_params_v20>::iterator it;
    int search_id = frame_id < 0 ? 0 : frame_id;
    SmartLock locker (_isp_params_cfg_mutex);

    if (_effecting_ispparam_map.size() == 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "can't search id %d,  _effecting_exp_mapsize is %d\n",
                        frame_id, _effecting_ispparam_map.size());
        return  XCAM_RETURN_ERROR_PARAM;
    }

    it = _effecting_ispparam_map.find(search_id);

    // havn't found
    if (it == _effecting_ispparam_map.end()) {
        std::map<int, rkisp_effect_params_v20>::reverse_iterator rit;

        rit = _effecting_ispparam_map.rbegin();
        do {
            LOGD_CAMHW_SUBM(ISP20HW_SUBM, "traverse _effecting_ispparam_map to find id %d, current id is [%d]\n",
                            search_id, rit->first);
            if (search_id >= rit->first ) {
                LOGD_CAMHW_SUBM(ISP20HW_SUBM, "exp-sync: can't find id %d, get latest id %d in _effecting_ispparam_map\n",
                                search_id, rit->first);
                break;
            }
        } while (++rit != _effecting_ispparam_map.rend());

        if (rit == _effecting_ispparam_map.rend()) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "can't find the latest effecting exposure for id %d, impossible case !", frame_id);
            return XCAM_RETURN_ERROR_PARAM;
        }

        ispParams = rit->second;
    } else {
        ispParams = it->second;
    }

    EXIT_CAMHW_FUNCTION();

    return XCAM_RETURN_NO_ERROR;
}

void CamHwIsp20::dumpRawnrFixValue(struct isp2x_rawnr_cfg * pRawnrCfg )
{
    printf("%s:(%d)  enter \n", __FUNCTION__, __LINE__);
    int i = 0;

    //(0x0004)
    printf("(0x0004) gauss_en:%d log_bypass:%d \n",
           pRawnrCfg->gauss_en,
           pRawnrCfg->log_bypass);

    //(0x0008 - 0x0010)
    printf("(0x0008 - 0x0010) filtpar0-2:%d %d %d \n",
           pRawnrCfg->filtpar0,
           pRawnrCfg->filtpar1,
           pRawnrCfg->filtpar2);

    //(0x0014 - 0x001c)
    printf("(0x0014 - 0x001c) dgain0-2:%d %d %d \n",
           pRawnrCfg->dgain0,
           pRawnrCfg->dgain1,
           pRawnrCfg->dgain2);

    //(0x0020 - 0x002c)
    for(int i = 0; i < ISP2X_RAWNR_LUMA_RATION_NUM; i++) {
        printf("(0x0020 - 0x002c) luration[%d]:%d \n",
               i, pRawnrCfg->luration[i]);
    }

    //(0x0030 - 0x003c)
    for(int i = 0; i < ISP2X_RAWNR_LUMA_RATION_NUM; i++) {
        printf("(0x0030 - 0x003c) lulevel[%d]:%d \n",
               i, pRawnrCfg->lulevel[i]);
    }

    //(0x0040)
    printf("(0x0040) gauss:%d \n",
           pRawnrCfg->gauss);

    //(0x0044)
    printf("(0x0044) sigma:%d \n",
           pRawnrCfg->sigma);

    //(0x0048)
    printf("(0x0048) pix_diff:%d \n",
           pRawnrCfg->pix_diff);

    //(0x004c)
    printf("(0x004c) thld_diff:%d \n",
           pRawnrCfg->thld_diff);

    //(0x0050)
    printf("(0x0050) gas_weig_scl1:%d  gas_weig_scl2:%d  thld_chanelw:%d \n",
           pRawnrCfg->gas_weig_scl1,
           pRawnrCfg->gas_weig_scl2,
           pRawnrCfg->thld_chanelw);

    //(0x0054)
    printf("(0x0054) lamda:%d \n",
           pRawnrCfg->lamda);

    //(0x0058 - 0x005c)
    printf("(0x0058 - 0x005c) fixw0-3:%d %d %d %d\n",
           pRawnrCfg->fixw0,
           pRawnrCfg->fixw1,
           pRawnrCfg->fixw2,
           pRawnrCfg->fixw3);

    //(0x0060 - 0x0068)
    printf("(0x0060 - 0x0068) wlamda0-2:%d %d %d\n",
           pRawnrCfg->wlamda0,
           pRawnrCfg->wlamda1,
           pRawnrCfg->wlamda2);


    //(0x006c)
    printf("(0x006c) rgain_filp-2:%d bgain_filp:%d\n",
           pRawnrCfg->rgain_filp,
           pRawnrCfg->bgain_filp);


    printf("%s:(%d)  exit \n", __FUNCTION__, __LINE__);
}



void CamHwIsp20::dumpTnrFixValue(struct rkispp_tnr_config  * pTnrCfg)
{
    int i = 0;
    printf("%s:(%d) enter \n", __FUNCTION__, __LINE__);
    //0x0080
    printf("(0x0080) opty_en:%d optc_en:%d gain_en:%d\n",
           pTnrCfg->opty_en,
           pTnrCfg->optc_en,
           pTnrCfg->gain_en);

    //0x0088
    printf("(0x0088) pk0_y:%d pk1_y:%d pk0_c:%d pk1_c:%d \n",
           pTnrCfg->pk0_y,
           pTnrCfg->pk1_y,
           pTnrCfg->pk0_c,
           pTnrCfg->pk1_c);

    //0x008c
    printf("(0x008c) glb_gain_cur:%d glb_gain_nxt:%d \n",
           pTnrCfg->glb_gain_cur,
           pTnrCfg->glb_gain_nxt);

    //0x0090
    printf("(0x0090) glb_gain_cur_div:%d gain_glb_filt_sqrt:%d \n",
           pTnrCfg->glb_gain_cur_div,
           pTnrCfg->glb_gain_cur_sqrt);

    //0x0094 - 0x0098
    for(i = 0; i < TNR_SIGMA_CURVE_SIZE - 1; i++) {
        printf("(0x0094 - 0x0098) sigma_x[%d]:%d \n",
               i, pTnrCfg->sigma_x[i]);
    }

    //0x009c - 0x00bc
    for(i = 0; i < TNR_SIGMA_CURVE_SIZE; i++) {
        printf("(0x009c - 0x00bc) sigma_y[%d]:%d \n",
               i, pTnrCfg->sigma_y[i]);
    }

    //0x00c4 - 0x00cc
    //dir_idx = 0;
    for(i = 0; i < TNR_LUMA_CURVE_SIZE; i++) {
        printf("(0x00c4 - 0x00cc) luma_curve[%d]:%d \n",
               i, pTnrCfg->luma_curve[i]);
    }

    //0x00d0
    printf("(0x00d0) txt_th0_y:%d txt_th1_y:%d \n",
           pTnrCfg->txt_th0_y,
           pTnrCfg->txt_th1_y);

    //0x00d4
    printf("(0x00d0) txt_th0_c:%d txt_th1_c:%d \n",
           pTnrCfg->txt_th0_c,
           pTnrCfg->txt_th1_c);

    //0x00d8
    printf("(0x00d8) txt_thy_dlt:%d txt_thc_dlt:%d \n",
           pTnrCfg->txt_thy_dlt,
           pTnrCfg->txt_thc_dlt);

    //0x00dc - 0x00ec
    for(i = 0; i < TNR_GFCOEF6_SIZE; i++) {
        printf("(0x00dc - 0x00ec) gfcoef_y0[%d]:%d \n",
               i, pTnrCfg->gfcoef_y0[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x00dc - 0x00ec) gfcoef_y1[%d]:%d \n",
               i, pTnrCfg->gfcoef_y1[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x00dc - 0x00ec) gfcoef_y2[%d]:%d \n",
               i, pTnrCfg->gfcoef_y2[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x00dc - 0x00ec) gfcoef_y3[%d]:%d \n",
               i, pTnrCfg->gfcoef_y3[i]);
    }

    //0x00f0 - 0x0100
    for(i = 0; i < TNR_GFCOEF6_SIZE; i++) {
        printf("(0x00f0 - 0x0100) gfcoef_yg0[%d]:%d \n",
               i, pTnrCfg->gfcoef_yg0[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x00f0 - 0x0100) gfcoef_yg1[%d]:%d \n",
               i, pTnrCfg->gfcoef_yg1[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x00f0 - 0x0100) gfcoef_yg2[%d]:%d \n",
               i, pTnrCfg->gfcoef_yg2[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x00f0 - 0x0100) gfcoef_yg3[%d]:%d \n",
               i, pTnrCfg->gfcoef_yg3[i]);
    }


    //0x0104 - 0x0110
    for(i = 0; i < TNR_GFCOEF6_SIZE; i++) {
        printf("(0x0104 - 0x0110) gfcoef_yl0[%d]:%d \n",
               i, pTnrCfg->gfcoef_yl0[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x0104 - 0x0110) gfcoef_yl1[%d]:%d \n",
               i, pTnrCfg->gfcoef_yl1[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x0104 - 0x0110) gfcoef_yl2[%d]:%d \n",
               i, pTnrCfg->gfcoef_yl2[i]);
    }

    //0x0114 - 0x0120
    for(i = 0; i < TNR_GFCOEF6_SIZE; i++) {
        printf("(0x0114 - 0x0120) gfcoef_cg0[%d]:%d \n",
               i, pTnrCfg->gfcoef_cg0[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x0114 - 0x0120) gfcoef_cg1[%d]:%d \n",
               i, pTnrCfg->gfcoef_cg1[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x0114 - 0x0120) gfcoef_cg2[%d]:%d \n",
               i, pTnrCfg->gfcoef_cg2[i]);
    }


    //0x0124 - 0x012c
    for(i = 0; i < TNR_GFCOEF6_SIZE; i++) {
        printf("(0x0124 - 0x012c) gfcoef_cl0[%d]:%d \n",
               i, pTnrCfg->gfcoef_cl0[i]);
    }
    for(i = 0; i < TNR_GFCOEF3_SIZE; i++) {
        printf("(0x0124 - 0x012c) gfcoef_cl1[%d]:%d \n",
               i, pTnrCfg->gfcoef_cl1[i]);
    }


    //0x0130 - 0x0134
    //dir_idx = 0;  i = lvl;
    for(i = 0; i < TNR_SCALE_YG_SIZE; i++) {
        printf("(0x0130 - 0x0134) scale_yg[%d]:%d \n",
               i, pTnrCfg->scale_yg[i]);
    }

    //0x0138 - 0x013c
    //dir_idx = 1;  i = lvl;
    for(i = 0; i < TNR_SCALE_YL_SIZE; i++) {
        printf("(0x0138 - 0x013c) scale_yl[%d]:%d \n",
               i, pTnrCfg->scale_yl[i]);
    }

    //0x0140 - 0x0148
    //dir_idx = 0;  i = lvl;
    for(i = 0; i < TNR_SCALE_CG_SIZE; i++) {
        printf("(0x0140 - 0x0148) scale_cg[%d]:%d \n",
               i, pTnrCfg->scale_cg[i]);
        printf("(0x0140 - 0x0148) scale_y2cg[%d]:%d \n",
               i, pTnrCfg->scale_y2cg[i]);
    }

    //0x014c - 0x0154
    //dir_idx = 1;  i = lvl;
    for(i = 0; i < TNR_SCALE_CL_SIZE; i++) {
        printf("(0x014c - 0x0154) scale_cl[%d]:%d \n",
               i, pTnrCfg->scale_cl[i]);
    }
    for(i = 0; i < TNR_SCALE_Y2CL_SIZE; i++) {
        printf("(0x014c - 0x0154) scale_y2cl[%d]:%d \n",
               i, pTnrCfg->scale_y2cl[i]);
    }

    //0x0158
    for(i = 0; i < TNR_WEIGHT_Y_SIZE; i++) {
        printf("(0x0158) weight_y[%d]:%d \n",
               i, pTnrCfg->weight_y[i]);
    }

    printf("%s:(%d) exit \n", __FUNCTION__, __LINE__);
}


void CamHwIsp20::dumpUvnrFixValue(struct rkispp_nr_config  * pNrCfg)
{
    int i = 0;
    printf("%s:(%d) exit \n", __FUNCTION__, __LINE__);
    //0x0080
    printf("(0x0088) uvnr_step1_en:%d uvnr_step2_en:%d nr_gain_en:%d uvnr_nobig_en:%d uvnr_big_en:%d\n",
           pNrCfg->uvnr_step1_en,
           pNrCfg->uvnr_step2_en,
           pNrCfg->nr_gain_en,
           pNrCfg->uvnr_nobig_en,
           pNrCfg->uvnr_big_en);

    //0x0084
    printf("(0x0084) uvnr_gain_1sigma:%d \n",
           pNrCfg->uvnr_gain_1sigma);

    //0x0088
    printf("(0x0088) uvnr_gain_offset:%d \n",
           pNrCfg->uvnr_gain_offset);

    //0x008c
    printf("(0x008c) uvnr_gain_uvgain:%d uvnr_step2_en:%d uvnr_gain_t2gen:%d uvnr_gain_iso:%d\n",
           pNrCfg->uvnr_gain_uvgain[0],
           pNrCfg->uvnr_gain_uvgain[1],
           pNrCfg->uvnr_gain_t2gen,
           pNrCfg->uvnr_gain_iso);


    //0x0090
    printf("(0x0090) uvnr_t1gen_m3alpha:%d \n",
           pNrCfg->uvnr_t1gen_m3alpha);

    //0x0094
    printf("(0x0094) uvnr_t1flt_mode:%d \n",
           pNrCfg->uvnr_t1flt_mode);

    //0x0098
    printf("(0x0098) uvnr_t1flt_msigma:%d \n",
           pNrCfg->uvnr_t1flt_msigma);

    //0x009c
    printf("(0x009c) uvnr_t1flt_wtp:%d \n",
           pNrCfg->uvnr_t1flt_wtp);

    //0x00a0-0x00a4
    for(i = 0; i < NR_UVNR_T1FLT_WTQ_SIZE; i++) {
        printf("(0x00a0-0x00a4) uvnr_t1flt_wtq[%d]:%d \n",
               i, pNrCfg->uvnr_t1flt_wtq[i]);
    }

    //0x00a8
    printf("(0x00a8) uvnr_t2gen_m3alpha:%d \n",
           pNrCfg->uvnr_t2gen_m3alpha);

    //0x00ac
    printf("(0x00ac) uvnr_t2gen_msigma:%d \n",
           pNrCfg->uvnr_t2gen_msigma);

    //0x00b0
    printf("(0x00b0) uvnr_t2gen_wtp:%d \n",
           pNrCfg->uvnr_t2gen_wtp);

    //0x00b4
    for(i = 0; i < NR_UVNR_T2GEN_WTQ_SIZE; i++) {
        printf("(0x00b4) uvnr_t2gen_wtq[%d]:%d \n",
               i, pNrCfg->uvnr_t2gen_wtq[i]);
    }

    //0x00b8
    printf("(0x00b8) uvnr_t2flt_msigma:%d \n",
           pNrCfg->uvnr_t2flt_msigma);

    //0x00bc
    printf("(0x00bc) uvnr_t2flt_wtp:%d \n",
           pNrCfg->uvnr_t2flt_wtp);
    for(i = 0; i < NR_UVNR_T2FLT_WT_SIZE; i++) {
        printf("(0x00bc) uvnr_t2flt_wt[%d]:%d \n",
               i, pNrCfg->uvnr_t2flt_wt[i]);
    }


    printf("%s:(%d) entor \n", __FUNCTION__, __LINE__);
}


void CamHwIsp20::dumpYnrFixValue(struct rkispp_nr_config  * pNrCfg)
{
    printf("%s:(%d) enter \n", __FUNCTION__, __LINE__);

    int i = 0;

    //0x0104 - 0x0108
    for(i = 0; i < NR_YNR_SGM_DX_SIZE; i++) {
        printf("(0x0104 - 0x0108) ynr_sgm_dx[%d]:%d \n",
               i, pNrCfg->ynr_sgm_dx[i]);
    }

    //0x010c - 0x012c
    for(i = 0; i < NR_YNR_SGM_Y_SIZE; i++) {
        printf("(0x010c - 0x012c) ynr_lsgm_y[%d]:%d \n",
               i, pNrCfg->ynr_lsgm_y[i]);
    }

    //0x0130
    for(i = 0; i < NR_YNR_CI_SIZE; i++) {
        printf("(0x0130) ynr_lci[%d]:%d \n",
               i, pNrCfg->ynr_lci[i]);
    }

    //0x0134
    for(i = 0; i < NR_YNR_LGAIN_MIN_SIZE; i++) {
        printf("(0x0134) ynr_lgain_min[%d]:%d \n",
               i, pNrCfg->ynr_lgain_min[i]);
    }

    //0x0138
    printf("(0x0138) ynr_lgain_max:%d \n",
           pNrCfg->ynr_lgain_max);


    //0x013c
    printf("(0x013c) ynr_lmerge_bound:%d ynr_lmerge_ratio:%d\n",
           pNrCfg->ynr_lmerge_bound,
           pNrCfg->ynr_lmerge_ratio);

    //0x0140
    for(i = 0; i < NR_YNR_LWEIT_FLT_SIZE; i++) {
        printf("(0x0140) ynr_lweit_flt[%d]:%d \n",
               i, pNrCfg->ynr_lweit_flt[i]);
    }

    //0x0144 - 0x0164
    for(i = 0; i < NR_YNR_SGM_Y_SIZE; i++) {
        printf("(0x0144 - 0x0164) ynr_hsgm_y[%d]:%d \n",
               i, pNrCfg->ynr_hsgm_y[i]);
    }

    //0x0168
    for(i = 0; i < NR_YNR_CI_SIZE; i++) {
        printf("(0x0168) ynr_hlci[%d]:%d \n",
               i, pNrCfg->ynr_hlci[i]);
    }

    //0x016c
    for(i = 0; i < NR_YNR_CI_SIZE; i++) {
        printf("(0x016c) ynr_lhci[%d]:%d \n",
               i, pNrCfg->ynr_lhci[i]);
    }

    //0x0170
    for(i = 0; i < NR_YNR_CI_SIZE; i++) {
        printf("(0x0170) ynr_hhci[%d]:%d \n",
               i, pNrCfg->ynr_hhci[i]);
    }

    //0x0174
    for(i = 0; i < NR_YNR_HGAIN_SGM_SIZE; i++) {
        printf("(0x0174) ynr_hgain_sgm[%d]:%d \n",
               i, pNrCfg->ynr_hgain_sgm[i]);
    }

    //0x0178 - 0x0188
    for(i = 0; i < 5; i++) {
        printf("(0x0178 - 0x0188) ynr_hweit_d[%d - %d]:%d %d %d %d \n",
               i * 4 + 0, i * 4 + 3,
               pNrCfg->ynr_hweit_d[i * 4 + 0],
               pNrCfg->ynr_hweit_d[i * 4 + 1],
               pNrCfg->ynr_hweit_d[i * 4 + 2],
               pNrCfg->ynr_hweit_d[i * 4 + 3]);
    }


    //0x018c - 0x01a0
    for(i = 0; i < 6; i++) {
        printf("(0x018c - 0x01a0) ynr_hgrad_y[%d - %d]:%d %d %d %d \n",
               i * 4 + 0, i * 4 + 3,
               pNrCfg->ynr_hgrad_y[i * 4 + 0],
               pNrCfg->ynr_hgrad_y[i * 4 + 1],
               pNrCfg->ynr_hgrad_y[i * 4 + 2],
               pNrCfg->ynr_hgrad_y[i * 4 + 3]);
    }

    //0x01a4 -0x01a8
    for(i = 0; i < NR_YNR_HWEIT_SIZE; i++) {
        printf("(0x01a4 -0x01a8) ynr_hweit[%d]:%d \n",
               i, pNrCfg->ynr_hweit[i]);
    }

    //0x01b0
    printf("(0x01b0) ynr_hmax_adjust:%d \n",
           pNrCfg->ynr_hmax_adjust);

    //0x01b4
    printf("(0x01b4) ynr_hstrength:%d \n",
           pNrCfg->ynr_hstrength);

    //0x01b8
    printf("(0x01b8) ynr_lweit_cmp0-1:%d %d\n",
           pNrCfg->ynr_lweit_cmp[0],
           pNrCfg->ynr_lweit_cmp[1]);

    //0x01bc
    printf("(0x01bc) ynr_lmaxgain_lv4:%d \n",
           pNrCfg->ynr_lmaxgain_lv4);

    //0x01c0 - 0x01e0
    for(i = 0; i < NR_YNR_HSTV_Y_SIZE; i++) {
        printf("(0x01c0 - 0x01e0 ) ynr_hstv_y[%d]:%d \n",
               i, pNrCfg->ynr_hstv_y[i]);
    }

    //0x01e4  - 0x01e8
    for(i = 0; i < NR_YNR_ST_SCALE_SIZE; i++) {
        printf("(0x01e4  - 0x01e8 ) ynr_st_scale[%d]:%d \n",
               i, pNrCfg->ynr_st_scale[i]);
    }

    printf("%s:(%d) exit \n", __FUNCTION__, __LINE__);

}


void CamHwIsp20::dumpSharpFixValue(struct rkispp_sharp_config  * pSharpCfg)
{
    printf("%s:(%d) enter \n", __FUNCTION__, __LINE__);
    int i = 0;

    //0x0080
    printf("(0x0080) alpha_adp_en:%d yin_flt_en:%d edge_avg_en:%d\n",
           pSharpCfg->alpha_adp_en,
           pSharpCfg->yin_flt_en,
           pSharpCfg->edge_avg_en);


    //0x0084
    printf("(0x0084) hbf_ratio:%d ehf_th:%d pbf_ratio:%d\n",
           pSharpCfg->hbf_ratio,
           pSharpCfg->ehf_th,
           pSharpCfg->pbf_ratio);

    //0x0088
    printf("(0x0088) edge_thed:%d dir_min:%d smoth_th4:%d\n",
           pSharpCfg->edge_thed,
           pSharpCfg->dir_min,
           pSharpCfg->smoth_th4);

    //0x008c
    printf("(0x008c) l_alpha:%d g_alpha:%d \n",
           pSharpCfg->l_alpha,
           pSharpCfg->g_alpha);


    //0x0090
    for(i = 0; i < 3; i++) {
        printf("(0x0090) pbf_k[%d]:%d  \n",
               i, pSharpCfg->pbf_k[i]);
    }



    //0x0094 - 0x0098
    for(i = 0; i < 6; i++) {
        printf("(0x0094 - 0x0098) mrf_k[%d]:%d  \n",
               i, pSharpCfg->mrf_k[i]);
    }


    //0x009c -0x00a4
    for(i = 0; i < 12; i++) {
        printf("(0x009c -0x00a4) mbf_k[%d]:%d  \n",
               i, pSharpCfg->mbf_k[i]);
    }


    //0x00a8 -0x00ac
    for(i = 0; i < 6; i++) {
        printf("(0x00a8 -0x00ac) hrf_k[%d]:%d  \n",
               i, pSharpCfg->hrf_k[i]);
    }


    //0x00b0
    for(i = 0; i < 3; i++) {
        printf("(0x00b0) hbf_k[%d]:%d  \n",
               i, pSharpCfg->hbf_k[i]);
    }


    //0x00b4
    for(i = 0; i < 3; i++) {
        printf("(0x00b4) eg_coef[%d]:%d  \n",
               i, pSharpCfg->eg_coef[i]);
    }

    //0x00b8
    for(i = 0; i < 3; i++) {
        printf("(0x00b8) eg_smoth[%d]:%d  \n",
               i, pSharpCfg->eg_smoth[i]);
    }


    //0x00bc - 0x00c0
    for(i = 0; i < 6; i++) {
        printf("(0x00bc - 0x00c0) eg_gaus[%d]:%d  \n",
               i, pSharpCfg->eg_gaus[i]);
    }


    //0x00c4 - 0x00c8
    for(i = 0; i < 6; i++) {
        printf("(0x00c4 - 0x00c8) dog_k[%d]:%d  \n",
               i, pSharpCfg->dog_k[i]);
    }


    //0x00cc - 0x00d0
    for(i = 0; i < SHP_LUM_POINT_SIZE; i++) {
        printf("(0x00cc - 0x00d0) lum_point[%d]:%d  \n",
               i, pSharpCfg->lum_point[i]);
    }

    //0x00d4
    printf("(0x00d4) pbf_shf_bits:%d  mbf_shf_bits:%d hbf_shf_bits:%d\n",
           pSharpCfg->pbf_shf_bits,
           pSharpCfg->mbf_shf_bits,
           pSharpCfg->hbf_shf_bits);


    //0x00d8 - 0x00dc
    for(i = 0; i < SHP_SIGMA_SIZE; i++) {
        printf("(0x00d8 - 0x00dc) pbf_sigma[%d]:%d  \n",
               i, pSharpCfg->pbf_sigma[i]);
    }

    //0x00e0 - 0x00e4
    for(i = 0; i < SHP_LUM_CLP_SIZE; i++) {
        printf("(0x00e0 - 0x00e4) lum_clp_m[%d]:%d  \n",
               i, pSharpCfg->lum_clp_m[i]);
    }

    //0x00e8 - 0x00ec
    for(i = 0; i < SHP_LUM_MIN_SIZE; i++) {
        printf("(0x00e8 - 0x00ec) lum_min_m[%d]:%d  \n",
               i, pSharpCfg->lum_min_m[i]);
    }

    //0x00f0 - 0x00f4
    for(i = 0; i < SHP_SIGMA_SIZE; i++) {
        printf("(0x00f0 - 0x00f4) mbf_sigma[%d]:%d  \n",
               i, pSharpCfg->mbf_sigma[i]);
    }

    //0x00f8 - 0x00fc
    for(i = 0; i < SHP_LUM_CLP_SIZE; i++) {
        printf("(0x00f8 - 0x00fc) lum_clp_h[%d]:%d  \n",
               i, pSharpCfg->lum_clp_h[i]);
    }

    //0x0100 - 0x0104
    for(i = 0; i < SHP_SIGMA_SIZE; i++) {
        printf("(0x0100 - 0x0104) hbf_sigma[%d]:%d  \n",
               i, pSharpCfg->hbf_sigma[i]);
    }

    //0x0108 - 0x010c
    for(i = 0; i < SHP_EDGE_LUM_THED_SIZE; i++) {
        printf("(0x0108 - 0x010c) edge_lum_thed[%d]:%d  \n",
               i, pSharpCfg->edge_lum_thed[i]);
    }

    //0x0110 - 0x0114
    for(i = 0; i < SHP_CLAMP_SIZE; i++) {
        printf("(0x0110 - 0x0114) clamp_pos[%d]:%d  \n",
               i, pSharpCfg->clamp_pos[i]);
    }

    //0x0118 - 0x011c
    for(i = 0; i < SHP_CLAMP_SIZE; i++) {
        printf("(0x0118 - 0x011c) clamp_neg[%d]:%d  \n",
               i, pSharpCfg->clamp_neg[i]);
    }

    //0x0120 - 0x0124
    for(i = 0; i < SHP_DETAIL_ALPHA_SIZE; i++) {
        printf("(0x0120 - 0x0124) detail_alpha[%d]:%d  \n",
               i, pSharpCfg->detail_alpha[i]);
    }

    //0x0128
    printf("(0x0128) rfl_ratio:%d  rfh_ratio:%d\n",
           pSharpCfg->rfl_ratio, pSharpCfg->rfh_ratio);

    // mf/hf ratio

    //0x012C
    printf("(0x012C) m_ratio:%d  h_ratio:%d\n",
           pSharpCfg->m_ratio, pSharpCfg->h_ratio);

    printf("%s:(%d) exit \n", __FUNCTION__, __LINE__);
}

XCamReturn
CamHwIsp20::setModuleCtl(rk_aiq_module_id_t moduleId, bool en)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (_cur_calib_infos.mfnr.enable && _cur_calib_infos.mfnr.motion_detect_en) {
        if ((moduleId == RK_MODULE_TNR) && (en == false)) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "motion detect is running, operate not permit!");
            return XCAM_RETURN_ERROR_FAILED;
        }
    }
    setModuleStatus(moduleId, en);
    return ret;
}

XCamReturn
CamHwIsp20::getModuleCtl(rk_aiq_module_id_t moduleId, bool &en)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    getModuleStatus(moduleId, en);
    return ret;
}

XCamReturn CamHwIsp20::notify_capture_raw()
{
    if (mRawProcUnit.ptr())
        return mRawProcUnit->notify_capture_raw();
    else
        return XCAM_RETURN_ERROR_FAILED;
}

XCamReturn CamHwIsp20::capture_raw_ctl(capture_raw_t type, int count, const char* capture_dir, char* output_dir)
{
    if (!mRawProcUnit.ptr())
        return XCAM_RETURN_ERROR_FAILED;

    if (type == CAPTURE_RAW_AND_YUV_SYNC)
        return mRawProcUnit->capture_raw_ctl(type);
    else if (type == CAPTURE_RAW_SYNC)
        return mRawProcUnit->capture_raw_ctl(type, count, capture_dir, output_dir);
    return XCAM_RETURN_ERROR_FAILED;
}

XCamReturn
CamHwIsp20::setIrcutParams(bool on)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();

    struct v4l2_control control;

    xcam_mem_clear (control);
    control.id = V4L2_CID_BAND_STOP_FILTER;
    if(on)
        control.value = IRCUT_STATE_CLOSED;
    else
        control.value = IRCUT_STATE_OPENED;
    if (mIrcutDev.ptr()) {
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "set ircut value: %d", control.value);
        if (mIrcutDev->io_control (VIDIOC_S_CTRL, &control) < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "set ircut value failed to device!");
            ret = XCAM_RETURN_ERROR_IOCTL;
        }
    }

    EXIT_CAMHW_FUNCTION();
    return ret;
}

uint64_t CamHwIsp20::getIspModuleEnState()
{
    return ispModuleEns;
}

XCamReturn CamHwIsp20::setSensorFlip(bool mirror, bool flip, int skip_frm_cnt)
{
    SmartPtr<BaseSensorHw> mSensorSubdev = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    int32_t skip_frame_sequence = 0;
    ret = mSensorSubdev->set_mirror_flip(mirror, flip, skip_frame_sequence);

    /* struct timespec tp; */
    /* clock_gettime(CLOCK_MONOTONIC, &tp); */
    /* int64_t skip_ts = (int64_t)(tp.tv_sec) * 1000 * 1000 * 1000 + (int64_t)(tp.tv_nsec); */

    if (_state == CAM_HW_STATE_STARTED && skip_frame_sequence != -1)
        mRawCapUnit->skip_frames(skip_frm_cnt, skip_frame_sequence);

    return ret;
}

XCamReturn CamHwIsp20::getSensorFlip(bool& mirror, bool& flip)
{
    SmartPtr<BaseSensorHw> mSensorSubdev = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();

    return mSensorSubdev->get_mirror_flip(mirror, flip);
}

XCamReturn CamHwIsp20::setSensorCrop(rk_aiq_rect_t& rect)
{
    XCamReturn ret;
    struct v4l2_crop crop;
    for (int i = 0; i < 3; i++) {
        SmartPtr<V4l2Device> mipi_tx = mRawCapUnit->get_tx_device(i).dynamic_cast_ptr<V4l2Device>();
        memset(&crop, 0, sizeof(crop));
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = mipi_tx->get_crop(crop);
        crop.c.left = rect.left;
        crop.c.top = rect.top;
        crop.c.width = rect.width;
        crop.c.height = rect.height;
        ret = mipi_tx->set_crop(crop);
    }
    _crop_rect = rect;
    return ret;
}

XCamReturn CamHwIsp20::getSensorCrop(rk_aiq_rect_t& rect)
{
    XCamReturn ret;
    struct v4l2_crop crop;
    SmartPtr<V4l2Device> mipi_tx = mRawCapUnit->get_tx_device(0).dynamic_cast_ptr<V4l2Device>();
    memset(&crop, 0, sizeof(crop));
    ret = mipi_tx->get_crop(crop);
    rect.left = crop.c.left;
    rect.top = crop.c.top;
    rect.width = crop.c.width;
    rect.height = crop.c.height;
    return ret;
}

void CamHwIsp20::setHdrGlobalTmoMode(int frame_id, bool mode)
{
    if (mNoReadBack)
        return;

    mRawProcUnit->set_hdr_global_tmo_mode(frame_id, mode);
}

void CamHwIsp20::setMulCamConc(bool cc)
{
    mRawProcUnit->setMulCamConc(cc);
    if (cc)
        mNoReadBack = false;
}

void CamHwIsp20::getShareMemOps(isp_drv_share_mem_ops_t** mem_ops)
{
    this->alloc_mem = (alloc_mem_t)&CamHwIsp20::allocMemResource;
    this->release_mem = (release_mem_t)&CamHwIsp20::releaseMemResource;
    this->get_free_item = (get_free_item_t)&CamHwIsp20::getFreeItem;
    *mem_ops = this;
}

void CamHwIsp20::allocMemResource(uint8_t id, void *ops_ctx, void *config, void **mem_ctx)
{
    int ret = -1;
    struct rkisp_meshbuf_info ldchbuf_info;
    struct rkispp_fecbuf_info  fecbuf_info;
    struct rkisp_meshbuf_info cacbuf_info;
    struct rkispp_fecbuf_size fecbuf_size;
    struct rkisp_meshbuf_size ldchbuf_size;
    struct rkisp_meshbuf_size cacbuf_size;
    uint8_t offset = id * ISP3X_MESH_BUF_NUM;

    CamHwIsp20 *isp20 = static_cast<CamHwIsp20*>((isp_drv_share_mem_ops_t*)ops_ctx);
    rk_aiq_share_mem_config_t* share_mem_cfg = (rk_aiq_share_mem_config_t *)config;

    SmartLock locker (isp20->_mem_mutex);
    if (share_mem_cfg->mem_type == MEM_TYPE_LDCH) {
        ldchbuf_size.unite_isp_id = id;
        ldchbuf_size.module_id = ISP3X_MODULE_LDCH;
        ldchbuf_size.meas_width = share_mem_cfg->alloc_param.width;
        ldchbuf_size.meas_height = share_mem_cfg->alloc_param.height;
        ret = isp20->mIspCoreDev->io_control(RKISP_CMD_SET_MESHBUF_SIZE, &ldchbuf_size);
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "alloc ldch buf failed!");
            *mem_ctx = nullptr;
            return;
        }
        xcam_mem_clear(ldchbuf_info);
        ldchbuf_info.unite_isp_id = id;
        ldchbuf_info.module_id = ISP3X_MODULE_LDCH;
        ret = isp20->mIspCoreDev->io_control(RKISP_CMD_GET_MESHBUF_INFO, &ldchbuf_info);
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "failed to get ldch buf info!!");
            *mem_ctx = nullptr;
            return;
        }
        rk_aiq_ldch_share_mem_info_t* mem_info_array =
            (rk_aiq_ldch_share_mem_info_t*)(isp20->_ldch_drv_mem_ctx.mem_info);
        for (int i = 0; i < ISP2X_MESH_BUF_NUM; i++) {
            mem_info_array[offset + i].map_addr =
                mmap(NULL, ldchbuf_info.buf_size[i], PROT_READ | PROT_WRITE, MAP_SHARED, ldchbuf_info.buf_fd[i], 0);
            if (MAP_FAILED == mem_info_array[offset + i].map_addr)
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "failed to map ldch buf!!");

            mem_info_array[offset + i].fd = ldchbuf_info.buf_fd[i];
            mem_info_array[offset + i].size = ldchbuf_info.buf_size[i];
            struct isp2x_mesh_head *head = (struct isp2x_mesh_head*)mem_info_array[offset + i].map_addr;
            mem_info_array[offset + i].addr = (void*)((char*)mem_info_array[offset + i].map_addr + head->data_oft);
            mem_info_array[offset + i].state = (char*)&head->stat;
        }
        *mem_ctx = (void*)(&isp20->_ldch_drv_mem_ctx);
    } else if (share_mem_cfg->mem_type == MEM_TYPE_FEC) {
        fecbuf_size.meas_width = share_mem_cfg->alloc_param.width;
        fecbuf_size.meas_height = share_mem_cfg->alloc_param.height;
        fecbuf_size.meas_mode = share_mem_cfg->alloc_param.reserved[0];
        ret = isp20->_ispp_sd->io_control(RKISPP_CMD_SET_FECBUF_SIZE, &fecbuf_size);
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "alloc fec buf failed!");
            *mem_ctx = nullptr;
            return;
        }
        xcam_mem_clear(fecbuf_info);
        ret = isp20->_ispp_sd->io_control(RKISPP_CMD_GET_FECBUF_INFO, &fecbuf_info);
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "failed to get fec buf info!!");
            *mem_ctx = nullptr;
            return;
        }
        rk_aiq_fec_share_mem_info_t* mem_info_array =
            (rk_aiq_fec_share_mem_info_t*)(isp20->_fec_drv_mem_ctx.mem_info);
        for (int i = 0; i < FEC_MESH_BUF_NUM; i++) {
            mem_info_array[i].map_addr =
                mmap(NULL, fecbuf_info.buf_size[i], PROT_READ | PROT_WRITE, MAP_SHARED, fecbuf_info.buf_fd[i], 0);
            if (MAP_FAILED == mem_info_array[i].map_addr)
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "failed to map fec buf!!");

            mem_info_array[i].fd = fecbuf_info.buf_fd[i];
            mem_info_array[i].size = fecbuf_info.buf_size[i];
            struct rkispp_fec_head *head = (struct rkispp_fec_head*)mem_info_array[i].map_addr;
            mem_info_array[i].meshxf =
                (unsigned char*)mem_info_array[i].map_addr + head->meshxf_oft;
            mem_info_array[i].meshyf =
                (unsigned char*)mem_info_array[i].map_addr + head->meshyf_oft;
            mem_info_array[i].meshxi =
                (unsigned short*)((char*)mem_info_array[i].map_addr + head->meshxi_oft);
            mem_info_array[i].meshyi =
                (unsigned short*)((char*)mem_info_array[i].map_addr + head->meshyi_oft);
            mem_info_array[i].state = (char*)&head->stat;
        }
        *mem_ctx = (void*)(&isp20->_fec_drv_mem_ctx);
    } else if (share_mem_cfg->mem_type == MEM_TYPE_CAC) {
        cacbuf_size.unite_isp_id = id;
        cacbuf_size.module_id = ISP3X_MODULE_CAC;
        cacbuf_size.meas_width = share_mem_cfg->alloc_param.width;
        cacbuf_size.meas_height = share_mem_cfg->alloc_param.height;
        ret = isp20->mIspCoreDev->io_control(RKISP_CMD_SET_MESHBUF_SIZE, &cacbuf_size);
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "alloc cac buf failed!");
            *mem_ctx = nullptr;
            return;
        }
        xcam_mem_clear(cacbuf_info);
        cacbuf_info.unite_isp_id = id;
        cacbuf_info.module_id = ISP3X_MODULE_CAC;
        ret = isp20->mIspCoreDev->io_control(RKISP_CMD_GET_MESHBUF_INFO, &cacbuf_info);
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "failed to get cac buf info!!");
            *mem_ctx = nullptr;
            return;
        }
        rk_aiq_cac_share_mem_info_t* mem_info_array =
            (rk_aiq_cac_share_mem_info_t*)(isp20->_cac_drv_mem_ctx.mem_info);
        for (int i = 0; i < ISP3X_MESH_BUF_NUM; i++) {
            mem_info_array[offset + i].map_addr =
                mmap(NULL, cacbuf_info.buf_size[i], PROT_READ | PROT_WRITE, MAP_SHARED, cacbuf_info.buf_fd[i], 0);
            if (MAP_FAILED == mem_info_array[offset + i].map_addr) {
                mem_info_array[offset + i].map_addr = NULL;
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "failed to map cac buf!!");
                *mem_ctx = nullptr;
                return;
            }

            mem_info_array[offset + i].fd = cacbuf_info.buf_fd[i];
            mem_info_array[offset + i].size = cacbuf_info.buf_size[i];
            struct isp2x_mesh_head* head = (struct isp2x_mesh_head*)mem_info_array[offset + i].map_addr;
            mem_info_array[offset + i].addr = (void*)((char*)mem_info_array[offset + i].map_addr + head->data_oft);
            mem_info_array[offset + i].state = (char*)&head->stat;
            LOGE(">>>>>>> Got CAC LUT fd %d for ISP %d", mem_info_array[offset + i].fd, id);
        }
        *mem_ctx = (void*)(&isp20->_cac_drv_mem_ctx);
    }
}

void CamHwIsp20::releaseMemResource(uint8_t id, void *mem_ctx)
{
    int ret = -1;
    drv_share_mem_ctx_t* drv_mem_ctx = (drv_share_mem_ctx_t*)mem_ctx;
    CamHwIsp20 *isp20 = (CamHwIsp20*)(drv_mem_ctx->ops_ctx);
    uint8_t offset = id * ISP3X_MESH_BUF_NUM;

    SmartLock locker (isp20->_mem_mutex);
    if (drv_mem_ctx->type == MEM_TYPE_LDCH) {
        rk_aiq_ldch_share_mem_info_t* mem_info_array =
            (rk_aiq_ldch_share_mem_info_t*)(drv_mem_ctx->mem_info);
        for (int i = 0; i < ISP2X_MESH_BUF_NUM; i++) {
            if (mem_info_array[offset + i].map_addr) {
                ret = munmap(mem_info_array[offset + i].map_addr, mem_info_array[offset + i].size);
                if (ret < 0)
                    LOGE_CAMHW_SUBM(ISP20HW_SUBM, "munmap ldch buf info!!");
                mem_info_array[offset + i].map_addr = NULL;
            }
            ::close(mem_info_array[offset + i].fd);
        }
    } else if (drv_mem_ctx->type == MEM_TYPE_FEC) {
        rk_aiq_fec_share_mem_info_t* mem_info_array =
            (rk_aiq_fec_share_mem_info_t*)(drv_mem_ctx->mem_info);
        for (int i = 0; i < FEC_MESH_BUF_NUM; i++) {
            if (mem_info_array[i].map_addr) {
                ret = munmap(mem_info_array[i].map_addr, mem_info_array[i].size);
                if (ret < 0)
                    LOGE_CAMHW_SUBM(ISP20HW_SUBM, "munmap fec buf info!!");
                mem_info_array[i].map_addr = NULL;
            }
            ::close(mem_info_array[i].fd);
        }
    } else if (drv_mem_ctx->type == MEM_TYPE_CAC) {
        rk_aiq_cac_share_mem_info_t* mem_info_array =
            (rk_aiq_cac_share_mem_info_t*)(drv_mem_ctx->mem_info);
        for (int i = 0; i < ISP3X_MESH_BUF_NUM; i++) {
            if (mem_info_array[offset + i].map_addr) {
                ret = munmap(mem_info_array[offset + i].map_addr, mem_info_array[offset + i].size);
                if (ret < 0)
                    LOGE_CAMHW_SUBM(ISP20HW_SUBM, "munmap cac buf info!!");
                mem_info_array[offset + i].map_addr = NULL;
            }
            ::close(mem_info_array[offset + i].fd);
        }
    }
}

void*
CamHwIsp20::getFreeItem(uint8_t id, void *mem_ctx)
{
    unsigned int idx;
    int retry_cnt = 3;
    drv_share_mem_ctx_t* drv_mem_ctx = (drv_share_mem_ctx_t*)mem_ctx;
    CamHwIsp20 *isp20 = (CamHwIsp20*)(drv_mem_ctx->ops_ctx);
    uint8_t offset = id * ISP3X_MESH_BUF_NUM;

    SmartLock locker (isp20->_mem_mutex);
    if (drv_mem_ctx->type == MEM_TYPE_LDCH) {
        rk_aiq_ldch_share_mem_info_t* mem_info_array =
            (rk_aiq_ldch_share_mem_info_t*)(drv_mem_ctx->mem_info);
        do {
            for (idx = 0; idx < ISP2X_MESH_BUF_NUM; idx++) {
                if (mem_info_array[offset + idx].state &&
                        (0 == mem_info_array[offset + idx].state[0])) {
                    return (void*)&mem_info_array[offset + idx];
                }
            }
        } while(retry_cnt--);
    } else if (drv_mem_ctx->type == MEM_TYPE_FEC) {
        rk_aiq_fec_share_mem_info_t* mem_info_array =
            (rk_aiq_fec_share_mem_info_t*)(drv_mem_ctx->mem_info);
        do {
            for (idx = 0; idx < FEC_MESH_BUF_NUM; idx++) {
                if (mem_info_array[id].state &&
                        (0 == mem_info_array[id].state[0])) {
                    return (void*)&mem_info_array[id];
                }
            }
        } while(retry_cnt--);
    } else if (drv_mem_ctx->type == MEM_TYPE_CAC) {
        rk_aiq_cac_share_mem_info_t* mem_info_array =
            (rk_aiq_cac_share_mem_info_t*)(drv_mem_ctx->mem_info);
        do {
            for (idx = 0; idx < ISP3X_MESH_BUF_NUM; idx++) {
                if (mem_info_array[offset + idx].state &&
                        (0 == mem_info_array[offset + idx].state[0])) {
                    return (void*)&mem_info_array[offset + idx];
                }
            }
        } while(retry_cnt--);
    }
    return NULL;
}

XCamReturn
CamHwIsp20::poll_event_ready (uint32_t sequence, int type)
{
//    if (type == V4L2_EVENT_FRAME_SYNC && mIspEvtsListener) {
//        SmartPtr<SensorHw> mSensor = mSensorDev.dynamic_cast_ptr<SensorHw>();
//        SmartPtr<Isp20Evt> evtInfo = new Isp20Evt(this, mSensor);
//        evtInfo->evt_code = type;
//        evtInfo->sequence = sequence;
//        evtInfo->expDelay = _exp_delay;

//        return  mIspEvtsListener->ispEvtsCb(evtInfo);
//    }

    return XCAM_RETURN_NO_ERROR;
}

SmartPtr<ispHwEvt_t>
CamHwIsp20::make_ispHwEvt (uint32_t sequence, int type, int64_t timestamp)
{
    if (type == V4L2_EVENT_FRAME_SYNC) {
        SmartPtr<SensorHw> mSensor = mSensorDev.dynamic_cast_ptr<SensorHw>();
        SmartPtr<Isp20Evt> evtInfo = new Isp20Evt(this, mSensor);
        evtInfo->evt_code = type;
        evtInfo->sequence = sequence;
        evtInfo->expDelay = _exp_delay;
        evtInfo->setSofTimeStamp(timestamp);

        return  evtInfo;
    }

    return nullptr;
}

XCamReturn
CamHwIsp20::poll_event_failed (int64_t timestamp, const char *msg)
{
    return XCAM_RETURN_ERROR_FAILED;
}
XCamReturn
CamHwIsp20::applyAnalyzerResult(SmartPtr<SharedItemBase> base, bool sync)
{
    return dispatchResult(base);
}

XCamReturn
CamHwIsp20::applyAnalyzerResult(cam3aResultList& list)
{
    return dispatchResult(list);
}

XCamReturn
CamHwIsp20::dispatchResult(cam3aResultList& list)
{
    cam3aResultList isp_result_list;
    for (auto& result : list) {
        switch (result->getType()) {
        case RESULT_TYPE_AEC_PARAM:
        case RESULT_TYPE_HIST_PARAM:
        case RESULT_TYPE_AWB_PARAM:
        case RESULT_TYPE_AWBGAIN_PARAM:
        case RESULT_TYPE_AF_PARAM:
        case RESULT_TYPE_DPCC_PARAM:
        case RESULT_TYPE_MERGE_PARAM:
        case RESULT_TYPE_TMO_PARAM:
        case RESULT_TYPE_CCM_PARAM:
        case RESULT_TYPE_LSC_PARAM:
        case RESULT_TYPE_BLC_PARAM:
        case RESULT_TYPE_RAWNR_PARAM:
        case RESULT_TYPE_GIC_PARAM:
        case RESULT_TYPE_DEBAYER_PARAM:
        case RESULT_TYPE_LDCH_PARAM:
        case RESULT_TYPE_LUT3D_PARAM:
        case RESULT_TYPE_DEHAZE_PARAM:
        case RESULT_TYPE_AGAMMA_PARAM:
        case RESULT_TYPE_ADEGAMMA_PARAM:
        case RESULT_TYPE_WDR_PARAM:
        case RESULT_TYPE_CSM_PARAM:
        case RESULT_TYPE_CGC_PARAM:
        case RESULT_TYPE_CONV422_PARAM:
        case RESULT_TYPE_YUVCONV_PARAM:
        case RESULT_TYPE_GAIN_PARAM:
        case RESULT_TYPE_CP_PARAM:
        case RESULT_TYPE_IE_PARAM:
        case RESULT_TYPE_MOTION_PARAM:
            isp_result_list.push_back(result);
            break;
        default:
            dispatchResult(result);
            break;
        }
    }

    if (isp_result_list.size() > 0) {
        handleIsp3aReslut(isp_result_list);
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CamHwIsp20::handleIsp3aReslut(cam3aResultList& list)
{
    ENTER_CAMHW_FUNCTION();
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (_is_exit) {
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "set 3a config bypass since ia engine has stop");
        return XCAM_RETURN_BYPASS;
    }

    if (_state == CAM_HW_STATE_PREPARED || _state == CAM_HW_STATE_STOPPED ||
            _state == CAM_HW_STATE_PAUSED) {
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "hdr-debug: %s: first set ispparams\n",
                        __func__);
        if (!mIspParamsDev->is_activated()) {
            ret = mIspParamsDev->start();
            if (ret < 0) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "prepare isp params dev err: %d\n", ret);
            }

            ret = hdr_mipi_prepare_mode(_hdr_mode);
            if (ret < 0) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "hdr mipi start err: %d\n", ret);
            }
        }

        for (auto& result : list) {
            result->setId(0);
            mParamsAssembler->addReadyCondition(result->getType());
        }
    }

    mParamsAssembler->queue(list);

    // set all ready params to drv
    while (_state == CAM_HW_STATE_STARTED &&
           mParamsAssembler->ready()) {
        SmartLock locker(_stop_cond_mutex);
        if (_isp_stream_status != ISP_STREAM_STATUS_STREAM_OFF) {
            if (setIspConfig() != XCAM_RETURN_NO_ERROR)
                break;
        } else {
            break;
        }
    }

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::dispatchResult(SmartPtr<cam3aResult> result)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (!result.ptr())
        return XCAM_RETURN_ERROR_PARAM;

    LOG1("%s enter, msg type(0x%x)", __FUNCTION__, result->getType());
    switch (result->getType())
    {
    case RESULT_TYPE_EXPOSURE_PARAM:
    {
        SmartPtr<RkAiqExpParamsProxy> exp = result.dynamic_cast_ptr<RkAiqExpParamsProxy>();
        ret = setExposureParams(exp);
        if (ret)
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "setExposureParams error %d id %d", ret, result->getId());
        break;
    }
    case RESULT_TYPE_AEC_PARAM:
    case RESULT_TYPE_HIST_PARAM:
    case RESULT_TYPE_AWB_PARAM:
    case RESULT_TYPE_AWBGAIN_PARAM:
    case RESULT_TYPE_AF_PARAM:
    case RESULT_TYPE_DPCC_PARAM:
    case RESULT_TYPE_MERGE_PARAM:
    case RESULT_TYPE_TMO_PARAM:
    case RESULT_TYPE_CCM_PARAM:
    case RESULT_TYPE_LSC_PARAM:
    case RESULT_TYPE_BLC_PARAM:
    case RESULT_TYPE_RAWNR_PARAM:
    case RESULT_TYPE_GIC_PARAM:
    case RESULT_TYPE_DEBAYER_PARAM:
    case RESULT_TYPE_LDCH_PARAM:
    case RESULT_TYPE_LUT3D_PARAM:
    case RESULT_TYPE_DEHAZE_PARAM:
    case RESULT_TYPE_AGAMMA_PARAM:
    case RESULT_TYPE_ADEGAMMA_PARAM:
    case RESULT_TYPE_WDR_PARAM:
    case RESULT_TYPE_CSM_PARAM:
    case RESULT_TYPE_CGC_PARAM:
    case RESULT_TYPE_CONV422_PARAM:
    case RESULT_TYPE_YUVCONV_PARAM:
    case RESULT_TYPE_GAIN_PARAM:
    case RESULT_TYPE_CP_PARAM:
    case RESULT_TYPE_IE_PARAM:
    case RESULT_TYPE_MOTION_PARAM:
    case RESULT_TYPE_CAC_PARAM:
        handleIsp3aReslut(result);
        break;
    case RESULT_TYPE_TNR_PARAM:
    case RESULT_TYPE_YNR_PARAM:
    case RESULT_TYPE_UVNR_PARAM:
    case RESULT_TYPE_SHARPEN_PARAM:
    case RESULT_TYPE_EDGEFLT_PARAM:
    case RESULT_TYPE_FEC_PARAM:
    case RESULT_TYPE_ORB_PARAM:
        handlePpReslut(result);
        break;
    case RESULT_TYPE_FOCUS_PARAM:
    {
        SmartPtr<RkAiqFocusParamsProxy> focus = result.dynamic_cast_ptr<RkAiqFocusParamsProxy>();
        ret = setFocusParams(focus);
        if (ret)
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "setFocusParams error %d", ret);
        break;
    }
    case RESULT_TYPE_IRIS_PARAM:
    {
        SmartPtr<RkAiqIrisParamsProxy> iris = result.dynamic_cast_ptr<RkAiqIrisParamsProxy>();
        ret = setIrisParams(iris, _cur_calib_infos.aec.IrisType);
        if (ret)
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "setIrisParams error %d", ret);
        break;
    }
    case RESULT_TYPE_CPSL_PARAM:
    {
        SmartPtr<RkAiqCpslParamsProxy> cpsl = result.dynamic_cast_ptr<RkAiqCpslParamsProxy>();
        ret = setCpslParams(cpsl);
        if (ret)
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "setCpslParams error %d", ret);
        break;
    }
    case RESULT_TYPE_FLASH_PARAM:
    {
#ifdef FLASH_CTL_DEBUG
        SmartPtr<rk_aiq_flash_setting_t> flash = result.dynamic_cast_ptr<rk_aiq_flash_setting_t>();
        ret = setFlParams(flash);
        if (ret)
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "setFlParams error %d", ret);
#endif
        break;
    }
    default:
        LOGE("unknown param type(0x%x)!", result->getType());
        break;
    }
    return ret;
    LOGD("%s exit", __FUNCTION__);
}

XCamReturn CamHwIsp20::notify_sof(SmartPtr<VideoBuffer>& buf)
{
    SmartPtr<SofEventBuffer> evtbuf = buf.dynamic_cast_ptr<SofEventBuffer>();
    SmartPtr<SofEventData> evtdata = evtbuf->get_data();
    SmartPtr<BaseSensorHw> mSensorSubdev = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();
    SmartPtr<LensHw> mLensSubdev = mLensDev.dynamic_cast_ptr<LensHw>();
    mSensorSubdev->handle_sof(evtdata->_timestamp, evtdata->_frameid);
    mRawProcUnit->notify_sof(evtdata->_timestamp, evtdata->_frameid);
    if (mLensSubdev.ptr())
        mLensSubdev->handle_sof(evtdata->_timestamp, evtdata->_frameid);
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
CamHwIsp20::handleIsp3aReslut(SmartPtr<cam3aResult> &result)
{
    ENTER_CAMHW_FUNCTION();
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (_is_exit) {
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "set 3a config bypass since ia engine has stop");
        return XCAM_RETURN_BYPASS;
    }

    if (_state == CAM_HW_STATE_PREPARED || _state == CAM_HW_STATE_STOPPED ||
            _state == CAM_HW_STATE_PAUSED) {
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "hdr-debug: %s: first set ispparams id[%d]\n",
                        __func__, result->getId());
        if (!mIspParamsDev->is_activated()) {
            ret = mIspParamsDev->start();
            if (ret < 0) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "prepare isp params dev err: %d\n", ret);
            }

            ret = hdr_mipi_prepare_mode(_hdr_mode);
            if (ret < 0) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "hdr mipi start err: %d\n", ret);
            }
        }

        mParamsAssembler->addReadyCondition(result->getType());
    }

    mParamsAssembler->queue(result);

    // set all ready params to drv
    while (_state == CAM_HW_STATE_STARTED &&
            mParamsAssembler->ready()) {
        if (setIspConfig() != XCAM_RETURN_NO_ERROR)
            break;
    }

    EXIT_CAMHW_FUNCTION();
    return ret;
}

void
CamHwIsp20::analyzePpInitEns(SmartPtr<cam3aResult> &result)
{
    if (result->getType() == RESULT_TYPE_TNR_PARAM) {
        // TODO: tnr init_ens should be always on ?
        SmartPtr<RkAiqIspTnrParamsProxy> tnr = nullptr;
        tnr = result.dynamic_cast_ptr<RkAiqIspTnrParamsProxy>();
        if (tnr.ptr()) {
            rk_aiq_isp_tnr_t& tnr_param = tnr->data()->result;
            if(tnr_param.tnr_en) {
                if (tnr_param.mode > 0)
                    mPpModuleInitEns |= ISPP_MODULE_TNR_3TO1;
                else
                    mPpModuleInitEns |= ISPP_MODULE_TNR;

            } else {
                mPpModuleInitEns &= ~ISPP_MODULE_TNR_3TO1;
            }
        }
    } else if (result->getType() == RESULT_TYPE_FEC_PARAM) {
        SmartPtr<RkAiqIspFecParamsProxy> fec = nullptr;
        fec = result.dynamic_cast_ptr<RkAiqIspFecParamsProxy>();
        if (fec.ptr()) {
            rk_aiq_isp_fec_t& fec_param = fec->data()->result;
            if(fec_param.fec_en) {
                if (fec_param.usage == ISPP_MODULE_FEC_ST) {
                    mPpModuleInitEns |= ISPP_MODULE_FEC_ST;
                } else if (fec_param.usage == ISPP_MODULE_FEC) {
                    mPpModuleInitEns |= ISPP_MODULE_FEC;
                }
            } else {
                mPpModuleInitEns &= ~ISPP_MODULE_FEC_ST;
            }
        }
    } else if (result->getType() == RESULT_TYPE_EDGEFLT_PARAM ||
               result->getType() == RESULT_TYPE_YNR_PARAM ||
               result->getType() == RESULT_TYPE_UVNR_PARAM ||
               result->getType() == RESULT_TYPE_SHARPEN_PARAM) {
        // TODO: nr,sharp init_ens always on ?
        mPpModuleInitEns |= ISPP_MODULE_SHP | ISPP_MODULE_NR;
    } else if (result->getType() == RESULT_TYPE_ORB_PARAM) {
        SmartPtr<RkAiqIspOrbParamsProxy> orb = nullptr;
        orb = result.dynamic_cast_ptr<RkAiqIspOrbParamsProxy>();
        if (orb.ptr()) {
            rk_aiq_isp_orb_t& orb_param = orb->data()->result;
            if(orb_param.orb_en)
                mPpModuleInitEns |= ISPP_MODULE_ORB;
            else
                mPpModuleInitEns &= ~ISPP_MODULE_ORB;
        }
    }
}

XCamReturn
CamHwIsp20::handlePpReslut(SmartPtr<cam3aResult> &result)
{
    ENTER_CAMHW_FUNCTION();
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    if (_is_exit) {
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "set pp config bypass since ia engine has stop");
        return XCAM_RETURN_BYPASS;
    }

    if (_state == CAM_HW_STATE_PREPARED || _state == CAM_HW_STATE_STOPPED ||
            _state == CAM_HW_STATE_PAUSED) {
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "RKISPP_CMD_SET_INIT_MODULE");
        analyzePpInitEns(result);
        if (_ispp_sd->io_control(RKISPP_CMD_SET_INIT_MODULE, &mPpModuleInitEns))
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "RKISPP_CMD_SET_INIT_MODULE ioctl failed");
    }
    setPpConfig(result);
    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::setIspConfig()
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    ENTER_CAMHW_FUNCTION();

    SmartPtr<V4l2Buffer> v4l2buf;
    uint32_t frameId = -1;
    {
        SmartLock locker (_isp_params_cfg_mutex);
        while (_effecting_ispparam_map.size() > 4)
            _effecting_ispparam_map.erase(_effecting_ispparam_map.begin());
    }
    if (mIspParamsDev.ptr()) {
        ret = mIspParamsDev->get_buffer(v4l2buf);
        if (ret) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "Can not get isp params buffer \n");
            return XCAM_RETURN_ERROR_PARAM;
        }
    } else
        return XCAM_RETURN_BYPASS;

    cam3aResultList ready_results;
    ret = mParamsAssembler->deQueOne(ready_results, frameId);
    if (ret != XCAM_RETURN_NO_ERROR) {
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "deque isp ready parameter failed\n");
        mIspParamsDev->return_buffer_to_pool (v4l2buf);
        return XCAM_RETURN_ERROR_PARAM;
    }

    LOGD_ANALYZER("----------%s, start config id(%d)'s isp params", __FUNCTION__, frameId);

    struct isp2x_isp_params_cfg update_params;

    update_params.module_en_update = 0;
    update_params.module_ens = 0;
    update_params.module_cfg_update = 0;
    if (_state == CAM_HW_STATE_STOPPED || _state == CAM_HW_STATE_PREPARED || _state == CAM_HW_STATE_PAUSED) {
        // update all ens
        _full_active_isp_params.module_en_update = ~0;
        // just re-config the enabled moddules
        _full_active_isp_params.module_cfg_update = _full_active_isp_params.module_ens;
    } else {
        _full_active_isp_params.module_en_update = 0;
        // use module_ens to store module status, so we can use it to restore
        // the init params for re-start and re-prepare
        /* _full_active_isp_params.module_ens = 0; */
        _full_active_isp_params.module_cfg_update = 0;
    }

    // merge all pending params

#if 0 // TODO: compatible with the following cases
    if (mIspSpThread.ptr()) {
        rk_aiq_isp_other_params_v20_t* isp20_other_result =
            static_cast<rk_aiq_isp_other_params_v20_t*>(aiqIspOtherResult->data().ptr());
        mIspSpThread->update_motion_detection_params(&isp20_other_result->motion);
    }

    ret = convertAiqMeasResultsToIspParams((void*)&update_params, aiqIspMeasResult, aiqIspOtherResult, _lastAiqIspMeasResult);
    if (ret != XCAM_RETURN_NO_ERROR) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "rkisp1_convert_results error\n");
    }

    forceOverwriteAiqIspCfg(update_params, aiqIspMeasResult, aiqIspOtherResult);
#else
    ret = overrideExpRatioToAiqResults(frameId, RK_ISP2X_HDRTMO_ID, ready_results, _hdr_mode);
    if (ret < 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "TMO convertExpRatioToAiqResults error!\n");
    }

    ret = overrideExpRatioToAiqResults(frameId, RK_ISP2X_HDRMGE_ID, ready_results, _hdr_mode);
    if (ret < 0) {
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "MERGE convertExpRatioToAiqResults error!\n");
    }

    if (frameId >= 0) {
        SmartPtr<cam3aResult> awb_res = get_3a_module_result(ready_results, RESULT_TYPE_AWB_PARAM);
        SmartPtr<RkAiqIspAwbParamsProxy> awbParams;
        if (awb_res.ptr()) {
            awbParams = awb_res.dynamic_cast_ptr<RkAiqIspAwbParamsProxy>();
            {
                SmartLock locker (_isp_params_cfg_mutex);
                _effecting_ispparam_map[frameId].awb_cfg = awbParams->data()->result;
            }
        } else {
            SmartLock locker (_isp_params_cfg_mutex);
            /* use the latest */
            if (!_effecting_ispparam_map.empty()) {
                _effecting_ispparam_map[frameId].awb_cfg = (_effecting_ispparam_map.rbegin())->second.awb_cfg;
                LOGW_CAMHW_SUBM(ISP20HW_SUBM, "use frame %d awb params for frame %d !\n",
                                frameId, (_effecting_ispparam_map.rbegin())->first);
            } else {
                LOGW_CAMHW_SUBM(ISP20HW_SUBM, "get awb params from 3a result failed for frame %d !\n", frameId);
            }
        }
    }

    if (merge_isp_results(ready_results, &update_params) != XCAM_RETURN_NO_ERROR)
        LOGE_CAMHW_SUBM(ISP20HW_SUBM, "ISP parameter translation error\n");
#endif
    uint64_t module_en_update_partial = 0;
    uint64_t module_cfg_update_partial = 0;
    gen_full_isp_params(&update_params, &_full_active_isp_params,
                        &module_en_update_partial, &module_cfg_update_partial);

    if (_state == CAM_HW_STATE_STOPPED)
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "ispparam ens 0x%llx, en_up 0x%llx, cfg_up 0x%llx",
                        _full_active_isp_params.module_ens,
                        _full_active_isp_params.module_en_update,
                        _full_active_isp_params.module_cfg_update);

#ifdef RUNTIME_MODULE_DEBUG
    _full_active_isp_params.module_en_update &= ~g_disable_isp_modules_en;
    _full_active_isp_params.module_ens |= g_disable_isp_modules_en;
    _full_active_isp_params.module_cfg_update &= ~g_disable_isp_modules_cfg_update;
    module_en_update_partial = _full_active_isp_params.module_en_update;
    module_cfg_update_partial = _full_active_isp_params.module_cfg_update;
#endif
    // dump_isp_config(&_full_active_isp_params, aiqIspMeasResult, aiqIspOtherResult);

    {
        SmartLock locker (_isp_params_cfg_mutex);
        if (frameId < 0) {
            _effecting_ispparam_map[0].isp_params = _full_active_isp_params;
        } else {
            _effecting_ispparam_map[frameId].isp_params = _full_active_isp_params;
        }
    }
    if (v4l2buf.ptr()) {
        struct isp2x_isp_params_cfg* isp_params;
        int buf_index = v4l2buf->get_buf().index;

        isp_params = (struct isp2x_isp_params_cfg*)v4l2buf->get_buf().m.userptr;
        *isp_params = _full_active_isp_params;
        isp_params->module_en_update = module_en_update_partial;
        isp_params->module_cfg_update = module_cfg_update_partial;
        //TODO: isp driver has bug now, lsc cfg_up should be set along with
        //en_up
        if (isp_params->module_cfg_update & ISP2X_MODULE_LSC)
            isp_params->module_en_update |= ISP2X_MODULE_LSC;
        isp_params->frame_id = frameId;

        SmartPtr<SensorHw> mSensorSubdev = mSensorDev.dynamic_cast_ptr<SensorHw>();
        if (mSensorSubdev.ptr()) {
            memset(&isp_params->exposure, 0, sizeof(isp_params->exposure));
            SmartPtr<RkAiqExpParamsProxy> expParam;

            if (mSensorSubdev->getEffectiveExpParams(expParam, frameId) < 0) {
                LOGE_CAMHW_SUBM(ISP20HW_SUBM, "frame_id(%d), get exposure failed!!!\n", frameId);
            } else {
                if (RK_AIQ_HDR_GET_WORKING_MODE(_hdr_mode) == RK_AIQ_WORKING_MODE_NORMAL) {
                    isp_params->exposure.linear_exp.analog_gain_code_global = \
                            expParam->data()->aecExpInfo.LinearExp.exp_sensor_params.analog_gain_code_global;
                    isp_params->exposure.linear_exp.coarse_integration_time = \
                            expParam->data()->aecExpInfo.LinearExp.exp_sensor_params.coarse_integration_time;
                } else {
                    isp_params->exposure.hdr_exp[0].analog_gain_code_global = \
                            expParam->data()->aecExpInfo.HdrExp[0].exp_sensor_params.analog_gain_code_global;
                    isp_params->exposure.hdr_exp[0].coarse_integration_time = \
                            expParam->data()->aecExpInfo.HdrExp[0].exp_sensor_params.coarse_integration_time;
                    isp_params->exposure.hdr_exp[1].analog_gain_code_global = \
                            expParam->data()->aecExpInfo.HdrExp[1].exp_sensor_params.analog_gain_code_global;
                    isp_params->exposure.hdr_exp[1].coarse_integration_time = \
                            expParam->data()->aecExpInfo.HdrExp[1].exp_sensor_params.coarse_integration_time;
                    isp_params->exposure.hdr_exp[2].analog_gain_code_global = \
                            expParam->data()->aecExpInfo.HdrExp[2].exp_sensor_params.analog_gain_code_global;
                    isp_params->exposure.hdr_exp[2].coarse_integration_time = \
                            expParam->data()->aecExpInfo.HdrExp[2].exp_sensor_params.coarse_integration_time;
                }
            }
        }

        if (mIspParamsDev->queue_buffer (v4l2buf) != 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "RKISP1: failed to ioctl VIDIOC_QBUF for index %d, %d %s.\n",
                            buf_index, errno, strerror(errno));
            mIspParamsDev->return_buffer_to_pool (v4l2buf);
            return XCAM_RETURN_ERROR_IOCTL;
        }

        ispModuleEns = _full_active_isp_params.module_ens;
        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "ispparam ens 0x%llx, en_up 0x%llx, cfg_up 0x%llx",
                        _full_active_isp_params.module_ens,
                        isp_params->module_en_update,
                        isp_params->module_cfg_update);

        LOGD_CAMHW_SUBM(ISP20HW_SUBM, "device(%s) queue buffer index %d, queue cnt %d, check exit status again[exit: %d]",
                        XCAM_STR (mIspParamsDev->get_device_name()),
                        buf_index, mIspParamsDev->get_queued_bufcnt(), _is_exit);
        if (_is_exit)
            return XCAM_RETURN_BYPASS;
    } else
        return XCAM_RETURN_BYPASS;

    EXIT_CAMHW_FUNCTION();
    return ret;
}

XCamReturn
CamHwIsp20::setPpConfig(SmartPtr<cam3aResult> &result)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    ENTER_CAMHW_FUNCTION();
    if (result->getType() == RESULT_TYPE_TNR_PARAM) {
        ret = mTnrStreamProcUnit->config_params(result->getId(), result);
    } else if (result->getType() == RESULT_TYPE_FEC_PARAM)
        ret = mFecParamStream->config_params(result->getId(), result);
    else if (result->getType() == RESULT_TYPE_EDGEFLT_PARAM ||
             result->getType() == RESULT_TYPE_YNR_PARAM ||
             result->getType() == RESULT_TYPE_UVNR_PARAM ||
             result->getType() == RESULT_TYPE_SHARPEN_PARAM ||
             result->getType() == RESULT_TYPE_ORB_PARAM) {
        ret = mNrStreamProcUnit->config_params(result->getId(), result);
    }
    EXIT_CAMHW_FUNCTION();
    return ret;
}

SmartPtr<cam3aResult>
CamHwIsp20::get_3a_module_result (cam3aResultList &results, int32_t type)
{
    cam3aResultList::iterator i_res = results.begin();
    SmartPtr<cam3aResult> res;

    auto it = std::find_if(
                  results.begin(), results.end(),
    [&](const SmartPtr<cam3aResult> &r) {
        return type == r->getType();
    });
    if (it != results.end()) {
        res = *it;
    }

    return res;
}

XCamReturn CamHwIsp20::get_stream_format(rkaiq_stream_type_t type, struct v4l2_format &format)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;

    switch (type)
    {
    case RKISP20_STREAM_MIPITX_S:
    case RKISP20_STREAM_MIPITX_M:
    case RKISP20_STREAM_MIPITX_L:
    {
        memset(&format, 0, sizeof(format));
        ret = mRawCapUnit->get_tx_device(0)->get_format(format);
        break;
    }
    case RKISP20_STREAM_SP:
    case RKISP20_STREAM_NR:
    {
        struct v4l2_subdev_format isp_fmt;
        isp_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        isp_fmt.pad = 2;
        ret = mIspCoreDev->getFormat(isp_fmt);
        if (ret == XCAM_RETURN_NO_ERROR) {
            SmartPtr<BaseSensorHw> sensorHw;
            sensorHw = mSensorDev.dynamic_cast_ptr<BaseSensorHw>();
            format.fmt.pix.width = isp_fmt.format.width;
            format.fmt.pix.height = isp_fmt.format.height;
            format.fmt.pix.pixelformat = get_v4l2_pixelformat(isp_fmt.format.code);
        }
        break;
    }
    default:
        ret = XCAM_RETURN_ERROR_PARAM;
        break;
    }
    return ret;
}

XCamReturn CamHwIsp20::get_sp_resolution(int &width, int &height, int &aligned_w, int &aligned_h)
{
    return mSpStreamUnit->get_sp_resolution(width, height, aligned_w, aligned_h);
}

bool CamHwIsp20::get_pdaf_support()
{
    bool pdaf_support = false;

    if (mPdafStreamUnit.ptr())
        pdaf_support = mPdafInfo.pdaf_support;

    return pdaf_support;
}

void CamHwIsp20::notify_isp_stream_status(bool on)
{
    if (on) {
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "camId:%d, %s on", mCamPhyId, __func__);
        XCamReturn ret = hdr_mipi_start_mode(_hdr_mode);
        if (ret < 0) {
            LOGE_CAMHW_SUBM(ISP20HW_SUBM, "hdr mipi start err: %d\n", ret);
        }
        _isp_stream_status = ISP_STREAM_STATUS_STREAM_ON;

        if (mHwResLintener) {
            SmartPtr<Isp20Evt> ispEvt =
                new Isp20Evt(this, mSensorDev.dynamic_cast_ptr<SensorHw>());

            SmartPtr<V4l2Device> dev(NULL);
            SmartPtr<Isp20EvtBuffer> ispEvtbuf =
                new Isp20EvtBuffer(ispEvt, dev);

            ispEvtbuf->_buf_type = VICAP_STREAM_ON_EVT;
            SmartPtr<VideoBuffer> vbuf = ispEvtbuf.dynamic_cast_ptr<VideoBuffer>();

            mHwResLintener->hwResCb(vbuf);
        }
    } else {
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "camId:%d, %s off", mCamPhyId, __func__);
        _isp_stream_status = ISP_STREAM_STATUS_STREAM_OFF;
        // if CIFISP_V4L2_EVENT_STREAM_STOP event is listened, isp driver
        // will wait isp params streaming off
        {
            SmartLock locker(_stop_cond_mutex);
            if (mIspParamStream.ptr())
                mIspParamStream->stop();
        }
        hdr_mipi_stop();
        LOGI_CAMHW_SUBM(ISP20HW_SUBM, "camId:%d, %s off done", mCamPhyId, __func__);
    }
}

}; //namspace RkCam
