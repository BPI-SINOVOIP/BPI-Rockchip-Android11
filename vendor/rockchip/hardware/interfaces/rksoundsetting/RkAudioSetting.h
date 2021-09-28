/*
 * Copyright 2018 Rockchip Electronics Co. LTD
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

#ifndef RKAUDIOSETTING_RKAUDIOSETTING_H_
#define RKAUDIOSETTING_RKAUDIOSETTING_H_

#include "RkAudioSettingInterface.h"

namespace android {

class RkAudioSetting {
 public :
    RkAudioSetting();
    virtual ~RkAudioSetting();
    virtual void setSelect(int device);
    virtual void setFormat(int device, int close, const char * format);
    virtual void setMode(int device, int mode);

    virtual int getSelect(int device);
    virtual int getMode(int device);
    virtual int getFormat(int device, const char *format);
    virtual void updataFormatForEdid();

 protected:
    RkAudioSettingInterface  *mAudioSetting;
    bool  mXMLReady;
};

}  // namespace android
#endif  // RKAUDIOSETTING_V1_0_DEFAULT_RKAUDIOSETTING_H
