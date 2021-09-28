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
 * date: 2019/06/28
 * module: RKAudioSetting.
 */

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "RkAudioSettingManager"

#ifdef DEBUG_FLAG
#undef DEBUG_FLAG
#endif
#define DEBUG_FLAG 0x0

#include "RkAudioSettingManager.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "tinystr.h"

namespace android {

#define RK_AUDIO_SETTING_CONFIG_FILE "/data/system/rt_audio_config.xml"
#define RK_AUDIO_SETTING_TEMP_FILE "/data/system/rt_audio_config_temp.xml"
#define RK_AUDIO_SETTING_SYSTEM_FILE "/system/etc/rt_audio_config.xml"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#endif

struct supportHdmiLevel {
    int hdmi_level;
    const char *value;
};

const struct supportHdmiLevel mSupportHdmiLevel[] = {
    {HDMI_AUDIO_AC3, "AC3"},
    {HDMI_AUDIO_E_AC3, "EAC3"},
    {HDMI_AUDIO_DTS, "DTS"},
    {HDMI_AUDIO_DTS_HD, "DTSHD"},
    {HDMI_AUDIO_MLP, "TRUEHD"},
    {HDMI_AUDIO_MLP, "MLP"},
};

RkAudioSettingManager::RkAudioSettingManager()
    : XMLDoc(NULL) {
    XMLDoc = new TiXmlDocument();
}

RkAudioSettingManager::~RkAudioSettingManager() {
    if (XMLDoc) {
        delete XMLDoc;
        XMLDoc = NULL;
    }
}

int RkAudioSettingManager::init() {
    int err = 0;
    if (access(RK_AUDIO_SETTING_CONFIG_FILE, F_OK) < 0) {

        if (access(RK_AUDIO_SETTING_TEMP_FILE, F_OK) >= 0) {
            remove(RK_AUDIO_SETTING_TEMP_FILE);
        }

        if (access(RK_AUDIO_SETTING_SYSTEM_FILE, F_OK) == 0) {
            FILE *fin = NULL;
            FILE *fout = NULL;
            char *buff = NULL;
            buff = (char *)malloc(1024);
            fin = fopen(RK_AUDIO_SETTING_SYSTEM_FILE, "r");
            fout = fopen(RK_AUDIO_SETTING_TEMP_FILE, "w");

            if (fout == NULL) {
                ALOGD("%s,%d, fdout is open fail",__FUNCTION__,__LINE__);
            }

            if (fin == NULL) {
                ALOGD("%s,%d, fin is open fail",__FUNCTION__,__LINE__);
            }
            if(fout && fin){
                while (1) {
                    int ret = fread(buff, 1, 1024, fin);

                    if (ret != 1024) {
                        fwrite(buff, ret, 1, fout);
                    } else {
                        fwrite(buff, 1024, 1, fout);
                    }
                    if (feof(fin))
                        break;
                }
            }
            if (fout != NULL) {
                fclose(fout);
                fout = NULL;
            }

            if (fin != NULL) {
                fclose(fin);
                fin = NULL;
            }

            if (buff) {
                free(buff);
                buff = NULL;
            }

            if (-1 == rename(RK_AUDIO_SETTING_TEMP_FILE, RK_AUDIO_SETTING_CONFIG_FILE)) {
                ALOGD("rename config file failed");
                remove(RK_AUDIO_SETTING_TEMP_FILE);
            } else {
                ALOGD("rename config file success");
            }
            sync();
        }
    }
    chmod(RK_AUDIO_SETTING_CONFIG_FILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    const char *errorStr = NULL;

    ALOGD("load XML file(%s)", RK_AUDIO_SETTING_CONFIG_FILE);
    if (access(RK_AUDIO_SETTING_CONFIG_FILE, F_OK) >= 0) {
        if (!XMLDoc->LoadFile(RK_AUDIO_SETTING_CONFIG_FILE)) {
            errorStr = XMLDoc->ErrorDesc();
            ALOGD("load XML file error(%s)", errorStr);
            remove(RK_AUDIO_SETTING_CONFIG_FILE);
            err = -1;
        } else {
            err = 0;
        }
    } else {
        ALOGD("not find XML file %s", RK_AUDIO_SETTING_CONFIG_FILE);
        err = -1;
    }
    return err;
}

int RkAudioSettingManager::getSelectValue(TiXmlElement *elem) {
    int ret = 0;
    TiXmlAttribute *mAttri = elem->FirstAttribute();
    if (mAttri) {
        if (strcmp(mAttri->Value(), "yes") == 0) {
            ret = 1;
        } else if (strcmp(mAttri->Value(), "no") == 0) {
            ret = 0;
        }
    }
    return ret;
}

void RkAudioSettingManager::setSelectValue(TiXmlElement *elem, const char *ch) {
    TiXmlAttribute *mAttri = elem->FirstAttribute();
    if (mAttri) {
        mAttri->SetValue(ch);
    }
}

int RkAudioSettingManager::setModeValue(TiXmlNode *node, const char *ch) {
    TiXmlElement *elem = node->ToElement();
    if (elem != NULL) {
        elem->Clear();
        TiXmlText *pValue = new TiXmlText(ch);
        elem->LinkEndChild(pValue);
    }
    return 1;
}

void RkAudioSettingManager::addFormatNode(int port, const char *format) {
    int ret = 0;
    TiXmlElement *addEle = NULL;
    const char *errorStr = NULL;
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        if (port == SELECT_DECODE) {
            addEle = mRoot->FirstChildElement("decode");
        } else if(port == SELECT_BITSTREAM) {
            addEle = mRoot->FirstChildElement("bitstream");
        }

        TiXmlNode *mNodeFormat = NULL;
        TiXmlNode *parent = NULL;
        parent=addEle->FirstChild("formats");

        mNodeFormat = addEle->FirstChild("formats")->FirstChild("format");
        TiXmlElement *pNewNode = new TiXmlElement("format");

        if (pNewNode == NULL) {
            ALOGD("addFormatNode create fail!");
            return;
        }

        TiXmlText *pNewValue = new TiXmlText(format);
        pNewNode->LinkEndChild(pNewValue);

        parent->ToElement()->InsertEndChild(*pNewNode);

        if (!XMLDoc->SaveFile()) {
            errorStr = XMLDoc->ErrorDesc();
            ALOGD("save XML file error(%s)", errorStr);
        }
    } else {
        ALOGD("addFormatNode value fail!");
        ret = -1;
    }
}

void RkAudioSettingManager::delectFormatNode(int port, const char *format) {
    int ret = 0;
    TiXmlElement *deleEle = NULL;
    const char *errorStr = NULL;
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        if (port == SELECT_DECODE) {
            deleEle = mRoot->FirstChildElement("decode");
        } else if(port == SELECT_BITSTREAM) {
            deleEle = mRoot->FirstChildElement("bitstream");
        }

        TiXmlNode *mNodeFormat = deleEle->FirstChild("formats")->FirstChild("format");

        while (mNodeFormat) {
            ALOGV("delectFormatNode read format--> %s", mNodeFormat->FirstChild()->Value());
            if (strcmp(mNodeFormat->FirstChild()->Value(), format) == 0) {
                TiXmlElement* pParentEle = mNodeFormat->Parent()->ToElement();
                if (pParentEle->RemoveChild(mNodeFormat)) {
                    if (!XMLDoc->SaveFile()) {
                        errorStr = XMLDoc->ErrorDesc();
                        ALOGD("save XML file error(%s)", errorStr);
                    }
                    break;
                } else {
                    ALOGD("delectFormatNode fail!");
                }
            }

            mNodeFormat = mNodeFormat->NextSibling();
        }

    } else {
        ALOGD("delectFormatNode value fail!");
        ret = -1;
    }
}

