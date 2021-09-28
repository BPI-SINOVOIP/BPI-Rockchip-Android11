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

#ifndef _CAMERA3_RKISP2GraphConfigManager_H_
#define _CAMERA3_RKISP2GraphConfigManager_H_

#include <gcss.h>
#include <hardware/camera3.h>
#include <memory>
#include <utility>
#include <vector>
#include "SharedItemPool.h"
#include "MediaCtlPipeConfig.h"
#include "MediaController.h"

namespace android {
namespace camera2 {
class Camera3Request;
namespace rkisp2 {

/**
 * Static data for graph settings for given sensor. Used to initialize
 * \class RKISP2GraphConfigManager.
 */
class GraphConfigNodes
{
public:
    GraphConfigNodes();
    ~GraphConfigNodes();
};

/**
 * \enum PlatformGraphConfigKey
 * List of keys that are Android Specific used in queries of settings by
 * the RKISP2GraphConfigManager.
 *
 * The enum should not overlap with the enum of tags already predefined by the
 * parser, hence the initial offset.
 */
#define GCSS_KEY(key, str) GCSS_KEY_##key,
enum PlatformGraphConfigKey {
    GCSS_ANDROID_KEY_START = GCSS_KEY_START_CUSTOM_KEYS,
    #include "platform_gcss_keys.h"
    #include "RKISP1_android_gcss_keys.h"
};
#undef GCSS_KEY


class RKISP2GraphConfig;

/**
 * \interface RKISP2IStreamConfigProvider
 *
 * This interface is used to expose the RKISP2GraphConfig settings selected at stream
 * config time. At the moment only exposes the MediaController configuration.
 *
 * It is used by the 3 units (Ctrl, Capture and Processing).
 * At the moment it is implemented by the RKISP2GraphConfigManager
 *
 *  TODO: Expose a full RKISP2GraphConfig Object selected.
 */
class RKISP2IStreamConfigProvider {
public:
    enum MediaType {
        CIO2 = 0,
        IMGU_COMMON,
        IMGU_VIDEO,
        IMGU_STILL,

        MEDIA_TYPE_MAX_COUNT
    };

    virtual ~RKISP2IStreamConfigProvider() { };
    virtual const MediaCtlConfig *getMediaCtlConfig(MediaType type) const = 0;
    virtual const MediaCtlConfig *getMediaCtlConfigPrev(MediaType type) const = 0;
    virtual std::shared_ptr<RKISP2GraphConfig> getBaseGraphConfig() = 0;    
    virtual void dumpMediaCtlConfig(const MediaCtlConfig &config) = 0;
};

/**
 * \class RKISP2GraphConfigManager
 *
 */
class RKISP2GraphConfigManager: public RKISP2IStreamConfigProvider
{
public:
    RKISP2GraphConfigManager(int32_t camId, GraphConfigNodes *nodes = nullptr);
    virtual ~RKISP2GraphConfigManager();
    /*
     * static methods for XML parsing
     */

    void setMediaCtl(std::shared_ptr<MediaController> sensorMediaCtl,std::shared_ptr<MediaController> imgMediaCtl) { mMediaCtl = sensorMediaCtl; mImgMediaCtl=imgMediaCtl; }
    /*
     * First Query
     */
    status_t configStreams(const std::vector<camera3_stream_t*> &activeStreams,
                           uint32_t operationMode,
                           int32_t testPatternMode);
    /*
     * Implementation of RKISP2IStreamConfigProvider
     */
    const MediaCtlConfig *getMediaCtlConfig(RKISP2IStreamConfigProvider::MediaType type) const;
    const MediaCtlConfig *getMediaCtlConfigPrev(RKISP2IStreamConfigProvider::MediaType type) const;
    void dumpMediaCtlConfig(const MediaCtlConfig &config) ;
    std::shared_ptr<RKISP2GraphConfig> getBaseGraphConfig();

    void getHwPathSize(const char* pathName, uint32_t &size);
    void getSensorOutputSize(uint32_t &size);
    void enableMainPathOnly(bool isOnlyEnableMp) { mIsOnlyEnableMp = isOnlyEnableMp; }
    bool isOnlyEnableMp() { return mIsOnlyEnableMp; }

    /*
     * Second query
     */
    std::shared_ptr<RKISP2GraphConfig> getGraphConfig(Camera3Request &request);

public:
    static const int32_t MAX_REQ_IN_FLIGHT = 10;
    const int32_t mCameraId;

public:
    // Disable copy constructor and assignment operator
    RKISP2GraphConfigManager(const RKISP2GraphConfigManager &);
    RKISP2GraphConfigManager& operator=(const RKISP2GraphConfigManager &);
    bool isVideoStream(camera3_stream_t *stream);
    // Debuging helpers
    void dumpStreamConfig(const std::vector<camera3_stream_t*> &streams);
    void dumpQuery(const std::map<GCSS::ItemUID, std::string> &query);
    status_t prepareGraphConfig(std::shared_ptr<RKISP2GraphConfig> gc);

    status_t mapStreamToKey(const std::vector<camera3_stream_t*> &streams,
                                    int& videoStreamCnt, int& stillStreamCnt);
private:

    bool mIsOnlyEnableMp;
    SharedItemPool<RKISP2GraphConfig> mGraphConfigPool;

    /**
     * Map to get the virtual sink id from a client stream pointer.
     * The uid is one of the GCSS keys defined for the virtual sinks, like
     * GCSS_KEY_VIDEO0 or GCSS_KEY_STILL1
     * From that we can derive the name using the id to string methods from
     * ItemUID class
     */
    std::map<camera3_stream_t*, uid_t> mStreamToSinkIdMap;

    MediaCtlConfig mMediaCtlConfigs[MEDIA_TYPE_MAX_COUNT];
    MediaCtlConfig mMediaCtlConfigsPrev[MEDIA_TYPE_MAX_COUNT];

    std::shared_ptr<MediaController> mMediaCtl;
     std::shared_ptr<MediaController> mImgMediaCtl;
};

} // namespace rkisp2
} // namespace camera2
} // namespace android
#endif
