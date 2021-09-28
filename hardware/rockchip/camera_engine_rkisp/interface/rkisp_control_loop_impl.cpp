#include "rkisp_control_loop_impl.h"
#include "rkisp_dev_manager.h"

#include <isp_poll_thread.h>
#include <isp_image_processor.h>
#include "iq/x3a_analyze_tuner.h"
#include "x3a_analyzer_rkiq.h"
#include "dynamic_analyzer_loader.h"

#include "mediactl-priv.h"
#include "mediactl.h"
#include "v4l2subdev.h"
#include "camera_metadata_hidden.h"
#include "rkcamera_vendor_tags.h"

#include <base/xcam_log.h>
#include <calib_xml/calibdb.h>

#define V4L2_CAPTURE_MODE_STILL 0x2000
#define V4L2_CAPTURE_MODE_VIDEO 0x4000
#define V4L2_CAPTURE_MODE_PREVIEW 0x8000

#define MAX_MEDIA_INDEX 16
#define MAX_SENSOR_NUM  16
#define AIQ_CONTEXT_CAST(context)  ((RkispDeviceManager*)(context))

#ifdef ANDROID_VERSION_ABOVE_8_X
#define RK_3A_TUNING_FILE_PATH  "/vendor/etc/camera/rkisp1/"
#elif defined(ANDROID_PLATEFORM)
#define RK_3A_TUNING_FILE_PATH  "/etc/camera/rkisp1/"
#else
#define RK_3A_TUNING_FILE_PATH  "/etc/iqfiles/"
#endif

using namespace XCam;
using namespace android;

CameraMetadata RkispDeviceManager::staticMeta;
static vendor_tag_ops_t rkcamera_vendor_tag_ops_instance;
extern CalibDb* CamIa10_construct_calib_maps(char *aiqb_data_file);

typedef enum RKISP_CL_STATE_enum {
    RKISP_CL_STATE_INVALID  = -1,
    RKISP_CL_STATE_INITED   =  0,
    RKISP_CL_STATE_PREPARED     ,
    RKISP_CL_STATE_STARTED      ,
    RKISP_CL_STATE_PAUSED       ,
} RKISP_CL_STATE_e;

int rkisp_cl_init(void** cl_ctx, const char* tuning_file_path,
                  const cl_result_callback_ops_t *callback_ops) {
    xcam_get_log_level();
    LOGD("--------------------------rkisp_cl_init");
    RkispDeviceManager *device_manager = new RkispDeviceManager(callback_ops);
    // deprecated, use auto selected iq file
#if 0
    if (tuning_file_path && access(tuning_file_path, F_OK) == 0) {
        device_manager->set_iq_path(tuning_file_path);
        device_manager->set_has_3a(true);
        LOGD("Enable 3a, using IQ file path %s", tuning_file_path);
    } else {
        device_manager->set_has_3a(false);
        LOGD("Disable 3a, don't find IQ file");
    }
#endif
    // set vendor ops
    RkCamera3VendorTags::get_vendor_tag_ops(&rkcamera_vendor_tag_ops_instance);
    set_camera_metadata_vendor_ops(&rkcamera_vendor_tag_ops_instance);
    device_manager->_cl_state = RKISP_CL_STATE_INITED;
    *cl_ctx = (void*)device_manager;
    return 0;
}