void RkAudioSettingManager::addDeviceNode(int port, const char *device) {
    int ret = 0;
    TiXmlElement *addEle = NULL;
    const char *errorStr = NULL;
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        if (port == SELECT_DECODE) {
            addEle = mRoot->FirstChildElement("decode");
        } else if(port == SELECT_BITSTREAM) {
            addEle = mRoot->FirstChildElement("bitstream");
        }
        TiXmlNode *mNodeFormat = NULL;
        TiXmlNode *parent = NULL;
        parent = addEle->FirstChild("devices");
        mNodeFormat = addEle->FirstChild("devices")->FirstChild("device");

        TiXmlElement *pNewNode = new TiXmlElement("device");

        if (pNewNode == NULL) {
            ALOGD("addDeviceNode create fail!");
            return;
        }

        TiXmlText *pNewValue = new TiXmlText(device);
        pNewNode->LinkEndChild(pNewValue);

        parent->ToElement()->InsertEndChild(*pNewNode);

        if (!XMLDoc->SaveFile()) {
            errorStr = XMLDoc->ErrorDesc();
            ALOGD("save XML file error(%s)", errorStr);
        }
    } else {
        ALOGD("addDeviceNode value fail!");
        ret = -1;
    }
}

void RkAudioSettingManager::delectDeviceNode(int port, const char *device) {
    int ret = 0;
    TiXmlElement *deleEle = NULL;
    const char *errorStr = NULL;
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        if (port == SELECT_DECODE) {
            deleEle = mRoot->FirstChildElement("decode");
        } else if(port == SELECT_BITSTREAM) {
            deleEle = mRoot->FirstChildElement("bitstream");
        }

        TiXmlNode *mNodeFormat = deleEle->FirstChild("devices")->FirstChild("device");

        while (mNodeFormat) {
            ALOGD("delectFormatNode read format--> %s", mNodeFormat->FirstChild()->Value());
            if (strcmp(mNodeFormat->FirstChild()->Value(), device) == 0) {
                TiXmlElement* pParentEle = mNodeFormat->Parent()->ToElement();
                if (pParentEle->RemoveChild(mNodeFormat)) {
                    if (!XMLDoc->SaveFile()) {
                        errorStr = XMLDoc->ErrorDesc();
                        ALOGD("save XML file error(%s)", errorStr);
                    }
                    break;
                } else {
                    ALOGD("delectDeviceNode fail!");
                }
            }

            mNodeFormat = mNodeFormat->NextSibling();
        }

    } else {
        ALOGD("delectDeviceNode value fail!");
        ret = -1;
    }
}

