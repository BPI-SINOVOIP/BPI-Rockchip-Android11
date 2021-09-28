/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "hwc_rk"

#include <inttypes.h>
#ifdef TARGET_BOARD_PLATFORM_RK3368
#include <hardware/img_gralloc_public.h>
#endif
#include "hwc_rockchip.h"
#include "hwc_util.h"

#if USE_GRALLOC_4
#include "src/mali_gralloc_formats.h"
#include "drmgralloc4.h"
#endif

namespace android {

int hwc_init_version()
{
    char acVersion[50];
    char acCommit[50];
    memset(acVersion,0,sizeof(acVersion));

    strcpy(acVersion,GHWC_VERSION);

#ifdef TARGET_BOARD_PLATFORM_RK3288
    strcat(acVersion,"-rk3288");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3368
    strcat(acVersion,"-rk3368");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3366
    strcat(acVersion,"-rk3366");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3399
    strcat(acVersion,"-rk3399");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3326
    strcat(acVersion,"-rk3326");
#endif


#ifdef TARGET_BOARD_PLATFORM_RK3126C
    strcat(acVersion,"-rk3126c");
#endif

#ifdef TARGET_BOARD_PLATFORM_RK3328
    strcat(acVersion,"-rk3328");
#endif

#ifdef RK_MID
    strcat(acVersion,"-MID");
#endif
#ifdef RK_BOX
    strcat(acVersion,"-BOX");
#endif
#ifdef RK_PHONE
    strcat(acVersion,"-PHONE");
#endif
#ifdef RK_VIR
    strcat(acVersion,"-VR");
#endif

    /* RK_GRAPHICS_VER=commit-id:067e5d0: only keep string after '=' */
    sscanf(RK_GRAPHICS_VER, "%*[^=]=%s", acCommit);

    property_set( PROPERTY_TYPE ".ghwc.version", acVersion);
    property_set( PROPERTY_TYPE ".ghwc.commit", acCommit);
    ALOGD(RK_GRAPHICS_VER);
    return 0;
}

#if USE_AFBC_LAYER

#ifdef TARGET_BOARD_PLATFORM_RK3368
int HALPixelFormatGetCompression(int iFormat)
{
	/* Extension format. Return only the compression bits. */
	if (iFormat >= 0x100 && iFormat <= 0x1FF)
		return (iFormat & 0x70) >> 4;

	/* Upstream formats are not compressible unless they are redefined as
	 * extension formats (e.g. RGB_565, BGRA_8888).
	 */
	return HAL_FB_COMPRESSION_NONE;
}
#endif

bool isAfbcInternalFormat(uint64_t internal_format)
{
#ifdef TARGET_BOARD_PLATFORM_RK3368
    return (HALPixelFormatGetCompression(internal_format)==HAL_FB_COMPRESSION_NONE)?false:true;
#elif defined(TARGET_BOARD_PLATFORM_RK3399)
    return (internal_format & GRALLOC_ARM_INTFMT_AFBC);             // for Midgard gralloc r14
#elif defined(TARGET_BOARD_PLATFORM_RK3326)
    ALOGD_IF(log_level(DBG_VERBOSE),"internal_format : %" PRIx64 ", MALI_GRALLOC_INTFMT_AFBC_BASIC : %llx", internal_format, MALI_GRALLOC_INTFMT_AFBC_BASIC);
    return (internal_format & MALI_GRALLOC_INTFMT_AFBC_BASIC);      // for Bifrost gralloc r8
#else
    UN_USED(internal_format);
    return false;
#endif
}
#endif

#if RK_INVALID_REFRESH
int init_thread_pamaters(threadPamaters* mThreadPamaters)
{
    if(mThreadPamaters) {
        mThreadPamaters->count = 0;
        pthread_mutex_init(&mThreadPamaters->mtx, NULL);
        pthread_mutex_init(&mThreadPamaters->mlk, NULL);
        pthread_cond_init(&mThreadPamaters->cond, NULL);
    } else {
        ALOGE("{%s}%d,mThreadPamaters is NULL",__FUNCTION__,__LINE__);
    }
    return 0;
}

int free_thread_pamaters(threadPamaters* mThreadPamaters)
{
    if(mThreadPamaters) {
        pthread_mutex_destroy(&mThreadPamaters->mtx);
        pthread_mutex_destroy(&mThreadPamaters->mlk);
        pthread_cond_destroy(&mThreadPamaters->cond);
    } else {
        ALOGE("{%s}%d,mThreadPamaters is NULL",__FUNCTION__,__LINE__);
    }
    return 0;
}

void TimeInt2Obj(int imSecond, timeval *ptVal)
{
    ptVal->tv_sec=imSecond/1000;
    ptVal->tv_usec=(imSecond%1000)*1000;
}

int hwc_static_screen_opt_set(bool isGLESComp)
{
    struct itimerval tv = {{0,0},{0,0}};
    if (!isGLESComp) {
        int interval_value = hwc_get_int_property( PROPERTY_TYPE ".vwb.time", "2500");
        interval_value = interval_value > 5000? 5000:interval_value;
        interval_value = interval_value < 250? 250:interval_value;
        TimeInt2Obj(interval_value,&tv.it_value);
        ALOGD_IF(log_level(DBG_VERBOSE),"reset timer!");
    } else {
        tv.it_value.tv_usec = 0;
        ALOGD_IF(log_level(DBG_VERBOSE),"close timer!");
    }
    setitimer(ITIMER_REAL, &tv, NULL);
    return 0;
}
#endif

#if RK_3D_VIDEO
#ifdef USE_HWC2
int detect_3d_mode(hwc_drm_display_t *hd, hwc_display_contents_1_t *display_content, int display)
{
    bool is_3d = false;
    int force3d = 0;
    unsigned int numlayer = display_content->numHwLayers;
    int needStereo = 0;

    for (unsigned int j = 0; j <(numlayer - 1); j++) {
        if(display_content->hwLayers[j].handle)
        {
            needStereo = hwc_get_handle_alreadyStereo(hd->gralloc, display_content->hwLayers[j].handle);
            if(needStereo > 0)
            {
                break;
            }
        }
    }

    if(!needStereo)
    {
        force3d = hwc_get_int_property( PROPERTY_TYPE ".hwc.force3d.primary","0");

        if(1==force3d || 2==force3d){
            if(display == 0 || display == 1)
                needStereo = force3d;
        }
    }

    if(needStereo)
    {
        is_3d = true;
        if(needStereo == 1)
            hd->stereo_mode = H_3D;
        else if (needStereo == 2)
            hd->stereo_mode = V_3D;
        else if (needStereo == 8)
            hd->stereo_mode = FPS_3D;
        else
            ALOGD_IF(log_level(DBG_VERBOSE),"It is unknow 3d mode needStereo=%d",needStereo);
    }

    for (unsigned int j = 0; j <(numlayer - 1); j++) {
        if(display_content->hwLayers[j].handle)
        {
            int ret = hwc_set_handle_displayStereo(hd->gralloc, display_content->hwLayers[j].handle, needStereo);
            if(ret < 0)
            {
                ALOGE("%s:hwc_set_handle_displayStereo fail", __FUNCTION__);
                break;
            }
        }
    }

    if (needStereo & 0x8000) {
        for (unsigned int j = 0; j <(numlayer - 1); j++) {
            if(display_content->hwLayers[j].handle)
            {
                int ret = hwc_set_handle_displayStereo(hd->gralloc, display_content->hwLayers[j].handle, needStereo & (~0x8000));
                if(ret < 0)
                {
                    ALOGE("%s:hwc_set_handle_displayStereo fail", __FUNCTION__);
                    break;
                }

                ret = hwc_set_handle_alreadyStereo(hd->gralloc, display_content->hwLayers[j].handle, 0);
                if(ret < 0)
                {
                    ALOGE("%s:hwc_set_handle_alreadyStereo fail", __FUNCTION__);
                    break;
                }
            }
        }
    }
    return is_3d;
}

#else
int detect_3d_mode(hwc_drm_display_t *hd, hwc_display_contents_1_t *display_content, int display)
{
    bool is_3d = false;
    int force3d = 0;
    unsigned int numlayer = display_content->numHwLayers;
    int needStereo = 0;

    for (unsigned int j = 0; j <(numlayer - 1); j++) {
        if(display_content->hwLayers[j].alreadyStereo) {
            needStereo = display_content->hwLayers[j].alreadyStereo;
            break;
        }
    }

    if(!needStereo)
    {
        force3d = hwc_get_int_property( PROPERTY_TYPE ".hwc.force3d.primary","0");

        if(1==force3d || 2==force3d){
            if(display == 0 || display == 1)
                needStereo = force3d;
        }
    }

    if(needStereo)
    {
        is_3d = true;
        if(needStereo == 1)
            hd->stereo_mode = H_3D;
        else if (needStereo == 2)
            hd->stereo_mode = V_3D;
        else if (needStereo == 8)
            hd->stereo_mode = FPS_3D;
        else
            ALOGD_IF(log_level(DBG_VERBOSE),"It is unknow 3d mode needStereo=%d",needStereo);
    }

    for (unsigned int j = 0; j <(numlayer - 1); j++) {
        display_content->hwLayers[j].displayStereo = needStereo;
    }

    if (needStereo & 0x8000) {
        for (unsigned int j = 0; j <(numlayer - 1); j++) {
            display_content->hwLayers[j].alreadyStereo = 0;
            display_content->hwLayers[j].displayStereo = (needStereo & (~0x8000));
        }
    }
#if 0
    if(1==display && numlayer > 1) {
        ALOGD_IF(log_level(DBG_VERBOSE),"Wake up hwc control stereo");
        pthread_mutex_lock(&mControlStereo->mlk);
        ctx->mControlStereo.count = needStereo;
        pthread_mutex_unlock(&mControlStereo->mlk);
        pthread_cond_signal(&mControlStereo->cond);
    }
#endif
    return is_3d;
}
#endif

#endif

#if 0
int map_3d_mode(int value, int flag)
{
    if(flag == READ_3D_MODE)
    {
        switch (value)
        {
            case 0:
                return 8;
            case 6:
                return 2;
            case 8:
                return 1;
            default:
                return 0;
        }
    }
    else if(flag == WRITE_3D_MODE)
    {
        switch (value)
        {
            case 1:
                return 8;
            case 2:
                return 6;
            case 8:
                return 0;
            default:
                return -1;
        }
    }
    else
    {
        ALOGE("%s:line=%d invalid flag =%d", __FUNCTION__, __LINE__, flag);
        return -1;
    }

}

/*
 * get or set 3d mode.
 * flag : 0--read 1--write
 */
int hwc_control_3dmode(int fd_3d, int value, int flag)
{
    int ret = 0;
    int iMode;
    ssize_t err;
    char buf[200];

    if(fd_3d < 0)
        return -1;

    switch(flag){
    case READ_3D_MODE: //read
        memset(buf,0,sizeof(buf));
        lseek(fd,0,SEEK_SET);
        err = read(fd, buf, sizeof(buf));
        if(err <= 0)
            ALOGW("read hdmi 3dmode err=%zd",err);

        int mode,hdmi3dmode;
        sscanf(buf,"3dmodes=%d cur3dmode=%d",&mode,&hdmi3dmode);
        ALOGI_IF(log_level(DBG_VERBOSE),"hdmi3dmode=%d,mode=%d",hdmi3dmode,mode);

        ret = map_3d_mode(value, READ_3D_MODE);
        break;

    case WRITE_3D_MODE: //write
        lseek(fd,0,SEEK_SET);
        iMode = map_3d_mode(value, WRITE_3D_MODE);
        char acMode[25];
        if(iMode != -1)
        {
            sprintf(acMode,"%d",iMode);
            ret = write(fd, acMode, 2);
            if(ret < 0)
            {
                ALOGE("change 3dmode to %d err is %s",value,strerror(errno));
            }
        }
        else
        {
            ALOGE("%s:line=%d invalid write mode", __FUNCTION__, __LINE__);
        }
        break;

    default:
        break;
    }
    return ret;
}

#endif

#ifdef USE_HWC2
int hwc_get_handle_displayStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if USE_GRALLOC_4
    // dummy
    return 0;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;

    memset(&rk_ashmem,0x00,sizeof(struct rk_ashmem_t));

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return rk_ashmem.displayStereo;
#endif  // USE_GRALLOC_4
}

int hwc_set_handle_displayStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd, int32_t displayStereo)
{
#if USE_GRALLOC_4
    // dummy
    return 0;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;

    memset(&rk_ashmem,0x00,sizeof(struct rk_ashmem_t));

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        goto exit;
    }

    if(displayStereo != rk_ashmem.displayStereo)
    {
        op = GRALLOC_MODULE_PERFORM_SET_RK_ASHMEM;
        rk_ashmem.displayStereo = displayStereo;

        if(gralloc && gralloc->perform)
            ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
        else
            ret = -EINVAL;

        if(ret != 0)
        {
            ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        }
    }

exit:
    return ret;
#endif  // USE_GRALLOC_4
}

int hwc_get_handle_alreadyStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if USE_GRALLOC_4
        // dummy
        return 0;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;

    memset(&rk_ashmem,0x00,sizeof(struct rk_ashmem_t));

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return rk_ashmem.alreadyStereo;
#endif  // USE_GRALLOC_4

}

int hwc_set_handle_alreadyStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd, int32_t alreadyStereo)
{
#if USE_GRALLOC_4
        // dummy
        return 0;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;

    memset(&rk_ashmem,0x00,sizeof(struct rk_ashmem_t));

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        goto exit;
    }

    if(alreadyStereo != rk_ashmem.alreadyStereo )
    {
        op = GRALLOC_MODULE_PERFORM_SET_RK_ASHMEM;
        rk_ashmem.alreadyStereo = alreadyStereo;

        if(gralloc && gralloc->perform)
            ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
        else
            ret = -EINVAL;

        if(ret != 0)
        {
            ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        }
    }

exit:
    return ret;
#endif  // USE_GRALLOC_4
}

int hwc_get_handle_layername(const gralloc_module_t *gralloc, buffer_handle_t hnd, char* layername, unsigned long len)
{
#if USE_GRALLOC_4
        std::string name;

        int err = gralloc4::get_name(hnd, name);
        if (err != android::OK)
        {
            ALOGE("Failed to get buffer format_requested, err : %d", err);
            return -1;
        }

        int str_size = strlen(name.c_str())+1;
        str_size = str_size > len ? len:str_size;
        strncpy(layername, name.c_str(),str_size);
        return 0;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;
    unsigned long str_size;

    if(!layername)
        return -EINVAL;

    memset(&rk_ashmem,0x00,sizeof(struct rk_ashmem_t));

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        goto exit;
    }

    str_size = strlen(rk_ashmem.LayerName)+1;
    str_size = str_size > len ? len:str_size;
    memcpy(layername,rk_ashmem.LayerName,str_size);

exit:
    return ret;
#endif  // USE_GRALLOC_4

}

int hwc_set_handle_layername(const gralloc_module_t *gralloc, buffer_handle_t hnd, const char* layername)
{
#if USE_GRALLOC_4
        // dummy
        return 0;
#else   // USE_GRALLOC_4

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;
    unsigned long str_size;

    if(!layername)
        return -EINVAL;

    memset(&rk_ashmem,0x00,sizeof(struct rk_ashmem_t));

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        goto exit;
    }

    op = GRALLOC_MODULE_PERFORM_SET_RK_ASHMEM;

    str_size = strlen(layername)+1;
    str_size = str_size > sizeof(rk_ashmem.LayerName) ? sizeof(rk_ashmem.LayerName):str_size;
    memcpy(rk_ashmem.LayerName,layername,str_size);

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

exit:
    return ret;
#endif  // USE_GRALLOC_4

}
#endif

int hwc_get_handle_width(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if USE_GRALLOC_4
        uint64_t width;

        int err = gralloc4::get_width(hnd, &width);
        if (err != android::OK)
        {
            ALOGE("Failed to get buffer width, err : %d", err);
            return -1;
        }

        return (int)width;
#else   // USE_GRALLOC_4

#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->width;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_WIDTH;
    int width = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &width);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return width;
#endif
#endif  // USE_GRALLOC_4

}

int hwc_get_handle_height(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if USE_GRALLOC_4
        uint64_t height;

        int err = gralloc4::get_height(hnd, &height);
        if (err != android::OK)
        {
            ALOGE("Failed to get buffer height, err : %d", err);
            return -1;
        }

        return (int)height;
#else   // USE_GRALLOC_4

#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->height;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_HEIGHT;
    int height = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &height);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return height;
#endif
#endif  // USE_GRALLOC_4

}

