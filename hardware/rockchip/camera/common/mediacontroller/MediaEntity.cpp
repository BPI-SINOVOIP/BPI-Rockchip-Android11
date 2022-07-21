/*
 * Copyright (C) 2014-2017 Intel Corporation
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
#define LOG_TAG "MediaEntity"

#include <unistd.h>
#include <sstream>
#include "MediaEntity.h"
#include "LogHelper.h"

NAMESPACE_DECLARATION {
MediaEntity::MediaEntity(struct media_entity_desc &entity, struct media_link_desc *links,
                         struct media_pad_desc *pads) :
    mInfo(entity),
    mDevice(nullptr)
{
    LOGI("@%s: %s, id: %d", __FUNCTION__, entity.name, entity.id);
    mLinks.reserve(entity.links);
    mPads.reserve(entity.pads);
    mLinks.clear();
    mPads.clear();

    if (links != nullptr) {
        for (int i = 0; i < entity.links; i++) {
            LOGI("link %d: src entity %d: %d --> sink entity %d:%d (%s%s%s)", i,
                links[i].source.entity, links[i].source.index,
                links[i].sink.entity, links[i].sink.index,
                (links[i].flags & MEDIA_LNK_FL_ENABLED)?"enabled":"disabled",
                (links[i].flags & MEDIA_LNK_FL_IMMUTABLE)?" immutable":"",
                (links[i].flags & MEDIA_LNK_FL_DYNAMIC)?" dynamic":"");
            mLinks.push_back(links[i]);
        }
    }

    if (pads != nullptr) {
        for (int i = 0; i < entity.pads; i++) {
            LOGI("pad %d (%s)", pads[i].index, (pads[i].flags & MEDIA_PAD_FL_SINK)?"SINK":"SOURCE");
            mPads.push_back(pads[i]);
        }
    }
}

MediaEntity::~MediaEntity()
{
    LOGI("@%s", __FUNCTION__);
    CLEAR(mInfo);
    mLinks.clear();
    mPads.clear();

    if (mDevice != nullptr) {
        if (mDevice->isOpen())
            mDevice->close();
        mDevice.reset();
        mDevice = nullptr;
    }
}

status_t MediaEntity::getDevice(std::shared_ptr<V4L2DeviceBase> &device)
{
    LOGI("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    if (mDevice == nullptr || !mDevice->isOpen()) {
        LOGI("Opening device");
        status = openDevice(mDevice);
    }
    if (status == NO_ERROR)
        device = mDevice;
    else
        device = nullptr;

    return status;
}

status_t MediaEntity::openDevice(std::shared_ptr<V4L2DeviceBase> &device)
{
    LOGI("@%s", __FUNCTION__);
    status_t status = UNKNOWN_ERROR;
    int ret = 0;
    char sysname[1024];
    int major = mInfo.v4l.major;
    int minor = mInfo.v4l.minor;
    std::ostringstream stringStream;
    stringStream << "/sys/dev/char/" << major << ":" << minor;
    std::string devNameStr = stringStream.str();
    ret = readlink(devNameStr.c_str(), sysname, sizeof(sysname));
    if (ret < 0) {
        LOGE("Unable to find device node");
    } else {
        sysname[ret] = 0;
        char *lastSlash = strrchr(sysname, '/');
        if (lastSlash == nullptr) {
            LOGE("Invalid sysfs device path");
            return status;
        }
        stringStream.str("");
        stringStream << "/dev/" << lastSlash + 1;
        devNameStr = stringStream.str();
        const char *devname = devNameStr.c_str();
        LOGI("Device node : %s", devname);

        device.reset();
        if (mInfo.type == MEDIA_ENT_T_DEVNODE_V4L) {
            device = std::make_shared<V4L2VideoNode>(devname);
        } else {
            device = std::make_shared<V4L2Subdevice>(devname);
        }
        status = device->open();
        if (status != NO_ERROR) {
            LOGE("Failed to open device %s", devname);
            device.reset();
        }
    }
    return status;
}

void MediaEntity::updateLinks(const struct media_link_desc *links)
{
    LOGI("@%s", __FUNCTION__);

    mLinks.clear();
    LOGI("entity name: %s, id: %d, pads: %d, links: %d",
          mInfo.name, mInfo.id, mInfo.pads, mInfo.links);
    for (int i = 0; i < mInfo.links; i++) {
        LOGI("link %d: pad %d --> sink entity %d:%d (%s%s%s)", i, links[i].source.index,
            links[i].sink.entity, links[i].sink.index, (links[i].flags & MEDIA_LNK_FL_ENABLED)?"enabled":"disabled",
            (links[i].flags & MEDIA_LNK_FL_IMMUTABLE)?" immutable":"", (links[i].flags & MEDIA_LNK_FL_DYNAMIC)?" dynamic":"");

        mLinks.push_back(links[i]);
    }
}

V4L2DeviceType MediaEntity::getType()
{
    LOGI("@%s", __FUNCTION__);

    switch (mInfo.type) {
    case MEDIA_ENT_T_DEVNODE_V4L:
        return DEVICE_VIDEO;
        break;
    case MEDIA_ENT_T_V4L2_SUBDEV:
        return SUBDEV_GENERIC;
        break;
    case MEDIA_ENT_T_V4L2_SUBDEV_SENSOR:
        return SUBDEV_SENSOR;
        break;
    case MEDIA_ENT_T_V4L2_SUBDEV_FLASH:
        return SUBDEV_FLASH;
        break;
    case MEDIA_ENT_T_V4L2_SUBDEV_LENS:
        return SUBDEV_LENS;
        break;
    default:
        LOGE("Unknown media entity type: %d", mInfo.type);
        return UNKNOWN_TYPE;
    }
}
} NAMESPACE_DECLARATION_END

