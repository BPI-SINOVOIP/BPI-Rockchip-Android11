/*
 * Copyright (c) 2020, Rockchip Electronics Co., Ltd
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h> /* low-level i/o */
#include <system/camera_metadata.h> //vendor Metadata
#include <camera_metadata_hidden.h>

//rkaiq head files
#include <mediactl.h>
#include <v4l2_device.h>
#include <base/xcam_log.h>
#include <rk-camera-module.h>
#include <RkAiqManager.h>
#include <RkAiqCalibDb.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <mediactl/mediactl-priv.h>

//local
#include "rkisp_control_aiq.h"
#include "rkisp_control_loop.h"
#include "rk_aiq_user_api_sysctl.h"
#include "rkcamera_vendor_tags.h"

//test
#include "rk_aiq_user_api_imgproc.h"

#include "rkaiq.h"
#include "AiqCameraHalAdapter.h"

#include "isp20/CamHwIsp20.h"

#define RK_3A_TUNING_FILE_PATH  "/vendor/etc/camera/rkisp2"
#define RK_3A_TUNING_FILE_PATH2 "/vendor/etc/camera/rkisp2/"

#define MAX_MEDIA_INDEX 16
#define MAX_SENSOR_NUM  16
#define AIQ_CONTEXT_CAST(context)  ((AiqCameraHalAdapter*)(context))

using namespace RkCam;
using namespace XCam;
using namespace android;

CameraMetadata AiqCameraHalAdapter::staticMeta;
static rkisp_metadata_info_t gDefMetadata[MAX_SENSOR_NUM];
static vendor_tag_ops_t rkcamera_vendor_tag_ops_instance;

typedef enum RKISP_CL_STATE_enum {
    RKISP_CL_STATE_INVALID  = -1,
    RKISP_CL_STATE_INITED   =  0,
    RKISP_CL_STATE_PREPARED     ,
    RKISP_CL_STATE_STARTED      ,
    RKISP_CL_STATE_PAUSED       ,
} RKISP_CL_STATE_e;

static int __rkisp_get_sensor_fmt_infos(SmartPtr<V4l2SubDevice> subDev, rkisp_metadata_info_t *metadata_info)
{
	struct v4l2_subdev_frame_interval_enum fintval_enum;
	struct v4l2_subdev_mbus_code_enum  code_enum;
	unsigned int max_res_w = 0, max_res_h = 0;
	unsigned int max_fps_w = 0, max_fps_h = 0, ret = 0;
	float fps = 0.0, max_res_fps = 0.0, max_fps = 0.0;

	ret = subDev->open ();
	if (ret < 0) {
		LOGE("failed to open sub device!");
		return ret;
	}
	memset(&code_enum, 0, sizeof(code_enum));
	code_enum.index = 0;
	if ((ret = subDev->io_control(VIDIOC_SUBDEV_ENUM_MBUS_CODE, &code_enum)) < 0){
		LOGE("enum mbus code failed!");
		return ret;
	}

	memset(&fintval_enum, 0, sizeof(fintval_enum));
	fintval_enum.pad = 0;
	fintval_enum.index = 0;
	fintval_enum.code = code_enum.code;
	while (subDev->io_control(VIDIOC_SUBDEV_ENUM_FRAME_INTERVAL, &fintval_enum) >= 0)
	{
		fps = (float)fintval_enum.interval.denominator / (float)fintval_enum.interval.numerator;
		if ((fintval_enum.width >= max_res_w) && (fintval_enum.height >= max_res_h)){
			if((fintval_enum.width > max_res_w) && (fintval_enum.height > max_res_h)) {
				max_res_fps = fps;
				max_res_w = fintval_enum.width;
				max_res_h = fintval_enum.height;
			} else if (fps > max_res_fps) {
				max_res_fps = fps;
			}
		}

		if (fps >= max_fps){
			if (fps > max_fps) {
				max_fps_w = fintval_enum.width;
				max_fps_h = fintval_enum.height;
				max_fps = fps;
			}else if((fintval_enum.width < max_fps_w) &&
					(fintval_enum.height < max_fps_h)) {
				max_fps_w = fintval_enum.width;
				max_fps_h = fintval_enum.height;
			}
		}
		fintval_enum.index++;
	}

	if (fintval_enum.index == 0){
		LOGE("enum frame interval error, size count is zero");
		return -1;
	}
	if((max_res_w == max_fps_w) && (max_res_h == max_fps_h)){
		metadata_info->res_num = 1;
		metadata_info->full_size.width = max_res_w;
		metadata_info->full_size.height = max_res_h;
		metadata_info->full_size.fps = max_res_fps;
	}else{
		metadata_info->res_num = 2;
		metadata_info->full_size.width = max_res_w;
		metadata_info->full_size.height = max_res_h;
		metadata_info->full_size.fps = max_res_fps;
		metadata_info->binning_size.width = max_fps_w;
		metadata_info->binning_size.height = max_fps_h;
		metadata_info->binning_size.fps = max_fps;
	}
	subDev->close ();
	return 0;
}

