/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/


#define ATRACE_TAG ATRACE_TAG_CAMERA
#define LOG_TAG "QCamera3CropRegionMapper"

#include <cmath>

// Camera dependencies
#include "QCamera3CropRegionMapper.h"
#include "QCamera3HWI.h"

extern "C" {
#include "mm_camera_dbg.h"
}

using namespace android;

namespace qcamera {

/*===========================================================================
 * FUNCTION   : QCamera3CropRegionMapper
 *
 * DESCRIPTION: Constructor
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
QCamera3CropRegionMapper::QCamera3CropRegionMapper()
        : mSensorW(0),
          mSensorH(0),
          mActiveArrayW(0),
          mActiveArrayH(0)
{
}

/*===========================================================================
 * FUNCTION   : ~QCamera3CropRegionMapper
 *
 * DESCRIPTION: destructor
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/

QCamera3CropRegionMapper::~QCamera3CropRegionMapper()
{
}

/*===========================================================================
 * FUNCTION   : update
 *
 * DESCRIPTION: update sensor active array size and sensor output size
 *
 * PARAMETERS :
 *   @active_array_w : active array width
 *   @active_array_h : active array height
 *   @sensor_w       : sensor output width
 *   @sensor_h       : sensor output height
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera3CropRegionMapper::update(uint32_t active_array_w,
        uint32_t active_array_h, uint32_t sensor_w,
        uint32_t sensor_h)
{
    // Sanity check
    if (active_array_w == 0 || active_array_h == 0 ||
            sensor_w == 0 || sensor_h == 0) {
        LOGE("active_array size and sensor output size must be non zero");
        return;
    }
    if (active_array_w < sensor_w || active_array_h < sensor_h) {
        LOGE("invalid input: active_array [%d, %d], sensor size [%d, %d]",
                 active_array_w, active_array_h, sensor_w, sensor_h);
        return;
    }
    mSensorW = sensor_w;
    mSensorH = sensor_h;
    mActiveArrayW = active_array_w;
    mActiveArrayH = active_array_h;

    LOGH("active_array: %d x %d, sensor size %d x %d",
            mActiveArrayW, mActiveArrayH, mSensorW, mSensorH);
}

/*===========================================================================
 * FUNCTION   : toActiveArray
 *
 * DESCRIPTION: Map crop rectangle from sensor output space to active array space
 *
 * PARAMETERS :
 *   @crop_left   : x coordinate of top left corner of rectangle
 *   @crop_top    : y coordinate of top left corner of rectangle
 *   @crop_width  : width of rectangle
 *   @crop_height : height of rectangle
 *   @zoom_ratio  : zoom ratio to be reverted for the input rectangles
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera3CropRegionMapper::toActiveArray(int32_t& crop_left, int32_t& crop_top,
        int32_t& crop_width, int32_t& crop_height, float zoom_ratio)
{
    if (mSensorW == 0 || mSensorH == 0 ||
            mActiveArrayW == 0 || mActiveArrayH == 0) {
        LOGE("sensor/active array sizes are not initialized!");
        return;
    }
    if (zoom_ratio < MIN_ZOOM_RATIO) {
        LOGE("Invalid zoom ratio %f", zoom_ratio);
        return;
    }

    // Map back to activeArray space
    float left = crop_left * mActiveArrayW / mSensorW;
    float top = crop_top * mActiveArrayH / mSensorH;
    float width = crop_width * mActiveArrayW / mSensorW;
    float height = crop_height * mActiveArrayH / mSensorH;

    // Revert zoom_ratio, so that now the crop rectangle is separate from
    // zoom_ratio. The coordinate is within the active array space which covers
    // the post-zoom FOV.
    left = left * zoom_ratio - (zoom_ratio - 1) * 0.5f * mActiveArrayW;
    top = top * zoom_ratio - (zoom_ratio - 1) * 0.5f * mActiveArrayH;
    width = width * zoom_ratio;
    height = height * zoom_ratio;

    crop_left = std::round(left);
    crop_top = std::round(top);
    crop_width = std::round(width);
    crop_height = std::round(height);

    boundToSize(crop_left, crop_top, crop_width, crop_height,
            mActiveArrayW, mActiveArrayH);
}

/*===========================================================================
 * FUNCTION   : toSensor
 *
 * DESCRIPTION: Map crop rectangle from active array space to sensor output space
 *
 * PARAMETERS :
 *   @crop_left   : x coordinate of top left corner of rectangle
 *   @crop_top    : y coordinate of top left corner of rectangle
 *   @crop_width  : width of rectangle
 *   @crop_height : height of rectangle
 *   @zoom_ratio  : zoom ratio to be applied to the input rectangles
 *
 * RETURN     : none
 *==========================================================================*/

void QCamera3CropRegionMapper::toSensor(int32_t& crop_left, int32_t& crop_top,
        int32_t& crop_width, int32_t& crop_height, float zoom_ratio)
{
    if (mSensorW == 0 || mSensorH == 0 ||
            mActiveArrayW == 0 || mActiveArrayH == 0) {
        LOGE("sensor/active array sizes are not initialized!");
        return;
    }

    applyZoomRatioHelper(crop_left, crop_top, crop_width, crop_height, zoom_ratio,
            true /*to_sensor*/);
}

/*===========================================================================
 * FUNCTION   : applyZoomRatioHelper
 *
 * DESCRIPTION: Apply zoom ratio to the crop region, and optionally map
 *              to sensor coordinate system
 *
 * PARAMETERS :
 *   @crop_left   : x coordinate of top left corner of rectangle
 *   @crop_top    : y coordinate of top left corner of rectangle
 *   @crop_width  : width of rectangle
 *   @crop_height : height of rectangle
 *   @zoom_ratio  : zoom ratio to be applied to the input rectangles
 *   @to_sensor   : whether the crop region is to be mapped to sensor coordinate
 *                  system
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera3CropRegionMapper::applyZoomRatioHelper(int32_t& crop_left, int32_t& crop_top,
        int32_t& crop_width, int32_t& crop_height, float zoom_ratio, bool to_sensor)
{
    if (zoom_ratio < MIN_ZOOM_RATIO) {
        LOGE("Invalid zoom ratio %f", zoom_ratio);
        return;
    }

    // Apply zoom_ratio to the input rectangle in activeArray space, so that
    // crop rectangle already takes zoom_ratio into account (in other words,
    // coordinate within the sensor native active array space).
    float left = crop_left / zoom_ratio +
            0.5f * mActiveArrayW * (1.0f - 1.0f / zoom_ratio);
    float top = crop_top / zoom_ratio +
            0.5f * mActiveArrayH * (1.0f - 1.0f / zoom_ratio);
    float width = crop_width / zoom_ratio;
    float height = crop_height / zoom_ratio;

    if (to_sensor) {
        // Map to sensor space.
        left = left * mSensorW / mActiveArrayW;
        top = top * mSensorH / mActiveArrayH;
        width = width * mSensorW / mActiveArrayW;
        height = height * mSensorH / mActiveArrayH;
    }

    crop_left = std::round(left);
    crop_top = std::round(top);
    crop_width = std::round(width);
    crop_height = std::round(height);

    LOGD("before bounding left %d, top %d, width %d, height %d",
         crop_left, crop_top, crop_width, crop_height);
    boundToSize(crop_left, crop_top, crop_width, crop_height,
            to_sensor ? mSensorW : mActiveArrayW,
            to_sensor ? mSensorH : mActiveArrayH);
    LOGD("after bounding left %d, top %d, width %d, height %d",
         crop_left, crop_top, crop_width, crop_height);
}

/*===========================================================================
 * FUNCTION   : applyZoomRatio
 *
 * DESCRIPTION: Apply zoom ratio to the crop region
 *
 * PARAMETERS :
 *   @crop_left   : x coordinate of top left corner of rectangle
 *   @crop_top    : y coordinate of top left corner of rectangle
 *   @crop_width  : width of rectangle
 *   @crop_height : height of rectangle
 *   @zoom_ratio  : zoom ratio to be applied to the input rectangles
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera3CropRegionMapper::applyZoomRatio(int32_t& crop_left, int32_t& crop_top,
        int32_t& crop_width, int32_t& crop_height, float zoom_ratio)
{
    if (mActiveArrayW == 0 || mActiveArrayH == 0) {
        LOGE("active array sizes are not initialized!");
        return;
    }

    applyZoomRatioHelper(crop_left, crop_top, crop_width, crop_height, zoom_ratio,
        false /*to_sensor*/);
}

