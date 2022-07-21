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

#ifndef CAMERA3_HAL_MEDIACONTROLLER_H_
#define CAMERA3_HAL_MEDIACONTROLLER_H_

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <linux/media.h>
#include <utils/Errors.h>
#include "MediaCtlPipeConfig.h"

NAMESPACE_DECLARATION {
class MediaEntity;

/**
 * \class MediaController
 *
 * This class is used for discovering and configuring the internal topology
 * of a media device. Devices are modelled as an oriented graph of building
 * blocks called media entities. The media entities are connected to each other
 * through pads.
 *
 * Each media entity corresponds to a V4L2 subdevice. This class is also used
 * for configuring the V4L2 subdevices.
 */
class MediaController
{

public:
    explicit MediaController(const char *path);
    ~MediaController();

    status_t init();

    status_t getMediaEntity(std::shared_ptr<MediaEntity> &entity, const char* name);
    status_t resetLinks();
    status_t configureLink(const MediaCtlLinkParams &linkParams);
    status_t setFormat(const MediaCtlFormatParams &formatParams);
    status_t setSelection(const char* entityName, int pad, int target, int top, int left, int width, int height);
    status_t setControl(const char* entityName, int controlId, int value, const char *controlName);
    status_t getSinkNamesForEntity(std::shared_ptr<MediaEntity> mediaEntity, std::vector<std::string> &names);
    status_t getMediaDevInfo(media_device_info &info);
    status_t enqueueMediaRequest(uint32_t mediaRequestId);
    status_t findMediaEntityById(int index, struct media_entity_desc &mediaEntityDesc);
    status_t findMediaEntityByName(char* name, struct media_entity_desc &mediaEntityDesc);
private:
    status_t open();
    status_t close();
    int xioctl(int request, void *arg) const;

    status_t getDeviceInfo();
    status_t findEntities();
    status_t getEntityNameForId(unsigned int entityId, std::string &entityName) const;

    status_t enumLinks(struct media_links_enum &linkInfo);
    status_t setupLink(struct media_link_desc &linkDesc);

private:
    std::string  mPath;     /*!< path to device in file system, ex: /dev/media0 */
    int          mFd;       /*!< file descriptor obtained when device is open */

    media_device_info                               mDeviceInfo;        /*!< media controller device info */
    /*!< media entity descriptors, Key: entity name */
    std::map<std::string, struct media_entity_desc> mEntityDesciptors;
    std::map<uint32_t, struct media_entity_desc> mEntityIdDesciptors;
    /*!< MediaEntities, Key: entity name */
    std::map<std::string, std::shared_ptr<MediaEntity>>  mEntities;

}; // class MediaController

} NAMESPACE_DECLARATION_END
#endif