static int
__rkisp_get_cam_module_info(V4l2SubDevice* sensor_sd, struct rkmodule_inf* mod_info) {
    if (sensor_sd->io_control(RKMODULE_GET_MODULE_INFO, mod_info) < 0) {
        LOGE("failed to get camera module info");
        return -1;
    }
    return 0;
}

static int
__rkisp_auto_select_iqfile(const struct rkmodule_inf* mod_info,
                           const char* sensor_entity_name,
                           char* iqfile_name)
{
    if (!mod_info || !sensor_entity_name || !iqfile_name)
        return -1;

    //const struct rkmodule_fac_inf* fac_inf = &mod_info->fac;
    const struct rkmodule_base_inf* base_inf = &mod_info->base;
    const char *sensor_name, *module_name, *lens_name;
    char sensor_name_full[32];

    if (!strlen(base_inf->module) || !strlen(base_inf->sensor) ||
        !strlen(base_inf->lens)) {
        LOGE("no camera module fac info, check the drv !");
        return -1;
    }
    sensor_name = base_inf->sensor;
    strncpy(sensor_name_full, sensor_name, 32);
    /* discriminate between then sensor connected to preisp and connect to
    ISP, add suffix "-preisp" to sensor name if connected to preisp.*/
    if (strstr(sensor_entity_name, "1608")) {
        strcat(sensor_name_full, "-preisp");
    }
    module_name = base_inf->module;
    lens_name = base_inf->lens;

    /* not use otp info for iq file name, because otp info may contain unvalid
     character */

    sprintf(iqfile_name, "%s_%s_%s.xml",
            sensor_name_full, module_name, lens_name);

    return 0;
}

static int __rkisp_get_iq_exp_infos(SmartPtr<V4l2SubDevice> subDev, rkisp_metadata_info_t *metadata_info)
{
	struct rkmodule_inf camera_mod_info;
	char iq_file_full_name[256];
	char iq_file_name[128];
	CamCalibDbContext_t * calibdb_p = NULL;
	int ret = 0;
    int array_size;

	ret = subDev->open ();
	if(ret < 0){
		LOGE("sub device open failed");
		return -1;
	}
	xcam_mem_clear (camera_mod_info);
	if (__rkisp_get_cam_module_info(subDev.ptr(), &camera_mod_info)) {
	   LOGE("failed to get cam module info");
	   return -1;
	}
	xcam_mem_clear (iq_file_full_name);
	xcam_mem_clear (iq_file_name);
	strcpy(iq_file_full_name, RK_3A_TUNING_FILE_PATH2);
	__rkisp_auto_select_iqfile(&camera_mod_info, metadata_info->entity_name, iq_file_name);
	strcat(iq_file_full_name, iq_file_name);
	if (access(iq_file_full_name, F_OK) == 0) {
		calibdb_p = RkAiqCalibDb::createCalibDb(iq_file_full_name);
		// if (calibdb_p) {
		// 	CalibDb_AecGainRange_t* GainRange;
		// 	GainRange = &(calibdb_p->sensor.GainRange);
		// 	array_size = calibdb_p->sensor.GainRange.array_size;
		// 	metadata_info->gain_range[0] = calibdb_p->sensor.GainRange.pGainRange[0];
		// 	metadata_info->gain_range[1] = calibdb_p->sensor.GainRange.pGainRange[array_size];
		// 	array_size = calibdb_p->aec.CommCtrl.stAeRoute.LinAeSeperate[0].array_size;
		// 	metadata_info->time_range[0] = calibdb_p->aec.CommCtrl.stAeRoute.LinAeSeperate[0].TimeDot[0];
		// 	metadata_info->time_range[1] = calibdb_p->aec.CommCtrl.stAeRoute.LinAeSeperate[0].TimeDot[array_size];
		// }
	}else {
		LOGW("calib file %s not found! Ignore it if not raw sensor.", iq_file_full_name);
	}
	subDev->close ();
	RkAiqCalibDb::releaseCalibDb();
	return 0;
}