/*===========================================================================
 * FUNCTION   : boundToSize
 *
 * DESCRIPTION: Bound a particular rectangle inside a bounding box
 *
 * PARAMETERS :
 *   @left    : x coordinate of top left corner of rectangle
 *   @top     : y coordinate of top left corner of rectangle
 *   @width   : width of rectangle
 *   @height  : height of rectangle
 *   @bound_w : width of bounding box
 *   @bound_y : height of bounding box
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera3CropRegionMapper::boundToSize(int32_t& left, int32_t& top,
            int32_t& width, int32_t& height, int32_t bound_w, int32_t bound_h)
{
    if (left < 0) {
        left = 0;
    }
    if (left >= bound_w) {
        left = bound_w - 1;
    }
    if (top < 0) {
        top = 0;
    }
    if (top >= bound_h) {
        top = bound_h - 1;
    }

    if ((left + width) > bound_w) {
        width = bound_w - left;
    }
    if ((top + height) > bound_h) {
        height = bound_h - top;
    }
}

/*===========================================================================
 * FUNCTION   : toActiveArray
 *
 * DESCRIPTION: Map co-ordinate from sensor output space to active array space
 *
 * PARAMETERS :
 *   @x   : x coordinate
 *   @y   : y coordinate
 *   @zoom_ratio  : zoom ratio to be applied to the input coordinates
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera3CropRegionMapper::toActiveArray(uint32_t& x, uint32_t& y, float zoom_ratio)
{
    if (mSensorW == 0 || mSensorH == 0 ||
            mActiveArrayW == 0 || mActiveArrayH == 0) {
        LOGE("sensor/active array sizes are not initialized!");
        return;
    }
    if ((x > static_cast<uint32_t>(mSensorW)) ||
            (y > static_cast<uint32_t>(mSensorH))) {
        LOGE("invalid co-ordinate (%d, %d) in (0, 0, %d, %d) space",
                 x, y, mSensorW, mSensorH);
        return;
    }
    if (zoom_ratio < MIN_ZOOM_RATIO) {
        LOGE("Invalid zoom ratio %f", zoom_ratio);
        return;
    }

    // Map back to activeArray space
    x = x * mActiveArrayW / mSensorW;
    y = y * mActiveArrayH / mSensorH;

    // Revert zoom_ratio, so that now the crop rectangle is separate from
    // zoom_ratio. The coordinate is within the active array space which covers
    // the post-zoom FOV.
    x = x * zoom_ratio - (zoom_ratio - 1) * 0.5f * mActiveArrayW;
    y = y * zoom_ratio - (zoom_ratio - 1) * 0.5f * mActiveArrayH;
}

/*===========================================================================
 * FUNCTION   : toSensor
 *
 * DESCRIPTION: Map co-ordinate from active array space to sensor output space
 *
 * PARAMETERS :
 *   @x   : x coordinate
 *   @y   : y coordinate
 *   @zoom_ratio  : zoom ratio to be applied to the input coordinates
 *
 * RETURN     : none
 *==========================================================================*/