int hwc_get_handle_stride(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if USE_GRALLOC_4
        int pixel_stride;

        int err = gralloc4::get_pixel_stride(hnd, &pixel_stride);
        if (err != android::OK)
        {
            ALOGE("Failed to get buffer pixel_stride, err : %d", err);
            return -1;
        }

        return pixel_stride;
#else   // USE_GRALLOC_4

#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->pixel_stride;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_STRIDE;
    int stride = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return stride;
#endif
#endif  // USE_GRALLOC_4

}

int hwc_get_handle_byte_stride(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if USE_GRALLOC_4
        int byte_stride;

        int err = gralloc4::get_byte_stride(hnd, &byte_stride);
        if (err != android::OK)
        {
            ALOGE("Failed to get buffer byte_stride, err : %d", err);
            return -1;
        }

        return byte_stride;
#else   // USE_GRALLOC_4

#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->stride;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_BYTE_STRIDE;
    int byte_stride = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &byte_stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return byte_stride;
#endif
#endif  // USE_GRALLOC_4

}

int hwc_get_handle_format(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if USE_GRALLOC_4
        int format_requested;

        int err = gralloc4::get_format_requested(hnd, &format_requested);
        if (err != android::OK)
        {
            ALOGE("Failed to get buffer format_requested, err : %d", err);
            return -1;
        }

        return format_requested;
#else   // USE_GRALLOC_4

#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->format;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_FORMAT;
    int format = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &format);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return format;
#endif
#endif  // USE_GRALLOC_4

}

int hwc_get_handle_usage(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if USE_GRALLOC_4
        uint64_t usage;

        int err = gralloc4::get_usage(hnd, &usage);
        if (err != android::OK)
        {
            ALOGE("Failed to get buffer usage, err : %d", err);
            return -1;
        }

        return (int)usage;
#else   // USE_GRALLOC_4

#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->usage;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_USAGE;
    int usage = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &usage);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return usage;
#endif
#endif  // USE_GRALLOC_4

}

int hwc_get_handle_size(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if USE_GRALLOC_4
        uint64_t allocation_size;

        int err = gralloc4::get_allocation_size(hnd, &allocation_size);
        if (err != android::OK)
        {
            ALOGE("Failed to get buffer allocation_size, err : %d", err);
            return -1;
        }

        return (int)allocation_size;
#else   // USE_GRALLOC_4

#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->size;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_SIZE;
    int size = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &size);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return size;
#endif
#endif  // USE_GRALLOC_4

}

/*
@func hwc_get_handle_attributes:get attributes from handle.Before call this api,As far as now,
    we need register the buffer first.May be the register is good for processer I think

@param hnd:
@param attrs: if size of attrs is small than 5,it will return EINVAL else
    width  = attrs[0]
    height = attrs[1]
    stride = attrs[2]
    format = attrs[3]
    size   = attrs[4]
*/
int hwc_get_handle_attributes(const gralloc_module_t *gralloc, buffer_handle_t hnd, std::vector<int> *attrs)
{

    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_ATTRIBUTES;

    if (!hnd)
        return -EINVAL;

    if(gralloc && gralloc->perform)
    {
        ret = gralloc->perform(gralloc, op, hnd, attrs);
    }
    else
    {
        ret = -EINVAL;
    }


    if(ret) {
       ALOGE("hwc_get_handle_attributes fail %d for:%s hnd=%p",ret,strerror(ret),hnd);
    }

    return ret;

}

int hwc_get_handle_attibute(const gralloc_module_t *gralloc, buffer_handle_t hnd, attribute_flag_t flag)
{
#if USE_GRALLOC_4
        switch ( flag )
        {
            case ATT_WIDTH:
                return hwc_get_handle_width(gralloc, hnd);
            case ATT_HEIGHT:
                return hwc_get_handle_height(gralloc, hnd);
            case ATT_STRIDE:
                return hwc_get_handle_stride(gralloc, hnd);
            case ATT_FORMAT:
                return hwc_get_handle_format(gralloc, hnd);
            case ATT_SIZE:
                return hwc_get_handle_size(gralloc, hnd);
            case ATT_BYTE_STRIDE:
                return hwc_get_handle_byte_stride(gralloc, hnd);
            default:
                LOG_ALWAYS_FATAL("unexpected flag : %d", flag);
                return -1;
        }
#else   // USE_GRALLOC_4

    std::vector<int> attrs;
    int ret=0;

    if(!hnd)
    {
        ALOGE("%s handle is null",__FUNCTION__);
        return -1;
    }

    ret = hwc_get_handle_attributes(gralloc, hnd, &attrs);
    if(ret < 0)
    {
        ALOGE("getHandleAttributes fail %d for:%s",ret,strerror(ret));
        return ret;
    }
    else
    {
        return attrs.at(flag);
    }

#endif  // USE_GRALLOC_4

}

/*
@func getHandlePrimeFd:get prime_fd  from handle.Before call this api,As far as now, we
    need register the buffer first.May be the register is good for processer I think

@param hnd:
@return fd: prime_fd. and driver can call the dma_buf_get to get the buffer

*/
int hwc_get_handle_primefd(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if USE_GRALLOC_4
        int share_fd;

        int err = gralloc4::get_share_fd(hnd, &share_fd);
        if (err != android::OK)
        {
            ALOGE("Failed to get buffer share_fd, err : %d", err);
            return -1;
        }

        return (int)share_fd;
#else   // USE_GRALLOC_4

#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->prime_fd;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD;
    int fd = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &fd);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return fd;
#endif
#endif  // USE_GRALLOC_4

}

#if RK_DRM_GRALLOC
uint32_t hwc_get_handle_phy_addr(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->phy_addr;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_PHY_ADDR;
    uint32_t phy_addr = 0;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &phy_addr);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return phy_addr;
#endif
}
#endif

uint32_t hwc_get_layer_colorspace(hwc_layer_1_t *layer)
{
    uint32_t colorspace = (layer->reserved[0]) | (layer->reserved[1] << 8) |
                            (layer->reserved[2] <<  16) | (layer->reserved[3] << 24);

     ALOGD_IF(log_level(DBG_VERBOSE),"%s: reserved[0]=0x%x,reserved[1]=0x%x,reserved[2]=0x%x,reserved[3]=0x%x colorspace=0x%x",__FUNCTION__,
            layer->reserved[0],layer->reserved[1],
            layer->reserved[2],layer->reserved[3],colorspace);

    return colorspace;
}

/*
    ColorSpace:         Linux standard definitions:	Android standard definitions:
    SRGB full range	V4L2_COLORSPACE_SRGB	    	HAL_DATASPACE_TRANSFER_SRGB
    Bt601 full range	V4L2_COLORSPACE_JPEG	    	HAL_DATASPACE_V0_JFIF
    Bt601 limit range	V4L2_COLORSPACE_SMPTE170M   	HAL_DATASPACE_V0_BT601_525/HAL_DATASPACE_V0_BT601_625
    Bt709 limit range	V4L2_COLORSPACE_REC709	    	HAL_DATASPACE_V0_BT709
    Bt2020 limit range	V4L2_COLORSPACE_BT2020	    	HAL_DATASPACE_STANDARD_BT2020
*/
#define CONTAIN_VALUE(value,mask) ((colorspace & mask) == value)
uint32_t colorspace_convert_to_linux(uint32_t colorspace)
{
    if (CONTAIN_VALUE(HAL_DATASPACE_STANDARD_BT2020, HAL_DATASPACE_STANDARD_MASK))
    {
        return V4L2_COLORSPACE_BT2020;
    }
    else if (CONTAIN_VALUE(HAL_DATASPACE_STANDARD_BT601_625, HAL_DATASPACE_STANDARD_MASK) &&
            CONTAIN_VALUE(HAL_DATASPACE_TRANSFER_SMPTE_170M, HAL_DATASPACE_TRANSFER_MASK))
    {
        if (CONTAIN_VALUE(HAL_DATASPACE_RANGE_FULL, HAL_DATASPACE_RANGE_MASK))
            return V4L2_COLORSPACE_JPEG;
        else if (CONTAIN_VALUE(HAL_DATASPACE_RANGE_LIMITED, HAL_DATASPACE_RANGE_MASK))
            return V4L2_COLORSPACE_SMPTE170M;
    }
    else if (CONTAIN_VALUE(HAL_DATASPACE_STANDARD_BT601_525, HAL_DATASPACE_STANDARD_MASK) &&
            CONTAIN_VALUE(HAL_DATASPACE_TRANSFER_SMPTE_170M, HAL_DATASPACE_TRANSFER_MASK) &&
            CONTAIN_VALUE(HAL_DATASPACE_RANGE_LIMITED, HAL_DATASPACE_RANGE_MASK))
    {
        return V4L2_COLORSPACE_SMPTE170M;
    }
    else if (CONTAIN_VALUE(HAL_DATASPACE_STANDARD_BT709, HAL_DATASPACE_STANDARD_MASK) &&
        CONTAIN_VALUE(HAL_DATASPACE_TRANSFER_SMPTE_170M, HAL_DATASPACE_TRANSFER_MASK) &&
        CONTAIN_VALUE(HAL_DATASPACE_RANGE_LIMITED, HAL_DATASPACE_RANGE_MASK))
    {
        return V4L2_COLORSPACE_REC709;
    }
    else if (CONTAIN_VALUE(HAL_DATASPACE_TRANSFER_SRGB, HAL_DATASPACE_TRANSFER_MASK))
    {
        return V4L2_COLORSPACE_SRGB;
    }

    //ALOGE("Unknow colorspace 0x%x",colorspace);
    return 0;
}

bool vop_support_format(uint32_t hal_format) {
  switch (hal_format) {
    case HAL_PIXEL_FORMAT_RGB_888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_YCrCb_NV12:
    case HAL_PIXEL_FORMAT_YCrCb_NV12_10:
    case FBDC_BGRA_8888:
    case FBDC_RGBA_8888:
        return true;
    default:
      return false;
  }
}

bool vop_support_scale(hwc_layer_1_t *layer,hwc_drm_display_t *hd) {
    float hfactor;
    float vfactor;
    DrmHwcRect<float> source_crop;
    DrmHwcRect<int> display_frame;

    source_crop = DrmHwcRect<float>(
      layer->sourceCropf.left, layer->sourceCropf.top,
      layer->sourceCropf.right, layer->sourceCropf.bottom);
    display_frame = DrmHwcRect<int>(
      hd->w_scale * layer->displayFrame.left, hd->h_scale * layer->displayFrame.top,
      hd->w_scale * layer->displayFrame.right, hd->h_scale * layer->displayFrame.bottom);

    if((layer->transform == HWC_TRANSFORM_ROT_90)
       ||(layer->transform == HWC_TRANSFORM_ROT_270)){
        hfactor = (float) (source_crop.bottom - source_crop.top)
                    / (display_frame.right - display_frame.left);
        vfactor = (float) (source_crop.right - source_crop.left)
                    / (display_frame.bottom - display_frame.top);
    } else {
            hfactor = (float) (source_crop.right - source_crop.left)
                        / (display_frame.right - display_frame.left);
            vfactor = (float) (source_crop.bottom - source_crop.top)
                        / (display_frame.bottom - display_frame.top);
    }
    if(hfactor >= 8.0 || vfactor >= 8.0 || hfactor <= 0.125 || vfactor <= 0.125  ){
        ALOGD_IF(log_level(DBG_DEBUG), "scale [%f,%f]not support! at line=%d", hfactor, vfactor, __LINE__);
        return false;
    }

    return true;
}

static bool is_rec1_intersect_rec2(DrmHwcRect<int>* rec1,DrmHwcRect<int>* rec2)
{
    int iMaxLeft,iMaxTop,iMinRight,iMinBottom;
    ALOGD_IF(log_level(DBG_DEBUG),"is_not_intersect: rec1[%d,%d,%d,%d],rec2[%d,%d,%d,%d]",rec1->left,rec1->top,
        rec1->right,rec1->bottom,rec2->left,rec2->top,rec2->right,rec2->bottom);

    iMaxLeft = rec1->left > rec2->left ? rec1->left: rec2->left;
    iMaxTop = rec1->top > rec2->top ? rec1->top: rec2->top;
    iMinRight = rec1->right <= rec2->right ? rec1->right: rec2->right;
    iMinBottom = rec1->bottom <= rec2->bottom ? rec1->bottom: rec2->bottom;

    if(iMaxLeft > iMinRight || iMaxTop > iMinBottom)
        return false;
    else
        return true;

    return false;
}

int is_x_intersect(DrmHwcRect<int>* rec,DrmHwcRect<int>* rec2)
{
    if(rec2->top == rec->top)
        return 1;
    else if(rec2->top < rec->top)
    {
        if(rec2->bottom > rec->top)
            return 1;
        else
            return 0;
    }
    else
    {
        if(rec->bottom > rec2->top  )
            return 1;
        else
            return 0;
    }
    return 0;
}


static bool is_layer_combine(DrmHwcLayer * layer_one,DrmHwcLayer * layer_two)
{
#if USE_MULTI_AREAS==0
     ALOGD_IF(log_level(DBG_SILENT),"USE_MULTI_AREAS disable, can't support multi region");
     return false;
#endif

 #ifdef TARGET_BOARD_PLATFORM_RK3328
     ALOGD_IF(log_level(DBG_SILENT),"rk3328 can't support multi region");
     return false;
 #endif
    //multi region only support RGBA888 RGBX8888 RGB888 565 BGRA888
    if(layer_one->format >= HAL_PIXEL_FORMAT_YCrCb_NV12
        || layer_two->format >= HAL_PIXEL_FORMAT_YCrCb_NV12
    //RK3288 Rk3326 multi region format must be the same
#if RK_MULTI_AREAS_FORMAT_LIMIT
        || (layer_one->format != layer_two->format)
#endif
        || layer_one->alpha!= layer_two->alpha
        || layer_one->is_scale || layer_two->is_scale
        || is_rec1_intersect_rec2(&layer_one->display_frame,&layer_two->display_frame)
 #if RK_HOR_INTERSECT_LIMIT
        || is_x_intersect(&layer_one->display_frame,&layer_two->display_frame)
 #endif
        )
    {
        ALOGD_IF(log_level(DBG_SILENT),"is_layer_combine layer one alpha=%d,is_scale=%d",layer_one->alpha,layer_one->is_scale);
        ALOGD_IF(log_level(DBG_SILENT),"is_layer_combine layer two alpha=%d,is_scale=%d",layer_two->alpha,layer_two->is_scale);
        return false;
    }

    return true;
}

static bool has_layer(std::vector<DrmHwcLayer*>& layer_vector,DrmHwcLayer &layer)
{
        for (std::vector<DrmHwcLayer*>::const_iterator iter = layer_vector.begin();
               iter != layer_vector.end(); ++iter) {
            if((*iter)->sf_handle==layer.sf_handle)
              if((*iter)->bClone_ == layer.bClone_)
                return true;
          }

          return false;
}

static int combine_layer(LayerMap& layer_map,std::vector<DrmHwcLayer>& layers,
                        int iPlaneSize, bool use_combine)
{
    /*Group layer*/
    int zpos = 0;
    size_t i,j;
    uint32_t sort_cnt=0;
    bool is_combine = false;

    layer_map.clear();

    for (i = 0; i < layers.size(); ) {
        if(!layers[i].bUse)
            continue;

        sort_cnt=0;
        if(i == 0)
        {
            layer_map[zpos].push_back(&layers[0]);
        }

        for(j = i+1; j < layers.size(); j++) {
            DrmHwcLayer &layer_one = layers[j];
            //layer_one.index = j;
            is_combine = false;

            for(size_t k = 0; k <= sort_cnt; k++ ) {
                DrmHwcLayer &layer_two = layers[j-1-k];
                //layer_two.index = j-1-k;
                //juage the layer is contained in layer_vector
                bool bHasLayerOne = has_layer(layer_map[zpos],layer_one);
                bool bHasLayerTwo = has_layer(layer_map[zpos],layer_two);

                //If it contain both of layers,then don't need to go down.
                if(bHasLayerOne && bHasLayerTwo)
                    continue;

                if(use_combine && is_layer_combine(&layer_one,&layer_two)) {
                    //append layer into layer_vector of layer_map_.
                    if(!bHasLayerOne && !bHasLayerTwo)
                    {
                        layer_map[zpos].emplace_back(&layer_one);
                        layer_map[zpos].emplace_back(&layer_two);
                        is_combine = true;
                    }
                    else if(!bHasLayerTwo)
                    {
                        is_combine = true;
                        for(std::vector<DrmHwcLayer*>::const_iterator iter= layer_map[zpos].begin();
                            iter != layer_map[zpos].end();++iter)
                        {
                            if((*iter)->sf_handle==layer_one.sf_handle)
                                if((*iter)->bClone_==layer_one.bClone_)
                                    continue;

                            if(!is_layer_combine(*iter,&layer_two))
                            {
                                is_combine = false;
                                break;
                            }
                        }

                        if(is_combine)
                            layer_map[zpos].emplace_back(&layer_two);
                    }
                    else if(!bHasLayerOne)
                    {
                        is_combine = true;
                        for(std::vector<DrmHwcLayer*>::const_iterator iter= layer_map[zpos].begin();
                            iter != layer_map[zpos].end();++iter)
                        {
                            if((*iter)->sf_handle==layer_two.sf_handle)
                                if((*iter)->bClone_==layer_two.bClone_)
                                    continue;

                            if(!is_layer_combine(*iter,&layer_one))
                            {
                                is_combine = false;
                                break;
                            }
                        }

                        if(is_combine)
                        {
                            layer_map[zpos].emplace_back(&layer_one);
                        }
                    }
                }

                if(!is_combine)
                {
                    //if it cann't combine two layer,it need start a new group.
                    if(!bHasLayerOne)
                    {
                        zpos++;
                        layer_map[zpos].emplace_back(&layer_one);
                    }
                    is_combine = false;
                    break;
                }
             }
             sort_cnt++; //update sort layer count
             if(!is_combine)
             {
                break;
             }
        }

        if(is_combine)  //all remain layer or limit MOST_WIN_ZONES layer is combine well,it need start a new group.
            zpos++;
        if(sort_cnt)
            i+=sort_cnt;    //jump the sort compare layers.
        else
            i++;
    }

#if RK_SORT_AREA_BY_XPOS
  //sort layer by xpos
  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        if(iter->second.size() > 1) {
            for(uint32_t i=0;i < iter->second.size()-1;i++) {
                for(uint32_t j=i+1;j < iter->second.size();j++) {
                     if(iter->second[i]->display_frame.left > iter->second[j]->display_frame.left) {
                        ALOGD_IF(log_level(DBG_DEBUG),"swap %s and %s",iter->second[i]->name.c_str(),iter->second[j]->name.c_str());
                        std::swap(iter->second[i],iter->second[j]);
                     }
                 }
            }
        }
  }
