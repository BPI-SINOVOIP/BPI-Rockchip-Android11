/*
 * Copyright 2019 Rockchip Electronics Co. LTD
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
 * author: shika.zhou@rock-chips.com
 * date: 2019/11/19
 * module: RkAudioSettingInterface
 */
#ifndef RKSOUNDSETTING_RKAUDIOSETTINGINTERFACE_H_
#define RKSOUNDSETTING_RKAUDIOSETTINGINTERFACE_H_

#include <sys/types.h>
#include <inttypes.h>
#include <android-base/logging.h>
#include <log/log.h>

namespace android {

class RkAudioSettingInterface {
 public:
    RkAudioSettingInterface() {};
    virtual ~RkAudioSettingInterface() {};
    virtual int init() = 0;
    virtual int getSelect(int device) = 0;
    virtual void setSelect(int device) = 0;
    virtual void setFormat(int device, int close, const char *format) = 0;
    virtual int getFormat(int device, const char *format) = 0;
    virtual void setMode(int device, int mode) = 0;
    virtual int getMode(int device) = 0;
    virtual void updataFormatForEdid() = 0;
};

}
#endif  //  RKSOUNDSETTING_RKAUDIOSETTINGINTERFACE_H