static int __get_device_path(struct media_entity *entity, char *devpath)
{
	char sysname[32];
	char target[1024];
	char *p;
	int ret = 0;

	sprintf(sysname, "/sys/dev/char/%u:%u", entity->info.v4l.major,
			entity->info.v4l.minor);
	ret = readlink(sysname, target, sizeof(target));
	if (ret < 0)
		return ret;

	target[ret] = '\0';
	p = strrchr(target, '/');
	if (p == NULL)
	  return -1;
	sprintf(devpath, "/dev/%s", p + 1);
	return 0;
}

static int __rkisp_get_all_sensor_devices
(
	SmartPtr<V4l2SubDevice> *subdev,
	int *count,
	rkisp_metadata_info_t *meta_info
)
{
    char sys_path[64], devpath[32];
    FILE *fp = NULL;
    struct media_device *device = NULL;
    uint32_t nents, j = 0, i = 0, index = 0;
	const struct media_entity_desc *entity_info = NULL;
	struct media_entity *entity = NULL;

    while (i < MAX_MEDIA_INDEX) {
        snprintf (sys_path, 64, "/dev/media%d", i++);
        fp = fopen (sys_path, "r");
        if (!fp)
          continue;
        fclose (fp);
        device = media_device_new (sys_path);

        /* Enumerate entities, pads and links. */
        media_device_enumerate (device);

        nents = media_get_entities_count (device);
        for (j = 0; j < nents; ++j) {
          entity = media_get_entity (device, j);
          entity_info = media_entity_get_info(entity);
          if ((NULL != entity_info) && (entity_info->type == MEDIA_ENT_T_V4L2_SUBDEV_SENSOR)){
              if (__get_device_path(entity, devpath) < 0){
                  LOGW("failed to get device path of (%s), skip it!", entity_info->name);
                  continue;
              }
              subdev[index] = new V4l2SubDevice (devpath);
              strcpy(meta_info[index].entity_name, entity_info->name);
              index++;
           }
        }
        media_device_unref (device);
    }
    *count = index;
    return 0;
}

int rkisp_construct_iq_default_metadatas(rkisp_metadata_info_t **meta_info, int *num)
{
    int nSensor = 0;
    SmartPtr<V4l2SubDevice> sensor_dev[MAX_SENSOR_NUM];

    __rkisp_get_all_sensor_devices(sensor_dev, &nSensor, gDefMetadata);

    for(int i = 0; i < nSensor; i++){
        if (__rkisp_get_iq_exp_infos(sensor_dev[i], &gDefMetadata[i]) < 0){
            goto EXIT;
        }
        if (__rkisp_get_sensor_fmt_infos(sensor_dev[i], &gDefMetadata[i]) < 0){
            goto EXIT;
        }
    }
    *meta_info = gDefMetadata;
    *num = nSensor;
    return 0;
EXIT:
	*meta_info = NULL;
	*num = 0;
	return -1;
}



int rkisp_cl_init(void** cl_ctx, const char* tuning_file_path,
                  const cl_result_callback_ops_t *callback_ops) {
    char* sns_entity_name = "m01_f_os04a10 1-0036-1";
    xcam_get_log_level();
    LOGD("--------------------------rk_aiq_uapi_sysctl_init");
    rk_aiq_sys_ctx_t* aiq_ctx = NULL;

    aiq_ctx = rk_aiq_uapi_sysctl_init(sns_entity_name, RK_3A_TUNING_FILE_PATH, NULL, NULL);

    *cl_ctx = (void*)aiq_ctx;

    return 0;
}