#else
  //sort layer by ypos
  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        if(iter->second.size() > 1) {
            for(uint32_t i=0;i < iter->second.size()-1;i++) {
                for(uint32_t j=i+1;j < iter->second.size();j++) {
                     if(iter->second[i]->display_frame.top > iter->second[j]->display_frame.top) {
                        ALOGD_IF(log_level(DBG_DEBUG),"swap %s and %s",iter->second[i]->name.c_str(),iter->second[j]->name.c_str());
                        std::swap(iter->second[i],iter->second[j]);
                     }
                 }
            }
        }
  }
#endif

  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        ALOGD_IF(log_level(DBG_DEBUG),"layer map id=%d,size=%zu",iter->first,iter->second.size());
        for(std::vector<DrmHwcLayer*>::const_iterator iter_layer = iter->second.begin();
            iter_layer != iter->second.end();++iter_layer)
        {
             ALOGD_IF(log_level(DBG_DEBUG),"\tlayer name=%s",(*iter_layer)->name.c_str());
        }
  }

    if((int)layer_map.size() > iPlaneSize)
    {
        ALOGD_IF(log_level(DBG_DEBUG),"map size=%zu should not bigger than plane size=%d", layer_map.size(), iPlaneSize);
        return -1;
    }

    return 0;
}

static bool rkHasPlanesWithSize(DrmCrtc *crtc, int layer_size) {
    DrmResources* drm = crtc->getDrmReoources();
    std::vector<PlaneGroup *>& plane_groups = drm->GetPlaneGroups();

    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(GetCrtcSupported(*crtc, (*iter)->possible_crtcs) && !(*iter)->bUse &&
                (*iter)->planes.size() == (size_t)layer_size)
                return true;
  }
  return false;
}

#if USE_AFBC_LAYER
static std::vector<DrmPlane *> rkGetNoAfbcUsablePlanes(DrmCrtc *crtc) {
    DrmResources* drm = crtc->getDrmReoources();
    std::vector<PlaneGroup *>& plane_groups = drm->GetPlaneGroups();
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_afbc(); }
                       );
  }
  return usable_planes;
}
#endif

static std::vector<DrmPlane *> rkGetNoYuvUsablePlanes(DrmCrtc *crtc) {
    DrmResources* drm = crtc->getDrmReoources();
    std::vector<PlaneGroup *>& plane_groups = drm->GetPlaneGroups();
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_yuv(); }
                       );
  }
  return usable_planes;
}

static std::vector<DrmPlane *> rkGetNoScaleUsablePlanes(DrmCrtc *crtc) {
    DrmResources* drm = crtc->getDrmReoources();
    std::vector<PlaneGroup *>& plane_groups = drm->GetPlaneGroups();
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_scale(); }
                       );
  }
  return usable_planes;
}

static std::vector<DrmPlane *> rkGetNoAlphaUsablePlanes(DrmCrtc *crtc) {
    DrmResources* drm = crtc->getDrmReoources();
    std::vector<PlaneGroup *>& plane_groups = drm->GetPlaneGroups();
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->alpha_property().id(); }
                       );
  }
  return usable_planes;
}

static std::vector<DrmPlane *> rkGetNoEotfUsablePlanes(DrmCrtc *crtc) {
    DrmResources* drm = crtc->getDrmReoources();
    std::vector<PlaneGroup *>& plane_groups = drm->GetPlaneGroups();
    std::vector<DrmPlane *> usable_planes;
    //loop plane groups.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
            if(!(*iter)->bUse)
                //only count the first plane in plane group.
                std::copy_if((*iter)->planes.begin(), (*iter)->planes.begin()+1,
                       std::back_inserter(usable_planes),
                       [=](DrmPlane *plane) {
                       return !plane->is_use() && plane->GetCrtcSupported(*crtc) && !plane->get_hdr2sdr(); }
                       );
  }
  return usable_planes;
}

//According to zpos and combine layer count,find the suitable plane.
// bReserve [IN]: True if want to reserve feature plane.
static bool MatchPlane(std::vector<DrmHwcLayer*>& layer_vector,
                               uint64_t* zpos,
                               DrmCrtc *crtc,
                               DrmResources *drm,
                               std::vector<DrmCompositionPlane>& composition_planes,
                               bool bMulArea,
                               bool is_interlaced,
                               int fbSize,
                               bool bReserve)
{
    uint32_t combine_layer_count = 0;
    uint32_t layer_size = layer_vector.size();
    bool b_yuv=false,b_scale=false,b_alpha=false,b_hdr2sdr=false,b_afbc=false;
    std::vector<PlaneGroup *> ::const_iterator iter;
    std::vector<PlaneGroup *>& plane_groups = drm->GetPlaneGroups();
    uint64_t rotation = 0;
    uint64_t alpha = 0xFF;
    uint16_t eotf = TRADITIONAL_GAMMA_SDR;

#ifndef TARGET_BOARD_PLATFORM_RK3288
    UN_USED(fbSize);
#endif

    //loop plane groups.
    for (iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
       ALOGD_IF(log_level(DBG_DEBUG),"line=%d,last zpos=%" PRIu64 ",group(%" PRIu64 ") zpos=%d,group bUse=%d,crtc=0x%x,possible_crtcs=0x%x",
                    __LINE__, *zpos, (*iter)->share_id, (*iter)->zpos, (*iter)->bUse, (1<<crtc->pipe()), (*iter)->possible_crtcs);
        //find the match zpos plane group
        if(!(*iter)->bUse && !(*iter)->b_reserved)
        {
            ALOGD_IF(log_level(DBG_DEBUG),"line=%d,layer_size=%d,planes size=%zu",__LINE__,layer_size,(*iter)->planes.size());

            //find the match combine layer count with plane size.
            if(layer_size <= (*iter)->planes.size())
            {
                //loop layer
                for(std::vector<DrmHwcLayer*>::const_iterator iter_layer= layer_vector.begin();
                    iter_layer != layer_vector.end();++iter_layer)
                {
                    //reset is_match to false
                    (*iter_layer)->is_match = false;

                    if(bMulArea
                        && !(*iter_layer)->is_yuv
                        && !(*iter_layer)->is_scale
                        && !((*iter_layer)->blending == DrmHwcBlending::kPreMult && (*iter_layer)->alpha != 0xFF)
                        && layer_size == 1
                        && layer_size < (*iter)->planes.size())
                    {
                        if(rkHasPlanesWithSize(crtc, layer_size))
                        {
                            ALOGD_IF(log_level(DBG_DEBUG),"Planes(%" PRIu64 ") don't need use multi area feature",(*iter)->share_id);
                            continue;
                        }
                    }

                    //loop plane
                    for(std::vector<DrmPlane*> ::const_iterator iter_plane=(*iter)->planes.begin();
                        !(*iter)->planes.empty() && iter_plane != (*iter)->planes.end(); ++iter_plane)
                    {
                        ALOGD_IF(log_level(DBG_DEBUG),"line=%d,crtc=0x%x,plane(%d) is_use=%d,possible_crtc_mask=0x%x",__LINE__,(1<<crtc->pipe()),
                                (*iter_plane)->id(),(*iter_plane)->is_use(),(*iter_plane)->get_possible_crtc_mask());
                        if(!(*iter_plane)->is_use() && (*iter_plane)->GetCrtcSupported(*crtc))
                        {
                            bool bNeed = false;

                            b_yuv  = (*iter_plane)->get_yuv();
                            if((*iter_layer)->is_yuv)
                            {
                                if(!b_yuv)
                                {
                                    ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) cann't support yuv",(*iter_plane)->id());
                                    continue;
                                }
                                else
                                    bNeed = true;
                            }

                            b_scale = (*iter_plane)->get_scale();
                            if((*iter_layer)->is_scale)
                            {
                                if(!b_scale)
                                {
                                    ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) cann't support scale",(*iter_plane)->id());
                                    continue;
                                }
                                else
                                {
                                    if((*iter_layer)->h_scale_mul >= 8.0 || (*iter_layer)->v_scale_mul >= 8.0 ||
                                        (*iter_layer)->h_scale_mul <= 0.125 || (*iter_layer)->v_scale_mul <= 0.125)
                                    {
                                        ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) cann't support scale factor(%f,%f)",
                                                (*iter_plane)->id(), (*iter_layer)->h_scale_mul, (*iter_layer)->v_scale_mul);
                                        continue;
                                    }
                                    else
                                        bNeed = true;
                                }
                            }

                            if ((*iter_layer)->blending == DrmHwcBlending::kPreMult)
                                alpha = (*iter_layer)->alpha;

#ifdef TARGET_BOARD_PLATFORM_RK3328
                            //disable global alpha feature for rk3328,since vop has bug on rk3328.
                            b_alpha = false;
#else
                            b_alpha = (*iter_plane)->alpha_property().id()?true:false;
#endif
                            if(alpha != 0xFF)
                            {
                                if(!b_alpha)
                                {
                                    ALOGV("layer name=%s,plane id=%d",(*iter_layer)->name.c_str(),(*iter_plane)->id());
                                    ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) cann't support alpha,layer alpha=0x%x,alpha id=%d",
                                            (*iter_plane)->id(),(*iter_layer)->alpha,(*iter_plane)->alpha_property().id());
                                    continue;
                                }
                                else
                                    bNeed = true;
                            }

                            eotf = (*iter_layer)->eotf;
                            b_hdr2sdr = (*iter_plane)->get_hdr2sdr();
                            if(eotf != TRADITIONAL_GAMMA_SDR)
                            {
                                if(!b_hdr2sdr)
                                {
                                    ALOGV("layer name=%s,plane id=%d",(*iter_layer)->name.c_str(),(*iter_plane)->id());
                                    ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) cann't support etof,layer eotf=%d,hdr2sdr=%d",
                                            (*iter_plane)->id(),(*iter_layer)->eotf,(*iter_plane)->get_hdr2sdr());
                                    continue;
                                }
                                else
                                    bNeed = true;
                            }

#if USE_AFBC_LAYER
                            b_afbc = (*iter_plane)->get_afbc();
                            if((*iter_layer)->is_afbc && (*iter_plane)->get_afbc_prop())
                            {
                                if(!b_afbc)
                                {
                                    ALOGV("layer name=%s,plane id=%d",(*iter_layer)->name.c_str(),(*iter_plane)->id());
                                    ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) cann't support afbc,layer", (*iter_plane)->id());
                                    continue;
                                }
                                else
                                    bNeed = true;
                            }
#else
                            UN_USED(b_afbc);

#endif

#ifdef TARGET_BOARD_PLATFORM_RK3288
                            int src_w,src_h;

                            src_w = (int)((*iter_layer)->source_crop.right - (*iter_layer)->source_crop.left);
#if RK_VIDEO_SKIP_LINE
                            if((*iter_layer)->SkipLine)
                            {
                                src_h = (int)((*iter_layer)->source_crop.bottom - (*iter_layer)->source_crop.top)/(*iter_layer)->SkipLine;
                            }
                            else
#endif
                                src_h = (int)((*iter_layer)->source_crop.bottom - (*iter_layer)->source_crop.top);

                            float src_size = (float)src_w * src_h;
                            if(src_size/fbSize > 0.75)
                            {
                                bNeed = true;
                                ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) need by big area,src_size=%f,fbSize=%d",(*iter_plane)->id(),src_size,fbSize);
                            }
#endif

                            //Reserve some plane with no need for specific features in current layer.
                            if(bReserve && !bNeed && !bMulArea && !is_interlaced)
                            {
#if USE_AFBC_LAYER
                                if(!(*iter_layer)->is_afbc && b_afbc)
                                {
                                    std::vector<DrmPlane *> no_afbc_planes = rkGetNoAfbcUsablePlanes(crtc);
                                    if(no_afbc_planes.size() > 0)
                                    {
                                        ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) don't need use afbc feature",(*iter_plane)->id());
                                        continue;
                                    }
                                }
#endif

                                if(!(*iter_layer)->is_yuv && b_yuv)
                                {
                                    std::vector<DrmPlane *> no_yuv_planes = rkGetNoYuvUsablePlanes(crtc);
                                    if(no_yuv_planes.size() > 0)
                                    {
                                        ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) don't need use yuv feature",(*iter_plane)->id());
                                        continue;
                                    }
                                }

                                if(!(*iter_layer)->is_scale && b_scale)
                                {
                                    std::vector<DrmPlane *> no_scale_planes = rkGetNoScaleUsablePlanes(crtc);
                                    if(no_scale_planes.size() > 0)
                                    {
                                        ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) don't need use scale feature",(*iter_plane)->id());
                                        continue;
                                    }
                                }

                                if(alpha == 0xFF && b_alpha)
                                {
                                    std::vector<DrmPlane *> no_alpha_planes = rkGetNoAlphaUsablePlanes(crtc);
                                    if(no_alpha_planes.size() > 0)
                                    {
                                        ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) don't need use alpha feature",(*iter_plane)->id());
                                        continue;
                                    }
                                }

                                if(eotf == TRADITIONAL_GAMMA_SDR && b_hdr2sdr)
                                {
                                    std::vector<DrmPlane *> no_eotf_planes = rkGetNoEotfUsablePlanes(crtc);
                                    if(no_eotf_planes.size() > 0)
                                    {
                                        ALOGD_IF(log_level(DBG_DEBUG),"Plane(%d) don't need use eotf feature",(*iter_plane)->id());
                                        continue;
                                    }
                                }
                            }
#if (RK_RGA_COMPSITE_SYNC | RK_RGA_PREPARE_ASYNC)
                            if(!drm->isSupportRkRga()
#if USE_AFBC_LAYER
                               || (*iter_layer)->is_afbc
#endif
                               )
#endif
                            {
                                rotation = 0;
                                if ((*iter_layer)->transform & DrmHwcTransform::kFlipH)
                                    rotation |= 1 << DRM_REFLECT_X;
                                if ((*iter_layer)->transform & DrmHwcTransform::kFlipV)
                                    rotation |= 1 << DRM_REFLECT_Y;
                                if ((*iter_layer)->transform & DrmHwcTransform::kRotate90)
                                    rotation |= 1 << DRM_ROTATE_90;
                                else if ((*iter_layer)->transform & DrmHwcTransform::kRotate180)
                                    rotation |= 1 << DRM_ROTATE_180;
                                else if ((*iter_layer)->transform & DrmHwcTransform::kRotate270)
                                    rotation |= 1 << DRM_ROTATE_270;
                                if(rotation && !(rotation & (*iter_plane)->get_rotate()))
                                    continue;
                            }

                            ALOGD_IF(log_level(DBG_DEBUG),"MatchPlane: match layer=%s,plane=%d,(*iter_layer)->index=%zu ,zops = %" PRIu64 "",(*iter_layer)->name.c_str(),
                                (*iter_plane)->id(),(*iter_layer)->index,*zpos);
                            //Find the match plane for layer,it will be commit.
                            composition_planes.emplace_back(DrmCompositionPlane::Type::kLayer, (*iter_plane), crtc, (*iter_layer)->zpos);
                            (*iter_layer)->is_match = true;
                            (*iter_plane)->set_use(true);
                            composition_planes.back().set_zpos(*zpos);
                            combine_layer_count++;
                            break;

                        }
                    }
                }
                if(combine_layer_count == layer_size)
                {
                    ALOGD_IF(log_level(DBG_DEBUG),"line=%d all match",__LINE__);
                    //update zpos for the next time.
                     *zpos += 1;
                    (*iter)->bUse = true;
                    return true;
                }
            }
            /*else
            {
                //1. cut out combine_layer_count to (*iter)->planes.size().
                //2. combine_layer_count layer assign planes.
                //3. extern layers assign planes.
                return false;
            }*/
        }

    }

    return false;
}