void RkAudioSettingManager::setFormats(int port, int cmd, const char *format) {
    switch(cmd) {
     case ADD_NODE:
        addFormatNode(port, format);
        break;
     case DELECT_NODE:
        delectFormatNode(port, format);
        break;
     default:
        ALOGD("cannot setFormats");
        break;
    }
}

void RkAudioSettingManager::setDevices(int port, int cmd, const char *device) {
    switch(cmd) {
     case ADD_NODE:
        addDeviceNode(port, device);
        break;
     case DELECT_NODE:
        delectDeviceNode(port, device);
        break;
     default:
        ALOGD("cannot setDevices");
        break;
    }
}

// apk interface
/* query  0: decode, 1: hdmi bitstream, 2: spdif passthrough
  * return 1 : support, 0: unsupport
  */
int RkAudioSettingManager::getSelect(int device) {
    int ret = 0;
    ALOGV("%s %d : device = %d",__FUNCTION__,__LINE__,device);
    if (device == 0) {
        ret = getAudioSettingSelect(0);
    } else if (device == 1) {
        if (getAudioSettingSelect(1) == 1) {
            ret = getAudioSettingBitstreamDevice("hdmi");
        } else {
            ret = 0;
        }
    } else if (device == 2) {
        if (getAudioSettingSelect(1) == 1) {
            ret = getAudioSettingBitstreamDevice("spdif");
        } else {
            ret = 0;
        }
    }
    return ret;
}

/*
 * 0->decode
 * 1->hdmi bitstream
 * 2->spdif passthrough
 */