int rkisp_cl_rkaiq_init(void** cl_ctx, const char* tuning_file_path,
                  const cl_result_callback_ops_t *callbacks_ops,
                  const char* sns_entity_name) {
    xcam_get_log_level();
    LOGD("--------------------------rk_aiq_uapi_sysctl_init");
    rk_aiq_sys_ctx_t* aiq_ctx = NULL;
    aiq_ctx = rk_aiq_uapi_sysctl_init(sns_entity_name, RK_3A_TUNING_FILE_PATH, NULL, NULL);
    RkCamera3VendorTags::get_vendor_tag_ops(&rkcamera_vendor_tag_ops_instance);
    set_camera_metadata_vendor_ops(&rkcamera_vendor_tag_ops_instance);

    LOGD("@%s(%d)aiq_ctx pointer(%p)",__FUNCTION__, __LINE__, aiq_ctx);

    AiqCameraHalAdapter *gAiqCameraHalAdapter = NULL;
    if(aiq_ctx){
        gAiqCameraHalAdapter = new AiqCameraHalAdapter(aiq_ctx->_rkAiqManager,aiq_ctx->_analyzer,aiq_ctx->_camHw);
        gAiqCameraHalAdapter->init(callbacks_ops);
        gAiqCameraHalAdapter->set_aiq_ctx(aiq_ctx);
    }
    *cl_ctx = (void*)gAiqCameraHalAdapter;
    LOGD("@%s(%d)cl_ctx pointer(%p)",__FUNCTION__, __LINE__, cl_ctx);
    return 0;
}

int rkisp_cl_prepare(void* cl_ctx,
                     const struct rkisp_cl_prepare_params_s* prepare_params) {
	LOGD("--------------------------rkisp_cl_prepare");
    char iq_file_full_name[128] = {'\0'};

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOGD("@%s(%d)cl_ctx pointer(%p)",__FUNCTION__, __LINE__, cl_ctx);

    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    rk_aiq_working_mode_t work_mode = RK_AIQ_WORKING_MODE_NORMAL;
    if (strcmp(prepare_params->work_mode, "NORMAL") == 0) {
        work_mode = RK_AIQ_WORKING_MODE_NORMAL;
    } else if (strcmp(prepare_params->work_mode, "HDR2") == 0) {
        work_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
    } else if (strcmp(prepare_params->work_mode, "HDR3") == 0) {
        work_mode = RK_AIQ_WORKING_MODE_ISP_HDR3;
    } else {
        //default
        work_mode = RK_AIQ_WORKING_MODE_NORMAL;
    }
    rk_aiq_static_info_t s_info;
    rk_aiq_uapi_sysctl_getStaticMetas(aiq_ctx->_sensor_entity_name, &s_info);
    // check if hdr mode is supported
    if (work_mode != 0) {
        bool b_work_mode_supported = false;
        rk_aiq_sensor_info_t* sns_info = &s_info.sensor_info;
        for (int i = 0; i < SUPPORT_FMT_MAX; i++)
            // TODO, should decide the resolution firstly,
            // then check if the mode is supported on this
            // resolution
            if ((sns_info->support_fmt[i].hdr_mode == 5/*HDR_X2*/ &&
                work_mode == RK_AIQ_WORKING_MODE_ISP_HDR2) ||
                (sns_info->support_fmt[i].hdr_mode == 6/*HDR_X3*/ &&
                 work_mode == RK_AIQ_WORKING_MODE_ISP_HDR3)) {
                b_work_mode_supported = true;
                break;
            }

        if (!b_work_mode_supported) {
            LOGE("ctx->_sensor_entity_name:%s",aiq_ctx->_sensor_entity_name);
            LOGE("\nWARNING !!!"
                   "work mode %d is not supported, changed to normal !!!\n\n",
                   work_mode);
            work_mode = RK_AIQ_WORKING_MODE_NORMAL;
        }
    }

    gAiqCameraHalAdapter->set_static_metadata (prepare_params->staticMeta);
    gAiqCameraHalAdapter->set_working_mode(work_mode);
    camera_metadata_entry mode_3dnr = gAiqCameraHalAdapter->get_static_metadata().find(RK_NR_FEATURE_3DNR_MODE);
    if(mode_3dnr.count == 1) {
	    ALOGI("RK_MODULE_NR:%d",mode_3dnr.data.u8[0]);
        rk_aiq_uapi_sysctl_setModuleCtl(aiq_ctx,RK_MODULE_NR, mode_3dnr.data.u8[0]);
    }

    ret = rk_aiq_uapi_sysctl_prepare(aiq_ctx, prepare_params->width, prepare_params->height, work_mode);

    CamHwIsp20::selectIqFile(aiq_ctx->_sensor_entity_name, iq_file_full_name);
    property_set(CAM_IQ_PROPERTY_KEY,iq_file_full_name);
    LOGD("--------------------------rkisp_cl_prepare done");

    return 0;
}