bool MatchPlanes(
  std::map<int, std::vector<DrmHwcLayer*>> &layer_map,
  DrmCrtc *crtc,
  DrmResources *drm,
  std::vector<DrmCompositionPlane>& composition_planes,
  bool bMulArea,
  bool is_interlaced,
  int fbSize)
{
    std::vector<PlaneGroup *>& plane_groups = drm->GetPlaneGroups();
    uint64_t last_zpos=0;
    bool bMatch = false;

#ifdef USE_PLANE_RESERVED
        uint64_t win1_reserved = hwc_get_int_property( PROPERTY_TYPE ".hwc.win1.reserved", "0");
        uint64_t win1_zpos = hwc_get_int_property( PROPERTY_TYPE ".hwc.win1.zpos", "0");
#endif


    //set use flag to false.
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
        (*iter)->bUse=false;
        for(std::vector<DrmPlane *> ::const_iterator iter_plane=(*iter)->planes.begin();
            iter_plane != (*iter)->planes.end(); ++iter_plane) {
            if((*iter_plane)->GetCrtcSupported(*crtc))  //only init the special crtc's plane
                (*iter_plane)->set_use(false);
        }
    }

    //clear composition_plane
    composition_planes.clear();

    for (LayerMap::iterator iter = layer_map.begin();
        iter != layer_map.end(); ++iter) {
#ifdef USE_PLANE_RESERVED
        if(win1_reserved > 0 && win1_zpos == last_zpos)
        {
            last_zpos++;
        }
#endif
        if(iter == layer_map.begin())
        {
            DrmHwcLayer* first_layer = (iter->second)[0];

            if(first_layer->alpha != 0xFF)
            {
              ALOGD_IF(log_level(DBG_DEBUG),"%s:line=%d  vop cann't support first layer with global alpha",__FUNCTION__,__LINE__);
              return false;
            }
        }
        bMatch = MatchPlane(iter->second, &last_zpos, crtc, drm, composition_planes, bMulArea, is_interlaced, fbSize, true);
        if(!bMatch)
        {
            ALOGD_IF(log_level(DBG_DEBUG),"hwc_prepare: first Cann't find the match plane for layer group %d",iter->first);
            bMatch = MatchPlane(iter->second, &last_zpos, crtc, drm, composition_planes, bMulArea, is_interlaced, fbSize, false);
            if(!bMatch)
            {
                ALOGD_IF(log_level(DBG_DEBUG),"hwc_prepare: second Cann't find the match plane for layer group %d",iter->first);
                return false;
            }
        }
    }

    return true;
}

float getPixelWidthByAndroidFormat(int format)
{
       float pixelWidth = 4.0;
       switch (format) {
               case HAL_PIXEL_FORMAT_RGBA_8888:
               case HAL_PIXEL_FORMAT_RGBX_8888:
               case HAL_PIXEL_FORMAT_BGRA_8888:
                       pixelWidth = 4.0;
                       break;

               case HAL_PIXEL_FORMAT_RGB_888:
                       pixelWidth = 3.0;
                       break;

               case HAL_PIXEL_FORMAT_RGB_565:
                       pixelWidth = 2.0;
                       break;

               case HAL_PIXEL_FORMAT_sRGB_A_8888:
               case HAL_PIXEL_FORMAT_sRGB_X_8888:
                       ALOGE("format 0x%x not support",format);
                       break;

               case HAL_PIXEL_FORMAT_YCbCr_422_SP:
               case HAL_PIXEL_FORMAT_YCrCb_420_SP:
               case HAL_PIXEL_FORMAT_YCbCr_422_I:
               case HAL_PIXEL_FORMAT_YCrCb_NV12:
               case HAL_PIXEL_FORMAT_YCrCb_NV12_VIDEO:
                       pixelWidth = 1.0;
                       break;

               case HAL_PIXEL_FORMAT_YCrCb_NV12_10:
                       pixelWidth = 2;
                       break;
               case HAL_PIXEL_FORMAT_YCbCr_422_SP_10:
               case HAL_PIXEL_FORMAT_YCrCb_420_SP_10:
                       pixelWidth = 1.0;
                       break;

               default:
                       ALOGE("format 0x%x not support",format);
                       break;
       }
       return pixelWidth;
}

static float vop_band_width(hwc_drm_display_t *hd, std::vector<DrmHwcLayer>& layers)
{
    float scale_factor = 0;

    if(hd->mixMode == HWC_MIX_DOWN || hd->mixMode == HWC_MIX_UP ||
        hd->mixMode == HWC_MIX_CROSS)
    {
        scale_factor += 1.0;
    }

    for(size_t i = 0; i < layers.size(); ++i)
    {
        scale_factor += layers[i].h_scale_mul * layers[i].v_scale_mul;
    }

    return scale_factor;
}

bool GetCrtcSupported(const DrmCrtc &crtc, uint32_t possible_crtc_mask) {
  return !!((1 << crtc.pipe()) & possible_crtc_mask);
}

bool match_process(DrmResources* drm, DrmCrtc *crtc, bool is_interlaced,
                        std::vector<DrmHwcLayer>& layers, int iPlaneSize, int fbSize,
                        std::vector<DrmCompositionPlane>& composition_planes)
{
    int zpos = 0;
    LayerMap layer_map;
    int iMatchCnt = 0;
    bool bMatch = false;

    if(!crtc)
    {
        ALOGE("%s:line=%d crtc is null",__FUNCTION__,__LINE__);
        return false;
    }

    //update zpos of layer
    for (size_t i = 0; i < layers.size(); ++i)
    {
      layers[i].zpos = zpos;
      zpos++;
    }

    int ret = combine_layer(layer_map, layers, iPlaneSize, !is_interlaced);
    if(ret == 0)
    {
        bool bMulArea = layers.size() > layer_map.size();
        bMatch = MatchPlanes(layer_map,crtc,drm,composition_planes, bMulArea, is_interlaced, fbSize);
    }

    if(bMatch)
    {
        for(std::vector<DrmHwcLayer>::const_iterator iter_layer= layers.begin();
                    iter_layer != layers.end();++iter_layer)
        {
            if((*iter_layer).is_match)
            {
                iMatchCnt++;
            }
        }

        if(iMatchCnt == (int)layers.size())
            return true;
    }

    return false;
}

static bool try_mix_policy(DrmResources* drm, DrmCrtc *crtc, bool is_interlaced,
                        std::vector<DrmHwcLayer>& layers, std::vector<DrmHwcLayer>& tmp_layers,
                        int iPlaneSize, std::vector<DrmCompositionPlane>& composition_planes,
                        int iFirst, int iLast, int fbSize)
{
    bool bAllMatch = false;

    if(iFirst < 0 || iLast < 0 || iFirst > iLast)
    {
        ALOGE("invalid value iFirst=%d, iLast=%d", iFirst, iLast);
        return false;
    }

    for(auto i = layers.begin(); i != layers.end();i++)
    {
        if((*i).raw_sf_layer->compositionType == HWC_MIX)
            (*i).raw_sf_layer->compositionType = HWC_FRAMEBUFFER;
    }

    /*************************mix down*************************
     many layers
    -----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
          GLES | 711aa61e80 | 0000 | 0000 | 00 | 0100 | RGBx_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.systemui.ImageWallpaper
          GLES | 711ab1ef00 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.launcher3/com.android.launcher3.Launcher
           HWC | 711aa61100 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
           HWC | 711ec5ad80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
           HWC | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 |  941,  810,  980,  859 | Sprite
    ************************************************************/
    ALOGD_IF(log_level(DBG_DEBUG), "Go into Mix policy");
    int interval = layers.size()-1-iLast;
    ALOGD_IF(log_level(DBG_DEBUG), "try_mix_policy iFirst=%d,interval=%d",iFirst,interval);
    for (auto i = layers.begin() + iFirst; i != layers.end() - interval;)
    {
        if((*i).bClone_)
            continue;

        (*i).bMix = true;
        (*i).raw_sf_layer->compositionType = HWC_MIX;

        //move gles layers
        tmp_layers.emplace_back(std::move(*i));
        i = layers.erase(i);
    }

    //add fb layer.
    int pos = iFirst;
    for (auto i = tmp_layers.begin(); i != tmp_layers.end();)
    {
        if((*i).raw_sf_layer->compositionType == HWC_FRAMEBUFFER_TARGET)
        {
            layers.insert(layers.begin() + pos, std::move(*i));
            pos++;
            i = tmp_layers.erase(i);
            continue;
        }
        i++;
    }

    bAllMatch = match_process(drm, crtc, is_interlaced, layers, iPlaneSize, fbSize, composition_planes);
    if(bAllMatch)
        return true;

    return false;
}

void move_fb_layer_to_tmp(std::vector<DrmHwcLayer>& layers, std::vector<DrmHwcLayer>& tmp_layers)
{
    for (auto i = layers.begin(); i != layers.end();)
    {
        if((*i).raw_sf_layer->compositionType == HWC_FRAMEBUFFER_TARGET)
        {
            tmp_layers.emplace_back(std::move(*i));
            i = layers.erase(i);
            continue;
        }
        i++;
    }
}

void resore_all_tmp_layers(std::vector<DrmHwcLayer>& layers, std::vector<DrmHwcLayer>& tmp_layers)
{
    for (auto i = tmp_layers.begin(); i != tmp_layers.end();)
    {
        layers.emplace_back(std::move(*i));
        i = tmp_layers.erase(i);
    }

    //sort
    for (auto i = layers.begin(); i != layers.end()-1; i++)
    {
        for (auto j = i+1; j != layers.end(); j++)
        {
            if((*i).index > (*j).index)
            {
                std::swap(*i, *j);
            }
        }
    }
}

void resore_tmp_layers_except_fb(std::vector<DrmHwcLayer>& layers, std::vector<DrmHwcLayer>& tmp_layers)
{
    for (auto i = tmp_layers.begin(); i != tmp_layers.end();)
    {
        layers.emplace_back(std::move(*i));
        i = tmp_layers.erase(i);
    }

    //sort by layer index
    for (auto i = layers.begin(); i != layers.end()-1; i++)
    {
        for (auto j = i+1; j != layers.end(); j++)
        {
            if((*i).index > (*j).index)
            {
                std::swap(*i, *j);
            }
        }
    }

    move_fb_layer_to_tmp(layers, tmp_layers);
}