void RkAudioSettingManager::setSelect(int device) {
    ALOGV("%s %d : device = %d",__FUNCTION__,__LINE__,device);
    if (device == 0) {
        setAudioSettingSelect(0);
    } else if(device == 1) {
        setAudioSettingSelect(1);
        if (getAudioSettingBitstreamDevice("hdmi") != 1) {
            setAudioSettingBitstreamDevice("hdmi");
        }
        if(getAudioSettingBitstreamFormat("MLP") == 0){
            setAudioSettingBitstreamFormat(0, "MLP");
        }
    } else if(device == 2) {
        setAudioSettingSelect(1);
        if (getAudioSettingBitstreamDevice("spdif") != 1) {
            setAudioSettingBitstreamDevice("spdif");
        }
        if(getAudioSettingBitstreamFormat("MLP") == 1){
            setAudioSettingBitstreamFormat(1, "MLP");
        }
    } else {
        ALOGD("connot setSelect  error device = %d", device);
    }
}

/*
 * param : device : 0 ->decode, 1->hdmi bitstream, 2->spdif passthrough
 *             close: 0-> add, 1-> delect
 *             format : codec format
 * return void
 */
void RkAudioSettingManager::setFormat(int device, int close, const char *format) {
    ALOGV("%s,%d, device = %d, close = %d, format = %s", __FUNCTION__,__LINE__,device,close,format);
    if (device == 0) {
        setAudioSettingDecodeFormat(close, format);
    } else if(device == 1) {
        setAudioSettingBitstreamFormat(close, format);
    } else if(device == 2) {
        setAudioSettingBitstreamFormat(close, format);
    }
}

/*
 * param : device  : 0 ->decode, 1->hdmi bitstream, 2->spdif passthrough
 *             format :   codec format
 * return : 1 : support, 0: unsupport
 */
int RkAudioSettingManager::getFormat(int device, const char *format) {
    int ret = 0;
    ALOGV("%s,%d, device = %d format = %s", __FUNCTION__,__LINE__,device,format);
    if (device == 0) {
        ret = getAudioSettingDecodeFormat(format);
    } else if(device == 1) {
        ret = getAudioSettingBitstreamFormat(format);
    } else if(device == 2) {
        ret = getAudioSettingBitstreamFormat(format);
    }
    return ret;
}

/*
 * param : device : 0 ->decode, 1->hdmi bitstream
 *             mode  : 1 : manual, multi_pcm, 0: decode_pcm, auto
 * return void
 */
void RkAudioSettingManager::setMode(int device, int mode) {
    ALOGV("%s,%d, device = %d, mode = %d", __FUNCTION__,__LINE__,device,mode);
    if (device == 0) {
        setAudioSettingDecodeMode(mode);
    } else if(device == 1) {
        setAudioSettingBitStreamMode(mode);
    }
}

/*
 * param : device  : 0 ->decode, 1->hdmi bitstream
 *             mode :   mode
 * return : 1 : manual, multi_pcm, 0: decode_pcm, auto
 */
int RkAudioSettingManager::getMode(int device) {
    ALOGV("%s,%d, device = %d", __FUNCTION__,__LINE__,device);
    int ret = 0;
    if (device == 0) {
        ret = getAudioSettingDecodeMode();
    } else if(device == 1) {
        ret = getAudioSettingBitStreamMode();
    } else if(device == 2) {
        ret = getAudioSettingBitStreamMode();
    }
    return ret;
}

void RkAudioSettingManager::updataFormatForEdid() {
    updataFormatForAutoMode();
}

/* (1)  parse hdmi edid information HDMI_EDID_NODE, get hdmi support information of audio.
  * (2)  according to audio format of edid info which parsed, updata XML bitstream formats when
  * select "auto" mode.
  */
void RkAudioSettingManager::updataFormatForAutoMode() {
    struct hdmi_audio_infors hdmi_edid;
    int i = 0;
    init_hdmi_audio(&hdmi_edid);
    ALOGV("%s,%d",__FUNCTION__,__LINE__);
    // get the format can be bistream?
    if (parse_hdmi_audio(&hdmi_edid) >= 0) {
        for(i = 0; i < ARRAY_SIZE(mSupportHdmiLevel); i++) {
            if(is_support_format(&hdmi_edid, mSupportHdmiLevel[i].hdmi_level)) {
                int support = getAudioSettingBitstreamFormat(mSupportHdmiLevel[i].value);
                if (support == 0) {
                    setAudioSettingBitstreamFormat(0, mSupportHdmiLevel[i].value);
                }
            } else {
                int support = getAudioSettingBitstreamFormat(mSupportHdmiLevel[i].value);
                if (support == 1) {
                    setAudioSettingBitstreamFormat(1, mSupportHdmiLevel[i].value);
                }
            }
        }
    }

    destory_hdmi_audio(&hdmi_edid);
}