int rkisp_cl_start(void* cl_ctx) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOGD("--------------------------rkisp_cl_start");

    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    gAiqCameraHalAdapter->start();
    ret = rk_aiq_uapi_sysctl_start(aiq_ctx);

    LOGD("--------------------------rkisp_cl_start done");
    return ret;
}

int rkisp_cl_set_frame_params(const void* cl_ctx,
                              const struct rkisp_cl_frame_metadata_s* frame_params) {
    int ret = 0;

    LOGD("--------------------------rkisp_cl_set_frame_params");
    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();

    ret = gAiqCameraHalAdapter->set_control_params(frame_params->id, frame_params->metas);
    if (ret != XCAM_RETURN_NO_ERROR) {
        LOGE("@%s %d: set_control_params failed ", __FUNCTION__, __LINE__);
    }

    return 0;
}

// implement interface stop as pause so we could keep all the 3a status,
// and could speed up 3a converged
int rkisp_cl_stop(void* cl_ctx) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    LOGD("--------------------------rkisp_cl_stop");

    gAiqCameraHalAdapter->stop();

    ret = rk_aiq_uapi_sysctl_stop(aiq_ctx, false);

    LOGD("--------------------------rkisp_cl_stop done");
    return 0;
}

void rkisp_cl_deinit(void* cl_ctx) {
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    LOGD("--------------------------rkisp_cl_deinit");
    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    gAiqCameraHalAdapter->deInit();
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    rk_aiq_uapi_sysctl_deinit(aiq_ctx);
    LOGD("--------------------------rkisp_cl_deinit done");
    if (gAiqCameraHalAdapter){
        delete gAiqCameraHalAdapter;
    }
}

int rkisp_cl_setBrightness(const void* cl_ctx, unsigned int level) {
    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    return (int) rk_aiq_uapi_setBrightness(aiq_ctx, level);
}
int rkisp_cl_getBrightness(const void* cl_ctx, unsigned int*level) {
    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    return (int) rk_aiq_uapi_getBrightness(aiq_ctx, level);
}

int rkisp_cl_setContrast(const void* cl_ctx, unsigned int level) {
    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    return (int) rk_aiq_uapi_setContrast(aiq_ctx, level);
}
int rkisp_cl_getContrast(const void* cl_ctx, unsigned int *level) {
    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    return (int) rk_aiq_uapi_getContrast(aiq_ctx, level);
}

int rkisp_cl_setSaturation(const void* cl_ctx, unsigned int level) {
    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    return (int) rk_aiq_uapi_setSaturation(aiq_ctx, level);
}
int rkisp_cl_getSaturation(const void* cl_ctx, unsigned int *level) {
    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    return (int) rk_aiq_uapi_getSaturation(aiq_ctx, level);
}

void setMulCamConc(const void* cl_ctx, bool cc) {
    AiqCameraHalAdapter * gAiqCameraHalAdapter = AIQ_CONTEXT_CAST (cl_ctx);
    rk_aiq_sys_ctx_t *aiq_ctx = gAiqCameraHalAdapter->get_aiq_ctx();
    rk_aiq_uapi_sysctl_setMulCamConc(aiq_ctx, true);
}
