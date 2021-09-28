/*
 * Copyright (C) 2017 Intel Corporation.
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


#include "RKISP2DevImpl.h"

namespace android {
namespace camera2 {
namespace rkisp2 {

EptzThread::EptzThread()
{
    isInit = false;
    runnable = true;
}

EptzThread::~EptzThread()
{
    runnable = false;
    if(isInit){
        isInit = false;
        rockx_destroy(rockx_handle);
    }
    mDetectDatas.clear();
    nnBufVecs.clear();
}

void EptzThread::setPreviewCfg(int preview_width, int preview_height){
    src_width = preview_width;
    src_height = preview_height;
}

void EptzThread::EptzInitCfg(int width, int height)
{
    ALOGI("rk-debug:new EptzThread initCfg begin");
    npu_width = width;
    npu_height = height;
    nnBufVecs.clear();
    mTexUsage = GraphicBuffer::USAGE_HW_TEXTURE | GraphicBuffer::USAGE_SW_READ_OFTEN | GraphicBuffer::USAGE_SW_WRITE_OFTEN;
    for (int i=0;i<2;i++) {
        sp<GraphicBuffer> buf;
        if(i == 0)
            buf = new GraphicBuffer(width, height, HAL_PIXEL_FORMAT_YCrCb_NV12, mTexUsage);
        else
            buf = new GraphicBuffer(width, height, HAL_PIXEL_FORMAT_RGBA_8888, mTexUsage);
        nnBufVecs.push_back(buf);
    }
    ALOGI("rk-debug:EptzThread initCfg preview wh[%d %d]", width, height);
    RockxInit("/vendor/etc/model/", "/vendor/etc/key.lic");
    EptzInit(width, height, width, height);
    isInit = true;
    ALOGI("rk-debug:new EptzThread initCfg success");
}

void EptzThread::setMode(int mode)
{
    ALOGI("rk-debug:EptzThread setEptzMode %d", mode);
    eptz_mode = mode;
}

void EptzThread::setOcclusionMode(int mode)
{
    ALOGI("rk-debug:EptzThread setOcclusionMode %d", mode);
    occlusion_mode = mode;
}

int EptzThread::getMode()
{
    return eptz_mode;
}

bool EptzThread::threadLoop(){
    static int mode = eptz_mode;
    static bool first_flag = true;
    static int cnt = 0;
    if (!runnable){
        usleep(30*1000);
        return true;
    }
    if (!isInit)
        EptzInitCfg(src_width, src_height); 

    if (mode != eptz_mode || first_flag){
        first_flag = false;
        switch (eptz_mode)
        {
        case 1:
            setEptzMode(1);
            mEptzInfo.eptz_fast_move_frame_judge = 5;
            mEptzInfo.eptz_zoom_frame_judge = 10;
            break;
        case 2:
            setEptzMode(2);
            mEptzInfo.eptz_fast_move_frame_judge = 10;
            mEptzInfo.eptz_zoom_frame_judge = 15;
            break;
        default:
            break;
        }
        mode = eptz_mode;
    }
    if(!has_img_data){
        usleep(60*1000);
        return true;
    }else{
        has_img_data = false;
    }
    //return true;
    sp<GraphicBuffer> buffer = nullptr;
    
    std::unique_lock<std::mutex> lk(mtx_);
    if (nnBufVecs.size() > 0) {
        buffer = nnBufVecs[0];
    }
    lk.unlock();
    if (buffer != nullptr) {
        void* imagedata = nullptr;
        buffer->lock(mTexUsage, &imagedata);
        if(mode)
            RockxDetectFace(imagedata, npu_width, npu_height, ROCKX_PIXEL_FORMAT_YUV420SP_NV12);
        if(occlusion_mode && cnt++ % 200 == 0){
            int ret = RockxDetectOcclusion(imagedata, npu_width, npu_height, ROCKX_PIXEL_FORMAT_YUV420SP_NV12);
            if(ret != -1){
                if(1 == ret)
                    property_set("vendor.camera.occlusion.status", "1");
                else
                    property_set("vendor.camera.occlusion.status", "0");
            }
        }
        buffer->unlock();         
        // if(cnt == 100){
        //     FILE* fp = fopen("/data/test.yuv", "wb+");
        //     if (fp && buffer != nullptr) {
        //         void* data = nullptr;
        //         buffer->lock(mTexUsage, &data);
        //         fwrite(data, 1, npu_width * npu_height * 3 / 2, fp);
        //         //RockxDetectFace(rockx_handle, npu_width, npu_height, ROCKX_PIXEL_FORMAT_YUV420SP_NV12);
        //         buffer->unlock();    
        //         LOGD("rk-debug: write buffer to /data/test.yuv ptr=%p", data);
        //         fclose(fp);
        //     } else {
        //         LOGD("rk-debug: fopen %d(%s)", errno, strerror(errno));
        //     }
        // }
    }
    return true;
}

int EptzThread::RockxInit(char *model_path, char *licence_path){
    rockx_ret_t ret = ROCKX_RET_SUCCESS;
    rockx_config_t rockx_configs;
    memset(&rockx_configs, 0, sizeof(rockx_config_t));

    LOGD("rk-debug rockx_add_config ROCKX_CONFIG_DATA_PATH=%s\n", model_path);
    rockx_add_config(&rockx_configs, ROCKX_CONFIG_DATA_PATH, model_path);
    rockx_add_config(&rockx_configs, ROCKX_CONFIG_LICENCE_KEY_PATH, licence_path);
    ret = rockx_create(&rockx_handle, ROCKX_MODULE_FACE_DETECTION_V2_HORIZONTAL, &rockx_configs, sizeof(rockx_config_t));
    if (ret != ROCKX_RET_SUCCESS) {
        LOGE("rk-debug init rockx module %d error %d\n", ROCKX_MODULE_FACE_DETECTION_V2_HORIZONTAL, ret);
        return ret;
    }else{
        ALOGI("rk-debug init rockx module success\n");
    }
    return ret;
}

int EptzThread::RockxDetectFace(void *in_data, int inWidth, int inHeight, int inPixelFmt){
    // initial rockx_image_t variable
    rockx_image_t input_image;
    input_image.width = inWidth;
    input_image.height = inHeight;
    input_image.data = (unsigned char *)in_data;
    input_image.pixel_format = (rockx_pixel_format)inPixelFmt;

    // create rockx_face_array_t for store result
    rockx_object_array_t face_array;
    memset(&face_array, 0, sizeof(rockx_object_array_t));

    // detect face
    rockx_ret_t ret = rockx_face_detect((rockx_handle_t)rockx_handle, &input_image, &face_array, nullptr);
    if (ret != ROCKX_RET_SUCCESS) {
        LOGD("rk-debug rockx_face_detect error %d\n", ret);
        return -1;
    }
    std::unique_lock<std::mutex> lk(face_mtx_);
    mDetectDatas.clear();
    for(int i = 0; i < face_array.count; i++){
        DetectData DetectData;
        DetectData.left = face_array.object[i].box.left;
        DetectData.top = face_array.object[i].box.top;
        DetectData.right = face_array.object[i].box.right;
        DetectData.bottom = face_array.object[i].box.bottom;
        DetectData.score = face_array.object[i].score;

        mDetectDatas.push_back(DetectData);
        LOGD("rk-debug  rockx_face_detect[%d] ltrb[%d %d %d %d] score[%.2f] \n", ret,
               DetectData.left,
               DetectData.top,
               DetectData.right,
               DetectData.bottom,
               DetectData.score);
    }
    lk.unlock();
    return 0;
}

int EptzThread::RockxDetectOcclusion(void *in_data, int inWidth, int inHeight, int inPixelFmt){
    // initial rockx_image_t variable
    rockx_image_t input_image;
    input_image.width = inWidth;
    input_image.height = inHeight;
    input_image.data = (unsigned char *)in_data;
    input_image.pixel_format = (rockx_pixel_format)inPixelFmt;

    int result = 0;
    // detect occlusion
    rockx_ret_t ret = rockx_image_detect_occlusion(&input_image, &result);
    if (ret != ROCKX_RET_SUCCESS) {
        LOGD("rk-debug rockx_face_detect error %d\n", ret);
        return -1;
    }
    return result;

}
void EptzThread::EptzInit(int mSrcWidth, int mSrcHeight, int mClipWidth, int mClipHeight){
    mEptzInfo.eptz_src_width = mSrcWidth;
    mEptzInfo.eptz_src_height = mSrcHeight;
    mEptzInfo.eptz_dst_width = mSrcWidth;
    mEptzInfo.eptz_dst_height = mSrcHeight;
    mEptzInfo.camera_dst_width = mClipWidth;
    mEptzInfo.camera_dst_height = mClipHeight;
    //2K以上sensor建议使用1280x720数据,低于2K使用640x360
    mEptzInfo.eptz_npu_width = npu_width;
    mEptzInfo.eptz_npu_height = npu_height;
    //V2远距离建议0.4，V3近距离建议0.6
    mEptzInfo.eptz_facedetect_score_shold = 0.40;
    mEptzInfo.eptz_zoom_speed = 1;
    mEptzInfo.eptz_fast_move_frame_judge = 5;
    mEptzInfo.eptz_zoom_frame_judge = 10;
    mEptzInfo.eptz_threshold_x = 80;
    mEptzInfo.eptz_threshold_y = 45;
    if (mEptzInfo.camera_dst_width >= 1920) {
        mEptzInfo.eptz_iterate_x = 6;
        mEptzInfo.eptz_iterate_y = 3;
    } else if (mEptzInfo.camera_dst_width >= 1280) {
        mEptzInfo.eptz_iterate_x = 6;
        mEptzInfo.eptz_iterate_y = 3;
    } else {
        mEptzInfo.eptz_iterate_x = 4;
        mEptzInfo.eptz_iterate_y = 2;
    }
    ALOGI("rk-debug eptz_info src_wh [%d %d] dst_wh[%d %d], threshold_xy[%d %d] "
           "iterate_xy[%d %d] \n",
           mEptzInfo.eptz_src_width, mEptzInfo.eptz_src_height,
           mEptzInfo.eptz_dst_width, mEptzInfo.eptz_dst_height, mEptzInfo.eptz_threshold_x,
           mEptzInfo.eptz_threshold_y, mEptzInfo.eptz_iterate_x, mEptzInfo.eptz_iterate_y);

    mLastXY[0] = 0;
    mLastXY[1] = 0;
    mLastXY[2] = mEptzInfo.eptz_dst_width;
    mLastXY[3] = mEptzInfo.eptz_dst_height;
    eptzConfigInit(&mEptzInfo);
}

void EptzThread::converData(RgaCropScale::Params rgain){
    RgaCropScale::Params eptzRgba, eptzout;
    //rga conf
    eptzRgba.fd = nnBufVecs[1]->handle->data[0];
    eptzRgba.fmt = HAL_PIXEL_FORMAT_RGBA_8888;
    eptzRgba.mirror = rgain.mirror;
    eptzRgba.width = rgain.width;
    eptzRgba.height = rgain.height;
    eptzRgba.offset_x = rgain.offset_x;
    eptzRgba.offset_y = rgain.offset_y;
    eptzRgba.width_stride = rgain.width_stride;
    eptzRgba.height_stride = rgain.height_stride;
    RgaCropScale::CropScaleNV12Or21(&rgain, &eptzRgba);

    //dst conf
    eptzout.fd = nnBufVecs[0]->handle->data[0];
    eptzout.fmt = rgain.fmt;
    eptzout.mirror = rgain.mirror;
    eptzout.width = rgain.width;
    eptzout.height = rgain.height;
    eptzout.offset_x = rgain.offset_x;
    eptzout.offset_y = rgain.offset_y;
    eptzout.width_stride = rgain.width_stride;
    eptzout.height_stride = rgain.height_stride;
    RgaCropScale::CropScaleNV12Or21(&rgain, &eptzout);
    has_img_data = true;
}

void EptzThread::calculateRect(RgaCropScale::Params *rgain){
    std::unique_lock<std::mutex> lk(face_mtx_);
    if(mDetectDatas.size()>0){
        int faceCount = mDetectDatas.size();
        EptzAiData eptz_ai_data;
        eptz_ai_data.face_data = (FaceData *)malloc(faceCount * sizeof(FaceData));
        eptz_ai_data.face_count = faceCount;
        if(eptz_ai_data.face_data){
            for(int i = 0; i<faceCount; i++){
                eptz_ai_data.face_data[i].left = mDetectDatas[i].left;
                eptz_ai_data.face_data[i].top = mDetectDatas[i].top;
                eptz_ai_data.face_data[i].right = mDetectDatas[i].right;
                eptz_ai_data.face_data[i].bottom = mDetectDatas[i].bottom;
                eptz_ai_data.face_data[i].score = mDetectDatas[i].score;
            }
        }
        calculateClipRect(&eptz_ai_data, mLastXY);
        if(eptz_ai_data.face_data)
            free(eptz_ai_data.face_data);
    }else{
        EptzAiData eptz_ai_data;
        eptz_ai_data.face_count = 0;
        calculateClipRect(&eptz_ai_data, mLastXY, true, 5);
    }
    lk.unlock();
    rgain->fd = nnBufVecs[1]->handle->data[0];
    rgain->fmt = HAL_PIXEL_FORMAT_RGBA_8888;
    rgain->offset_x = mLastXY[0];
    rgain->offset_y = mLastXY[1];
    rgain->width = mLastXY[2];
    rgain->height = mLastXY[3];
}

} /* namespace rkisp2 */
} /* namespace camera2 */
} /* namespace android */