bool mix_policy(DrmResources* drm, DrmCrtc *crtc, hwc_drm_display_t *hd,
                std::vector<DrmHwcLayer>& layers, int iPlaneSize, int fbSize,
                std::vector<DrmCompositionPlane>& composition_planes)
{
    bool bAllMatch = false, bHasSkipLayer = false;
    std::vector<DrmHwcLayer> tmp_layers;
    int skipCnt = 0;
    int iUsePlane = 0;
    std::vector<PlaneGroup *>& plane_groups = drm->GetPlaneGroups();
   // Since we can't composite HWC_SKIP_LAYERs by ourselves, we'll let SF
    // handle all layers in between the first and last skip layers. So find the
    // outer indices and mark everything in between as HWC_FRAMEBUFFER
    std::pair<int, int> skip_layer_indices(-1, -1);
    std::pair<int, int> layer_indices(-1, -1);


    if(!crtc)
    {
        ALOGE("%s:line=%d crtc is null",__FUNCTION__,__LINE__);
        return false;
    }

    //save fb into tmp_layers
    move_fb_layer_to_tmp(layers, tmp_layers);

    //caculate the first and last skip layer
    for (int i = 0; i < (int)layers.size(); ++i) {
      DrmHwcLayer& layer = layers[i];

      if (!layer.bSkipLayer)
        continue;

      if (skip_layer_indices.first == -1)
        skip_layer_indices.first = i;
        skip_layer_indices.second = i;
    }

    if(skip_layer_indices.first != -1)
    {
        bHasSkipLayer = true;
        skipCnt = skip_layer_indices.second - skip_layer_indices.first + 1;
    }

    //OPT: Adjust skip_layer_indices.first and skip_layer_indices.second to limit in iPlaneSize.
    if(!hd->is_3d && bHasSkipLayer && ((int)layers.size() - skipCnt + 1) > iPlaneSize)
    {
        int tmp_index = -1;
        if(skip_layer_indices.first != 0)
        {
            tmp_index = skip_layer_indices.first;
            //try decrease first skip index to 0.
            skip_layer_indices.first = 0;
            skipCnt = skip_layer_indices.second - skip_layer_indices.first + 1;
            if(((int)layers.size() - skipCnt + 1) > iPlaneSize && skip_layer_indices.second != (int)layers.size()-1)
            {
                skip_layer_indices.first = tmp_index;
                tmp_index = skip_layer_indices.second;
                //try increase second skip index to last index.
                skip_layer_indices.second = layers.size()-1;
                skipCnt = skip_layer_indices.second - skip_layer_indices.first + 1;
                if(((int)layers.size() - skipCnt + 1) > iPlaneSize)
                {
                    ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,skip_layer_indices.first, tmp_index);
                    goto FailMatch;
                }
            }
        }
        else
        {
            if(skip_layer_indices.second != (int)layers.size()-1)
            {
                //try increase second skip index to last index-1.
                skip_layer_indices.second = layers.size()-2;
                skipCnt = skip_layer_indices.second + 1;
                if(((int)layers.size() - skipCnt + 1) > iPlaneSize)
                {
                    ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,skip_layer_indices.first, tmp_index);
                    goto FailMatch;
                }
            }
            else
            {
                ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,skip_layer_indices.first, tmp_index);
                goto FailMatch;
            }
        }
    }

    /*************************mix skip layer*************************/
    if(!hd->is_3d && bHasSkipLayer && ((int)layers.size() - skipCnt + 1) <= iPlaneSize)
    {
        ALOGD_IF(log_level(DBG_DEBUG), "%s:has skip layer (%d,%d)",__FUNCTION__,skip_layer_indices.first, skip_layer_indices.second);

        if(hd->mixMode != HWC_MIX_CROSS)
            hd->mixMode = HWC_MIX_CROSS;
        bAllMatch = try_mix_policy(drm, crtc, hd->is_interlaced, layers, tmp_layers, iPlaneSize, composition_planes,
                                    skip_layer_indices.first, skip_layer_indices.second, fbSize);
        if(bAllMatch)
            goto AllMatch;
        else
        {
            ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,skip_layer_indices.first, skip_layer_indices.second);
            goto FailMatch;
        }
    }

    /*************************mix 3d layer(mix up)*************************/
    if(hd->is_3d)
    {
        ALOGD_IF(log_level(DBG_DEBUG), "%s:mix 3d (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
        if(hd->mixMode != HWC_MIX_3D)
            hd->mixMode = HWC_MIX_3D;

        if(hd->stereo_mode == H_3D || hd->stereo_mode == V_3D || hd->stereo_mode == FPS_3D)
        {
            if(layers[0].stereo)
            {
                layer_indices.first = 1;
                layer_indices.second = layers.size() - 1;

                bAllMatch = try_mix_policy(drm, crtc, hd->is_interlaced, layers, tmp_layers, iPlaneSize, composition_planes,
                                    layer_indices.first, layer_indices.second, fbSize);
                if(bAllMatch)
                    goto AllMatch;
                else
                {
                    //ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,skip_layer_indices.first, skip_layer_indices.second);
                    resore_tmp_layers_except_fb(layers, tmp_layers);
                }
            }
            else
            {
                ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,skip_layer_indices.first, skip_layer_indices.second);
                goto FailMatch;
            }
        }
    }

    /*************************common match*************************/
    bAllMatch = match_process(drm, crtc, hd->is_interlaced, layers, iPlaneSize, fbSize, composition_planes);

    if(bAllMatch)
        goto AllMatch;

    if( layers.size() < 2 /*|| iPlaneSize < 4*/)
    {
        ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d fail match iPlaneSize=%d, layer size=%d",__FUNCTION__,__LINE__,iPlaneSize,(int)layers.size());
        goto FailMatch;
    }


    /*************************mix up*************************
     Video ovelay
    -----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
           HWC | 711aa61e80 | 0000 | 0000 | 00 | 0100 | RGBx_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.systemui.ImageWallpaper
           HWC | 711ab1ef00 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.launcher3/com.android.launcher3.Launcher
           HWC | 711aa61700 | 0000 | 0000 | 00 | 0100 | ? 00000017  |    0.0,    0.0, 3840.0, 2160.0 |  600,  562, 1160,  982 | SurfaceView - MediaView
          GLES | 711ab1e580 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,  560.0,  420.0 |  600,  562, 1160,  982 | MediaView
          GLES | 70b34c9c80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
          GLES | 70b34c9080 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
          GLES | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 | 1136, 1194, 1175, 1243 | Sprite
    ************************************************************/
    if(!hd->bPreferMixDown)
    {
        if(hd->mixMode != HWC_MIX_UP)
            hd->mixMode = HWC_MIX_UP;
        if((int)layers.size() < 4)
            layer_indices.first = layers.size() - 2;
        else
            layer_indices.first = iPlaneSize - 1;
        layer_indices.second = layers.size() - 1;
        ALOGD_IF(log_level(DBG_DEBUG), "%s:mix up for video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
        bAllMatch = try_mix_policy(drm, crtc,hd->is_interlaced,  layers, tmp_layers, iPlaneSize, composition_planes,
                            layer_indices.first, layer_indices.second, fbSize);
        if(bAllMatch)
            goto AllMatch;
        else
       {
          resore_tmp_layers_except_fb(layers, tmp_layers);
          if(hd->isVideo)
          for(-- layer_indices.first;layer_indices.first>0 ; -- layer_indices.first)
           {
                 ALOGD_IF(log_level(DBG_DEBUG), "%s:mix up for video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
                 bAllMatch = try_mix_policy(drm, crtc,hd->is_interlaced,  layers, tmp_layers, iPlaneSize, composition_planes,
                 layer_indices.first, layer_indices.second, fbSize);
                 if(bAllMatch)
                 goto AllMatch;
                 resore_tmp_layers_except_fb(layers, tmp_layers);
           }
       }
    }

    /*************************mix down*************************
     Sprite layer
    -----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
          GLES | 711aa61e80 | 0000 | 0000 | 00 | 0100 | RGBx_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.systemui.ImageWallpaper
          GLES | 711ab1ef00 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.launcher3/com.android.launcher3.Launcher
          GLES | 711aa61100 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
           HWC | 711ec5ad80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
           HWC | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 |  941,  810,  980,  859 | Sprite
    ************************************************************/
    if(layers.size() >= 4 && layers.size() <= 6 )
    {
        if(hd->mixMode != HWC_MIX_DOWN)
            hd->mixMode = HWC_MIX_DOWN;
        layer_indices.first = 0;
        layer_indices.second = 2;
        ALOGD_IF(log_level(DBG_DEBUG), "%s:mix down (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
        bAllMatch = try_mix_policy(drm, crtc, hd->is_interlaced, layers, tmp_layers, iPlaneSize, composition_planes,
                            layer_indices.first, layer_indices.second, fbSize);
        if(bAllMatch)
            goto AllMatch;
        else
            resore_tmp_layers_except_fb(layers, tmp_layers);
    }

    if(hd->bPreferMixDown && ((int)layers.size() > iPlaneSize))
    {
        if(hd->mixMode != HWC_MIX_DOWN)
            hd->mixMode = HWC_MIX_DOWN;
        layer_indices.first = 0;
        layer_indices.second = layers.size() - iPlaneSize;
        ALOGD_IF(log_level(DBG_DEBUG), "%s:mix down (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
        bAllMatch = try_mix_policy(drm, crtc, hd->is_interlaced, layers, tmp_layers, iPlaneSize, composition_planes,
                            layer_indices.first, layer_indices.second, fbSize);
        if(bAllMatch)
            goto AllMatch;
        else
            resore_tmp_layers_except_fb(layers, tmp_layers);

    }

    /*************************mix up*************************
     Many layers
     ************************************************************/
    if(!hd->isVideo)
    {
        if(hd->mixMode != HWC_MIX_UP)
            hd->mixMode = HWC_MIX_UP;
        if((int)layers.size() < 4)
            layer_indices.first = layers.size() - 2;
        else
            layer_indices.first = 3;
        layer_indices.second = layers.size() - 1;
        ALOGD_IF(log_level(DBG_DEBUG), "%s:mix up (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
        bAllMatch = try_mix_policy(drm, crtc, hd->is_interlaced, layers, tmp_layers, iPlaneSize, composition_planes,
                            layer_indices.first, layer_indices.second, fbSize);
        if(bAllMatch)
            goto AllMatch;
        else
            goto FailMatch;
    }
    else
    {
       goto FailMatch;
    }

AllMatch:
#if 1
    /*************************vop band width limit*************************/
    for (std::vector<PlaneGroup *> ::const_iterator iter = plane_groups.begin();
       iter != plane_groups.end(); ++iter) {
        if(GetCrtcSupported(*crtc, (*iter)->possible_crtcs) && (*iter)->bUse)
            iUsePlane++;
    }

    if(iUsePlane >= hd->iPlaneSize && !hd->isHdr)
    {
        float scale_factor = vop_band_width(hd, layers);
        float head_factor = 0.0, tail_factor = 0.0;
        if(scale_factor > 4.5)
        {
            ALOGD_IF(log_level(DBG_DEBUG), "scale_factor=%f is so big",scale_factor);
            if(layers.size() >= 4 && !bHasSkipLayer)
            {
                resore_tmp_layers_except_fb(layers, tmp_layers);

                for(int k = 0; k < 2; k++)
                {
                    head_factor += layers[k].h_scale_mul * layers[k].v_scale_mul;
                }

                for(size_t k = layers.size()-2; k < layers.size(); k++)
                {
                    tail_factor += layers[k].h_scale_mul * layers[k].v_scale_mul;
                }

                if(head_factor > tail_factor)
                {
                    //mix down
                    if(hd->mixMode != HWC_MIX_DOWN)
                        hd->mixMode = HWC_MIX_DOWN;
                    layer_indices.first = 0;
                    layer_indices.second = 1;
                    ALOGD_IF(log_level(DBG_DEBUG), "%s:mix down (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
                    bAllMatch = try_mix_policy(drm, crtc, hd->is_interlaced, layers, tmp_layers, iPlaneSize, composition_planes,
                                        layer_indices.first, layer_indices.second, fbSize);
                    scale_factor = vop_band_width(hd, layers);
                    if(bAllMatch && scale_factor <= 3.3)
                    {
                        return true;
                    }
                    else
                    {
                        ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d vop band with is too big,fail match (%d,%d),scale_factor=%f",
                                __FUNCTION__, __LINE__, layer_indices.first, layer_indices.second, scale_factor);
                        goto FailMatch;
                    }
                }
                else
                {
                    //mix up
                    if(hd->mixMode != HWC_MIX_UP)
                        hd->mixMode = HWC_MIX_UP;
                    layer_indices.first = layers.size() - 2;
                    layer_indices.second = layers.size() - 1;
                    ALOGD_IF(log_level(DBG_DEBUG), "%s:mix up (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
                    bAllMatch = try_mix_policy(drm, crtc, hd->is_interlaced, layers, tmp_layers, iPlaneSize, composition_planes,
                                        layer_indices.first, layer_indices.second, fbSize);
                    scale_factor = vop_band_width(hd, layers);
                    if(bAllMatch && scale_factor <= 3.3)
                        return true;
                    else
                    {
                        ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d vop band with is too big,fail match (%d,%d),scale_factor=%f",
                                __FUNCTION__, __LINE__, layer_indices.first, layer_indices.second, scale_factor);
                        goto FailMatch;
                    }
                }
            }
            else
            {
                ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d vop band with is too big,fail match layers.size=%zu",__FUNCTION__,__LINE__,layers.size());
                goto FailMatch;
            }
        }
    }
#endif

    return true;
FailMatch:
    ALOGD_IF(log_level(DBG_DEBUG), "%s:line=%d Fail match",__FUNCTION__,__LINE__);
    //restore tmp layers to layers.
    resore_all_tmp_layers(layers, tmp_layers);
    //reset mix mode.
    hd->mixMode = HWC_DEFAULT;

    return false;
}

#if RK_VIDEO_UI_OPT
void video_ui_optimize(const gralloc_module_t *gralloc, hwc_display_contents_1_t *display_content, hwc_drm_display_t *hd)
{
    int ret = 0;
    int format = 0;
    int num_layers = display_content->numHwLayers;
    if(num_layers == 3)
    {
        hwc_layer_1_t *first_layer = &display_content->hwLayers[0];
        if(first_layer->handle)
        {
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
            format = hwc_get_handle_attibute(gralloc, first_layer->handle, ATT_FORMAT);
#else
            format = hwc_get_handle_format(gralloc, first_layer->handle);
#endif
            if(format == HAL_PIXEL_FORMAT_YCrCb_NV12 || format == HAL_PIXEL_FORMAT_YCrCb_NV12_10)
            {
                bool bDiff = true;
                int iUiFd = 0;
                hwc_layer_1_t * second_layer =  &display_content->hwLayers[1];
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
                format = hwc_get_handle_attibute(gralloc, second_layer->handle, ATT_FORMAT);
#else
                format = hwc_get_handle_format(gralloc, second_layer->handle);
#endif
                if(second_layer->handle &&
                    (format == HAL_PIXEL_FORMAT_RGBA_8888 ||
                    format == HAL_PIXEL_FORMAT_RGBX_8888 ||
                    format == HAL_PIXEL_FORMAT_BGRA_8888)
                  )
                {
                    iUiFd = hwc_get_handle_primefd(gralloc, second_layer->handle);
                    bDiff = (iUiFd != hd->iUiFd);

                    if(bDiff)
                    {
                        hd->bHideUi = false;
                        /* Update the backup ui fd */
                        hd->iUiFd = iUiFd;
                    }
                    else if(!hd->bHideUi)
                    {
#if (!RK_PER_MODE && RK_DRM_GRALLOC)
                        int iWidth = hwc_get_handle_attibute(gralloc,second_layer->handle,ATT_WIDTH);
                        int iHeight = hwc_get_handle_attibute(gralloc,second_layer->handle,ATT_HEIGHT);
#else
                        int iWidth = hwc_get_handle_width(gralloc,second_layer->handle);
                        int iHeight = hwc_get_handle_height(gralloc,second_layer->handle);
#endif
                        unsigned int *cpu_addr = NULL;;

#if 0
                        IMG_native_handle_t * pvHandle = (IMG_native_handle_t *)second_layer->handle;
                        cpu_addr= (unsigned int *)pvHandle->pvBase;
#else
#if USE_GRALLOC_4
                        ret = gralloc4::lock(second_layer->handle,
                                             GRALLOC_USAGE_SW_READ_MASK,
                                             0,
                                             0,
                                             iWidth,
                                             iHeight,
                                             (void **)&cpu_addr);
#else   // USE_GRALLOC_4
                        ret = gralloc->lock(gralloc, second_layer->handle, GRALLOC_USAGE_SW_READ_MASK,
                                0, 0, iWidth, iHeight, (void **)&cpu_addr);
#endif  // USE_GRALLOC_4
                        if( (ret != 0) || (cpu_addr == NULL) || (cpu_addr == MAP_FAILED) )
                        {
                            ALOGD("%s:line=%d lock failed w=%d,h=%d,cpu_addr=%p", __FUNCTION__, __LINE__, iWidth, iHeight, cpu_addr);
                        }
                        else
#endif
                        {
                            ret = DetectValidData((int *)(cpu_addr),iWidth,iHeight);
                            if(!ret)
                            {
                                hd->bHideUi = true;
                                ALOGD_IF(log_level(DBG_VERBOSE), "@video UI close,iWidth=%d,iHeight=%d",iWidth,iHeight);
                            }
                        }
#if USE_GRALLOC_4
                    gralloc4::unlock(second_layer->handle);
#else   // USE_GRALLOC_4
                    gralloc->unlock(gralloc, second_layer->handle);
#endif  // USE_GRALLOC_4

                    }

                    if(hd->bHideUi)
                    {
                        second_layer->compositionType = HWC_NODRAW;
                    }
                    else
                    {
                        second_layer->compositionType = HWC_FRAMEBUFFER;
                    }
                }
            }
        }
    }
}
#endif

void hwc_list_nodraw(hwc_display_contents_1_t  *list)
{
    if (list == NULL)
    {
        return;
    }
    for (unsigned int i = 0; i < list->numHwLayers - 1; i++)
    {
        list->hwLayers[i].compositionType = HWC_NODRAW;
    }
    return;
}

void hwc_sync_release(hwc_display_contents_1_t  *list)
{
	for (int i=0; i< (int)list->numHwLayers; i++){
		hwc_layer_1_t* layer = &list->hwLayers[i];
		if (layer == NULL){
			return ;
		}
		if (layer->acquireFenceFd>0){
#if RK_PRINT_LAYER_NAME
			ALOGV(">>>close acquireFenceFd:%d,layername=%s",layer->acquireFenceFd,layer->LayerName);
#endif
			close(layer->acquireFenceFd);
			list->hwLayers[i].acquireFenceFd = -1;
		}
	}

	if (list->outbufAcquireFenceFd>0){
		ALOGV(">>>close outbufAcquireFenceFd:%d",list->outbufAcquireFenceFd);
		close(list->outbufAcquireFenceFd);
		list->outbufAcquireFenceFd = -1;
	}
}

const char* hwc_get_baseparameter_file(void)
{
    int i = 0;

    while (device_template[i]) {
        if (!access(device_template[i], R_OK | W_OK)){
            return device_template[i];
            }
        i++;
    }
    return NULL;
}
static struct file_base_parameter base_parameter;
static bool enableBaseparameter = false;
#define BASE_OFFSET 8*1024

bool hwc_have_baseparameter(void)
{
     ALOGI_IF(log_level(DBG_INFO),"BP: have baseparameter exit (%d)",enableBaseparameter);
     return enableBaseparameter;
}


int hwc_get_baseparameter_config(char *parameter, int display, int flag, int type)
{
    unsigned int w=0,h=0,hsync_start=0,hsync_end=0,htotal=0;
    unsigned int vsync_start=0,vsync_end=0,vtotal=0,flags=0,clock=0;
    unsigned int  format = 0,depthc =0;
    unsigned short brightness = 0,contrast = 0,saturation = 0,hue = 0;
    unsigned short leftscale = 0,rightscale = 0,topscale = 0,bottomscale = 0;
    int res = 0;
    static int bcsh_flag = 0x0;
    float vfresh=0;
    char value_new[128] = {0};

    switch(flag){
        case BP_UPDATE:
            {
                memset(&base_parameter,0x00,sizeof(file_base_parameter));
                int file;
                const char *baseparameterfile = hwc_get_baseparameter_file();
                if (!baseparameterfile) {
                    ALOGW("BP: baseparamter file cann't be find.");
                    enableBaseparameter = false;
                    return -1;
                }
                file = open(baseparameterfile, O_RDWR);
                if (file < 0) {
                    ALOGW("BP: baseparamter file can not be opened,");
                    enableBaseparameter = false;
                    return -1;
                }
                 // caculate file's size and read it
                unsigned int length = lseek(file, 0L, SEEK_END);
                lseek(file, 0L, SEEK_SET);
                if(length < sizeof(base_parameter)) {
                    ALOGW("BP: baseparamter data's length is error\n");
                    enableBaseparameter = false;
                    sync();
                    close(file);
                    return -1;
                }
                read(file, (void*)&(base_parameter.main), sizeof(base_parameter.main));
                lseek(file, BASE_OFFSET, SEEK_SET);
                read(file, (void*)&(base_parameter.aux), sizeof(base_parameter.aux));
                close(file);
                enableBaseparameter = true;
                break;
            }
        case BP_RESOLUTION:
            {
                if(enableBaseparameter == false)
                {
                    ALOGW("BP: RESOLUTION baseparameter is not ready,can't use it !");
                    return -1;
                }
                if(display == HWC_DISPLAY_PRIMARY){
                    int i = 0;
                    bool type_found = false;

                    for( i = 0 ; i < SCREEN_LIST_MAX ; i++)
                    {
                        if(type == base_parameter.main.screen_list[i].type)
                        {
                            type_found = true;
                            break;
                        }
                    }

                    if(type_found){
                        ALOGI_IF(log_level(DBG_INFO),"BP: Main screen type %d be found",type);
                        w = base_parameter.main.screen_list[i].resolution.hdisplay;
                        h = base_parameter.main.screen_list[i].resolution.vdisplay;
                        vsync_start = base_parameter.main.screen_list[i].resolution.vsync_start;
                        hsync_start = base_parameter.main.screen_list[i].resolution.hsync_start;
                        hsync_end = base_parameter.main.screen_list[i].resolution.hsync_end;
                        htotal = base_parameter.main.screen_list[i].resolution.htotal;
                        vsync_end = base_parameter.main.screen_list[i].resolution.vsync_end;
                        vtotal = base_parameter.main.screen_list[i].resolution.vtotal;
                        flags = base_parameter.main.screen_list[i].resolution.flags;
                        clock = base_parameter.main.screen_list[i].resolution.clock;
                    }else{
                        ALOGI_IF(log_level(DBG_INFO),"BP: Main screen type %d not found,to use Auto",type);
                    }

                    if(flags & DRM_MODE_FLAG_INTERLACE){
                        vfresh = (float)clock / (htotal*vtotal) * 2;
                    }else{
                        vfresh = (float)clock / (htotal*vtotal);
                    }

                    if(vfresh < 1)
                        vfresh = vfresh * 1000;

                    if( !type_found || base_parameter.main.screen_list[i].feature & RESOLUTION_AUTO ||
                           w * h <= 0 || w * h > 4096 * 2160 ){
                        strcpy(parameter,"Auto");
                        property_set("persist." PROPERTY_TYPE ".resolution.main",parameter);
                        ALOGI_IF(log_level(DBG_INFO),"BP: resolution main %s",parameter);
                    }else{
                         sprintf(parameter,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x", w, h, vfresh, hsync_start,hsync_end,\
                             htotal,vsync_start,vsync_end,vtotal, flags);
                         property_set("persist." PROPERTY_TYPE ".resolution.main",parameter);
                         ALOGI_IF(log_level(DBG_INFO),"BP: resolution main %s \n",parameter);
                     }
                }else{
                    int i = 0;
                    bool type_found = false;

                    for( i = 0 ; i < 5 ; i++)
                    {
                        if(type == base_parameter.aux.screen_list[i].type)
                        {
                            type_found = true;
                            break;
                        }
                    }

                    if(type_found){
                        ALOGI_IF(log_level(DBG_INFO),"BP: Aux screen type %d found",type);
                        w = base_parameter.aux.screen_list[i].resolution.hdisplay;
                        h = base_parameter.aux.screen_list[i].resolution.vdisplay;
                        vsync_start = base_parameter.aux.screen_list[i].resolution.vsync_start;
                        hsync_start = base_parameter.aux.screen_list[i].resolution.hsync_start;
                        hsync_end = base_parameter.aux.screen_list[i].resolution.hsync_end;
                        htotal = base_parameter.aux.screen_list[i].resolution.htotal;
                        vsync_end = base_parameter.aux.screen_list[i].resolution.vsync_end;
                        vtotal = base_parameter.aux.screen_list[i].resolution.vtotal;
                        flags = base_parameter.aux.screen_list[i].resolution.flags;
                        clock = base_parameter.aux.screen_list[i].resolution.clock;
                    }else{
                        ALOGI_IF(log_level(DBG_INFO),"BP: Aux screen type %d not found,to use Auto",type);
                    }

                    if(flags & DRM_MODE_FLAG_INTERLACE){
                        vfresh = (float)clock / (htotal*vtotal) * 2;
                    }else{
                        vfresh = (float)clock / (htotal*vtotal);
                    }

                    if(vfresh < 1)
                        vfresh = vfresh*1000;

                    if( !type_found || base_parameter.aux.screen_list[i].feature & RESOLUTION_AUTO ||
                           w * h <= 0 || w * h > 4096 * 2160 ){
                        strcpy(parameter,"Auto");
                        property_set("persist." PROPERTY_TYPE ".resolution.aux",parameter);
                        ALOGI_IF(log_level(DBG_INFO),"BP: resolution aux %s",parameter);
                    }else{
                         sprintf(parameter,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x", w, h, vfresh, hsync_start,hsync_end,\
                             htotal,vsync_start,vsync_end,vtotal, flags);
                         property_set("persist." PROPERTY_TYPE ".resolution.aux",parameter);
                         ALOGI_IF(log_level(DBG_INFO),"BP: resolution aux %s \n",parameter);
                     }
                }
                break;
            }
        case BP_FB_SIZE:
            {
                if(enableBaseparameter == false)
                {
                    ALOGW("BP: FB_SIZE baseparameter is not ready,can't use it !");
                    return -1;
                }
                if(display == HWC_DISPLAY_PRIMARY){
                    w = base_parameter.main.hwc_info.framebuffer_width;
                    h = base_parameter.main.hwc_info.framebuffer_height;
                    vfresh = base_parameter.main.hwc_info.fps;

                    if( w * h > 0 && w * h <= 4096 * 2160 && vfresh > 0 &&
                            vfresh <= 120){
                        sprintf(parameter,"%dx%d@%f", w, h, vfresh);
                        property_set("persist." PROPERTY_TYPE ".framebuffer.main",parameter);
                        ALOGI_IF(log_level(DBG_INFO),"BP:main %s \n",parameter);
                    }else{
#ifdef RK_BOX
                        strcpy(parameter,"1920x1080@60");
                        ALOGI_IF(log_level(DBG_INFO),"BP:main fb_size default %s \n",parameter);
                        property_set("persist." PROPERTY_TYPE ".framebuffer.main",parameter);
#else
                        ALOGI_IF(log_level(DBG_INFO),"BP:main fb_size=%dx%d@%f err,set 0x0@60",w,h,vfresh);
                        property_set("persist." PROPERTY_TYPE ".framebuffer.main","0x0@60");
#endif
                    }
                }else{
                    w = base_parameter.aux.hwc_info.framebuffer_width;
                    h = base_parameter.aux.hwc_info.framebuffer_height;
                    vfresh = base_parameter.aux.hwc_info.fps;

                    if( w * h > 0 && w * h <= 4096 * 2160 && vfresh > 0 &&
                            vfresh <= 120){
                        sprintf(parameter,"%dx%d@%f", w, h, vfresh);
                        property_set("persist." PROPERTY_TYPE ".framebuffer.aux",parameter);
                        ALOGI_IF(log_level(DBG_INFO),"BP:aux %s \n",parameter);
                    }else{
#ifdef RK_BOX
                        strcpy(parameter,"1920x1080@60");
                        ALOGI_IF(log_level(DBG_INFO),"BP:aux fb_size default %s \n",parameter);
                        property_set("persist." PROPERTY_TYPE ".framebuffer.aux",parameter);
#else
                        ALOGI_IF(log_level(DBG_INFO),"BP:aux fb_size=%dx%d@%f err,set 0x0@60",w,h,vfresh);
                        property_set("persist." PROPERTY_TYPE ".framebuffer.aux","0x0@60");
#endif
                    }
                }
                break;
            }
        case BP_DEVICE:
            {
                if(enableBaseparameter == false)
                {
                    ALOGW("BP: DEVICE baseparameter is not ready,can't use it !");
                    return -1;
                }

                if( display == HWC_DISPLAY_PRIMARY ){
                    strcpy(parameter,base_parameter.main.hwc_info.device);
                    ALOGI_IF(log_level(DBG_INFO),"BP: dev_primary = %s",parameter);
                }else{
                    strcpy(parameter,base_parameter.aux.hwc_info.device);
                    ALOGI_IF(log_level(DBG_INFO),"BP: dev_extend = %s",parameter);
                }
                break;
            }
        case BP_BRIGHTNESS:
            if(display == HWC_DISPLAY_PRIMARY){
                brightness = base_parameter.main.bcsh.brightness;
                if(!(bcsh_flag & 0x1) && enableBaseparameter){
                    bcsh_flag |= 0x1;
                    sprintf(value_new,"%d",(brightness > 0 && brightness <= 100  ? brightness : 50));
                    property_set("persist." PROPERTY_TYPE ".brightness.main",value_new);
                    ALOGI_IF(log_level(DBG_INFO),"BP: first set main brightness: %s",value_new);
                }
                ALOGI_IF(log_level(DBG_INFO),"BP: main brightness %d",property_get_int32("persist." PROPERTY_TYPE ".brightness.main",
                    brightness > 0 && brightness <= 100  ? brightness : 50));
                return property_get_int32("persist." PROPERTY_TYPE ".brightness.main",
                    brightness > 0 && brightness <= 100  ? brightness : 50);
            }else{
                brightness = base_parameter.aux.bcsh.brightness;
                if(!(bcsh_flag & 0x10) && enableBaseparameter){
                    bcsh_flag |= 0x10;
                    sprintf(value_new,"%d",(brightness > 0 && brightness <= 100  ? brightness : 50));
                    property_set("persist." PROPERTY_TYPE ".brightness.aux",value_new);
                    ALOGI_IF(log_level(DBG_INFO),"BP: first set aux brightness: %s",value_new);
                }
                ALOGI_IF(log_level(DBG_INFO),"BP: aux brightness %d",property_get_int32("persist." PROPERTY_TYPE ".brightness.aux",
                    brightness > 0 && brightness <= 100  ? brightness : 50));
                return property_get_int32("persist." PROPERTY_TYPE ".brightness.aux",
                    brightness > 0 && brightness <= 100  ? brightness : 50);
            }
            break;
        case BP_CONTRAST:
            if(display == HWC_DISPLAY_PRIMARY){
                contrast =  base_parameter.main.bcsh.contrast;
                if(!(bcsh_flag & 0x2) && enableBaseparameter){
                    bcsh_flag |= 0x2;
                    sprintf(value_new,"%d",(contrast > 0 && contrast <= 100  ? contrast : 50));
                    property_set("persist." PROPERTY_TYPE ".contrast.main",value_new);
                    ALOGI_IF(log_level(DBG_INFO),"BP: first set main contrast: %s",value_new);
                }
                ALOGI_IF(log_level(DBG_INFO),"BP: main contrast %d",property_get_int32("persist." PROPERTY_TYPE ".contrast.main",
                    contrast > 0 && contrast <=100  ? contrast : 50));
                return property_get_int32("persist." PROPERTY_TYPE ".contrast.main",
                    contrast > 0 && contrast <=100  ? contrast : 50);

            }else{
                contrast =  base_parameter.aux.bcsh.contrast;
                if(!(bcsh_flag & 0x20) && enableBaseparameter){
                    bcsh_flag |= 0x20;
                    sprintf(value_new,"%d",(contrast > 0 && contrast <= 100  ? contrast : 50));
                    property_set("persist." PROPERTY_TYPE ".contrast.aux",value_new);
                    ALOGI_IF(log_level(DBG_INFO),"BP: first set aux contrast: %s",value_new);
                }
                ALOGI_IF(log_level(DBG_INFO),"BP: aux contrast %d",property_get_int32("persist." PROPERTY_TYPE ".contrast.aux",
                    contrast > 0 && contrast <= 100  ? contrast : 50));
                return property_get_int32("persist." PROPERTY_TYPE ".contrast.aux",
                    contrast > 0 && contrast <= 100  ? contrast : 50);
            }
            break;
        case BP_SATURATION:
            if(display == HWC_DISPLAY_PRIMARY){
                saturation =  base_parameter.main.bcsh.saturation;
                if(!(bcsh_flag & 0x4) && enableBaseparameter){
                    bcsh_flag |= 0x4;
                    sprintf(value_new,"%d",(saturation > 0 && saturation <= 100  ? saturation : 50));
                    property_set("persist." PROPERTY_TYPE ".saturation.main",value_new);
                    ALOGI_IF(log_level(DBG_INFO),"BP: first set main saturation: %s",value_new);
                }
                ALOGI_IF(log_level(DBG_INFO),"BP: main saturation %d",property_get_int32("persist." PROPERTY_TYPE ".saturation.main",
                    saturation > 0 && saturation <= 100  ? saturation : 50));
                return property_get_int32("persist." PROPERTY_TYPE ".saturation.main",
                    saturation > 0 && saturation <= 100  ? saturation : 50);
            }else{
                saturation =  base_parameter.aux.bcsh.saturation;
                if(!(bcsh_flag & 0x40) && enableBaseparameter){
                    bcsh_flag |= 0x40;
                    sprintf(value_new,"%d",(saturation > 0 && saturation <= 100  ? saturation : 50));
                    property_set("persist." PROPERTY_TYPE ".saturation.aux",value_new);
                    ALOGI_IF(log_level(DBG_INFO),"BP: first set aux saturation: %s",value_new);
                }
                ALOGI_IF(log_level(DBG_INFO),"BP: aux saturation %d",property_get_int32("persist." PROPERTY_TYPE ".saturation.aux",
                    saturation > 0 && saturation <= 100  ? saturation : 50));
                return property_get_int32("persist." PROPERTY_TYPE ".saturation.aux",
                    saturation > 0 && saturation <= 100  ? saturation : 50);
            }
            break;
        case BP_HUE:
            if(display == HWC_DISPLAY_PRIMARY){
                hue =  base_parameter.main.bcsh.hue;
                if(!(bcsh_flag & 0x8) && enableBaseparameter){
                    bcsh_flag |= 0x8;
                    sprintf(value_new,"%d",(hue > 0 && hue <= 100  ? hue : 50));
                    property_set("persist." PROPERTY_TYPE ".hue.main",value_new);
                    ALOGI_IF(log_level(DBG_INFO),"BP: first set main hue: %s",value_new);
                }
                ALOGI_IF(log_level(DBG_INFO),"BP: main hue %d",property_get_int32("persist." PROPERTY_TYPE ".hue.main",
                    hue > 0 && hue <= 100  ? hue : 50));
                return property_get_int32("persist." PROPERTY_TYPE ".hue.main",
                    hue > 0 && hue <= 100  ? hue : 50);

            }else{
                hue =  base_parameter.aux.bcsh.hue;
                if(!(bcsh_flag & 0x80) && enableBaseparameter){
                    bcsh_flag |= 0x80;
                    sprintf(value_new,"%d",(hue > 0 && hue <= 100  ? hue : 50));
                    property_set("persist." PROPERTY_TYPE ".hue.aux",value_new);
                    ALOGI_IF(log_level(DBG_INFO),"BP: first set aux hue: %s",value_new);
                }
                ALOGI_IF(log_level(DBG_INFO),"BP: aux hue %d",property_get_int32("persist." PROPERTY_TYPE ".hue.aux",
                    hue > 0 && hue <= 100  ? hue : 50));
                return property_get_int32("persist." PROPERTY_TYPE ".hue.aux",
                    hue > 0 && hue <= 100  ? hue : 50);
            }
            break;
         case BP_COLOR:
            if(display == HWC_DISPLAY_PRIMARY){
                int i = 0;
                bool type_found = false;

                for( i = 0 ; i < SCREEN_LIST_MAX ; i++ )
                {
                    if(type == base_parameter.main.screen_list[i].type)
                    {
                        type_found = true;
                        break;
                    }
                }
                if(type_found)
                {
                    ALOGW("BP: Main screen type %d found",type);
                    format = base_parameter.main.screen_list[i].format;
                    depthc = base_parameter.main.screen_list[i].depthc;
                }else{
                    ALOGW("BP: Main screen type %d not found,to use default color",type);
                }

                res = hwc_parse_format_into_prop(display,format,depthc);
                if( !type_found || base_parameter.main.screen_list[i].feature & COLOR_AUTO || res){
                    //Auto:output_ycbcr_high_subsampling - Automatic
                    hwc_parse_format_into_prop(display,4,0);
                    strcpy(parameter,"4-0");
                }else{
                        sprintf(parameter,"%u-%u",format,depthc);
                }
                ALOGI_IF(log_level(DBG_INFO),"BP: main color %s",parameter);
            }else{
                int i = 0;
                bool type_found = false;

                for( i = 0 ; i < SCREEN_LIST_MAX ; i++ )
                {
                    if(type == base_parameter.aux.screen_list[i].type)
                    {
                        type_found = true;
                        break;
                    }
                }
                if(type_found)
                {
                    ALOGW("BP: Aux screen type %d found",type);
                    format = base_parameter.aux.screen_list[i].format;
                    depthc = base_parameter.aux.screen_list[i].depthc;
                }else{
                    ALOGW("BP: Aux screen type %d not found,to use default color",type);
                }

                res = hwc_parse_format_into_prop(display,format,depthc);
                if(!type_found || base_parameter.aux.screen_list[i].feature & COLOR_AUTO || res ){
                    //Auto:output_ycbcr_high_subsampling - Automatic
                    hwc_parse_format_into_prop(display,4,0);
                    strcpy(parameter,"4-0");
                }else{
                        sprintf(parameter,"%u-%u",format,depthc);
                }
                ALOGI_IF(log_level(DBG_INFO),"BP: aux color %s",parameter);
            }
            break;
         case BP_OVERSCAN:
            if(display == HWC_DISPLAY_PRIMARY){
                leftscale = base_parameter.main.scan.leftscale;
                topscale = base_parameter.main.scan.topscale;
                rightscale = base_parameter.main.scan.rightscale;
                bottomscale = base_parameter.main.scan.bottomscale;
                sprintf(parameter,"overscan %d,%d,%d,%d",
                    leftscale > 0 && leftscale <= 100 ? leftscale : 100,
                    topscale > 0 && topscale <= 100 ? topscale : 100,
                    rightscale > 0 && rightscale <= 100 ? rightscale : 100,
                    bottomscale > 0 && bottomscale <= 100 ? bottomscale : 100);
                property_set("persist." PROPERTY_TYPE ".overscan.main",parameter);
                ALOGI_IF(log_level(DBG_INFO),"BP: main overscan %s",parameter);
            }else{
                leftscale = base_parameter.aux.scan.leftscale;
                topscale = base_parameter.aux.scan.topscale;
                rightscale = base_parameter.aux.scan.rightscale;
                bottomscale = base_parameter.aux.scan.bottomscale;
                sprintf(parameter,"overscan %d,%d,%d,%d",
                    leftscale > 0 && leftscale <= 100 ? leftscale : 100,
                    topscale > 0 && topscale <= 100 ? topscale : 100,
                    rightscale > 0 && rightscale <= 100 ? rightscale : 100,
                    bottomscale > 0 && bottomscale <= 100 ? bottomscale : 100);
                property_set("persist." PROPERTY_TYPE ".overscan.aux",parameter);
                ALOGI_IF(log_level(DBG_INFO),"BP: aux overscan %s",parameter);
            }
            break;
        default:
            ALOGW("BP:The flag is error ,check it !");
            return -1;
        }
    return 0;
}

void hwc_set_baseparameter_config(DrmResources *drm)
{
  char save_comfig[PROPERTY_VALUE_MAX];
  property_get("persist." PROPERTY_TYPE ".saveconfig",save_comfig,"0");
  if(atoi(save_comfig))
  {
    char buf[256];
    bool isMainHdmiConnected = false;
    bool isAuxHdmiConnected = false;
    int foundMainIdx = -1, foundAuxIdx = -1;
    DrmConnector* primary = drm->GetConnectorFromType(HWC_DISPLAY_PRIMARY);
    DrmConnector* extend  = drm->GetConnectorFromType(HWC_DISPLAY_PRIMARY);

    int file;
    const char *baseparameterfile = hwc_get_baseparameter_file();
    if (!baseparameterfile) {
        ALOGW("BP: baseparamter file can not be find");
        sync();
        return;
    }
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        ALOGW("base paramter file can not be opened");
        sync();
        return;
    }

  if (primary != NULL) {
      std::vector<DrmMode> mModes = primary->modes();
      char resolution[PROPERTY_VALUE_MAX];
      unsigned int w = 0,h = 0,hsync_start = 0,hsync_end = 0,htotal = 0;
      unsigned int vsync_start = 0,vsync_end = 0,vtotal = 0,flags = 0;
      float vfresh = 0.0000;

      property_get("persist." PROPERTY_TYPE ".resolution.main", resolution, "0x0@0.00-0-0-0-0-0-0-0");
      if (strncmp(resolution, "Auto", 4) != 0 && strncmp(resolution, "0x0p0-0", 7) !=0)
          sscanf(resolution,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x", &w, &h, &vfresh, &hsync_start,&hsync_end,
                  &htotal,&vsync_start,&vsync_end, &vtotal, &flags);
      for (size_t c = 0; c < mModes.size(); ++c){
        const DrmMode& info = mModes[c];
        char curDrmModeRefresh[16];
        char curRefresh[16];
        float mModeRefresh;
        if (info.flags() & DRM_MODE_FLAG_INTERLACE)
            mModeRefresh = info.clock() * 2 / (float)(info.v_total() * info.h_total()) * 1000.0f;
        else
            mModeRefresh = info.clock() / (float)(info.v_total() * info.h_total()) * 1000.0f;
        sprintf(curDrmModeRefresh, "%.2f", mModeRefresh);
        sprintf(curRefresh, "%.2f", vfresh);
        if (info.h_display() == w &&
                info.v_display() == h &&
                info.h_sync_start() == hsync_start &&
                info.h_sync_end() == hsync_end &&
                info.h_total() == htotal &&
                info.v_sync_start() == vsync_start &&
                info.v_sync_end() == vsync_end &&
                info.v_total() == vtotal &&
                atof(curDrmModeRefresh) == atof(curRefresh)) {
          foundMainIdx = c;
          sprintf(buf, "display=%d,iface=%d,enable=%d,mode=%s\n",
                  primary->display(), primary->get_type(), primary->state(), resolution);
          break;
        }
      }
    }

  if (extend != NULL) {
    std::vector<DrmMode> mModes = extend->modes();
    char resolution[PROPERTY_VALUE_MAX];
    unsigned int w = 0,h = 0,hsync_start = 0,hsync_end = 0,htotal = 0;
    unsigned int vsync_start = 0,vsync_end = 0,vtotal = 0,flags = 0;
    float vfresh = 0;

    property_get("persist." PROPERTY_TYPE ".resolution.aux", resolution, "0x0@0.00-0-0-0-0-0-0-0");
    if (strncmp(resolution, "Auto", 4) != 0 && strncmp(resolution, "0x0p0-0", 7) !=0)
        sscanf(resolution,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x", &w, &h, &vfresh, &hsync_start,&hsync_end,&htotal,&vsync_start,&vsync_end,
                &vtotal, &flags);
    for (size_t c = 0; c < mModes.size(); ++c){
        const DrmMode& info = mModes[c];
        char curDrmModeRefresh[16];
        char curRefresh[16];
        float mModeRefresh;
        if (info.flags() & DRM_MODE_FLAG_INTERLACE)
            mModeRefresh = info.clock() * 2 / (float)(info.v_total()* info.h_total()) * 1000.0f;
        else
            mModeRefresh = info.clock() / (float)(info.v_total()* info.h_total()) * 1000.0f;
        sprintf(curDrmModeRefresh, "%.2f", mModeRefresh);
        sprintf(curRefresh, "%.2f", vfresh);
        if (info.h_display() == w &&
                info.v_display() == h &&
                info.h_sync_start() == hsync_start &&
                info.h_sync_end() == hsync_end &&
                info.h_total() == htotal &&
                info.v_sync_start() == vsync_start &&
                info.v_sync_end() == vsync_end &&
                info.v_total() == vtotal &&
                atof(curDrmModeRefresh) == atoi(curRefresh)) {
          foundAuxIdx = c;
          break;
        }
      }
    }
    for (auto &conn : drm->connectors()) {
      if (conn->state() == DRM_MODE_CONNECTED
              && (conn->get_type() == DRM_MODE_CONNECTOR_HDMIA)
              && (conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT))
        isMainHdmiConnected = true;
      else if(conn->state() == DRM_MODE_CONNECTED
              && (conn->get_type() == DRM_MODE_CONNECTOR_HDMIA)
              && (conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT))
        isAuxHdmiConnected = true;
    }
    ALOGI_IF(log_level(DBG_INFO),"BP:nativeSaveConfig: size=%d isMainHdmiConnected=%d", (int)sizeof(base_parameter.main), isMainHdmiConnected);
    for (auto &conn : drm->connectors()) {
      if (conn->state() == DRM_MODE_CONNECTED
              && (conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT)) {
        char property[PROPERTY_VALUE_MAX];
        int w = 0, h = 0, hsync_start = 0, hsync_end = 0, htotal = 0;
        int vsync_start = 0, vsync_end = 0, vtotal = 0,flags = 0;
        int left = 0, top = 0, right = 0, bottom = 0;
        float vfresh = 0;
        int slot = hwc_findSuitableInfoSlot(&base_parameter.main, conn->get_type());
        if (isMainHdmiConnected && conn->get_type() == DRM_MODE_CONNECTOR_TV)
            continue;

        base_parameter.main.screen_list[slot].type = conn->get_type();
        base_parameter.main.screen_list[slot].feature &= AUTO_BIT_RESET;
        property_get("persist." PROPERTY_TYPE ".resolution.main", property, "0x0@0.00-0-0-0-0-0-0-0");
        if (strncmp(property, "Auto", 4) != 0 && strncmp(property, "0x0p0-0", 7) !=0) {
            ALOGI_IF(log_level(DBG_INFO),"BP:saveConfig resolution = %s", property);
            std::vector<DrmMode> mModes = primary->modes();
            sscanf(property,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x", &w, &h, &vfresh, &hsync_start,&hsync_end,&htotal,&vsync_start,&vsync_end,
                    &vtotal, &flags);

            ALOGI_IF(log_level(DBG_INFO),"BP:last base_parameter.main.resolution.hdisplay = %d,  vdisplay=%d(%s@%f)",
                    base_parameter.main.screen_list[slot].resolution.hdisplay,
                    base_parameter.main.screen_list[slot].resolution.vdisplay,
                    base_parameter.main.hwc_info.device,  base_parameter.main.hwc_info.fps);
            base_parameter.main.screen_list[slot].resolution.hdisplay = w;
            base_parameter.main.screen_list[slot].resolution.vdisplay = h;
            base_parameter.main.screen_list[slot].resolution.hsync_start = hsync_start;
            base_parameter.main.screen_list[slot].resolution.hsync_end = hsync_end;
            if (foundMainIdx != -1)
                base_parameter.main.screen_list[slot].resolution.clock = mModes[foundMainIdx].clock();
            else if (flags & DRM_MODE_FLAG_INTERLACE)
                base_parameter.main.screen_list[slot].resolution.clock = (htotal*vtotal*vfresh/2)/1000.0f;
            else
                base_parameter.main.screen_list[slot].resolution.clock = (htotal*vtotal*vfresh)/1000.0f;
            base_parameter.main.screen_list[slot].resolution.htotal = htotal;
            base_parameter.main.screen_list[slot].resolution.vsync_start = vsync_start;
            base_parameter.main.screen_list[slot].resolution.vsync_end = vsync_end;
            base_parameter.main.screen_list[slot].resolution.vtotal = vtotal;
            base_parameter.main.screen_list[slot].resolution.flags = flags;
            ALOGI_IF(log_level(DBG_INFO),"BP:saveBaseParameter foundMainIdx=%d clock=%d", foundMainIdx, base_parameter.main.screen_list[slot].resolution.clock);
        } else {
            base_parameter.main.screen_list[slot].feature|= RESOLUTION_AUTO;
            memset(&base_parameter.main.screen_list[slot].resolution, 0, sizeof(base_parameter.main.screen_list[slot].resolution));
        }

        memset(property,0,sizeof(property));
        property_get("persist." PROPERTY_TYPE ".overscan.main", property, "overscan 100,100,100,100");
        sscanf(property, "overscan %d,%d,%d,%d",
                &left,
                &top,
                &right,
                &bottom);
        base_parameter.main.scan.leftscale = (unsigned short)left;
        base_parameter.main.scan.topscale = (unsigned short)top;
        base_parameter.main.scan.rightscale = (unsigned short)right;
        base_parameter.main.scan.bottomscale = (unsigned short)bottom;

        memset(property,0,sizeof(property));
        property_get("persist." PROPERTY_TYPE ".color.main", property, "Auto");
        if (strncmp(property, "Auto", 4) != 0){
            if (strstr(property, "RGB") != 0)
                base_parameter.main.screen_list[slot].format = output_rgb;
            else if (strstr(property, "YCBCR444") != 0)
                base_parameter.main.screen_list[slot].format = output_ycbcr444;
            else if (strstr(property, "YCBCR422") != 0)
                base_parameter.main.screen_list[slot].format = output_ycbcr422;
            else if (strstr(property, "YCBCR420") != 0)
                base_parameter.main.screen_list[slot].format = output_ycbcr420;
            else {
                base_parameter.main.screen_list[slot].feature |= COLOR_AUTO;
                base_parameter.main.screen_list[slot].format = output_ycbcr_high_subsampling;
            }

            if (strstr(property, "8bit") != NULL)
                base_parameter.main.screen_list[slot].depthc = depth_24bit;
            else if (strstr(property, "10bit") != NULL)
                base_parameter.main.screen_list[slot].depthc = depth_30bit;
            else
                base_parameter.main.screen_list[slot].depthc = Automatic;
            ALOGD("saveConfig: color=%d-%d", base_parameter.main.screen_list[slot].format, base_parameter.main.screen_list[slot].depthc);
        } else {
            base_parameter.main.screen_list[slot].depthc = Automatic;
            base_parameter.main.screen_list[slot].format = output_ycbcr_high_subsampling;
            base_parameter.main.screen_list[slot].feature |= COLOR_AUTO;
        }

        memset(property,0,sizeof(property));
        property_get("persist." PROPERTY_TYPE ".hdcp1x.main", property, "0");
        if (atoi(property) > 0)
            base_parameter.main.screen_list[slot].feature |= HDCP1X_EN;

        memset(property,0,sizeof(property));
        property_get("persist." PROPERTY_TYPE ".resolution_white.main", property, "0");
        if (atoi(property) > 0)
            base_parameter.main.screen_list[slot].feature |= RESOLUTION_WHITE_EN;
        hwc_save_BcshConfig(HWC_DISPLAY_PRIMARY_BIT);
    } else if(conn->state() == DRM_MODE_CONNECTED
            && (conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT)
            && (conn->encoder() != NULL)) {
      char property[PROPERTY_VALUE_MAX];
      int w = 0, h = 0, hsync_start = 0, hsync_end = 0, htotal = 0;
      int vsync_start = 0, vsync_end = 0, vtotal = 0,flags = 0;
      int left = 0, top = 0, right = 0, bottom = 0;
      float vfresh = 0;
      int slot = hwc_findSuitableInfoSlot(&base_parameter.aux, conn->get_type());

      if (isAuxHdmiConnected && conn->get_type() == DRM_MODE_CONNECTOR_TV)
          continue;

      base_parameter.aux.screen_list[slot].type = conn->get_type();
      base_parameter.aux.screen_list[slot].feature &= AUTO_BIT_RESET;
      property_get("persist." PROPERTY_TYPE ".resolution.aux", property, "0x0p0-0");
      if (strncmp(property, "Auto", 4) != 0 && strncmp(property, "0x0p0-0", 7) !=0) {
          std::vector<DrmMode> mModes = extend->modes();
          sscanf(property,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x", &w, &h, &vfresh, &hsync_start,&hsync_end,&htotal,&vsync_start,&vsync_end,
                  &vtotal, &flags);
          base_parameter.aux.screen_list[slot].resolution.hdisplay = w;
          base_parameter.aux.screen_list[slot].resolution.vdisplay = h;
          if (foundMainIdx != -1)
              base_parameter.aux.screen_list[slot].resolution.clock = mModes[foundMainIdx].clock();
          else if (flags & DRM_MODE_FLAG_INTERLACE)
              base_parameter.aux.screen_list[slot].resolution.clock = (htotal*vtotal*vfresh/2) / 1000.0f;
          else
              base_parameter.aux.screen_list[slot].resolution.clock = (htotal*vtotal*vfresh) / 1000.0f;
          base_parameter.aux.screen_list[slot].resolution.hsync_start = hsync_start;
          base_parameter.aux.screen_list[slot].resolution.hsync_end = hsync_end;
          base_parameter.aux.screen_list[slot].resolution.htotal = htotal;
          base_parameter.aux.screen_list[slot].resolution.vsync_start = vsync_start;
          base_parameter.aux.screen_list[slot].resolution.vsync_end = vsync_end;
          base_parameter.aux.screen_list[slot].resolution.vtotal = vtotal;
          base_parameter.aux.screen_list[slot].resolution.flags = flags;
      } else {
          base_parameter.aux.screen_list[slot].feature |= RESOLUTION_AUTO;
          memset(&base_parameter.aux.screen_list[slot].resolution, 0, sizeof(base_parameter.aux.screen_list[slot].resolution));
      }

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".overscan.aux", property, "overscan 100,100,100,100");
      sscanf(property, "overscan %d,%d,%d,%d",
              &left,
              &top,
              &right,
              &bottom);
      base_parameter.aux.scan.leftscale = (unsigned short)left;
      base_parameter.aux.scan.topscale = (unsigned short)top;
      base_parameter.aux.scan.rightscale = (unsigned short)right;
      base_parameter.aux.scan.bottomscale = (unsigned short)bottom;

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".color.aux", property, "Auto");
      if (strncmp(property, "Auto", 4) != 0){
          char color[16];
          char depth[16];

          sscanf(property, "%s-%s", color, depth);
          if (strncmp(color, "RGB", 3) == 0)
              base_parameter.aux.screen_list[slot].format = output_rgb;
          else if (strncmp(color, "YCBCR444", 8) == 0)
              base_parameter.aux.screen_list[slot].format = output_ycbcr444;
          else if (strncmp(color, "YCBCR422", 8) == 0)
              base_parameter.aux.screen_list[slot].format = output_ycbcr422;
          else if (strncmp(color, "YCBCR420", 8) == 0)
              base_parameter.aux.screen_list[slot].format = output_ycbcr420;
          else {
              base_parameter.aux.screen_list[slot].feature |= COLOR_AUTO;
              base_parameter.aux.screen_list[slot].format = output_ycbcr_high_subsampling;
          }

          if (strncmp(depth, "8bit", 4) == 0)
              base_parameter.aux.screen_list[slot].depthc = depth_24bit;
          else if (strncmp(depth, "10bit", 5) == 0)
              base_parameter.aux.screen_list[slot].depthc = depth_30bit;
          else
              base_parameter.aux.screen_list[slot].depthc = Automatic;
      } else {
        base_parameter.aux.screen_list[slot].feature |= COLOR_AUTO;
        base_parameter.aux.screen_list[slot].depthc = Automatic;
        base_parameter.aux.screen_list[slot].format = output_ycbcr_high_subsampling;
      }

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".hdcp1x.aux", property, "0");
      if (atoi(property) > 0)
        base_parameter.aux.screen_list[slot].feature |= HDCP1X_EN;

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".resolution_white.aux", property, "0");
      if (atoi(property) > 0)
        base_parameter.aux.screen_list[slot].feature |= RESOLUTION_WHITE_EN;
      /*add for BCSH*/
      hwc_save_BcshConfig(HWC_DISPLAY_EXTERNAL_BIT);
    }
  }

    lseek(file, 0L, SEEK_SET);
    write(file, (char*)(&base_parameter.main), sizeof(base_parameter.main));
    lseek(file, BASE_OFFSET, SEEK_SET);
    write(file, (char*)(&base_parameter.aux), sizeof(base_parameter.aux));
    close(file);
    sync();

}
    return ;
}

void hwc_save_BcshConfig(int dpy){
  if (dpy == HWC_DISPLAY_PRIMARY_BIT){
      char property[PROPERTY_VALUE_MAX];

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".brightness.main", property, "0");
      if (atoi(property) > 0)
          base_parameter.main.bcsh.brightness = atoi(property);
      else
          base_parameter.main.bcsh.brightness = DEFAULT_BRIGHTNESS;

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".contrast.main", property, "0");
      if (atoi(property) > 0)
          base_parameter.main.bcsh.contrast = atoi(property);
      else
          base_parameter.main.bcsh.contrast = DEFAULT_CONTRAST;

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".saturation.main", property, "0");
      if (atoi(property) > 0)
          base_parameter.main.bcsh.saturation = atoi(property);
      else
          base_parameter.main.bcsh.saturation = DEFAULT_SATURATION;

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".hue.main", property, "0");
      if (atoi(property) > 0)
          base_parameter.main.bcsh.hue = atoi(property);
      else
          base_parameter.main.bcsh.hue = DEFAULT_HUE;
  } else {
      char property[PROPERTY_VALUE_MAX];

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".brightness.aux", property, "0");
      if (atoi(property) > 0)
          base_parameter.aux.bcsh.brightness = atoi(property);
      else
          base_parameter.aux.bcsh.brightness = DEFAULT_BRIGHTNESS;

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".contrast.aux", property, "0");
      if (atoi(property) > 0)
          base_parameter.aux.bcsh.contrast = atoi(property);
      else
          base_parameter.aux.bcsh.contrast = DEFAULT_CONTRAST;

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".saturation.aux", property, "0");
      if (atoi(property) > 0)
          base_parameter.aux.bcsh.saturation = atoi(property);
      else
          base_parameter.aux.bcsh.saturation = DEFAULT_SATURATION;

      memset(property,0,sizeof(property));
      property_get("persist." PROPERTY_TYPE ".hue.aux", property, "0");
      if (atoi(property) > 0)
          base_parameter.aux.bcsh.hue = atoi(property);
      else
          base_parameter.aux.bcsh.hue = DEFAULT_HUE;
  }
}
int hwc_findSuitableInfoSlot(struct disp_info* info, int type)
{
    int found=0;
    for (int i=0;i<5;i++) {
        if (info->screen_list[i].type !=0 && info->screen_list[i].type == type) {
            found = i;
            break;
        } else if (info->screen_list[i].type !=0 && found == false){
            found++;
        }
    }
    if (found == -1) {
        found = 0;
        ALOGI_IF(log_level(DBG_INFO),"BP:noting saved, used the first slot");
    }
    ALOGI_IF(log_level(DBG_INFO),"BP:findSuitableInfoSlot: %d type=%d", found, type);
    return found;
}