static int __rkisp_get_isp_ver(V4l2Device* vdev, int* isp_ver) {
    struct v4l2_capability cap;

    if (vdev->query_cap(cap) == XCAM_RETURN_NO_ERROR) {
        char *p;
        p = strrchr((char*)cap.driver, '_');
        if (p == NULL)
            goto out;
        // version info:
        // rk3399,rk3288: 0
        // rk3326:        2
        // rk1808:        3
        if (*(p + 1) != 'v')
            goto out;
        *isp_ver = atoi(p + 2);

        return 0;
    }

out:
    LOGE("get isp version failed !");
    return -1;
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
    // To discriminate between then sensor connected to preisp and connect to
    // ISP, add suffix "-preisp" to sensor name if connected to preisp.
    if (strstr(sensor_entity_name, "1608")) {
        strcat(sensor_name_full, "-preisp");
    }
    module_name = base_inf->module;
    lens_name = base_inf->lens;

    // not use otp info for iq file name, because otp info may contain unvalid
    // character
#if 0
    if (fac_inf->flag) {
        if (!strlen(fac_inf->module) || !strlen(fac_inf->lens)) {
            LOGE("no camera module fac info, check the drv !");
            return -1;
        }
        // prefer to use otp info
        module_name = fac_inf->module;
        lens_name = fac_inf->lens;
    }
#endif
    sprintf(iqfile_name, "%s_%s_%s.xml",
            sensor_name_full, module_name, lens_name);

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

static rkisp_metadata_info_t gDefMetadata[MAX_SENSOR_NUM];

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

static int __rkisp_get_iq_exp_infos(SmartPtr<V4l2SubDevice> subDev, rkisp_metadata_info_t *metadata_info)
{
	struct rkmodule_inf camera_mod_info;
	char iq_file_full_name[256];
	char iq_file_name[128];
	CalibDb* calibdb_p = NULL;
	int ret = 0;

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
	strcpy(iq_file_full_name, RK_3A_TUNING_FILE_PATH);
	__rkisp_auto_select_iqfile(&camera_mod_info, metadata_info->entity_name, iq_file_name);
	strcat(iq_file_full_name, iq_file_name);
	if (access(iq_file_full_name, F_OK) == 0) {
		calibdb_p = CamIa10_construct_calib_maps(iq_file_full_name);
		if (calibdb_p) {
			CamCalibAecGlobal_t* pAecGlobal;
			CamCalibDbGetAecGlobal(calibdb_p->GetCalibDbHandle(), &pAecGlobal);
			if((pAecGlobal == NULL) || (pAecGlobal->GainRange.array_size%7 != 0)){
				LOGE("iq xml gain range size error!");
				return -1;
			}
			CamCalibAecExpSeparate_t* pExpSeparate = NULL;
			CamCalibDbGetExpSeparateByName(calibdb_p->GetCalibDbHandle(),pAecGlobal,"NORMAL", &pExpSeparate);
			if(pExpSeparate == NULL){
				LOGE("CamCalibDbGetExpSeparateByName fail");
				return -1;
			}

			int index = (pAecGlobal->GainRange.array_size/7 - 1)*7+1;
			metadata_info->gain_range[0] = pAecGlobal->GainRange.pGainRange[0];
			metadata_info->gain_range[1] = pAecGlobal->GainRange.pGainRange[index];
			metadata_info->time_range[0] = pExpSeparate->ecmTimeDot.fCoeff[0];
			metadata_info->time_range[1] = pExpSeparate->ecmTimeDot.fCoeff[5];
		}
	}else {
		LOGW("calib file %s not found! Ignore it if not raw sensor.", iq_file_full_name);
	}
	subDev->close ();
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


static int __rkisp_get_sensor_name(const char* vnode, char* sensor_name) {
    char sys_path[64];
    struct media_device *device = NULL;
    uint32_t nents, j, i = 0;
    FILE *fp;
    int ret;

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
          struct media_entity *entity = media_get_entity (device, j);
          const char *devname = media_entity_get_devname (entity);
          if (NULL != devname) {
            // get dev path
            char devpath[32];
            char sysname[32];
            char target[1024];
            char *p;

            sprintf(sysname, "/sys/dev/char/%u:%u", entity->info.v4l.major,
                entity->info.v4l.minor);
            ret = readlink(sysname, target, sizeof(target));
            if (ret < 0)
                return -errno;

            target[ret] = '\0';
            p = strrchr(target, '/');
            if (p == NULL)
              continue ;
            sprintf(devpath, "/dev/%s", p + 1);

            LOGD("entity name %s", entity->info.name);
            if (!strcmp (devpath, vnode)) {
              strcpy(sensor_name, entity->info.name);
              media_device_unref (device);
              return 0;
            }
          }
        }
        media_device_unref (device);
    }

    return -1;
}

