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

#ifndef CAMERA3_HAL_MEDIAENTITY_H_
#define CAMERA3_HAL_MEDIAENTITY_H_

#include "v4l2device.h"
#include <vector>
#include <linux/media.h>

NAMESPACE_DECLARATION {
enum V4L2DeviceType {
        DEVICE_VIDEO,   /*!< MEDIA_ENT_T_DEVNODE_V4L */
        SUBDEV_GENERIC, /*!< MEDIA_ENT_T_V4L2_SUBDEV */
        SUBDEV_SENSOR,  /*!< MEDIA_ENT_T_V4L2_SUBDEV_SENSOR */
        SUBDEV_FLASH,   /*!< MEDIA_ENT_T_V4L2_SUBDEV_FLASH */
        SUBDEV_LENS,    /*!< MEDIA_ENT_T_V4L2_SUBDEV_LENS */
        UNKNOWN_TYPE
    };

/**
 * \class MediaEntity
 *
 * This class models a media entity, which is a basic media hardware or software
 * building block (e.g. sensor, scaler, CSI-2 receiver).
 *
 * Each media entity has one or more pads and links. A pad is a connection endpoint
 * through which an entity can interact with other entities. Data produced by an entity
 * flows from the entity's output to one or more entity inputs. A link is a connection
 * between two pads, either on the same entity or on different entities. Data flows
 * from a source pad to a sink pad.
 */
class MediaEntity
{
public:
    MediaEntity(struct media_entity_desc &entity, struct media_link_desc *links,
                    struct media_pad_desc *pads);
    ~MediaEntity();

    status_t getDevice(std::shared_ptr<V4L2DeviceBase> &device);
    void updateLinks(const struct media_link_desc *links);
    V4L2DeviceType getType();
    void getLinkDesc(std::vector<media_link_desc>  &links) const { links = mLinks; }
    void getEntityDesc(struct media_entity_desc &entity) const { entity = mInfo; }
    void getPadDesc(struct media_pad_desc &pad, int index) const { pad = mPads[index]; }
    const char* getName() const { return mInfo.name; }

private:
    status_t openDevice(std::shared_ptr<V4L2DeviceBase> &device);

private:
    struct media_entity_desc        mInfo;       /*!< media entity descriptor info */
    std::vector<struct media_link_desc>  mLinks;      /*!< media entity links */
    std::vector<struct media_pad_desc>   mPads;       /*!< media entity pads */

    std::shared_ptr<V4L2DeviceBase>              mDevice;     /*!< V4L2 video node or subdevice */

}; // class MediaEntity

} NAMESPACE_DECLARATION_END
#endif