int hwc_parse_format_into_prop(int display,unsigned int format,unsigned int depthc) {

    if(display == HWC_DISPLAY_PRIMARY){
        if (format == DRM_HDMI_OUTPUT_YCBCR_HQ &&
                depthc == ROCKCHIP_DEPTH_DEFAULT) {
            property_set("persist." PROPERTY_TYPE ".color.main","Auto");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_DEFAULT_RGB &&
                depthc == ROCKCHIP_HDMI_DEPTH_8) {
            property_set("persist." PROPERTY_TYPE ".color.main","RGB-8bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_DEFAULT_RGB &&
                depthc == ROCKCHIP_HDMI_DEPTH_10) {
            property_set("persist." PROPERTY_TYPE ".color.main","RGB-10bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR444 &&
                depthc == ROCKCHIP_HDMI_DEPTH_8) {
            property_set("persist." PROPERTY_TYPE ".color.main","YCBCR444-8bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR444 &&
                depthc == ROCKCHIP_HDMI_DEPTH_10) {
            property_set("persist." PROPERTY_TYPE ".color.main","YCBCR444-10bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR422 &&
                depthc == ROCKCHIP_HDMI_DEPTH_8) {
            property_set("persist." PROPERTY_TYPE ".color.main","YCBCR422-8bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR422 &&
                depthc == ROCKCHIP_HDMI_DEPTH_10) {
            property_set("persist." PROPERTY_TYPE ".color.main","YCBCR422-10bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR420 &&
                depthc == ROCKCHIP_HDMI_DEPTH_8) {
            property_set("persist." PROPERTY_TYPE ".color.main","YCBCR420-8bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR420 &&
                depthc == ROCKCHIP_HDMI_DEPTH_10) {
            property_set("persist." PROPERTY_TYPE ".color.main","YCBCR420-10bit");
            return 0;
        }
    }else{
        if (format == DRM_HDMI_OUTPUT_YCBCR_HQ &&
                depthc == ROCKCHIP_DEPTH_DEFAULT) {
            property_set("persist." PROPERTY_TYPE ".color.aux","Auto");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_DEFAULT_RGB &&
                depthc == ROCKCHIP_HDMI_DEPTH_8) {
            property_set("persist." PROPERTY_TYPE ".color.aux","RGB-8bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_DEFAULT_RGB &&
                depthc == ROCKCHIP_HDMI_DEPTH_10) {
            property_set("persist." PROPERTY_TYPE ".color.aux","RGB-10bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR444 &&
                depthc == ROCKCHIP_HDMI_DEPTH_8) {
            property_set("persist." PROPERTY_TYPE ".color.aux","YCBCR444-8bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR444 &&
                depthc == ROCKCHIP_HDMI_DEPTH_10) {
            property_set("persist." PROPERTY_TYPE ".color.aux","YCBCR444-10bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR422 &&
                depthc == ROCKCHIP_HDMI_DEPTH_8) {
            property_set("persist." PROPERTY_TYPE ".color.aux","YCBCR422-8bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR422 &&
                depthc == ROCKCHIP_HDMI_DEPTH_10) {
            property_set("persist." PROPERTY_TYPE ".color.aux","YCBCR422-10bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR420 &&
                depthc == ROCKCHIP_HDMI_DEPTH_8) {
            property_set("persist." PROPERTY_TYPE ".color.aux","YCBCR420-8bit");
            return 0;
        }

        if (format == DRM_HDMI_OUTPUT_YCBCR420 &&
                depthc == ROCKCHIP_HDMI_DEPTH_10) {
            property_set("persist." PROPERTY_TYPE ".color.aux","YCBCR420-10bit");
            return 0;
        }
    }
        ALOGI_IF(log_level(DBG_INFO),"BP: baseparameter color is invalid.");
        return -1;
}

bool hwc_video_to_area(DrmHwcRect<float> &source_yuv,DrmHwcRect<int> &display_yuv,int scaleMode){
    float s_letf, s_top, s_right, s_bottom;
    float s_width,s_height;
    int d_letf, d_top, d_right, d_bottom;
    int d_width,d_height;

    s_letf = source_yuv.left;
    s_top = source_yuv.top;
    s_right = source_yuv.right;
    s_bottom = source_yuv.bottom;
    s_width = s_right - s_letf;
    s_height = s_bottom - s_top;

    d_letf = display_yuv.left;
    d_top = display_yuv.top;
    d_right = display_yuv.right;
    d_bottom = display_yuv.bottom;
    d_width = d_right - d_letf;
    d_height = d_bottom - d_top;

    switch (scaleMode){
        case VIDEO_SCALE_AUTO_SCALE :
            if(s_width * d_height > s_height * d_width){
                d_top += ( d_height - s_height * d_width / s_width ) / 2;
                d_bottom -= ( d_height - s_height * d_width / s_width ) / 2;
            }else{
                d_letf += ( d_width - s_width * d_height / s_height) / 2;
                d_right -= ( d_width - s_width * d_height / s_height) / 2;
            }
            break;
        case VIDEO_SCALE_4_3_SCALE :
            if(4 * d_height  < 3 * d_width){
                d_letf += (d_width - d_height * 4 / 3) / 2;
                d_right -= (d_width - d_height * 4 / 3) / 2;
            }else if(4 * d_height  > 3 * d_width){
                d_top += (d_height - d_width * 3 / 4) / 2;
                d_bottom -= (d_height - d_width * 3 / 4) / 2;
            }
            break;
        case VIDEO_SCALE_16_9_SCALE :
            if(16 * d_height  < 9 * d_width){
                d_letf += (d_width - d_height * 16 / 9) / 2;
                d_right -= (d_width - d_height * 16 / 9) / 2;
            }else if(16 * d_height  > 9 * d_width){
                d_top += (d_width - d_width * 9 / 16) / 2;
                d_bottom -= (d_width - d_width * 9 / 16) / 2;
            }
            break;
        case VIDEO_SCALE_ORIGINAL :
            if(s_width > d_width){
                d_letf = 0;
                //d_right = d_right;
            }else{
                d_letf = (d_width - s_width) / 2;
                d_right -= (d_width - s_width) / 2;
            }
            if(s_height > d_height){
                d_top = 0;
                //d_bottom = d_bottom;
            }else{
                d_top = (d_height - s_height) / 2;
                d_bottom -= (d_height - s_height ) / 2;
            }
            break;
        default :
            ALOGE("ScaleMode[%d] is invalid ",scaleMode);
            return false;
    }
    ALOGD_IF(log_level(DBG_VERBOSE),"Video area change [%d,%d,%d,%d]:[%d,%d,%d,%d] => [%d,%d,%d,%d]",
    (int)source_yuv.left,(int)source_yuv.top,(int)source_yuv.right,(int)source_yuv.bottom,
    display_yuv.left,display_yuv.top,display_yuv.right,display_yuv.bottom,
    d_letf,d_top,d_right,d_bottom);

    display_yuv.left = d_letf;
    display_yuv.top = d_top;
    display_yuv.right = d_right;
    display_yuv.bottom = d_bottom;
    return true;
}

int hwc_SetGamma(DrmResources *drm)
{
    int ret = -1;
    if(hwc_have_baseparameter()){
        DrmConnector *primary = drm->GetConnectorFromType(HWC_DISPLAY_PRIMARY);
        if (primary != NULL && primary->state() == DRM_MODE_CONNECTED) {
            int size = base_parameter.main.mlutdata.size;
            if(size > 0){
                uint16_t* red = (uint16_t*)malloc(size*sizeof(uint16_t));
                uint16_t* green = (uint16_t*)malloc(size*sizeof(uint16_t));
                uint16_t* blue = (uint16_t*)malloc(size*sizeof(uint16_t));

                for (int i = 0; i < size; i++) {
                    red[i] = base_parameter.main.mlutdata.lred[i];
                }
                for (int i = 0; i < size; i++) {
                    green[i] = base_parameter.main.mlutdata.lgreen[i];
                }
                for (int i = 0; i < size; i++) {
                    blue[i] = base_parameter.main.mlutdata.lblue[i];
                }
                if (hwc_isGammaSetEnable(primary->get_type())) {
                    DrmCrtc *crtc = drm->GetCrtcFromConnector(primary);
                    if(crtc != NULL){
                        int mCurCrtcId = crtc->id();
                        ret = hwc_setGamma(drm->fd(), mCurCrtcId, (int)size, (uint16_t*)red, (uint16_t*)green, (uint16_t*)blue);
                        if (ret<0){
                            ALOGW("BP: nativeSetGamma failed: Primary size=%d r[%d %d] rgb_size= %d %d %d red[%d %d]", size,
                                    red[0],red[1],size, size, size ,red[0], red[1]);
                        }else{
                            ALOGI_IF(log_level(DBG_INFO),"BP: nativeSetGamma success: Primary size=%d r[%d %d] rgb_size= %d %d %d red[%d %d]", size,
                                    red[0],red[1],size, size, size ,red[0], red[1]);
                        }
                    }else{

                        ALOGW("BP: nativeSetGamma failed: Primary crtc is NULL");
                    }
                }else{
                    ALOGW("BP: Device type %d is not supprot Gamma",primary->get_type());
                }
                free(red);
                free(green);
                free(blue);
                red   = NULL;
                green = NULL;
                blue  = NULL;
           }else{
              ALOGW("BP: Gamma size = %d is err",size);
           }
        }
        DrmConnector *extend = drm->GetConnectorFromType(HWC_DISPLAY_EXTERNAL);
        if(extend != NULL && extend->state() == DRM_MODE_CONNECTED){
             int size = base_parameter.aux.mlutdata.size;
             if(size > 0){
                 uint16_t* red = (uint16_t*)malloc(size*sizeof(uint16_t));
                 uint16_t* green = (uint16_t*)malloc(size*sizeof(uint16_t));
                 uint16_t* blue = (uint16_t*)malloc(size*sizeof(uint16_t));

                 for (int i = 0; i < size; i++) {
                     red[i] = base_parameter.aux.mlutdata.lred[i];
                 }
                 for (int i = 0; i < size; i++) {
                     green[i] = base_parameter.aux.mlutdata.lgreen[i];
                 }
                 for (int i = 0; i < size; i++) {
                     blue[i] = base_parameter.aux.mlutdata.lblue[i];
                 }
                 if (hwc_isGammaSetEnable(extend->get_type())) {
                     DrmCrtc *crtc = drm->GetCrtcFromConnector(extend);
                     if(crtc != NULL){
                       int mCurCrtcId = crtc->id();
                       ret = hwc_setGamma(drm->fd(), mCurCrtcId, (int)size, (uint16_t*)red, (uint16_t*)green, (uint16_t*)blue);
                       if (ret<0){
                           ALOGW("BP: nativeSetGamma failed: Extend size=%d r[%d %d] rgb_size= %d %d %d red[%d %d]",size,
                                   red[0],red[1],size, size, size ,red[0], red[1]);
                       }else{
                          ALOGI_IF(log_level(DBG_INFO),"BP: nativeSetGamma success: Extend size=%d r[%d %d] rgb_size= %d %d %d red[%d %d]", size,
                                  red[0],red[1],size, size, size ,red[0], red[1]);
                      }
                    }else{
                      ALOGW("BP: nativeSetGamma failed: Extend crtc is NULL");
                    }
                 }else{
                    ALOGW("BP: Device type %d is not supprot Gamma",extend->get_type());
                 }
                 free(red);
                 free(green);
                 free(blue);
                 red   = NULL;
                 green = NULL;
                 blue  = NULL;
            }else{
              ALOGW("BP: Gamma size = %d is err",size);
           }
        }
    }
    return ret;
}

bool hwc_isGammaSetEnable(int type) {
    return type == DRM_MODE_CONNECTOR_eDP || type == DRM_MODE_CONNECTOR_LVDS ||
        type == DRM_MODE_CONNECTOR_DSI || type == DRM_MODE_CONNECTOR_DPI;
}
int hwc_setGamma(int fd, uint32_t crtc_id, uint32_t size,
        uint16_t *red, uint16_t *green, uint16_t *blue)
{
    int ret = drmModeCrtcSetGamma(fd, crtc_id, size, red, green, blue);
    if (ret < 0)
        ALOGE("fail to SetGamma %d(%s)", ret, strerror(errno));
    return ret;
}


}