void QCamera3CropRegionMapper::toSensor(uint32_t& x, uint32_t& y, float zoom_ratio)
{
    if (mSensorW == 0 || mSensorH == 0 ||
            mActiveArrayW == 0 || mActiveArrayH == 0) {
        LOGE("sensor/active array sizes are not initialized!");
        return;
    }
    if ((x > static_cast<uint32_t>(mActiveArrayW)) ||
            (y > static_cast<uint32_t>(mActiveArrayH))) {
        LOGE("invalid co-ordinate (%d, %d) in (0, 0, %d, %d) space",
                 x, y, mSensorW, mSensorH);
        return;
    }
    if (zoom_ratio < MIN_ZOOM_RATIO) {
        LOGE("Invalid zoom ratio %f", zoom_ratio);
        return;
    }

    // Apply zoom_ratio to the input coordinate in activeArray space, so that
    // coordinate already takes zoom_ratio into account (in other words,
    // coordinate within the sensor native active array space).
    x = x / zoom_ratio +
            0.5f * mActiveArrayW * (1.0f - 1.0f / zoom_ratio);
    y = y / zoom_ratio +
            0.5f * mActiveArrayH * (1.0f - 1.0f / zoom_ratio);

    // Map to sensor space.
    x = x * mSensorW / mActiveArrayW;
    y = y * mSensorH / mActiveArrayH;
}

}; //end namespace android
