/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#ifndef _CAMERA3_HAL_MEDIACTLPIPECONFIG_H_
#define _CAMERA3_HAL_MEDIACTLPIPECONFIG_H_

#include <vector>
#include <string>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

using std::string;

NAMESPACE_DECLARATION {
typedef struct {
    string name;
    string type;
    int isysNodeName;
} MediaCtlElement;

typedef struct {
    int outputWidth;
    int outputHeight;
    string name;
    int id;
} ConfigProperties;

typedef struct {
    int Width;
    int Height;
} FrameTimingCalcSize;

typedef struct {
    string srcName;
    int srcPad;
    string sinkName;
    int sinkPad;
    bool enable;
    int flags;
} MediaCtlLinkParams;

typedef struct {
    string entityName;
    int pad;
    int width;
    int height;
    int formatCode;
    int stride;
    int field;
    int quantization;
} MediaCtlFormatParams;

typedef struct {
    string entityName;
    int pad;
    int target;
    int top;
    int left;
    int width;
    int height;
} MediaCtlSelectionParams;

typedef struct {
    string entityName;
    struct v4l2_selection select;
} MediaCtlSelectionVideoParams;

typedef struct {
    string entityName;
    int controlId;
    int value;
    string controlName;
} MediaCtlControlParams;

typedef enum {
    MEDIACTL_PARAMS_TYPE_CTLSEL,
    MEDIACTL_PARAMS_TYPE_VIDSEL,
    MEDIACTL_PARAMS_TYPE_FMT,
    MEDIACTL_PARAMS_TYPE_CTL
} MediaCtlParamsType;

// media pipelines settings require specific order, this
// is used to record all the settings order of
// MediaCtlControlParams, MediaCtlSelectionVideoParams,
// MediaCtlFormatParams
typedef struct {
  MediaCtlParamsType type;
  size_t index;
} MediaCtlParamsOrder;

/**
 * \struct MediaCtlSingleConfig
 *
 * This struct is holding all the possible
 * media ctl pipe configurations for the
 * camera in use.
 * It is holding the commands parameters for
 * setting up a camera pipe.
 *
 */
typedef struct {
    ConfigProperties mCameraProps;
    FrameTimingCalcSize mFTCSize;
    std::vector<MediaCtlLinkParams> mLinkParams;
    std::vector<MediaCtlFormatParams> mFormatParams;
    std::vector<MediaCtlSelectionParams> mSelectionParams;
    std::vector<MediaCtlSelectionVideoParams> mSelectionVideoParams;
    std::vector<MediaCtlControlParams> mControlParams;
    std::vector<MediaCtlElement> mVideoNodes;
    std::vector<MediaCtlParamsOrder> mParamsOrder;
} MediaCtlConfig;

} NAMESPACE_DECLARATION_END
#endif  // _CAMERA3_HAL_MEDIACTLPIPECONFIG_H_