//----------------------------------------------------------
// getAudioSettingSelect :
// param--> 0 : decode, 1 : bitstream
// return--> 0 : "no",  1 : "yes", -1: fail
int RkAudioSettingManager::getAudioSettingSelect(int port) {
    int ret = 0;
    TiXmlElement *Ele = NULL;
    ALOGV("%s, %d,port=%d",__FUNCTION__,__LINE__,port);
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        if (port == 0) {
            Ele = mRoot->FirstChildElement("decode");
            return getSelectValue(Ele);
        } else if(port == 1) {
            Ele = mRoot->FirstChildElement("bitstream");
            return getSelectValue(Ele);
        }
    } else {
        ALOGD("getAudioSettingSelect value fail!");
        ret = -1;
    }
    return ret;
}

// setAudioSettingSelect :
// param--> 0 : decode, 1 : bitstream
// return--> 0 : success,  -1 : fail
void RkAudioSettingManager::setAudioSettingSelect(int port) {
    TiXmlElement *deEle = NULL;
    TiXmlElement *bsEle = NULL;
    const char *errorStr = NULL;
    ALOGV("%s, %d, port=%d",__FUNCTION__,__LINE__,port);
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        deEle = mRoot->FirstChildElement("decode");
        bsEle = mRoot->FirstChildElement("bitstream");
        if (port == 0) {
            setSelectValue(deEle, "yes");
            setSelectValue(bsEle, "no");
        } else if(port == 1) {
            setSelectValue(deEle, "no");
            setSelectValue(bsEle, "yes");
        }
    } else {
        ALOGD("setAudioSettingSelect value fail!");
        return;
    }

    if (!XMLDoc->SaveFile()) {
        errorStr = XMLDoc->ErrorDesc();
        ALOGD("save XML file error(%s)", errorStr);
    }
}

// getAudioSettingDecodeMode :
// return--> 0 : decode_pcm,  1 : multi_pcm, -1: fail
int RkAudioSettingManager::getAudioSettingDecodeMode() {
    int ret = 0;
    TiXmlElement *deEle = NULL;
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        deEle = mRoot->FirstChildElement("decode");
        TiXmlNode *mNodeMode = deEle->FirstChild("mode");

        if (mNodeMode->FirstChild()) {
            if(strcmp(mNodeMode->FirstChild()->Value(), "decode_pcm") == 0) {
                ALOGD("return getAudioSettingDecodeMode decode_pcm");
                return 0;
            }

            if(strcmp(mNodeMode->FirstChild()->Value(), "multi_pcm") == 0) {
                ALOGD("return getAudioSettingDecodeMode multi_pcm");
                return 1;
            }
        }
    } else {
        ALOGD("getAudioSettingDecodeMode value fail!");
        ret = -1;
    }
    return ret;
}

// setAudioSettingDecodeMode :
// param--> 0 : decode_pcm,    1 : multi_pcm,
void RkAudioSettingManager::setAudioSettingDecodeMode(int mode) {
    TiXmlElement *deEle = NULL;
    const char *errorStr = NULL;
    ALOGV("%s, %d,mode =%d",__FUNCTION__,__LINE__,mode);
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        deEle = mRoot->FirstChildElement("decode");
        TiXmlNode *mNodeMode = deEle->FirstChild("mode");

        if (mNodeMode->FirstChild()) {
            if (mode == 0) {
                setModeValue(mNodeMode, "decode_pcm");
            } else if(mode == 1) {
                setModeValue(mNodeMode, "multi_pcm");
            }
        }
    } else {
        ALOGD("setAudioSettingDecodeMode value fail!");
    }

    if (!XMLDoc->SaveFile()) {
        errorStr = XMLDoc->ErrorDesc();
        ALOGD("save XML file error(%s)", errorStr);
    }
}

