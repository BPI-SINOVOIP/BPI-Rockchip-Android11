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

PSLSRC += \
    psl/rkisp2/RKISP2GraphConfigManager.cpp \
    psl/rkisp2/RKISP2PSLConfParser.cpp \
    psl/rkisp2/RKISP2CameraCapInfo.cpp \
    psl/rkisp2/RKISP2CtrlLoop.cpp \
    psl/rkisp2/RKISP2GraphConfig.cpp \
    psl/rkisp2/RKISP2CameraHw.cpp \
    psl/rkisp2/RKISP2ControlUnit.cpp \
    psl/rkisp2/RKISP2ImguUnit.cpp \
    psl/rkisp2/RKISP2SettingsProcessor.cpp \
    psl/rkisp2/RKISP2Metadata.cpp \
    psl/rkisp2/tasks/RKISP2ExecuteTaskBase.cpp \
    psl/rkisp2/tasks/RKISP2ITaskEventSource.cpp \
    psl/rkisp2/tasks/RKISP2ICaptureEventSource.cpp \
    psl/rkisp2/tasks/RKISP2ITaskEventListener.cpp \
    psl/rkisp2/tasks/RKISP2JpegEncodeTask.cpp \
    psl/rkisp2/workers/RKISP2FrameWorker.cpp \
    psl/rkisp2/workers/RKISP2OutputFrameWorker.cpp \
    psl/rkisp2/workers/RKISP2InputFrameWorker.cpp \
    psl/rkisp2/workers/RKISP2PostProcessPipeline.cpp \
    psl/rkisp2/RKISP2MediaCtlHelper.cpp \
    common/platformdata/gc/FormatUtils.cpp \
    psl/rkisp1/RKISP1Common.cpp \
    psl/CameraBuffer.cpp \
    psl/HwStreamBase.cpp \
    psl/TaskThreadBase.cpp \
    psl/NodeTypes.cpp \
    psl/RgaCropScale.cpp

ifeq ($(PRODUCT_HAVE_EPTZ),true)
PSLSRC += \
	psl/rkisp2/RKISP2DevImpl.cpp
endif

PSLSRC += \
	psl/rkisp2/RKISP2FecUnit.cpp

STRICTED_CPPFLAGS := \
                    -Wno-unused-parameter \
                    -Wno-sign-compare -std=c++11 \
                    -Wall -Wno-unused-function -Wno-unused-value \
                    -fstack-protector -fPIE -fPIC -D_FORTIFY_SOURCE=2 \
                    -Wformat -Wformat-security

PSLCPPFLAGS += \
    $(STRICTED_CPPFLAGS) \
    -I$(LOCAL_PATH)/common/platformdata/metadataAutoGen/6.0.1 \
    -I$(LOCAL_PATH)/psl \
    -I$(LOCAL_PATH)/psl/rkisp1 \
    -I$(LOCAL_PATH)/psl/rkisp1/tunetool \
    -I$(LOCAL_PATH)/psl/rkisp1/tasks \
    -I$(LOCAL_PATH)/psl/rkisp2 \
    -I$(LOCAL_PATH)/include \
    -I$(LOCAL_PATH)/include/ia_imaging \
    -I$(LOCAL_PATH)/include/rk_imaging \
    -DCAMERA_RKISP2_SUPPORT \
    -DHAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_RK=0x10F
