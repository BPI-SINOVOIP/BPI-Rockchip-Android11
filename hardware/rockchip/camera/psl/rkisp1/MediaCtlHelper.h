/*
 * Copyright (C) 2017 Intel Corporation
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

#ifndef PSL_RKISP1_MEDIACTLHELPER_H_
#define PSL_RKISP1_MEDIACTLHELPER_H_

#include "GraphConfigManager.h"
#include "MediaController.h"
#include "v4l2device.h"
#include "NodeTypes.h"

namespace android {
namespace camera2 {

class MediaCtlHelper
{
public:
    class IOpenCallBack {
    public:
        virtual status_t opened(NodeTypes isysNodeName,
                std::shared_ptr<V4L2VideoNode> videoNode) = 0;
        virtual ~IOpenCallBack();
    };

    MediaCtlHelper(std::shared_ptr<MediaController> mediaCtl,
            IOpenCallBack *openCallBack, bool isIMGU = false);
    virtual ~MediaCtlHelper();

    status_t configure(IStreamConfigProvider &graphConfigMgr,
            IStreamConfigProvider::MediaType type);
    status_t configurePipe(IStreamConfigProvider &graphConfigMgr,
                           IStreamConfigProvider::MediaType pipeType,
                           bool resetFormat = false);
    bool isMediaTypeForPipe(IStreamConfigProvider::MediaType pipeType) const {
        return (pipeType == IStreamConfigProvider::IMGU_VIDEO
             || pipeType == IStreamConfigProvider::IMGU_STILL);
    }
    status_t configureImguNodes(IStreamConfigProvider &graphConfigMgr);

    std::map<NodeTypes, std::shared_ptr<V4L2VideoNode>> getConfiguredNodesPerName()
    {
        return mConfiguredNodesPerName;
    }

    void getConfigedHwPathSize(const char* pathName, uint32_t &size);
    void getConfigedSensorOutputSize(uint32_t &size);

public:
    /**
     * \struct ConfigurationResults
     * Contains relevant information for the clients of this class after the
     * Input system has been configured.
     * Input system configuration also set the configuration of the sensor.
     */
    struct ConfigurationResults {
        int pixelFormat;  /**< V4L2 pixel format produced by the input system
                               pipe */

        ConfigurationResults() :
            pixelFormat(0) {
        }

    };

    ConfigurationResults& getConfigResults()
    {
        return mConfigResults;
    }

private:
    status_t openVideoNodes();
    status_t openVideoNode(const char *entityName, NodeTypes isysNodeName);
    status_t resetLinks(const MediaCtlConfig *config);
    status_t closeVideoNodes();

private:
    ConfigurationResults mConfigResults;

    IOpenCallBack* mOpenVideoNodeCallBack;
    std::shared_ptr<MediaController> mMediaCtl;

    MediaCtlConfig mConfigedMediaCtlConfigs[IStreamConfigProvider::MEDIA_TYPE_MAX_COUNT];
    const MediaCtlConfig *mMediaCtlConfig;
    const MediaCtlConfig *mPipeConfig;
    IStreamConfigProvider::MediaType mConfigedPipeType;

    std::vector<std::shared_ptr<V4L2VideoNode>>  mConfiguredNodes;        /**< Configured video nodes */
    std::map<NodeTypes, std::shared_ptr<V4L2VideoNode>> mConfiguredNodesPerName;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_MEDIACTLHELPER_H_ */