// getAudioSettingBitStreamMode :
// return--> 0 : auto,  1 : manual, -1: fail
int RkAudioSettingManager::getAudioSettingBitStreamMode() {
    int ret = 0;
    TiXmlElement *bsEle = NULL;
    ALOGV("%s, %d",__FUNCTION__,__LINE__);
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        bsEle = mRoot->FirstChildElement("bitstream");
        TiXmlNode *mNodeMode = bsEle->FirstChild("mode");

        if (mNodeMode->FirstChild()) {
            if(strcmp(mNodeMode->FirstChild()->Value(), "auto") == 0) {
                ALOGV("test return getAudioSettingDecodeMode decode_pcm");
                return 0;
            }

            if(strcmp(mNodeMode->FirstChild()->Value(), "manual") == 0) {
                ALOGV("test return getAudioSettingDecodeMode multi_pcm");
                return 1;
            }
        }
    } else {
        ALOGD("getAudioSettingBitStreamMode value fail!");
        ret = -1;
    }
    ALOGV("%s, %d",__FUNCTION__,__LINE__);
    return ret;
}

// setAudioSettingBitStreamMode :
// param--> 0 : auto,  1 : manual
void RkAudioSettingManager::setAudioSettingBitStreamMode(int mode) {
    TiXmlElement *deEle = NULL;
    const char *errorStr = NULL;
    ALOGV("%s, %d, mode =%d",__FUNCTION__,__LINE__,mode);
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        deEle = mRoot->FirstChildElement("bitstream");
        TiXmlNode *mNodeMode = deEle->FirstChild("mode");

        if (mNodeMode->FirstChild()) {
            if (mode == 0) {
                setModeValue(mNodeMode, "auto");
            } else if(mode == 1) {
                setModeValue(mNodeMode, "manual");
            }
        }
    } else {
        ALOGD("setAudioSettingBitStreamMode value fail!");
    }

    if (!XMLDoc->SaveFile()) {
        errorStr = XMLDoc->ErrorDesc();
        ALOGD("save XML file error(%s)", errorStr);
    }
}

// getAudioSettingDecodeFormat :
// param--> format
// return--> 0 : not support,  1 : support, -1: fail
int RkAudioSettingManager::getAudioSettingDecodeFormat(const char *ch) {
    int ret = 0;
    TiXmlElement *deEle = NULL;
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        deEle = mRoot->FirstChildElement("decode");
        TiXmlNode *fNodeDevice = deEle->FirstChild("formats")->FirstChild("format");

        while (fNodeDevice) {
           if (fNodeDevice->FirstChild()) {
               ALOGV("getAudioSettingDecodeFormat format = %s, ch = %s",fNodeDevice->FirstChild()->Value(), ch);
               if (strcmp(fNodeDevice->FirstChild()->Value(), ch) == 0) {
                   return 1;
               }
           }

           fNodeDevice = fNodeDevice->NextSibling();
       }
    } else {
        ALOGD("getAudioSettingDecodeFormat value fail!");
        ret = -1;
    }
    ALOGV("%s, %d",__FUNCTION__,__LINE__);
    return ret;
}

// setAudioSettingDecodeFormat :
// param--> format
void RkAudioSettingManager::setAudioSettingDecodeFormat(int close, const char *ch) {
    if (close == 1) {
        setFormats(SELECT_DECODE, DELECT_NODE, ch);
    } else if(close == 0) {
        setFormats(SELECT_DECODE, ADD_NODE, ch);
    } else {
        ALOGD("cannot setAudioSettingDecodeFormat");
    }
}

// getAudioSettingBitstreamFormat :
// param--> format
// return--> 0 : not support,  1 : support, -1: fail
int RkAudioSettingManager::getAudioSettingBitstreamFormat(const char *ch) {
    int ret = 0;
    TiXmlElement *bsEle = NULL;
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        bsEle = mRoot->FirstChildElement("bitstream");
        TiXmlNode *fNodeDevice = bsEle->FirstChild("formats")->FirstChild("format");

        while (fNodeDevice) {
           if (fNodeDevice->FirstChild()) {
                ALOGD("getAudioSettingBitstreamFormat format = %s, ch = %s",fNodeDevice->FirstChild()->Value(), ch);
                if (strcmp(fNodeDevice->FirstChild()->Value(), ch) == 0) {
                    return 1;
                }
           }

           fNodeDevice = fNodeDevice->NextSibling();
       }
   } else {
       ALOGD("getAudioSettingBitstreamFormat value fail!");
       ret = -1;
   }
   return ret;
}

