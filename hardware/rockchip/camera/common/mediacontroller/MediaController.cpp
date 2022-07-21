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
#define LOG_TAG "MediaController"

#include "MediaController.h"
#include "MediaEntity.h"
#include "v4l2device.h"
#include "LogHelper.h"
#include "SysCall.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

NAMESPACE_DECLARATION {

MediaController::MediaController(const char *path) :
    mPath(path),
    mFd(-1)
{
    LOGI("@%s", __FUNCTION__);
    CLEAR(mDeviceInfo);
    mEntities.clear();
    mEntityDesciptors.clear();
    mEntityIdDesciptors.clear();
}

MediaController::~MediaController()
{
    LOGI("@%s", __FUNCTION__);
    mEntityDesciptors.clear();
    mEntityIdDesciptors.clear();
    mEntities.clear();
    close();
}

status_t MediaController::init()
{
    LOGI("@%s %s", __FUNCTION__, mPath.c_str());
    status_t status = NO_ERROR;

    status = open();
    if (status != NO_ERROR) {
        LOGE("Error opening media device");
        return status;
    }

    status = getDeviceInfo();
    if (status != NO_ERROR) {
        LOGE("Error getting media info");
        return status;
    }

    status = findEntities();
    if (status != NO_ERROR) {
        LOGE("Error finding media entities");
        return status;
    }

    return status;
}

status_t MediaController::open()
{
    LOGI("@%s %s", __FUNCTION__, mPath.c_str());
    status_t ret = NO_ERROR;
    struct stat st;

    if (mFd != -1) {
        LOGW("Trying to open a device already open");
        return NO_ERROR;
    }

    if (stat (mPath.c_str(), &st) == -1) {
        LOGE("Error stat media device %s: %s",
             mPath.c_str(), strerror(errno));
        return UNKNOWN_ERROR;
    }

    if (!S_ISCHR (st.st_mode)) {
        LOGE("%s is not a device", mPath.c_str());
        return UNKNOWN_ERROR;
    }

    mFd = SysCall::open(mPath.c_str(), O_RDWR);
    if (mFd < 0) {
        if (mFd == -EPERM) {
            // Return permission denied, to allow skipping this device.
            // Our HAL may not want to really use this device at all.
            ret = PERMISSION_DENIED;
        } else {
            LOGE("Error opening media device %s: %d (%s)",
                mPath.c_str(), mFd, strerror(errno));
            ret = UNKNOWN_ERROR;
        }
    }

    return ret;
}

status_t MediaController::close()
{
    LOGI("@%s device : %s", __FUNCTION__, mPath.c_str());

    if (mFd == -1) {
        LOGW("Device not opened!");
        return INVALID_OPERATION;
    }

    if (SysCall::close(mFd) < 0) {
        LOGE("Close media device failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    mFd = -1;
    return NO_ERROR;
}

int MediaController::xioctl(int request, void *arg) const
{
    int ret(0);
    if (mFd == -1) {
        LOGE("%s invalid device closed!",__FUNCTION__);
        return INVALID_OPERATION;
    }

    do {
        ret = SysCall::ioctl (mFd, request, arg);
    } while (-1 == ret && EINTR == errno);

    if (ret < 0)
        LOGW ("%s: Request 0x%x failed: %s", __FUNCTION__, request, strerror(errno));

    return ret;
}

status_t MediaController::getDeviceInfo()
{
    CLEAR(mDeviceInfo);
    int ret = xioctl(MEDIA_IOC_DEVICE_INFO, &mDeviceInfo);
    if (ret < 0) {
        LOGE("Failed to get media device information");
        return UNKNOWN_ERROR;
    }

    LOGI("Media device driver: %s\n"
         "model: %s\n" "serial: %s\n"
         "bus info: %s\n"
         "hw revision: %d\n"
         "driver version: %d.\n",
         mDeviceInfo.driver,
         mDeviceInfo.model,
         mDeviceInfo.serial,
         mDeviceInfo.bus_info,
         mDeviceInfo.hw_revision,
         mDeviceInfo.driver_version);

    return NO_ERROR;
}

status_t MediaController::enqueueMediaRequest(uint32_t mediaRequestId) {
    UNUSED(mediaRequestId);
    LOGE("Function not implemented in Kernel");
    return BAD_VALUE;
}

status_t MediaController::findEntities()
{
    status_t status = NO_ERROR;
    struct media_links_enum linksEnum;
    struct media_link_desc *links = nullptr;
    struct media_pad_desc *pads = nullptr;
    struct media_entity_desc entityDesc;
    int count = 0;
    std::shared_ptr<MediaEntity> entity;

    // Loop until all media entities are found
    CLEAR(entityDesc);
    CLEAR(linksEnum);
    entityDesc.id |= MEDIA_ENT_ID_FLAG_NEXT;
    while (xioctl(MEDIA_IOC_ENUM_ENTITIES, &entityDesc) >= 0) {
        mEntityDesciptors.insert(std::make_pair(entityDesc.name, entityDesc));
        mEntityIdDesciptors.insert(std::make_pair(entityDesc.id, entityDesc));
        LOGI("entity name: %s, id: %d, pads: %d, links: %d",
              entityDesc.name, entityDesc.id, entityDesc.pads, entityDesc.links);
        entityDesc.id |= MEDIA_ENT_ID_FLAG_NEXT;
        count++;
    }
    LOGD("@%s: %d media entities found", __FUNCTION__, count);

    if (mEntityDesciptors.size() > 0)
        status = NO_ERROR;

    return status;
}

status_t MediaController::getEntityNameForId(unsigned int entityId, std::string &entityName) const
{
    LOGI("@%s", __FUNCTION__);
    status_t status = UNKNOWN_ERROR;

    if (mEntityIdDesciptors.empty()) {
        LOGW("No media Id descriptors, try use mEntityDesciptors!");
        if (mEntityDesciptors.empty()) {
            LOGE("No media descriptors");
            return UNKNOWN_ERROR;
        }

        for (const auto &entityDesciptors : mEntityDesciptors) {
            if (entityId == entityDesciptors.second.id) {
                entityName = entityDesciptors.second.name;
                return NO_ERROR;
            }
        }
        return UNKNOWN_ERROR;
    }
    entityName = mEntityIdDesciptors.find(entityId)->second.name;

    return NO_ERROR;
}

/**
 * Function to get the sink names for a given media entity.
 *
 * \param[IN] media entity instance
 * \param[OUT] sink names for the media entity
 */
status_t MediaController::getSinkNamesForEntity(std::shared_ptr<MediaEntity> mediaEntity, std::vector<std::string> &names)
{
    LOGI("@%s", __FUNCTION__);
    status_t status = UNKNOWN_ERROR;
    std::vector<media_link_desc> links;

    if (mediaEntity == nullptr) {
        LOGE("mediaEntity instance is null");
        return UNKNOWN_ERROR;
    }

    mediaEntity->getLinkDesc(links);
    if (links.empty()) {
        return NO_ERROR;
    } else {
        for (unsigned int i = 0; i < links.size(); i++) {
            std::string name;
            status = getEntityNameForId(links[0].sink.entity, name);
            if (status != NO_ERROR) {
                LOGE("Error getting name for Id");
                return status;
            }
            names.push_back(name);
        }
    }

    return NO_ERROR;
}

status_t MediaController::getMediaDevInfo(media_device_info &info)
{
    LOGI("@%s", __FUNCTION__);

    if (mFd < 0) {
        LOGE("Media controller isn't initialized");
        return UNKNOWN_ERROR;
    }

    info = mDeviceInfo;

    return NO_ERROR;
}

status_t MediaController::enumLinks(struct media_links_enum &linkInfo)
{
    LOGI("@%s", __FUNCTION__);
    int ret = 0;
    ret = xioctl(MEDIA_IOC_ENUM_LINKS, &linkInfo);
    if (ret < 0) {
        LOGE("Enumerating entity links failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

/**
 * Find description for given entity ID
 *
 * Using media controller here to query entity with given index.
 */
status_t MediaController::findMediaEntityById(int index, struct media_entity_desc &mediaEntityDesc)
{
    LOGI("@%s", __FUNCTION__);
    int ret = 0;
    CLEAR(mediaEntityDesc);
    mediaEntityDesc.id = index;

    if (mEntityIdDesciptors.empty()) {
        LOGW("No media Id descriptors, try enum entities again!");
        ret = xioctl(MEDIA_IOC_ENUM_ENTITIES, &mediaEntityDesc);
        if (ret < 0) {
            LOGW("Enumerating entities failed: %s", strerror(errno));
            return UNKNOWN_ERROR;
        }
    }

    mediaEntityDesc = mEntityIdDesciptors.find(index)->second;

    return NO_ERROR;
}

/**
 * Find description for given entity shink name
 *
 * Using media controller here to query entity with given shink name.
 */
status_t MediaController::findMediaEntityByName(char* name, struct media_entity_desc &mediaEntityDesc)
{
    LOGI("@%s", __FUNCTION__);
    status_t status = UNKNOWN_ERROR;

    if (mEntityDesciptors.empty()) {
        LOGE("No media descriptors");
        return UNKNOWN_ERROR;
    }

    for (const auto &entityDesciptors : mEntityDesciptors) {
        LOGE("entityDesciptors.first:%s",entityDesciptors.first.c_str());

        if (strncmp(entityDesciptors.second.name, name, strlen(name)) == 0 &&
                     entityDesciptors.second.name[strlen(name)] == '\0') {
            ALOGE("@%s match:%s",__FUNCTION__,entityDesciptors.second.name);
            return NO_ERROR;
        }
    }
    return status;
}

status_t MediaController::setFormat(const MediaCtlFormatParams &formatParams)
{
    LOGI("@%s entity %s pad %d (%dx%d) format(0x%x)", __FUNCTION__,
                               formatParams.entityName.c_str(), formatParams.pad,
                               formatParams.width, formatParams.height,
                               formatParams.formatCode);
    std::shared_ptr<MediaEntity> entity;
    status_t status = NO_ERROR;
    const char* entityName = formatParams.entityName.c_str();

    status = getMediaEntity(entity, entityName);
    if (status != NO_ERROR) {
        LOGD("@%s: getting MediaEntity \"%s\" failed", __FUNCTION__, entityName);
        return status;
    }
    if (entity->getType() == DEVICE_VIDEO) {
        std::shared_ptr<V4L2VideoNode> node;
        FrameInfo config;

        CLEAR(config);
        config.format = formatParams.formatCode;
        config.width = formatParams.width;
        config.height = formatParams.height;
        config.stride = formatParams.stride;
        config.field = formatParams.field;
        status = entity->getDevice((std::shared_ptr<V4L2DeviceBase>&) node);
        if (!node || status != NO_ERROR) {
            LOGE("@%s: error opening device \"%s\"", __FUNCTION__, entityName);
            return status;
        }
        status = node->setFormat(config);
    } else {
        std::shared_ptr<V4L2Subdevice> subdev;
        status = entity->getDevice((std::shared_ptr<V4L2DeviceBase>&) subdev);
        if (!subdev || status != NO_ERROR) {
            LOGE("@%s: error opening device \"%s\"", __FUNCTION__, entityName);
            return status;
        }
        status = subdev->setFormat(formatParams.pad, formatParams.width,
                                   formatParams.height, formatParams.formatCode,
                                   formatParams.field, formatParams.quantization);
    }
    return status;
}

status_t MediaController::setSelection(const char* entityName, int pad, int target, int top, int left, int width, int height)
{
    LOGI("@%s, entity %s, pad:%d, top:%d, left:%d, width:%d, height:%d", __FUNCTION__,
         entityName, pad, top, left, width, height);
    std::shared_ptr<MediaEntity> entity;
    std::shared_ptr<V4L2Subdevice> subdev;

    status_t status = getMediaEntity(entity, entityName);
    if (status != NO_ERROR) {
        LOGE("@%s: getting MediaEntity \"%s\" failed", __FUNCTION__, entityName);
        return status;
    }
    status = entity->getDevice((std::shared_ptr<V4L2DeviceBase>&) subdev);
    if (!subdev || status != NO_ERROR) {
        LOGE("@%s: error opening device \"%s\"", __FUNCTION__, entityName);
        return status;
    }
    return subdev->setSelection(pad, target, top, left, width, height);
}

status_t MediaController::setControl(const char* entityName, int controlId, int value, const char *controlName)
{
    LOGI("@%s entity %s ctrl ID %d value %d name %s", __FUNCTION__,
                                                         entityName, controlId,
                                                         value,controlName);
    std::shared_ptr<MediaEntity> entity = nullptr;
    std::shared_ptr<V4L2Subdevice> subdev = nullptr;

    status_t status = getMediaEntity(entity, entityName);
    if (status != NO_ERROR) {
        LOGE("@%s: getting MediaEntity \"%s\" failed", __FUNCTION__, entityName);
        return status;
    }
    status = entity->getDevice((std::shared_ptr<V4L2DeviceBase>&) subdev);
    if (!subdev || status != NO_ERROR) {
        LOGE("@%s: error opening device \"%s\"", __FUNCTION__, entityName);
        return status;
    }
    return subdev->setControl(controlId, value, controlName);
}

/**
 * Enable or disable link between two given media entities
 */
status_t MediaController::configureLink(const MediaCtlLinkParams &linkParams)
{
    LOGI("%s @%s: %s \"%s\":%d->\"%s\":%d[%d]", mPath.c_str(),__FUNCTION__,
         linkParams.enable?"enable":"disable",
         linkParams.srcName.c_str(), linkParams.srcPad,
         linkParams.sinkName.c_str(), linkParams.sinkPad,linkParams.enable);
    status_t status = NO_ERROR;

    std::shared_ptr<MediaEntity> srcEntity, sinkEntity;
    struct media_link_desc linkDesc;
    struct media_pad_desc srcPadDesc, sinkPadDesc;

    status = getMediaEntity(srcEntity, linkParams.srcName.c_str());
    if (status != NO_ERROR) {
        LOGD("@%s: getting MediaEntity \"%s\" failed",
             __FUNCTION__, linkParams.srcName.c_str());
        return status;
    }

    status = getMediaEntity(sinkEntity, linkParams.sinkName.c_str());
    if (status != NO_ERROR) {
        LOGE("@%s: getting MediaEntity \"%s\" failed",
             __FUNCTION__, linkParams.sinkName.c_str());
        return status;
    }
    CLEAR(linkDesc);
    CLEAR(srcPadDesc);
    CLEAR(sinkPadDesc);
    srcEntity->getPadDesc(srcPadDesc, linkParams.srcPad);
    sinkEntity->getPadDesc(sinkPadDesc, linkParams.sinkPad);

    linkDesc.source = srcPadDesc;
    linkDesc.sink = sinkPadDesc;

    if (linkParams.enable) {
        linkDesc.flags |= linkParams.flags;
    } else if (linkParams.flags & MEDIA_LNK_FL_DYNAMIC) {
        linkDesc.flags |= MEDIA_LNK_FL_DYNAMIC;
        linkDesc.flags &= ~MEDIA_LNK_FL_ENABLED;
    } else {
        linkDesc.flags &= ~MEDIA_LNK_FL_ENABLED;
    }

    status = setupLink(linkDesc);

    // refresh source entity links
    if (status == NO_ERROR) {
        struct media_entity_desc entityDesc;
        struct media_links_enum linksEnum;
        CLEAR(entityDesc);
        CLEAR(linksEnum);
        struct media_link_desc *links = nullptr;

        sinkEntity->getEntityDesc(entityDesc);
        links = new struct media_link_desc[entityDesc.links];
        memset(links, 0, sizeof(struct media_link_desc) * entityDesc.links);

        linksEnum.entity = entityDesc.id;
        linksEnum.pads = nullptr;
        linksEnum.links = links;
        status = enumLinks(linksEnum);
        if (status == NO_ERROR) {
            sinkEntity->updateLinks(linksEnum.links);
        }
        delete[] links;
    }

    return status;
}

status_t MediaController::setupLink(struct media_link_desc &linkDesc)
{
    LOGI("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    int ret = xioctl(MEDIA_IOC_SETUP_LINK, &linkDesc);
    if (ret < 0) {
        LOGE("Link setup failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }
    return status;
}

/**
 * Resets (disables) all links between entities
 */
status_t MediaController::resetLinks()
{
    LOGI("@%s start!", __FUNCTION__);
    status_t status = NO_ERROR;
    for (const auto &entityDesciptors : mEntityDesciptors) {
        struct media_entity_desc entityDesc;
        struct media_entity_desc entitySrcDesc;
        struct media_entity_desc entitySinkDesc;
        struct media_links_enum linksEnum;
        CLEAR(entityDesc);
        CLEAR(linksEnum);
        struct media_link_desc *links = nullptr;

        entityDesc = entityDesciptors.second;
        links = new struct media_link_desc[entityDesc.links];
        memset(links, 0, sizeof(media_link_desc) * entityDesc.links);
        LOGI("@%s entityDesc name: %s, id: %d, links: %d",
             __FUNCTION__, entityDesc.name, entityDesc.id, entityDesc.links);

        linksEnum.entity = entityDesc.id;
        linksEnum.pads = nullptr;
        linksEnum.links = links;
        status = enumLinks(linksEnum);
        if (status != NO_ERROR) {
            delete[] links;
            break;
        }
        // Disable all links, except the immutable ones
        for (int j = 0; j < entityDesc.links; j++) {
            if (links[j].flags & MEDIA_LNK_FL_IMMUTABLE)
                continue;
            entitySrcDesc = mEntityIdDesciptors.find(links[j].source.entity)->second;
            entitySinkDesc = mEntityIdDesciptors.find(links[j].sink.entity)->second;
            links[j].flags &= ~MEDIA_LNK_FL_ENABLED;
            LOGI("%s @%s: %s \"%s\":%d->\"%s\":%d[%d]", mPath.c_str(),__FUNCTION__,
                 (links[j].flags & MEDIA_LNK_FL_ENABLED)?"enabled":"disabled",
                 entitySrcDesc.name, links[j].source.index,
                 entitySinkDesc.name, links[j].sink.index,links[j].flags);
            setupLink(links[j]);
        }
        delete[] links;
    }

    return status;
}

status_t MediaController::getMediaEntity(std::shared_ptr<MediaEntity> &entity, const char* name)
{
    status_t status = NO_ERROR;

    std::string entityName(name);
    struct media_entity_desc entityDesc;
    struct media_links_enum linksEnum;
    CLEAR(entityDesc);
    CLEAR(linksEnum);
    struct media_link_desc *links = nullptr;
    struct media_pad_desc *pads = nullptr;

    LOGI("@%s, entityName:%s!", __FUNCTION__, entityName.c_str());
    // check whether the MediaEntity object has already been created
    std::map<std::string, std::shared_ptr<MediaEntity>>::iterator itEntities =
                                     mEntities.find(entityName);
    std::map<std::string, struct media_entity_desc>::iterator itEntityDesc =
                                     mEntityDesciptors.find(entityName);
    if (itEntities != mEntities.end()) {
        entity = itEntities->second;
    } else if (itEntityDesc != mEntityDesciptors.end()) {
        // MediaEntity object not yet created, so create it
        entityDesc = itEntityDesc->second;
        if (entityDesc.links > 0) {
            links = new struct media_link_desc[entityDesc.links];
            memset(links, 0, sizeof(media_link_desc) * entityDesc.links);
        }

        if (entityDesc.pads > 0) {
            pads = new struct media_pad_desc[entityDesc.pads];
            memset(pads, 0, sizeof(media_pad_desc) * entityDesc.pads);
        }

        LOGI("Creating entity - name: %s, id: %d, links: %d, pads: %d",
             entityDesc.name, entityDesc.id, entityDesc.links, entityDesc.pads);

        linksEnum.entity = entityDesc.id;
        linksEnum.pads = pads;
        linksEnum.links = links;
        status = enumLinks(linksEnum);

        if (status == NO_ERROR) {
            entity = std::make_shared<MediaEntity>(entityDesc, links, pads);
            mEntities.insert(std::make_pair(entityDesc.name, entity));
        }
        if (entityDesc.links > 0)
            delete[] links;
        if (entityDesc.pads > 0)
            delete[] pads;
    } else {
        status = UNKNOWN_ERROR;
    }

    return status;
}

} NAMESPACE_DECLARATION_END
