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
 * module: RkAudioSettingManager
 */

#ifndef RKSOUNDSETTING_RKAUDIOSETTINGMANAGER_H_
#define RKSOUNDSETTING_RKAUDIOSETTINGMANAGER_H_

#include "RkAudioSettingInterface.h"
#include "tinyxml.h"
#include "audio_hw_hdmi.h"

namespace android {

enum {
    SELECT_DECODE     = 0,
    SELECT_BITSTREAM  = 1,
    ADD_NODE      = 2,
    DELECT_NODE     = 3,
};

class RkAudioSettingManager : public RkAudioSettingInterface {
 public:
    RkAudioSettingManager();
    virtual ~RkAudioSettingManager();
    int init();
    int getSelect(int device);
    void setSelect(int device);
    void setFormat(int device, int close, const char *format);
    int getFormat(int device, const char *format);
    void setMode(int device, int mode);
    int getMode(int device);
    void updataFormatForEdid();

 protected:
    int getAudioSettingSelect(int port);
    void setAudioSettingSelect(int port);
    int getAudioSettingDecodeDevice(const char *device);
    void setAudioSettingDecodeDevice(int close, const char *device);
    int getAudioSettingBitstreamDevice(const char *device);
    void setAudioSettingBitstreamDevice(const char *device);
    int getAudioSettingDecodeMode();
    void setAudioSettingDecodeMode(int mode);
    int getAudioSettingBitStreamMode();
    void setAudioSettingBitStreamMode(int mode);
    int getAudioSettingDecodeFormat(const char *ch);
    void setAudioSettingDecodeFormat(int close, const char *ch);
    int getAudioSettingBitstreamFormat(const char *ch);
    void setAudioSettingBitstreamFormat(int close, const char *ch);
    int getSelectValue(TiXmlElement *elem);
    void setSelectValue(TiXmlElement *elem, const char *ch);
    int setModeValue(TiXmlNode *node, const char *ch);
    void addFormatNode(int port, const char *format);
    void delectFormatNode(int port, const char *format);
    void addDeviceNode(int port, const char *device);
    void delectDeviceNode(int port, const char *device);
    void setFormats(int port, int cmd, const char *format);
    void setDevices(int port, int cmd, const char *device);
    void updataFormatForAutoMode();
 private:
    TiXmlDocument *XMLDoc;
};

}

#endif  //  RKSOUNDSETTING_RKAUDIOSETTINGMANAGER_H_