// setAudioSettingBitstreamFormat :
// param--> format
void RkAudioSettingManager::setAudioSettingBitstreamFormat(int close, const char *ch) {
    if (close == 1) {
        setFormats(SELECT_BITSTREAM, DELECT_NODE, ch);
    } else if(close == 0) {
        setFormats(SELECT_BITSTREAM, ADD_NODE, ch);
    } else {
        ALOGD("cannot setAudioSettingBitstreamFormat");
    }
}

// getAudioSettingDecodeDevice :
// param--> device
// return--> 0 : not support,  1 : support, -1: fail
int RkAudioSettingManager::getAudioSettingDecodeDevice(const char *device) {
    int ret = 0;
    TiXmlElement *bsEle = NULL;
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        bsEle = mRoot->FirstChildElement("decode");
        TiXmlNode *fNodeDevice = bsEle->FirstChild("devices")->FirstChild("device");

        while (fNodeDevice) {
           if (fNodeDevice->FirstChild()) {
               if (strcmp(fNodeDevice->FirstChild()->Value(), device) == 0) {
                   return 1;
               }
           }

           fNodeDevice = fNodeDevice->NextSibling();
        }
    } else {
        ALOGD("getAudioSettingDecodeDevice value fail!");
        ret = -1;
    }
    return ret;
}

// setAudioSettingDecodeDevice :
// param--> device
void RkAudioSettingManager::setAudioSettingDecodeDevice(int close, const char *device) {
    if (close == 1) {
        setDevices(SELECT_DECODE, DELECT_NODE, device);
    } else if(close == 0) {
        setDevices(SELECT_DECODE, ADD_NODE, device);
    } else {
        ALOGD("cannot setAudioSettingDecodeDevice");
    }
}

// getAudioSettingBitstreamDevice :
// param--> device
// return--> 0 : not support,  1 : support, -1: fail
int RkAudioSettingManager::getAudioSettingBitstreamDevice(const char *device) {
    int ret = 0;
    TiXmlElement *bsEle = NULL;
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        bsEle = mRoot->FirstChildElement("bitstream");
        TiXmlNode *fNodeDevice = bsEle->FirstChild("devices")->FirstChild("device");

        while (fNodeDevice) {
           if (fNodeDevice->FirstChild()) {
               if (strcmp(fNodeDevice->FirstChild()->Value(), device) == 0) {
                   return 1;
               }
           }

           fNodeDevice = fNodeDevice->NextSibling();
        }
    } else {
        ALOGD("getAudioSettingBitstreamDevice value fail!");
        ret = -1;
    }
    return ret;
}

// setAudioSettingDecodeDevice :
// param--> device
void RkAudioSettingManager::setAudioSettingBitstreamDevice(const char *device) {
    TiXmlElement *deEle = NULL;
    const char *errorStr = NULL;
    if (XMLDoc != NULL) {
        TiXmlElement *mRoot = XMLDoc->RootElement();
        deEle = mRoot->FirstChildElement("bitstream");
        TiXmlNode *mNodeDevice = deEle->FirstChild("devices")->FirstChild("device");

        if (mNodeDevice->FirstChild()) {
            TiXmlElement *elem = mNodeDevice->ToElement();
            if (elem != NULL) {
                elem->Clear();
                TiXmlText *pValue = new TiXmlText(device);
                elem->LinkEndChild(pValue);
            }
        }
    } else {
        ALOGD("setAudioSettingBitstreamDevice value fail!");
    }

    if (!XMLDoc->SaveFile()) {
        errorStr = XMLDoc->ErrorDesc();
        ALOGD("save XML file error(%s)", errorStr);
    }
}

}