int rkisp_cl_prepare(void* cl_ctx,
                     const struct rkisp_cl_prepare_params_s* prepare_params) {
	LOGD("--------------------------rkisp_cl_prepare");

    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    RkispDeviceManager *device_manager = AIQ_CONTEXT_CAST (cl_ctx);
    SmartPtr<V4l2SubDevice> isp_dev = NULL;
    SmartPtr<V4l2SubDevice> sensor_dev = NULL;
    SmartPtr<V4l2SubDevice> vcm_dev = NULL;
    SmartPtr<V4l2SubDevice> fl_dev[RKISP_SENSOR_ATTACHED_FLASH_MAX_NUM];
    SmartPtr<V4l2Device> stats_dev = NULL;
    SmartPtr<V4l2Device> param_dev = NULL;

    if (device_manager->_cl_state == RKISP_CL_STATE_INVALID) {
        LOGE("%s: cl haven't been init %d", __FUNCTION__, device_manager->_cl_state);
        return -1;
    }
    if (device_manager->_cl_state >= RKISP_CL_STATE_PREPARED) {
        LOGI("%s: cl has already been prepared, now in state %d",
             __FUNCTION__, device_manager->_cl_state);
        return 0;
    }
	LOGD("rkisp_cl_prepare, isp: %s, sensor: %s, stats: %s, params: %s, lens: %s, "
         "fl 0: %s, fl 1: %s",
        prepare_params->isp_sd_node_path,
        prepare_params->sensor_sd_node_path,
        prepare_params->isp_vd_stats_path,
        prepare_params->isp_vd_params_path,
        prepare_params->lens_sd_node_path,
        prepare_params->flashlight_sd_node_path[0],
        prepare_params->flashlight_sd_node_path[1]);

    isp_dev = new V4l2SubDevice (prepare_params->isp_sd_node_path);
    ret = isp_dev->open ();
    if (ret == XCAM_RETURN_NO_ERROR) {
        isp_dev->subscribe_event (V4L2_EVENT_FRAME_SYNC);
        device_manager->set_event_subdevice(isp_dev);
    } else {
        LOGE("failed to open isp subdev");
        return -1;
    }

    sensor_dev = new V4l2SubDevice (prepare_params->sensor_sd_node_path);
    ret = sensor_dev->open ();
    if (ret == XCAM_RETURN_NO_ERROR) {
        //sensor_dev->subscribe_event (V4L2_EVENT_FRAME_SYNC);
        char sensor_name[32];
        if (__rkisp_get_sensor_name(prepare_params->sensor_sd_node_path, sensor_name))
            LOGW("%s: can't get sensor name");
        device_manager->set_sensor_subdevice(sensor_dev, sensor_name);
    } else {
        LOGE("failed to open isp subdev");
        return -1;
    }

    stats_dev = new V4l2Device (prepare_params->isp_vd_stats_path);
    stats_dev->set_sensor_id (0);
    stats_dev->set_capture_mode (V4L2_CAPTURE_MODE_VIDEO);
    stats_dev->set_buf_type(V4L2_BUF_TYPE_META_CAPTURE);
    stats_dev->set_mem_type (V4L2_MEMORY_MMAP);
    stats_dev->set_buffer_count (4);
    ret = stats_dev->open ();
    if (ret == XCAM_RETURN_NO_ERROR) {
        device_manager->set_isp_stats_device (stats_dev);
    } else {
        LOGE("failed to open statistics dev");
        return -1;
    }

    int isp_ver = 0;
    if (__rkisp_get_isp_ver(stats_dev.ptr(), &isp_ver))
        LOGW("get isp version failed, please check ISP driver !");
    LOGD("isp version is %d !", isp_ver);
    device_manager->set_isp_ver(isp_ver);
    param_dev = new V4l2Device (prepare_params->isp_vd_params_path);
    param_dev->set_sensor_id (0);
    param_dev->set_capture_mode (V4L2_CAPTURE_MODE_VIDEO);
    param_dev->set_buf_type(V4L2_BUF_TYPE_META_OUTPUT);
    param_dev->set_mem_type (V4L2_MEMORY_MMAP);
    param_dev->set_buffer_count (4);
    ret = param_dev->open ();
    if (ret == XCAM_RETURN_NO_ERROR) {
        device_manager->set_isp_params_device (param_dev);
    } else {
        LOGE("failed to open parameter dev");
        return -1;
    }

    if (prepare_params->lens_sd_node_path) {
        vcm_dev = new V4l2SubDevice(prepare_params->lens_sd_node_path);
        ret = vcm_dev->open ();
        if (ret != XCAM_RETURN_NO_ERROR) {
            LOGE("failed to open lens subdev");
            return -1;
        }
    }

    for (int i = 0; i < RKISP_SENSOR_ATTACHED_FLASH_MAX_NUM; i++) {
        if (prepare_params->flashlight_sd_node_path[i]) {
            fl_dev[i] = new V4l2SubDevice(prepare_params->flashlight_sd_node_path[i]);
            ret = fl_dev[i]->open ();
            if (ret != XCAM_RETURN_NO_ERROR) {
                LOGE("failed to open flashlight subdev");
                return -1;
            }
        } else
            fl_dev[i] = nullptr;
    }

    SmartPtr<IspController> isp_controller = new IspController ();
    isp_controller->set_sensor_subdev(sensor_dev);
    isp_controller->set_isp_stats_device(stats_dev);
    isp_controller->set_isp_params_device(param_dev);
    isp_controller->set_isp_ver(isp_ver);
    if (vcm_dev.ptr())
        isp_controller->set_vcm_subdev(vcm_dev);
    isp_controller->set_fl_subdev(fl_dev);

    SmartPtr<IspPollThread> isp_poll_thread = new IspPollThread ();
    isp_poll_thread->set_isp_controller (isp_controller);
    device_manager->set_poll_thread (isp_poll_thread);
    device_manager->set_isp_controller (isp_controller);

    SmartPtr<ImageProcessor> isp_processor = new IspImageProcessor (isp_controller, true);
    device_manager->add_image_processor (isp_processor);

    struct rkmodule_inf camera_mod_info;
    xcam_mem_clear (camera_mod_info);
    if (__rkisp_get_cam_module_info(sensor_dev.ptr() , &camera_mod_info)) {
            LOGE("failed to get cam module info");
            return -1;
    }

    char iq_file_full_name[256];
    char iq_file_name[128];
    xcam_mem_clear (iq_file_full_name);
    xcam_mem_clear (iq_file_name);
    strcpy(iq_file_full_name, RK_3A_TUNING_FILE_PATH);
    if (__rkisp_auto_select_iqfile(&camera_mod_info,
                                   device_manager->get_sensor_entity_name(),
                                   iq_file_name)) {
        LOGE("failed to get iq file name !");
        device_manager->set_has_3a(false);
        //return -1;
    } else {
        strcat(iq_file_full_name, iq_file_name);
        if (access(iq_file_full_name, F_OK) == 0) {
            device_manager->set_iq_path(iq_file_full_name);
            device_manager->set_has_3a(true);
#ifdef ANDROID_VERSION_ABOVE_8_X
            property_set(CAM_IQ_PROPERTY_KEY,iq_file_full_name);
#endif
        } else {
            LOGE("can't access iq file %s !", iq_file_full_name);
            device_manager->set_has_3a(false);
        }
    }

    SmartPtr<X3aAnalyzerRKiq> aiq_analyzer =
        new X3aAnalyzerRKiq (device_manager, isp_controller, device_manager->get_iq_path());
    CamOTPGlobal_t cam_otp;
    cam_otp.awb.enable = camera_mod_info.awb.flag;
    cam_otp.awb.golden_r_value = camera_mod_info.awb.r_value;
    cam_otp.awb.golden_gr_value = camera_mod_info.awb.gr_value;
    cam_otp.awb.golden_gb_value = camera_mod_info.awb.gb_value;
    cam_otp.awb.golden_b_value = camera_mod_info.awb.b_value;
    cam_otp.lsc.enable = camera_mod_info.lsc.flag;
    aiq_analyzer->setOtpInfo(cam_otp);
    device_manager->set_3a_analyzer (aiq_analyzer);

    device_manager->set_static_metadata (prepare_params->staticMeta);

    device_manager->prepare ();

    device_manager->_cl_state = RKISP_CL_STATE_PREPARED;
    LOGD("--------------------------rkisp_cl_prepare done");

    return 0;
}

