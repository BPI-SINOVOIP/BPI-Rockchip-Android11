# Copyright (C) 2017 Intel Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

PSLSRC = \
    psl/rkisp1/GraphConfigManager.cpp \
    psl/rkisp1/PSLConfParser.cpp \
    psl/rkisp1/RKISP1CameraCapInfo.cpp \
    psl/rkisp1/GraphConfig.cpp \
    psl/rkisp1/RKISP1Common.cpp \
    psl/rkisp1/RKISP1CameraHw.cpp \
    psl/rkisp1/ControlUnit.cpp \
    psl/rkisp1/ImguUnit.cpp \
    psl/rkisp1/SettingsProcessor.cpp \
    psl/rkisp1/Metadata.cpp \
    psl/rkisp1/tasks/ExecuteTaskBase.cpp \
    psl/rkisp1/tasks/ITaskEventSource.cpp \
    psl/rkisp1/tasks/ICaptureEventSource.cpp \
    psl/rkisp1/tasks/ITaskEventListener.cpp \
    psl/rkisp1/tasks/JpegEncodeTask.cpp \
    psl/rkisp1/workers/FrameWorker.cpp \
    psl/rkisp1/workers/OutputFrameWorker.cpp \
    psl/rkisp1/workers/InputFrameWorker.cpp \
    psl/rkisp1/workers/PostProcessPipeline.cpp \
    psl/rkisp1/MediaCtlHelper.cpp \
    psl/rkisp1/tunetool/TuningServer.cpp \
    common/platformdata/gc/FormatUtils.cpp \
    psl/rkisp1/RkCtrlLoop.cpp \
    psl/CameraBuffer.cpp \
    psl/HwStreamBase.cpp \
    psl/TaskThreadBase.cpp \
    psl/NodeTypes.cpp \
    psl/RgaCropScale.cpp


STRICTED_CPPFLAGS := \
                    -Wno-unused-parameter \
                    -Wno-sign-compare -std=c++11 \
                    -Wall -Wno-unused-function -Wno-unused-value \
                    -fstack-protector -fPIE -fPIC -D_FORTIFY_SOURCE=2 \
                    -Wformat -Wformat-security

PSLCPPFLAGS = \
    $(STRICTED_CPPFLAGS) \
    -I$(LOCAL_PATH)/common/platformdata/metadataAutoGen/6.0.1 \
    -I$(LOCAL_PATH)/psl \
    -I$(LOCAL_PATH)/psl/rkisp1 \
    -I$(LOCAL_PATH)/psl/rkisp1/tunetool \
    -I$(LOCAL_PATH)/include \
    -I$(LOCAL_PATH)/include/ia_imaging \
    -I$(LOCAL_PATH)/include/rk_imaging \
    -DCAMERA_RKISP1_SUPPORT \
    -DHAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_RK=0x10F