int rkisp_cl_start(void* cl_ctx) {
    int ret = 0;
    LOGD("--------------------------rkisp_cl_start");
    RkispDeviceManager *device_manager = AIQ_CONTEXT_CAST (cl_ctx);

    if (device_manager->_cl_state < RKISP_CL_STATE_PREPARED) {
        LOGE("%s: invalid cl state %d", __FUNCTION__, device_manager->_cl_state);
        return -1;
    }
    if (device_manager->_cl_state == RKISP_CL_STATE_PAUSED) {
        device_manager->resume_dequeue ();
    } else {
        ret = device_manager->start();
        if (ret != XCAM_RETURN_NO_ERROR) {
            device_manager->stop();
            device_manager->pause_dequeue ();
        }
    }
    device_manager->_cl_state = RKISP_CL_STATE_STARTED;
    LOGD("--------------------------rkisp_cl_start done");

    return ret;
}

int rkisp_cl_set_frame_params(const void* cl_ctx,
                              const struct rkisp_cl_frame_metadata_s* frame_params) {
    int ret = 0;
    LOGD("--------------------------rkisp_cl_set_frame_params");
    RkispDeviceManager *device_manager = AIQ_CONTEXT_CAST (cl_ctx);

    ret = device_manager->set_control_params(frame_params->id, frame_params->metas);
    if (ret != XCAM_RETURN_NO_ERROR) {
        LOGE("@%s %d: set_control_params failed ", __FUNCTION__, __LINE__);
    }
    return 0;
}

// implement interface stop as pause so we could keep all the 3a status,
// and could speed up 3a converged
int rkisp_cl_stop(void* cl_ctx) {
    RkispDeviceManager *device_manager = AIQ_CONTEXT_CAST (cl_ctx);
    LOGD("--------------------------rkisp_cl_stop");
    //device_manager->stop();
    device_manager->pause_dequeue ();
    device_manager->_cl_state = RKISP_CL_STATE_PAUSED;
    LOGD("--------------------------rkisp_cl_stop done");
    return 0;
}

void rkisp_cl_deinit(void* cl_ctx) {
    LOGD("--------------------------rkisp_cl_deinit");
    RkispDeviceManager *device_manager = AIQ_CONTEXT_CAST (cl_ctx);
    if (device_manager->is_running ()) {
        device_manager->stop();
        device_manager->pause_dequeue ();
    }
    device_manager->_cl_state = RKISP_CL_STATE_INVALID;
    delete device_manager;
    device_manager = NULL;
    LOGD("--------------------------rkisp_cl_deinit done");
}

