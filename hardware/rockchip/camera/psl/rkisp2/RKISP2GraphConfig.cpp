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

#define LOG_TAG "RKISP2GraphConfig"

#include "RKISP2GraphConfig.h"
#include "RKISP2GraphConfigManager.h"
#include "LogHelper.h"
#include "FormatUtils.h"
#include "NodeTypes.h"
#include "Camera3GFXFormat.h"
#include "PlatformData.h"
#include <GCSSParser.h>
#include <v4l2device.h>
#include <linux/v4l2-subdev.h>
#include <algorithm>
#include "FormatUtils.h"

#include "MediaEntity.h"

using GCSS::GraphConfigNode;
using std::string;
using std::vector;
using std::map;
using std::set;

namespace android {
namespace camera2 {
namespace rkisp2 {

extern int32_t gDumpType;

namespace gcu = graphconfig::utils;

// TODO: Change the format attribute natively as integer attribute
#ifndef VIDEO_RECORDING_FORMAT
#define VIDEO_RECORDING_FORMAT TILE
#endif

#define MEDIACTL_PAD_OUTPUT_NUM 2
#define MEDIACTL_PAD_VF_NUM 3
#define MEDIACTL_PAD_PV_NUM 4
#define SCALING_FACTOR 1

#define ISP_DEFAULT_OUTPUT_FORMAT MEDIA_BUS_FMT_YUYV8_2X8
#define VIDEO_DEFAULT_OUTPUT_FORMAT V4L2_PIX_FMT_NV12

const string csi2_without_port = "rockchip-csi2-dphy0";

/* isp port name*/
const string MEDIACTL_INPUTNAME = "input";
const string MEDIACTL_OUTPUTNAME = "output";

/*video entity name*/
const string MEDIACTL_PARAMETERNAME = "rkisp1-input-params";

const string MEDIACTL_VIDEONAME = "rkisp1_mainpath";
const string MEDIACTL_STILLNAME = "rkisp1_mainpath";

const string MEDIACTL_PREVIEWNAME = "rkisp1_selfpath";
const string MEDIACTL_POSTVIEWNAME = "postview";

const string MEDIACTL_STATNAME = "rkisp1-statistics";
const string MEDIACTL_VIDEONAME_CIF = "stream_cif";
const string MEDIACTL_VIDEONAME_CIF_MIPI_ID0 = "stream_cif_mipi_id0";

RKISP2GraphConfig::RKISP2GraphConfig() :
        mManager(nullptr),
        mSettings(nullptr),
        mReqId(0),
        mMetaEnabled(false),
        mFallback(false),
        mPipeType(PIPE_PREVIEW),
        mSourceType(SRC_NONE)
{
    //CLEAR(mProgramGroup);
    mSourcePortName.clear();
    mSinkPeerPort.clear();
    mStreamToSinkIdMap.clear();
    mStream2TuningMap.clear();
    createKernelListStructures();
    mCSIBE = CSI_BE + "0";
    mIsMipiInterface = false;
    mSensorLinkedToCIF = false;
    mMpOutputRaw = false;
    mMainNodeName.clear();
    mSecondNodeName.clear();
}

RKISP2GraphConfig::~RKISP2GraphConfig()
{
    fullReset();
}
/*
 * Full reset
 * This is called whenever we want to reset the whole object. Currently that
 * is only, when RKISP2GraphConfig object is destroyed.
 */
void RKISP2GraphConfig::fullReset()
{
    mSourcePortName.clear();
    mSinkPeerPort.clear();
    mStreamToSinkIdMap.clear();
    mStreamIds.clear();
    deleteKernelInfo();
    delete mSettings;
    mSettings = nullptr;
    mManager = nullptr;
    mReqId = 0;
    mStream2TuningMap.clear();
}
/*
 * Reset
 * This is called per frame
 */
void RKISP2GraphConfig::reset(RKISP2GraphConfig *me)
{
    if (CC_LIKELY(me != nullptr)) {
        me->mReqId = 0;
    } else {
        LOGE("Trying to reset a null RKISP2GraphConfig - BUG!");
    }
}

void RKISP2GraphConfig::deleteKernelInfo() {
}

void RKISP2GraphConfig::createKernelListStructures()
{
}

const GCSS::IGraphConfig* RKISP2GraphConfig::getInterface(Node *node) const
{
    if (!node)
        return nullptr;

    return node;
}

const GCSS::IGraphConfig* RKISP2GraphConfig::getInterface() const
{
    return mSettings;
}

/**
 * Per frame initialization of graph config.
 * Updates request id
 * \param[in] reqId
 */
void RKISP2GraphConfig::init(int32_t reqId)
{
    mReqId = reqId;
}

/**
 * Prepare graph config once per stream config.
 * \param[in] manager
 * \param[in] streamToSinkIdMap
 */
status_t RKISP2GraphConfig::prepare(RKISP2GraphConfigManager *manager,
                              StreamToSinkMap &streamToSinkIdMap)
{
    mStreamIds.clear();
    mManager = manager;
    mStreamToSinkIdMap.clear();
    mStreamToSinkIdMap = streamToSinkIdMap;
    return OK;
}

/**
 * Prepare graph config once per stream config.
 * \param[in] manager
 * \param[in] settings
 * \param[in] streamToSinkIdMap
 * \param[in] active
 */
status_t RKISP2GraphConfig::prepare(RKISP2GraphConfigManager *manager,
                              Node *settings,
                              StreamToSinkMap &streamToSinkIdMap,
                              bool fallback)
{
    mStreamIds.clear();
    mManager = manager;
    //it will cause memory leak when recording video many times without
    //exit camera app. In this case, framework invoke config_streams
    //many times and do not invoke flush, thereby invoking this function
    //twice without calling the func fullReset and casue the memory leak.
    if (mSettings)
        delete mSettings;
    mSettings = settings;
    mFallback = fallback;
    status_t ret = OK;

    if (CC_UNLIKELY(settings == nullptr)) {
        LOGW("Settings is nullptr!! - BUG?");
        return UNKNOWN_ERROR;
    }

    ret = analyzeSourceType();
    if (ret != OK) {
        LOGE("Failed to analyze source type");
        return ret;
    }

    ret = getActiveOutputPorts(streamToSinkIdMap);
    if (ret != OK) {
        LOGE("Failed to get output ports");
        return ret;
    }

    ret = generateKernelListsForStreams();
    if (ret != OK) {
        LOGE("Failed to generate kernel list");
        return ret;
    }

    calculateSinkDependencies();
    storeTuningModes();
    return ret;
}

/**
 * Store the tuning modes for each stream id into a map that can be used on a
 * per frame basis.
 * This method is executed once per stream configuration.
 * The tuning mode is used by AIC to find the correct tuning tables in CPF.
 *
 */
void RKISP2GraphConfig::storeTuningModes()
{
    GraphConfigNode::const_iterator it = mSettings->begin();
    css_err_t ret = css_err_none;
    GraphConfigNode *result = nullptr;
    int32_t tuningMode = 0;
    int32_t streamId = 0;
    mStream2TuningMap.clear();

    while (it != mSettings->end()) {
        ret = mSettings->getDescendant(GCSS_KEY_TYPE, "program_group", it, &result);
        if (ret == css_err_none) {
            ret = result->getValue(GCSS_KEY_STREAM_ID, streamId);
            if (ret != css_err_none) {
                string pgName;
                // This should  not fail
                ret = result->getValue(GCSS_KEY_NAME, pgName);
                LOGW("Failed to find stream id for PG %s", pgName.c_str());
                continue;
            }
            tuningMode = 0; // default value in case it is not found
            ret = result->getValue(GCSS_KEY_TUNING_MODE, tuningMode);
            if (ret != css_err_none) {
                string pgName;
                // This should  not fail
                ret = result->getValue(GCSS_KEY_NAME, pgName);
                LOGW("Failed t find tuning mode for PG %s, defaulting to %d",
                        pgName.c_str(), tuningMode);
            }
            mStream2TuningMap[streamId] =  tuningMode;
        }
    }
}
/**
 * Retrieve the tuning mode associated with a given stream id.
 *
 * The tuning mode is defined by IQ-studio and represent and index to different
 * set of tuning parameters in the AIQB (a.k.a CPF)
 *
 * The tuning mode is an input parameter for AIC.
 * \param [in] streamId Identifier for the branch (video/still/isa)
 * \return tuning mode, if stream id is not found defaults to 0
 */
int32_t RKISP2GraphConfig::getTuningMode(int32_t streamId)
{
    auto item = mStream2TuningMap.find(streamId);
    if (item != mStream2TuningMap.end()) {
        return item->second;
    }
    LOGW("Could not find tuning mode for requested stream id %d", streamId);
    return 0;
}

/*
 * According to the node, analyze the source type:
 * TPG or sensor
 */
status_t RKISP2GraphConfig::analyzeSourceType()
{
    css_err_t ret = css_err_none;
    Node *inputDevNode = nullptr;
    ret = mSettings->getDescendant(GCSS_KEY_SENSOR, &inputDevNode);
    if (ret == css_err_none) {
        mSourceType = SRC_SENSOR;
        mSourcePortName = SENSOR_PORT_NAME;
    } else {
        LOGI("No sensor node from the graph");
    }
    return OK;
}

/**
 * Finds the sink nodes and the output port peer. Use streamToSinkIdMap
 * since we are intrested only in sinks that serve a stream. Takes an
 * internal copy of streamToSinkIdMap to be used later.
 *
 * \param[in] streamToSinkIdMap to get the virtual sink id from a client stream pointer
 * \return OK in case of success.
 * \return UNKNOWN_ERROR or BAD_VALUE in case of fail.
 */
status_t RKISP2GraphConfig::getActiveOutputPorts(const StreamToSinkMap &streamToSinkIdMap)
{
    status_t status = OK;
    css_err_t ret = css_err_none;
    NodesPtrVector sinks;

    mStreamToSinkIdMap.clear();
    mStreamToSinkIdMap = streamToSinkIdMap;
    mSinkPeerPort.clear();

    StreamToSinkMap::const_iterator it;
    it = streamToSinkIdMap.begin();

    for (; it != streamToSinkIdMap.end(); ++it) {
        sinks.clear();
        status = graphGetSinksByName(GCSS::ItemUID::key2str(it->second), sinks);
        if (CC_UNLIKELY(status != OK) || sinks.empty()) {
            string sinkName = GCSS::ItemUID::key2str(it->second);
            LOGE("Found %zu sinks, expecting 1 for sink %s", sinks.size(),
                 sinkName.c_str());
            return BAD_VALUE;
        }

        Node *sink = sinks[0];
        Node *outputPort = nullptr;

        // Get the sinkname for getting the output port
        string sinkName;
        ret =  sink->getValue(GCSS_KEY_NAME, sinkName);
        if (CC_UNLIKELY(ret != css_err_none)) {
            LOGE("Failed to get sink name");
            return BAD_VALUE;
        }

        int32_t streamId = -1;
        ret = sink->getValue(GCSS_KEY_STREAM_ID, streamId);
        if (CC_UNLIKELY(ret != css_err_none)) {
            LOGE("Failed to get stream id");
            return BAD_VALUE;
        }

        outputPort = getOutputPortForSink(sinkName);
        if (outputPort == nullptr) {
            LOGE("No output port found for sink");
            return UNKNOWN_ERROR;
        }

        mSinkPeerPort[sink] = outputPort;
    }

    return OK;
}

string RKISP2GraphConfig::getNodeName(Node * node)
{
    string nodeName("");
    if (node == nullptr) {
        LOGE("Node is nullptr");
        return nodeName;
    }

    node->getValue(GCSS_KEY_NAME, nodeName);
    return nodeName;
}

/**
 * Finds the output port which is the peer to the sink node.
 *
 * Gets root node, and finds the sink with the given name. Use portGetPeer()
 * to find the output port.
 * \return GraphConfigNode in case of success.
 * \return nullptr in case of fail.
 */
RKISP2GraphConfig::Node *RKISP2GraphConfig::getOutputPortForSink(const string &sinkName)
{
    css_err_t ret = css_err_none;
    status_t retErr = OK;
    Node *rootNode = nullptr;
    Node *portNode = nullptr;
    Node *peerNode = nullptr;

    rootNode = mSettings->getRootNode();
    if (rootNode == nullptr) {
        LOGE("Couldn't get root node, BUG!");
        return nullptr;
    }
    ret = rootNode->getDescendantByString(sinkName, &portNode);
    if (ret != css_err_none) {
        LOGE("Error getting sink");
        return nullptr;
    }
    retErr = portGetPeer(portNode, &peerNode);
    if (retErr != OK) {
        LOGE("Error getting peer");
        return nullptr;
    }
    return portNode;
}

/**
 * Returns true if the given node is used to output a video record
 * stream. The sink name is found and used to find client stream from the
 * mStreamToSinkIdMap.
 * Then the video encoder gralloc flag is checked from the stream flags of the
 * client stream.
 * \param[in] peer output port to find the sink node of.
 * \return true if sink port serves a video record stream.
 * \return false if sink port does not serve a video record stream.
 */
bool RKISP2GraphConfig::isVideoRecordPort(Node *sink)
{
    css_err_t ret = css_err_none;
    string sinkName;
    camera3_stream_t* clientStream = nullptr;

    if (sink == nullptr) {
        LOGE("No sink node provided");
        return false;
    }

    ret = sink->getValue(GCSS_KEY_NAME, sinkName);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to get sink name");
        return false;
    }

    // Find the client stream for the sink port
    StreamToSinkMap::iterator it1;
    it1 = mStreamToSinkIdMap.begin();

    for (; it1 != mStreamToSinkIdMap.end(); ++it1) {
        if (GCSS::ItemUID::key2str(it1->second) == sinkName) {
            clientStream = it1->first;
            break;
        }
    }

    if (clientStream == nullptr) {
        LOGE("Failed to find client stream");
        return false;
    }

    if (CHECK_FLAG(clientStream->usage, GRALLOC_USAGE_HW_VIDEO_ENCODER)) {
        LOGD("%s is video record port", NODE_NAME(sink));
        return true;
    }

    return false;
}

/**
 * Takes a stream id, and checks if it exists in the graph.
 *
 * \param[in] streamId
 * \return true if found, false otherwise
 */
bool RKISP2GraphConfig::hasStreamInGraph(int streamId)
{
    status_t status;
    StreamsVector streamsFound;

    status = graphGetStreamIds(streamsFound);
    if (status != OK)
        return false;

    for (size_t i = 0; i < streamsFound.size(); i++) {
        if (streamsFound[i] == streamId)
            return true;
    }
    return false;
}

/**
 * check whether the kernel is in this stream
 *
 * \param[in] streamId stream id.
 * \param[in] kernelId kernel id.
 * \param[out] whether the kernel in this stream
 * \return true the kernel is in this stream
 * \return false the kernel isn't in this stream.
 *
 */
bool RKISP2GraphConfig::isKernelInStream(uint32_t streamId, uint32_t kernelId)
{
    return false;
}

/**
 * get program group id for some kernel
 *
 * \param[in] streamId stream id.
 * \param[in] kernelId kernel pal uuid.
 * \param[out] program group id that contain this kernel with the same stream id
 * \return error if can't find the kernel id in any ot the PGs in this stream
 */
status_t RKISP2GraphConfig::getPgIdForKernel(const uint32_t streamId, const int32_t kernelId, int32_t &pgId)
{
    css_err_t ret = css_err_none;
    status_t retErr;
    NodesPtrVector programGroups;

    // Get all program groups with the stream id
    retErr = streamGetProgramGroups(streamId, programGroups);
    if (retErr != OK) {
        LOGE("ERROR: couldn't get program groups");
        return retErr;
    }

    // Go through all the program groups with the selected streamID
    for (uint32_t i = 0; i < programGroups.size(); i++) {
        /* Iterate through program group nodes, find kernel and get the PG id */
        GCSS::GraphConfigItem::const_iterator it = programGroups[i]->begin();
        while (it != programGroups[i]->end()) {
            Node *kernelNode = nullptr;
            // Look for kernel with the requested uuid
            ret = programGroups[i]->getDescendant(GCSS_KEY_PAL_UUID,
                                                  kernelId,
                                                  it,
                                                  &kernelNode);
            if (ret != css_err_none)
                continue;

            ret = programGroups[i]->getValue(GCSS_KEY_PG_ID, pgId);
            if (CC_LIKELY(ret == css_err_none)) {
                LOGI("got the pgid:%d for kernel id:%d in stream:%d",
                        pgId, kernelId, streamId);
                return NO_ERROR;
            }
            LOGE("ERROR: Couldn't get pg id for kernel %d", kernelId);
            return BAD_VALUE;
        }
    }
    LOGE("ERROR: Couldn't get pal_uuid");
    return BAD_VALUE;
}

/**
 * Retrieve all the sinks in the current graph configuration that match the
 * input parameter string in their name attribute.
 *
 * If the name to match is empty it returns all the nodes of type sink
 *
 * \param[in] name String containing the name to match.
 * \param[out] sink List of sinks that match that name
 * \return OK in case of success
 * \return UNKNOWN_ERROR if no sinks were found in the graph config.
 *
 */
status_t RKISP2GraphConfig::graphGetSinksByName(const std::string &name,
                                          NodesPtrVector &sinks)
{
    css_err_t ret = css_err_none;
    GraphConfigNode *result;
    NodesPtrVector allSinks;
    string foundName;
    GraphConfigNode::const_iterator it = mSettings->begin();

    while (it != mSettings->end()) {
        ret = mSettings->getDescendant(GCSS_KEY_TYPE, "sink", it, &result);
        if (ret == css_err_none) {
            allSinks.push_back(result);
        }
    }

    if (allSinks.empty()) {
        LOGE("Failed to find any sinks -check graph config file");
        return UNKNOWN_ERROR;
    }
    /*
     * if the name is empty it means client wants all sinks.
     */
    if (name.empty()) {
        sinks = allSinks;
        return OK;
    }

    for (size_t i = 0; i < allSinks.size(); i++) {
        allSinks[i]->getValue(GCSS_KEY_NAME, foundName);
        if (foundName.find(name) != string::npos) {
            sinks.push_back(allSinks[i]);
        }
    }

    return OK;
}

/*
 * Imgu helper functions
 */
status_t RKISP2GraphConfig::graphGetDimensionsByName(const std::string &name,
                                          int &widht, int &height)
{
    widht = 0;
    height = 0;
    Node *csiBEOutput = nullptr;

    // Get csi_be node. If not found, try csi_be_soc. If not found return error.
    int ret = mSettings->getDescendantByString(name, &csiBEOutput);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't find node: %s", name.c_str());
        return UNKNOWN_ERROR;
    }

    ret = getDimensions(csiBEOutput, widht, height);
    if (ret != OK) {
        LOGE("Error: Couldn't find dimensions from <%s>", name.c_str());
        return UNKNOWN_ERROR;
    }

    return OK;
}

/*
 * Imgu helper functions
 */
status_t RKISP2GraphConfig::graphGetDimensionsByName(const std::string &name,
                                          unsigned short &widht, unsigned short &height)
{
    int w, h;
    int ret = graphGetDimensionsByName(name, w, h);
    widht = w;
    height = h;

    return ret;
}

/**
 * This method creates SinkDependency structure for every active sink found in
 * the graph. These structs allow quick access to information that is required
 * by other methods.
 * Active sinks are the ones that have a connection to an active port.
 * This list of active sinks(mSinkPeerPort) has to be filled before this method
 * is executed.
 * For every virtual sink we store the name (as a key) and the terminal id of
 * the input port of the stream associated with that stream. This input port
 * will be the destination of the buffers from the capture unit.
 *
 * This method is used during init()
 * If we would have different settings per frame then this would be enough
 * to detect the active ISA nodes, but we are not there yet. we are still using
 * the base graph settings every frame.
 */
void RKISP2GraphConfig::calculateSinkDependencies()
{
    status_t status = OK;
    Node *streamInputPort = nullptr;
    NodesPtrVector sinks;
    std::string sinkName;
    SinkDependency aSinkDependency;
    uint32_t stageId; //not needed
    mSinkDependencies.clear();
    mIsaOutputPort2StreamId.clear();
    map<Node*, Node*>::iterator sinkIter = mSinkPeerPort.begin();

    for (; sinkIter != mSinkPeerPort.end(); sinkIter++) {
        sinkIter->first->getValue(GCSS_KEY_NAME, sinkName);
        aSinkDependency.sinkGCKey = GCSS::ItemUID::str2key(sinkName);
        aSinkDependency.streamId = sinkGetStreamId(sinkIter->first);
        status = streamGetInputPort(aSinkDependency.streamId,
                                    &streamInputPort);
        if (CC_UNLIKELY(status != OK)) {
            LOGE("Failed to get input port for stream %d associated to sink %s",
                    aSinkDependency.streamId, sinkName.c_str());
            continue;
        }
        status = portGetFourCCInfo(*streamInputPort,
                                   stageId,
                                   aSinkDependency.streamInputPortId);
        if (CC_UNLIKELY(status != OK)) {
            LOGE("Failed to get stream %d input port 4CC code",
                    aSinkDependency.streamId);
            continue;
        }
        LOGI("Adding dependency %s stream id %d", sinkName.c_str(), aSinkDependency.streamId);
        mSinkDependencies.push_back(aSinkDependency);

        // get the output port of capture unit
        Node* isaOutPutPort = nullptr;
        status = portGetPeer(streamInputPort, &isaOutPutPort);
        if (CC_UNLIKELY(status != OK)) {
            LOGE("Fail to get isa output port for sink %s", sinkName.c_str());
            continue;
        }
        std::string fullName;
        status = portGetFullName(isaOutPutPort, fullName);
        if (CC_UNLIKELY(status != OK)) {
            LOGE("Fail to get isa output port name");
            continue;
        }
        int32_t streamId = portGetStreamId(isaOutPutPort);
        if (streamId != -1 &&
            mIsaOutputPort2StreamId.find(fullName) == mIsaOutputPort2StreamId.end())
            mIsaOutputPort2StreamId[fullName] = streamId;
    }
}

/**
 * This method is used by the GC Manager that has access to the request
 * to inform us of what are the active sinks.
 * Using the sink dependency information we can then know which ISA ports
 * are active for this GC.
 *
 * Once we have different settings per request then we can incorporate this
 * method into calculateSinkDependencies.
 *
 * \param[in] activeSinks Vector with GCSS_KEY's of the active sinks in a
 *                        request
 */
void RKISP2GraphConfig::setActiveSinks(std::vector<uid_t> &activeSinks)
{
    mIsaActiveDestinations.clear();
    uid_t activeDest = 0;

    for (size_t i = 0; i < activeSinks.size(); i++) {
        for (size_t j = 0; j < mSinkDependencies.size(); j++) {
            if (mSinkDependencies[j].sinkGCKey == activeSinks[i]) {
                activeDest = mSinkDependencies[j].streamInputPortId;
                mIsaActiveDestinations[activeDest] = activeDest;
            }
        }
    }
}

/**
 * This method is used by the GC Manager that has access to the request
 * to inform us of what will the stream id be used.
 * Using the sink dependency information we can then know which stream ids
 * are active for this GC.
 *
 * Once we have different settings per request then we can incorporate this
 * method into calculateSinkDependencies.
 *
 * \param[in] activeSinks Vector with GCSS_KEY's of the active sinks in a
 *                        request
 */
void RKISP2GraphConfig::setActiveStreamId(const std::vector<uid_t> &activeSinks)
{
    mActiveStreamId.clear();
    int32_t activeStreamId = 0;
    status_t status = NO_ERROR;

    for (size_t i = 0; i < activeSinks.size(); i++) {
        for (size_t j = 0; j < mSinkDependencies.size(); j++) {
            if (mSinkDependencies[j].sinkGCKey == activeSinks[i]) {
                activeStreamId = mSinkDependencies[j].streamId;
                mActiveStreamId.insert(activeStreamId);
                // get the input port for this stream
                RKISP2GraphConfig::Node *port;
                RKISP2GraphConfig::Node *peer;
                status = streamGetInputPort(activeStreamId, &port);
                if (status != NO_ERROR) {
                    LOGD("Fail to get input port for this stream %d", activeStreamId);
                    continue;
                }

                status = portGetPeer(port, &peer);
                if (status != NO_ERROR) {
                    LOGE("fail to get peer for the port");
                    continue;
                }

                // get peer's stream Id
                activeStreamId = portGetStreamId(peer);
                if (activeStreamId == -1) {
                    LOGE("fail to get the stream id for %s peer port %s", NODE_NAME(port), NODE_NAME(peer));
                    continue;
                }
                if (mActiveStreamId.find(activeStreamId) == mActiveStreamId.end())
                    mActiveStreamId.insert(activeStreamId);
            }
        }
    }
}

/**
 * returns the number of buffers the ISA will produce for a given request.
 */
int32_t RKISP2GraphConfig::getIsaOutputCount() const
{
    return mIsaActiveDestinations.size();
}

bool RKISP2GraphConfig::isIsaOutputDestinationActive(uid_t destinationPortId) const
{
    std::map<uid_t, uid_t>::const_iterator it = mIsaActiveDestinations.begin();
    for (; it != mIsaActiveDestinations.end(); ++it) {
        if (destinationPortId == it->first) {
            return true;
        }
    }
    return false;
}

bool RKISP2GraphConfig::isIsaStreamActive(int32_t streamId) const
{
    if (mActiveStreamId.find(streamId) != mActiveStreamId.end())
        return true;
    return false;
}

status_t RKISP2GraphConfig::getActiveDestinations(std::vector<uid_t> &terminalIds) const
{
    std::map<uid_t, uid_t>::const_iterator it = mIsaActiveDestinations.begin();
    for (; it != mIsaActiveDestinations.end(); ++it) {
        terminalIds.push_back(it->first);
    }

    return OK;
}

/**
 * Query the connection info structs for a given pipeline defined by
 * stream id.
 *
 * \param[in] sinkName to be used as key to get pipeline connections
 * \param[out] stream id connect with sink
 * \param[out] connections for pipeline configuation
 * \return OK in case of success.
 * \return UNKNOWN_ERROR or BAD_VALUE in case of fail.
 * \if sinkName is not supported, NAME_NOT_FOUND is returned.
 * \sink name support list as below defined in graph_descriptor.xml
 * \<sink name="video0"/>
 * \<sink name="video1"/>
 * \<sink name="video2"/>
 * \<sink name="still0"/>
 * \<sink name="still1"/>
 * \<sink name="still2"/>
 * \<sink name="raw"/>
 */
status_t RKISP2GraphConfig::pipelineGetInternalConnections(
                          const std::string &sinkName,
                          int &streamId,
                          std::vector<PSysPipelineConnection> &confVector)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    status_t status = OK;
    css_err_t ret = css_err_none;
    NodesPtrVector sinks;
    NodesPtrVector programGroups;
    NodesPtrVector alreadyConnectedPorts;
    Node *peerPort = nullptr;
    Node *port = nullptr;
    PSysPipelineConnection aConnection;

    CLEAR(aConnection);

    alreadyConnectedPorts.clear();
    status = graphGetSinksByName(sinkName, sinks);
    if (CC_UNLIKELY(status != OK) || sinks.empty()) {
        LOGD("No %s sinks in graph", sinkName.c_str());
        return NAME_NOT_FOUND;
    }

    streamId = sinkGetStreamId(sinks[0]);
    if (CC_UNLIKELY(streamId <= 0)) {
        LOGE("Sink node lacks stream id attribute - fix your config");
        return BAD_VALUE;
    }

    status = streamGetProgramGroups(streamId, programGroups);
    if (CC_UNLIKELY(status != OK || programGroups.empty())) {
        LOGE("No Program groups associated with stream id %d", streamId);
        return BAD_VALUE;
    }

    for (size_t i = 0; i < programGroups.size(); i++) {

        Node::const_iterator it = programGroups[i]->begin();

        while (it != programGroups[i]->end()) {
            ret = programGroups[i]->getDescendant(GCSS_KEY_TYPE, "port",
                                                it, &port);
            if (ret != css_err_none)
                continue;

            /*
             * Since we are iterating through the ports
             * check if this port is already connected to avoid setting
             * the connection twice
             */
            if (std::find(alreadyConnectedPorts.begin(),
                          alreadyConnectedPorts.end(),
                          port) != alreadyConnectedPorts.end()) {
                continue;
            }
            LOGI("Configuring Port from PG[%zu]", i);

            status = portGetFormat(port, aConnection.portFormatSettings);
            if (CC_UNLIKELY(status != OK)) {
                LOGE("Failed to get port format info in port from PG[%zu] "
                     "from stream id %d", i, streamId);
                return BAD_VALUE;
            }
            if (aConnection.portFormatSettings.enabled == 0) {
                LOGI("Port from PG[%zu] from stream id %d disabled",
                        i, streamId);
                confVector.push_back(aConnection);
                continue;
            } else {
                LOGI("Port: 0x%x format(%dx%d)fourcc: %s bpl: %d bpp: %d",
                        aConnection.portFormatSettings.terminalId,
                        aConnection.portFormatSettings.width,
                        aConnection.portFormatSettings.height,
                        v4l2Fmt2Str(aConnection.portFormatSettings.fourcc),
                        aConnection.portFormatSettings.bpl,
                        aConnection.portFormatSettings.bpp);
            }

            /*
             * for each port get the connection info and pass it
             * to the pipeline object
             */
            status = portGetConnection(port,
                                       aConnection.connectionConfig,
                                       &peerPort);
            if (CC_UNLIKELY(status != OK )) {
                LOGE("Failed to create connection info in port from PG[%zu]"
                     "from stream id %d", i, streamId);
                return BAD_VALUE;
            }

            aConnection.hasEdgePort = false;
            if (isPipeEdgePort(port)) {
                camera3_stream_t *clientStream = nullptr;
                status = portGetClientStream(peerPort, &clientStream);
                if (CC_UNLIKELY(status != OK)) {
                    LOGE("Failed to find client stream for v-sink");
                    return UNKNOWN_ERROR;
                }
                aConnection.stream = clientStream;
                aConnection.hasEdgePort = true;
            }
            confVector.push_back(aConnection);
            alreadyConnectedPorts.push_back(port);
            alreadyConnectedPorts.push_back(peerPort);
        }
    }

    return OK;
}

/**
 * Find distinct stream ids from the graph and return them in a vector.
 * \param streamIds Vector to be populated with stream ids.
 */
status_t RKISP2GraphConfig::graphGetStreamIds(StreamsVector &streamIds)
{
    GraphConfigNode *result;
    int32_t streamId = -1;
    css_err_t ret;

    GraphConfigNode::const_iterator it = mSettings->begin();
    while (it != mSettings->end()) {
        bool found = false;
        // Find all program groups
        ret = mSettings->getDescendant(GCSS_KEY_TYPE, "hw",
                                       it, &result);
        if (ret != css_err_none)
            continue;

        ret = result->getValue(GCSS_KEY_STREAM_ID, streamId);
        if (ret != css_err_none)
            continue;

        // If stream id is not yet in vector, add it
        StreamsVector::iterator ite = streamIds.begin();
        for(; ite !=streamIds.end(); ++ite) {
            if (streamId == *ite) {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        streamIds.push_back(streamId);
    }

    if (streamIds.empty()) {
        LOGE("Failed to find any streamIds %d)", streamId);
        return UNKNOWN_ERROR;
    }
    return OK;
}

/**
 * Retrieve the stream id associated with a given sink.
 * The stream id represents the branch of the PSYS processing nodes that
 * precedes this sink.
 * This id is used for IQ tunning purposes.
 *
 * \param[in] sink sink node to query
 * \return stream id or -1 in case of error
 */
int32_t RKISP2GraphConfig::sinkGetStreamId(Node *sink)
{
    css_err_t ret = css_err_none;
    int32_t streamId = -1;
    string type;

    if (CC_UNLIKELY(sink == nullptr)) {
        LOGE("Invalid Node, cannot get the sink stream id");
        return -1;
    }

    ret = sink->getValue(GCSS_KEY_TYPE, type);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to get Node Type");
        return -1;
    }
    if (CC_UNLIKELY(type != "sink")) {
        LOGE("Node is not a sink");
        return -1;
    }
    ret = sink->getValue(GCSS_KEY_STREAM_ID, streamId);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to get stream ID");
        return -1;
    }
    return streamId;
}

int32_t RKISP2GraphConfig::portGetStreamId(Node *port)
{
    css_err_t ret = css_err_none;
    RKISP2GraphConfig::Node *ancestor = nullptr;
    int32_t streamId = -1;

    if (CC_UNLIKELY(port == nullptr)) {
        LOGE("Invalid Node, cannot get the port stream id");
        return -1;
    }
    ret = port->getAncestor(&ancestor);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to get port's ancestor");
        return -1;
    }

    ret = ancestor->getValue(GCSS_KEY_STREAM_ID, streamId);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to get stream ID %s", NODE_NAME(ancestor));
        return -1;
    }
    return streamId;
}

/**
 * Retrieve a list of program groups that belong to a  given stream id.
 * Iterates through the graph configuration storing the program groups
 * that match this stream id into the provided vector.
 *
 * \param[in] streamId Id of the stream to match.
 * \param[out] programGroups Vector with the nodes that match the criteria.
 */
status_t RKISP2GraphConfig::streamGetProgramGroups(int32_t streamId,
                                             NodesPtrVector &programGroups)
{
    css_err_t ret = css_err_none;
    GraphConfigNode *result;
    NodesPtrVector allProgramGroups;
    int32_t streamIdFound = -1;

    GraphConfigNode::const_iterator it = mSettings->begin();

    while (it != mSettings->end()) {

        ret = mSettings->getDescendant(GCSS_KEY_TYPE, "hw",
                                       it, &result);
        if (ret == css_err_none)
            allProgramGroups.push_back(result);
    }

    if (allProgramGroups.empty()) {
        LOGE("Failed to find any HW's for stream id %d"
             " BUG(check graph config file)", streamId);
        return UNKNOWN_ERROR;
    }

    for (size_t i = 0; i < allProgramGroups.size(); i++) {
        ret = allProgramGroups[i]->getValue(GCSS_KEY_STREAM_ID, streamIdFound);
        if ((ret == css_err_none) && (streamIdFound == streamId)) {
            programGroups.push_back(allProgramGroups[i]);
        }
    }

    return OK;
}

status_t RKISP2GraphConfig::streamGetInputPort(int32_t streamId, Node **port)
{
    css_err_t ret = css_err_none;
    Node *result = nullptr;
    Node *pgNode = nullptr;
    int32_t streamIdFound = -1;
    *port = nullptr;
    Node::const_iterator it = mSettings->begin();

    while (it != mSettings->end()) {

        ret = mSettings->getDescendant(GCSS_KEY_TYPE, "hw",
                                       it, &pgNode);
        if (ret != css_err_none)
            continue;

        ret = pgNode->getValue(GCSS_KEY_STREAM_ID, streamIdFound);
        if ((ret == css_err_none) && (streamIdFound == streamId)) {

            Node::const_iterator it = pgNode->begin();
            while (it != pgNode->end()) {
                ret = pgNode->getDescendant(GCSS_KEY_TYPE, "port",
                                                it, &result);
                if (ret != css_err_none)
                    continue;
                int32_t direction = portGetDirection(result);
                if (direction == PORT_DIRECTION_INPUT) {
                    /*
                     * TODO: add check that the port is at the edge of the stream
                     * this could be done checking that the peer's port is in a PG
                     * that belongs to a different stream id or to a virtual sink
                     *
                     */
                     *port = result;
                     return OK;
                }
            }
        }
    }
    return (*port == nullptr) ? BAD_VALUE : OK;
}

/**
 *
 * Traverse the graph settings to find program groups that belong to
 * the given stream id.
 * collect the output ports of those program groups whose peer has a different
 * stream ID.
 * It also stores the UID of the peer port of each output port.
 * This is useful to detect wither the peer is active or not.
 *
 * \param [in] streamId  The stream id we want to find the output ports from.
 * \param [out] outputPorts Vector of pointers to Nodes with the ports.
 * \param [out] peerPorts  Vector of pointers to Nodes with the peer ports.
 */
status_t
RKISP2GraphConfig::streamGetConnectedOutputPorts(int32_t streamId,
                                           NodesPtrVector &outputPorts,
                                           NodesPtrVector &peerPorts)
{
    css_err_t ret = css_err_none;
    status_t status = OK;
    Node *pgNode = nullptr;
    Node *port;
    Node *peer = nullptr;
    int32_t streamIdFound = -1;
    int32_t peerStreamId = 0;
    outputPorts.clear();
    peerPorts.clear();

    Node::const_iterator it = mSettings->begin();

    while (it != mSettings->end()) {
        ret = mSettings->getDescendant(GCSS_KEY_TYPE, "program_group",
                                       it, &pgNode);
        if (ret != css_err_none)
            continue;
        ret = pgNode->getValue(GCSS_KEY_STREAM_ID, streamIdFound);
        if ((ret == css_err_none) && (streamIdFound == streamId)) {

            Node::const_iterator it = pgNode->begin();

            while (it != pgNode->end()) {
                ret = pgNode->getDescendant(GCSS_KEY_TYPE, "port", it, &port);
                if (ret != css_err_none)
                    continue;

                int32_t direction = portGetDirection(port);

                if (direction == PORT_DIRECTION_OUTPUT) {
                    status = portGetPeer(port, &peer);
                    if (status == INVALID_OPERATION)
                        continue; // disabled terminal
                    if (status == OK) {
                        peerStreamId = portGetStreamId(peer);
                        if (peerStreamId != streamId) {
                            outputPorts.push_back(port);
                            peerPorts.push_back(peer);
                        }
                    }
                }
            }
        }
    }
    if (outputPorts.empty())
        LOGW("No outputports for stream %d", streamId);
    return OK;
}

/**
 * Retrieve the graph config node of the port that is connected to a given port.
 *
 * \param[in] port Node with the info of the port that we want to find its peer.
 * \param[out] peer Pointer to a node where the peer node reference will be
 *                  stored
 * \return OK
 * \return INVALID_OPERATION if the port is disabled.
 * \return BAD_VALUE if any of the graph settings is incorrect.
 */
status_t RKISP2GraphConfig::portGetPeer(Node *port, Node **peer)
{

    css_err_t ret = css_err_none;
    int32_t enabled = 1;
    string peerName;

    if (CC_UNLIKELY(port == nullptr || peer == nullptr)) {
       LOGE("Invalid Node, cannot get the peer port");
       return BAD_VALUE;
    }
    ret = port->getValue(GCSS_KEY_ENABLED, enabled);
    if (ret == css_err_none && !enabled) {
        LOGI("This port is disabled, keep on getting the connection");
        return INVALID_OPERATION;
    }
    ret = port->getValue(GCSS_KEY_PEER, peerName);
    if (ret != css_err_none) {
        LOGE("Error getting peer attribute");
        return BAD_VALUE;
    }
    ret = mSettings->getDescendantByString(peerName, peer);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to find peer by name %s", peerName.c_str());
        return BAD_VALUE;
    }
    return OK;
}

/**
 * Generate the connection configuration information for a given port.
 *
 * This connection configuration  information is required by CIPF to build
 * the pipeline
 *
 * \param[in] port Pointer to the port node
 * \param[out] connectionInfo Reference to the connection info object
 * \param[out] peerPort Reference to the peer port
 * \return OK in case of success
 * \return BAD_VALUE in case of error while retrieving the information.
 * \return INVALID_OPERATION in case of the port being disabled.
 */
status_t RKISP2GraphConfig::portGetConnection(Node *port,
                                        ConnectionConfig &connectionInfo,
                                        Node **peerPort)
{
    status_t status = OK;
    css_err_t ret = css_err_none;
    int32_t direction;

    status = portGetPeer(port, peerPort);
    if (CC_UNLIKELY(status != OK)) {
        if (status == INVALID_OPERATION) {
            LOGE("Port %s disabled, cannot get the connection",
                    getNodeName(port).c_str());
        } else {
            LOGE("Failed to get the peer port for port %s",getNodeName(port).c_str());
        }
        return status;
    }

    ret = port->getValue(GCSS_KEY_DIRECTION, direction);
    if (CC_UNLIKELY(ret != css_err_none)) {
       LOGE("Failed to get port direction");
       return BAD_VALUE;
    }

    /*
     * Iterations are not used
     */
    connectionInfo.mSinkIteration = 0;
    connectionInfo.mSourceIteration = 0;

    if (direction == PORT_DIRECTION_INPUT) {
        // input port is the sink in a connection
        status = portGetFourCCInfo(*port,
                                    connectionInfo.mSinkStage,
                                    connectionInfo.mSinkTerminal);
        if (CC_UNLIKELY(status != OK)) {
            LOGE("Failed to create fourcc info for sink port");
            return BAD_VALUE;
        }
        if (*peerPort != nullptr && !portIsVirtual(*peerPort)) {
            status = portGetFourCCInfo(**peerPort,
                                       connectionInfo.mSourceStage,
                                       connectionInfo.mSourceTerminal);
            if (CC_UNLIKELY(status != OK)) {
                LOGE("Failed to create fourcc info for source port");
                return BAD_VALUE;
            }
        } else {
            connectionInfo.mSourceStage = 0;
            connectionInfo.mSourceTerminal = 0;
        }
    } else {
        //output port is the source in a connection
        status = portGetFourCCInfo(*port,
                                    connectionInfo.mSourceStage,
                                    connectionInfo.mSourceTerminal);
        if (CC_UNLIKELY(status != OK)) {
            LOGE("Failed to create fourcc info for source port");
            return BAD_VALUE;
        }

        if (*peerPort != nullptr && !portIsVirtual(*peerPort)) {
            status = portGetFourCCInfo(**peerPort,
                                    connectionInfo.mSinkStage,
                                    connectionInfo.mSinkTerminal);
            if (CC_UNLIKELY(status != OK)) {
                LOGE("Failed to create fourcc info for sink port");
                return BAD_VALUE;
            }
        } else {
            connectionInfo.mSinkStage = 0;
            connectionInfo.mSinkTerminal = 0;
        }
    }

    return status;
}

/**
 * Retrieve the format information of a port
 * if the port doesn't have any format set, it gets the format from the peer
 * port (i.e. the port connected to this one)
 *
 * \param[in] port Port to query the format.
 * \param[out] format Format settings for this port.
 */
status_t RKISP2GraphConfig::portGetFormat(Node *port,
                                    PortFormatSettings &format)
{
    GraphConfigNode *peerNode = nullptr; // The peer port node
    GraphConfigNode *tmpNode = port; // The port node node we are interrogating
    css_err_t ret = css_err_none;
    uint32_t stageId; // ignored

    if (CC_UNLIKELY(port == nullptr)) {
        LOGE("Invalid parameter, could not get port format");
        return BAD_VALUE;
    }

    ret = port->getValue(GCSS_KEY_ENABLED, format.enabled);
    if (ret != css_err_none) {
        // if not present by default is enabled
        format.enabled = 1;
    }

    status_t status = portGetFourCCInfo(*tmpNode, stageId, format.terminalId);
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Could not get port uid");
        return INVALID_OPERATION;
    }

    // if disabled there is no need to query the format
    if (format.enabled == 0) {
        return OK;
    }

    format.width = 0;
    format.height = 0;

    ret = port->getValue(GCSS_KEY_WIDTH, format.width);
    if (ret != css_err_none) {
        /*
         * It could be the port configuration is not in settings, that is normal
         * it means that we need to ask the format from the peer.
         */
        ret = portGetPeer(port, &peerNode);
        if (CC_UNLIKELY(ret != css_err_none)) {
            LOGE("Could not find peer port - Fix your graph");
            return BAD_VALUE;
        }

        tmpNode = peerNode;

        ret = tmpNode->getValue(GCSS_KEY_WIDTH, format.width);
        if (CC_UNLIKELY(ret != css_err_none)) {
            LOGE("Could not find port format info: width (from peer)");
            return BAD_VALUE;
        }
    }

    ret = tmpNode->getValue(GCSS_KEY_HEIGHT, format.height);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Could not find port format info: height");
        return BAD_VALUE;
    }

    string fourccFormat;
    ret = tmpNode->getValue(GCSS_KEY_FORMAT, fourccFormat);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Could not find port format info: fourcc");
        return BAD_VALUE;
    }

    format.fourcc = get_fourcc(fourccFormat[0],fourccFormat[1],
                              fourccFormat[2],fourccFormat[3]);

    format.bpl = gcu::getBpl(format.fourcc, format.width);
    LOGI("bpl set to %d for %s", format.bpl, fourccFormat.c_str());

    // if settings are specifying bpl, owerwrite the calculated one
    int bplFromSettings = 0;
    ret = tmpNode->getValue(GCSS_KEY_BYTES_PER_LINE, bplFromSettings);
    if (CC_UNLIKELY(ret == css_err_none)) {
        LOGI("Overwriting bpl(%d) from settings %d", format.bpl, bplFromSettings);
        format.bpl = bplFromSettings;
    }

    format.bpp = gcu::getBppFromCommon(format.fourcc);

    return OK;
}

/**
 * Return the port direction
 *
 * \param[in] port Reference to port Graph node
 * \return 0 if it is an input port
 * \return 1 if it is an output port
 */
int32_t RKISP2GraphConfig::portGetDirection(Node *port)
{
    int32_t direction = 0;
    css_err_t ret = css_err_none;
    ret = port->getValue(GCSS_KEY_DIRECTION, direction);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to retrieve port direction, default to input");
    }

    return direction;
}

/**
 * Return the port full name
 * The port full name is made out from:
 * - the name program group it belongs to
 * - the name of the port
 * separated by ":"
 *
 * \param[in] port Reference to port Graph node
 * \param[out] fullName reference to a string to store the full name
 *
 * \return OK if everything went fine.
 * \return BAD_VALUE if any of the graph queries failed.
 */
status_t RKISP2GraphConfig::portGetFullName(Node *port, string &fullName)
{
    string portName, ancestorName;
    Node *ancestor;
    css_err_t ret = css_err_none;

    if (CC_UNLIKELY(port == nullptr)) {
        LOGE("Invalid parameter, could not get port full name");
        return BAD_VALUE;
    }

    ret = port->getAncestor(&ancestor);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to retrieve port ancestor");
        return BAD_VALUE;
    }
    ret = ancestor->getValue(GCSS_KEY_NAME, ancestorName);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to get ancestor name for port");
        port->dumpNodeTree(port, 1);
        return BAD_VALUE;
    }

    ret = port->getValue(GCSS_KEY_NAME, portName);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to retrieve port name");
        return BAD_VALUE;
    }

    fullName = ancestorName + ":" + portName;
    return OK;
}

/**
 * Return true if the port is a virtual port, this is the end point
 * of the graph.
 * Virtual ports are the nodes of type sink.
 *
 * \param[in] port Reference to port Graph node
 * \return true if it is a virtual port
 * \return false if it is not a virtual port
 */
bool RKISP2GraphConfig::portIsVirtual(Node *port)
{
    string type;
    css_err_t ret = css_err_none;
    ret = port->getValue(GCSS_KEY_TYPE, type);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to retrieve port type, default to input");
    }

    return (type == string("sink"));
}

/**
 * For a given port node it constructs the fourCC code used in the connection
 * object. This is constructed from the program group id.
 *
 * \param[in] portNode
 * \param[out] stageId Fourcc code that describes the PG where this node is
 *                     contained
 * \param[out] terminalID Fourcc code that describes the port, in FW jargon,
 *                        this is a PG terminal.
 * \return OK in case of no error
 * \return BAD_VALUE in case some error is found
 */
status_t RKISP2GraphConfig::portGetFourCCInfo(Node &portNode,
                                        uint32_t &stageId, uint32_t &terminalId)
{
    Node *pgNode; // The Program group node
    css_err_t ret = css_err_none;
    int32_t portId;
    string type, subsystem;

    ret = portNode.getValue(GCSS_KEY_ID, portId);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to get port's id");
        portNode.dumpNodeTree(&portNode, 1);
        return BAD_VALUE;
    }

    ret = portNode.getAncestor(&pgNode);
    if (CC_UNLIKELY(ret != css_err_none || pgNode == nullptr)) {
       LOGE("Failed to get port ancestor");
       return BAD_VALUE;
    }

    ret = pgNode->getValue(GCSS_KEY_TYPE, type);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to get port's ancestor type ");
        pgNode->dumpNodeTree(pgNode, 1);
        return BAD_VALUE;
    }

    ret = pgNode->getValue(GCSS_KEY_SUBSYSTEM, subsystem);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to get port's ancestor subsystem ");
        pgNode->dumpNodeTree(pgNode, 1);
        return BAD_VALUE;
    }

    if (type == string("hw")) {
        stageId = 0;
        terminalId = portId;
    }
    return OK;
}

/**
 * Return the the terminal id of the peer port.
 *
 * Given a name of a port in canonical format (i.e. isa:non_scaled_ouptut)
 * this method returns the terminal uid (the fourcc code) associated with its
 * peer port.
 *
 * \param[in] name Port name in canonical format
 * \param[out] terminalId Terminal id of the peer port
 *
 * \return BAD_VALUE in case name is empty
 * \return INVALID_OPERATION if the port or peer was not found in the graph
 * \return OK
 */
status_t RKISP2GraphConfig::portGetPeerIdByName(string name,
                                          uid_t &terminalId)
{
    uid_t stageId; // not used
    css_err_t ret;
    status_t retErr;
    Node *portNode = nullptr;
    Node *peerNode = nullptr;

    if (name.empty())
        return BAD_VALUE;

    ret = mSettings->getDescendantByString(name, &portNode);
    if (CC_UNLIKELY(ret != css_err_none)) {
       LOGE("Failed to find port %s.", name.c_str());
       return INVALID_OPERATION;
    }

    retErr = portGetPeer(portNode, &peerNode);
    if (ret != OK || peerNode == nullptr) {
        LOGE("Failed to find peer for port %s.", name.c_str());
        return INVALID_OPERATION;
    }

    portGetFourCCInfo(*peerNode, stageId, terminalId);
    return OK;
}

/**
 * This method is used by pSysIsaTask to get the stream ids
 * which are used in setting, at the same time return the
 * mIsaOutputPort2StreamId
 * \param[out] isaStreamIdVector
 * \param[out] isaOutputPort2StreamIdMap
 */
status_t RKISP2GraphConfig::getIsaStreamIds(vector<int32_t> &isaStreamIdVector,
                                      map<string, int32_t> &isaOutputPort2StreamIdMap)
{
    for (auto isaOutputPort : mIsaOutputPort2StreamId) {
         int32_t streamIdFound = isaOutputPort.second;
         // save the stream id into the vector
         unsigned int i = 0;
         for (; i < isaStreamIdVector.size(); i ++) {
              if (isaStreamIdVector[i] == streamIdFound) {
                  break;
              }
         }
         if (i == isaStreamIdVector.size())
             isaStreamIdVector.push_back(streamIdFound);
    }

    if (isaStreamIdVector.size() == 0) {
        LOGE("Fail to get stream id");
        return UNKNOWN_ERROR;
    }
    isaOutputPort2StreamIdMap = mIsaOutputPort2StreamId;
    return OK;
}

/**
 * retrieve the pointer to the client stream associated with a virtual sink
 *
 * I.e. access the mapping done at stream config time between the pointers
 * to camera3_stream_t and the names video0, video1, still0 etc...
 *
 * \param[in] port Node to the virtual sink (with name videoX or stillX etc..)
 * \param[out] stream Pointer to the client stream associated with that virtual
 *                    sink.
 * \return OK
 * \return BAD_VALUE in case of invalid parameters (null pointers)
 * \return INVALID_OPERATION in case the Node is not a virtual sink.
 */
status_t RKISP2GraphConfig::portGetClientStream(Node *port, camera3_stream_t **stream)
{
    css_err_t ret = css_err_none;
    string portName;

    if (CC_UNLIKELY(!port || !stream)) {
        LOGE("Could not get client stream - bad parameters");
        return BAD_VALUE;
    }

    if (!portIsVirtual(port)) {
        LOGE("Trying to find the client stream from a non virtual port");
        return INVALID_OPERATION;
    }

    ret = port->getValue(GCSS_KEY_NAME, portName);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Failed to get name for port");
        port->dumpNodeTree(port, 1);
        return BAD_VALUE;
    }

    uid_t vPortId = GCSS::ItemUID::str2key(portName);
    /* *stream = mManager->getStreamByVirtualId(vPortId); */

    return OK;
}

/**
 * A port is at the edge of the video stream (pipeline) if its peer is in a PG
 * that has a different streamID (a.k.a. pipeline id) or if its peer is a
 * virtual sink.
 *
 * Here we check for both conditions and return true if this port is at either
 * edge of a pipeline
 */
bool RKISP2GraphConfig::isPipeEdgePort(Node *port)
{
    status_t status = OK;
    css_err_t ret = css_err_none;
    RKISP2GraphConfig::Node *peer = nullptr;
    RKISP2GraphConfig::Node *peerAncestor = nullptr;
    int32_t portDirection;
    int32_t streamId = -1;
    int32_t peerStreamId = -1;
    string peerType;

    portDirection = portGetDirection(port);

    status = portGetPeer(port, &peer);
    if (status == INVALID_OPERATION) {
        LOGI("port is disabled, so it is an edge port");
        return true;
    }
    if (CC_UNLIKELY(status != OK)) {
        LOGE("Failed to create fourcc info for source port");
        return false;
    }

    streamId = portGetStreamId(port);
    if (CC_UNLIKELY(streamId < 0))
        return false;
    /*
     * get the stream id of the peer port
     * we also check the ancestor for that. If the peer is a virtual sink then
     * it does not have ancestor.
     */
    if (!portIsVirtual(peer)) {
        ret = peer->getAncestor(&peerAncestor);
        if (CC_UNLIKELY(ret != css_err_none)) {
            LOGE("Failed to get peer's ancestor");
            return false;
        }
        ret = peerAncestor->getValue(GCSS_KEY_STREAM_ID, peerStreamId);
        if (CC_UNLIKELY(ret != css_err_none)) {
            LOGE("Failed to get stream ID of peer PG");
            return false;
        }
        /*
         * Retrieve the type of node the peer ancestor is. It could be is not a
         * program group node but a sink or hw block
         */
        peerAncestor->getValue(GCSS_KEY_TYPE, peerType);
    }

    if (portDirection == RKISP2GraphConfig::PORT_DIRECTION_INPUT) {
        /*
         *  input port,
         *  if the peer is a source or hw block then it is on the edge,
         *  or if the peer is on a different stream id
         */
        if ((streamId != peerStreamId) || (peerType == string("hw"))) {
            return true;
        }
    } else {
        /*
         *  output port,
         *  if the peer is a virtual port, or has a different stream id
         *  then it is on the edge,
         */
        if (portIsVirtual(peer) || (streamId != peerStreamId)) {
            return true;
        }
    }
    return false;
}

/**
 * Parse the information of the sensor node in the graph and store it in the
 * provided SourceNodeInfo struct.
 *
 * \param[in] sensorNode  Pointer to the graph config node that represents
 *                        the sensor.
 * \param[out] info  Data structure where the information is stored.
 */
status_t RKISP2GraphConfig::parseSensorNodeInfo(Node* sensorNode,
                                          SourceNodeInfo &info)
{
    status_t retErr = OK;
    css_err_t ret = css_err_none;
    string metadata;
    string tmp;

    ret = sensorNode->getValue(GCSS_KEY_CSI_PORT, info.csiPort);
    if (CC_UNLIKELY(ret  !=  css_err_none)) {
       LOGE("Error: Couldn't get csi port from the graph");
       // DVP sensor has no csiPort
       //return UNKNOWN_ERROR;
    }

    ret = sensorNode->getValue(GCSS_KEY_SENSOR_NAME, info.name);
    if (CC_UNLIKELY(ret  !=  css_err_none)) {
       LOGE("Error: Couldn't get sensor name from sensor");
       return UNKNOWN_ERROR;
    }

    ret = sensorNode->getValue(GCSS_KEY_LINK_FREQ, info.link_freq);
    if (CC_UNLIKELY(ret !=  css_err_none)) {
        info.link_freq = "0"; // default to zero
    }

    // Find i2c address for the sensor from sensor info
    const CameraHWInfo *camHWInfo = PlatformData::getCameraHWInfo();
    for (size_t i = 0; i < camHWInfo->mSensorInfo.size(); i++) {
        if (camHWInfo->mSensorInfo[i].mSensorName == info.name)
            info.i2cAddress = camHWInfo->mSensorInfo[i].mI2CAddress;
    }
    if (info.i2cAddress == "") {
        LOGW("Couldn't get i2c address from Platformdata");
        //return UNKNOWN_ERROR;
    }

    ret = sensorNode->getValue(GCSS_KEY_METADATA, metadata);
    if (CC_UNLIKELY(ret  !=  css_err_none)) {
       LOGE("Error: Couldn't get metadata enabled from sensor");
       return UNKNOWN_ERROR;
    }
    info.metadataEnabled = atoi(metadata.c_str());

    ret = sensorNode->getValue(GCSS_KEY_MODE_ID, info.modeId);
    if (CC_UNLIKELY(ret  !=  css_err_none)) {
        LOGE("Error: Couldn't get sensor mode id from sensor");
        return UNKNOWN_ERROR;
    }

    ret = sensorNode->getValue(GCSS_KEY_BAYER_ORDER, info.nativeBayer);
    if (CC_UNLIKELY(ret != css_err_none)) {
        LOGE("Error: Couldn't get native bayer order from sensor");
        // SOC sensor has no bayer_order
        //return UNKNOWN_ERROR;
    }

    retErr = getDimensions(sensorNode, info.output.w, info.output.h);
    if (retErr != OK) {
        LOGE("Error: Couldn't get values from sensor");
        return UNKNOWN_ERROR;
    }

    ret = sensorNode->getValue(GCSS_KEY_INTERLACED, tmp);
    if (ret != css_err_none) {
        LOGW("Couldn't get interlaced field from sensor");
    } else {
        info.interlaced = atoi(tmp.c_str());
    }

    // v-flip is not mandatory. Some sensors may not have this control
    ret = sensorNode->getValue(GCSS_KEY_VFLIP, info.verticalFlip);

    // h-flip is not mandatory. Some sensors may not have this control
    ret = sensorNode->getValue(GCSS_KEY_HFLIP, info.horizontalFlip);

    Node *port0Node = nullptr;
    ret = sensorNode->getDescendant(GCSS_KEY_PORT_0, &port0Node);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get port_0");
        return UNKNOWN_ERROR;
    }
    ret = port0Node->getValue(GCSS_KEY_FORMAT, tmp);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get format from the graph");
        return UNKNOWN_ERROR;
    }
    /* find mbus format from common format in settings */
    info.output.mbusFormat = gcu::getMBusFormat(get_fourcc(tmp[0], tmp[1],
                                                          tmp[2], tmp[3]));

    /*
     * Get size and cropping from pixel array to use in format and selection
     */
    Node *pixelArrayOutput = nullptr;
    ret = sensorNode->getDescendantByString("pixel_array:output",
                                            &pixelArrayOutput);
    if (CC_UNLIKELY(ret  !=  css_err_none)) {
        LOGE("Error: Couldn't get pixel array node from the graph");
        return UNKNOWN_ERROR;
    }

    retErr = getDimensions(pixelArrayOutput, info.pa.out.w,
                                             info.pa.out.h,
                                             info.pa.out.l,
                                             info.pa.out.t);
    if (retErr != OK) {
        LOGE("Error: Couldn't get values from pixel array output");
        return UNKNOWN_ERROR;
    }

    if (info.i2cAddress == "")
        info.pa.name = info.name;
    else
        info.pa.name = info.name+ " " + info.i2cAddress;
    /* Populate the formats for each subdevice
     * The format for the Pixel Array is determined by the native bayer order
     * and the bpp selected by the settings.
     * We extract the bpp from the format in the sensor port.
     *
     * The format in the sensor output port may be different that the
     * pixel array format because the sensor may be changing the effective
     * bayer order by flipping or internal cropping
     * */
    int32_t bpp = gcu::getBpp(info.output.mbusFormat);
    info.pa.out.mbusFormat = gcu::getMBusFormat(info.nativeBayer, bpp);
    return OK;
}

/*
 * Create mediaCtlConfig structure to configure sensor mode. All settings are
 * retrieved from graph config settings and applied one by one.
 *
 * TODO a lot of string values because of android gcss keys, which does not
 * have support for ints. Some could be moved to gcss_keys
 *
 * \param[out] mediaCtlConfig  the struct to populate
 * \return OK              at success
 * \return UNKNOWN ERROR   at failure
 */

status_t RKISP2GraphConfig::getMediaCtlData(MediaCtlConfig *mediaCtlConfig)
{
    CheckError((!mediaCtlConfig), BAD_VALUE, "@%s null ptr\n", __FUNCTION__);

    ConfigProperties cameraProps;      // camera properties
    css_err_t ret = css_err_none;
    status_t retErr = OK;
    string formatStr;
    SourceNodeInfo sourceInfo;
    Node *sourceNode = nullptr;

    string csi2;
    if (mSourceType == SRC_SENSOR) {
        ret = mSettings->getDescendant(GCSS_KEY_SENSOR, &sourceNode);
        if (ret != css_err_none) {
            LOGE("Error: Couldn't get sensor node from the graph");
            return UNKNOWN_ERROR;
        }
        retErr = parseSensorNodeInfo(sourceNode, sourceInfo);
        if (retErr != OK) {
            LOGE("Error: Couldn't get sensor node info");
            return UNKNOWN_ERROR;
        }

        // get the next entity port of the "ov5670 binner 11-0010" which is dynamical.
        // it could "rkisp1-csi2 0" or "rkisp1-csi2 1", then it will get 0 or 1.
        int port = 0;
        std::shared_ptr<MediaEntity> entity = nullptr;
        string entityName;
        if (sourceInfo.i2cAddress == "")
            entityName = sourceInfo.name;
        else
            entityName = sourceInfo.name + " " + sourceInfo.i2cAddress;

        LOGI("entityName:%s\n", entityName.c_str());
        status_t ret = mMediaCtl->getMediaEntity(entity, entityName.c_str());
        if (ret != NO_ERROR) {
            LOGE("@%s, fail to call getMediaEntity, ret:%d\n", __FUNCTION__, ret);
            return UNKNOWN_ERROR;
        }
        std::vector<media_link_desc> links;
        entity->getLinkDesc(links);
        LOGI("@%s, links number:%zu\n", __FUNCTION__, links.size());
        if (links.size()) {
            struct media_pad_desc* pad = &links[0].sink;
            LOGI("@%s, sink entity:%d, flags:%d, index:%d\n",
                __FUNCTION__, pad->entity, pad->flags, pad->index);
            struct media_entity_desc entityDesc;
            mMediaCtl->findMediaEntityById(pad->entity, entityDesc);
            LOGI("@%s, name:%s\n", __FUNCTION__, entityDesc.name);
            string name = entityDesc.name;
            // check if mipi or DVP interface
            std::size_t mipi = name.find("dphy");
            if (mipi == std::string::npos) {
                sourceInfo.dvp = true;
                mCSIBE = entityName;
                if (name.find("cif") != std::string::npos)
                   mSensorLinkedToCIF = true;
            } else {
                std::size_t p = name.find(" ");
                if (p != std::string::npos) {
                    string s;
                    s.append(name, p + 1, 1);
                    port = std::stoi(s);
                    LOGI("@%s, port:%d\n", __FUNCTION__, port);
                }
                // get csi2 and cio2 names
                //csi2 = csi2_without_port + std::to_string(port);
                //mCSIBE = CSI_BE + std::to_string(port);
                csi2 = csi2_without_port;
                mCSIBE = CSI_BE;
                /* TODO: should config according to real topology, sensor to isp link is decided by the sensor interface,
                * link is different for mipi and dvp.
                *  mipi sensor --- > csi ---> isp
                * dvp sensor ----> isp
                */
                addLinkParams(entityName, 0, csi2, 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
                LOGI(" csi2 is:%s, cio2 is:%s\n", csi2.c_str(), mCSIBE.c_str());
            }
        }
    } else {
        LOGE("Error: No source");
        return UNKNOWN_ERROR;
    }

#if 0//needn't get the default expsure here, get it from 3a lib instead
    // Add control params
    retErr = addControls(sourceNode, sourceInfo, mediaCtlConfig);
    if (retErr != OK) {
        return UNKNOWN_ERROR;
    }
#endif
    /*
     * Add Camera properties to mediaCtlConfig
     */
    if (!sourceInfo.dvp) {
        int id = 0;
        ret = sourceNode->getValue(GCSS_KEY_ID, id);
        if (ret != css_err_none) {
            LOGE("Error: Couldn't get sensor id from sensor");
            return UNKNOWN_ERROR;
        }
        std::string cameraName = sourceInfo.name + " " + sourceInfo.modeId;
        cameraProps.outputWidth = sourceInfo.output.w;
        cameraProps.outputHeight = sourceInfo.output.h;
        cameraProps.id = id;
        cameraProps.name = cameraName.c_str();
        mediaCtlConfig->mCameraProps = cameraProps;

        mediaCtlConfig->mFTCSize.Width = sourceInfo.output.w;
        mediaCtlConfig->mFTCSize.Height = sourceInfo.output.h;

        Node *pixelFormatterIn = nullptr, *pixelFormatterOut = nullptr;
        Node *csiBEOutput = nullptr, *csiBESocOutput = nullptr;
        int pfInW = 0, pfInH = 0, pfOutW = 0, pfOutH = 0, pfLeft = 0, pfTop = 0;
        bool pfPresent = false;

        // Get csi_be node. If not found, try csi_be_soc. If not found return error.
        ret = mSettings->getDescendantByString("csi_be:output", &csiBEOutput);
        if (ret != css_err_none) {

            ret = mSettings->getDescendantByString("csi_be_soc:output", &csiBESocOutput);
            if (ret != css_err_none) {
                LOGE("Error: Couldn't get csi_be or csi_be_soc nodes from the graph");
                return UNKNOWN_ERROR;
            }
            // get format from _soc
            ret = csiBESocOutput->getValue(GCSS_KEY_FORMAT, formatStr);
            if (ret != css_err_none) {
                LOGE("Error: Couldn't get format from the graph");
                return UNKNOWN_ERROR;
            }
        } else {
            ret = csiBEOutput->getValue(GCSS_KEY_FORMAT, formatStr);
            if (ret != css_err_none) {
                LOGE("Error: Couldn't get format from the graph");
                return UNKNOWN_ERROR;
            }
        }

        /* sanity check, we have at least one CSI-BE */
        if (CC_UNLIKELY(csiBESocOutput == nullptr && csiBEOutput == nullptr)) {
            LOGE("Error: CSI BE Output nullptr");
            return UNKNOWN_ERROR;
        }

        std::string pixelFormatterInput = "bxt_pixelformatter:input";
        std::string pixelFormatterOutput = "bxt_pixelformatter:output";
        std::string inputPort;
        std::string outputPort;
        if (csiBEOutput != nullptr) {
            inputPort = "csi_be:" + pixelFormatterInput;
            outputPort = "csi_be:" + pixelFormatterOutput;
        } else {
            inputPort = "csi_be_soc:" + pixelFormatterInput;
            outputPort = "csi_be_soc:" + pixelFormatterOutput;
        }

        /* Get cropping values from the pixel formatter input. Output resolution
         * comes from the csi be output. Some graphs may not use pixel formatter.*/
        ret = mSettings->getDescendantByString(inputPort,
                                                &pixelFormatterIn);
        if (ret != css_err_none) {
            LOGW("Couldn't get pixel formatter input, skipping");
        } else {
            pfPresent = true;
            ret = mSettings->getDescendantByString(outputPort,
                                                    &pixelFormatterOut);
            if (ret != css_err_none) {
                LOGE("Error: Couldn't get pixel formatter output");
                return UNKNOWN_ERROR;
            }

            retErr = getDimensions(pixelFormatterIn, pfInW, pfInH, pfLeft, pfTop);
            if (retErr != OK) {
                LOGE("Error: Couldn't get values from pixel formatter input");
                return UNKNOWN_ERROR;
            }

            retErr = getDimensions(pixelFormatterOut, pfOutW, pfOutH);
            if (retErr != OK) {
                LOGE("Error: Couldn't get values from pixel formatter output");
                return UNKNOWN_ERROR;
            }
        }

        int csiBEOutW = 0, csiBEOutH = 0;
        int csiBESocOutW = 0, csiBESocOutH = 0;
        if (csiBEOutput != nullptr) {
            retErr = getDimensions(csiBEOutput, csiBEOutW,csiBEOutH);
            if (retErr != OK) {
                LOGE("Error: Couldn't values from csi be output");
                return UNKNOWN_ERROR;
            }
        } else {
            retErr = getDimensions(csiBESocOutput, csiBESocOutW, csiBESocOutH);
            if (retErr != OK) {
                LOGE("Error: Couldn't get values from csi be soc out");
                return UNKNOWN_ERROR;
            }
            LOGI("pfInW:%d, pfLeft:%d, pfTop:%d,pfOutW:%d,pfOutH:%d,csiBESocOutW:%d,csiBESocOutH:%d",
                  pfInW, pfLeft, pfTop, pfOutW, pfOutH, csiBESocOutW,csiBESocOutH);
        }

        /* Boolean to tell whether there is pixel formatter cropping.
         * This affects which selections are made */
        bool pixelFormatter = false;
        if (pfInW != pfOutW || pfInH != pfOutH || pfLeft != 0 || pfTop != 0)
            pixelFormatter = true;

        /*
         * If CSI BE SOC is not used, we must have ISA. Get video crop, scaled and
         * non scaled output from ISA and apply the formats. Otherwise add formats
         * for CSI BE SOC.
         */
        Node *isaNode = nullptr;
        Node *cropVideoIn = nullptr, *cropVideoOut = nullptr;
        int videoCropW = 0, videoCropH = 0, videoCropT = 0, videoCropL = 0;
        int videoCropOutW = 0, videoCropOutH = 0;

        /*
         * First get and set values when CSI BE SOC is not used
         */
        if (csiBESocOutput == nullptr) {
            ret = mSettings->getDescendant(GCSS_KEY_CSI_BE, &isaNode);
            if (ret != css_err_none) {
                LOGE("Error: Couldn't get isa node");
                return UNKNOWN_ERROR;
            }

            // Check if there is video cropping available. It is zero as default.
            ret = isaNode->getDescendantByString("csi_be:output",
                                                 &cropVideoOut);
            if (ret == css_err_none) {
                ret = isaNode->getDescendantByString("csi_be:input",
                                                     &cropVideoIn);
            }
            if (ret == css_err_none) {
                retErr = getDimensions(cropVideoIn, videoCropW, videoCropH,
                        videoCropL, videoCropT);
                if (retErr != OK) {
                    LOGE("Error: Couldn't get values from crop video input");
                    return UNKNOWN_ERROR;
                }
                retErr = getDimensions(cropVideoOut, videoCropOutW, videoCropOutH);
                if (retErr != OK) {
                    LOGE("Error: Couldn't get values from crop video output");
                    return UNKNOWN_ERROR;
                }
            }
        }

        // rkisp1-csi2 0 or 1
        addFormatParams(csi2, csiBESocOutW, csiBESocOutH,
                        0, sourceInfo.output.mbusFormat, 0, 0, mediaCtlConfig);
        addFormatParams(csi2, csiBESocOutW, csiBESocOutH,
                        1, sourceInfo.output.mbusFormat, 0, 0, mediaCtlConfig);
    }
    /* Set sensor pixel array parameter to the attributes in 'sensor_mode' node,
     * ignore the attributes in pixel_array and binner node due to upstream driver
     * removed binner and scaler subdev.
     */
    addFormatParams(sourceInfo.pa.name, sourceInfo.output.w, sourceInfo.output.h,
                    0, sourceInfo.output.mbusFormat, 0, 0, mediaCtlConfig);

    /* Start populating selections into mediaCtlConfig
     * entity name, width, height, left crop, top crop, target, pad, config */
    addSelectionParams(sourceInfo.pa.name,
                       sourceInfo.pa.out.w,
                       sourceInfo.pa.out.h,
                       sourceInfo.pa.out.l,
                       sourceInfo.pa.out.t,
                       V4L2_SEL_TGT_CROP, 0 /* sink pad */, mediaCtlConfig);

    //if (gDumpType & CAMERA_DUMP_MEDIA_CTL)
        dumpMediaCtlConfig(*mediaCtlConfig);

    return OK;
}

status_t RKISP2GraphConfig::getNodeInfo(const ia_uid uid, const Node &parent, int* width, int* height)
{
    status_t status = OK;

    Node *node = nullptr;
    status = parent.getDescendant(uid, &node);
    if (status != css_err_none) {
        LOGE("pipe log <%s> node is not present in graph (descriptor or settings) - continuing.",GCSS::ItemUID::key2str(uid));
        return UNKNOWN_ERROR;
    }
    status = node->getValue(GCSS_KEY_WIDTH, *width);
    if (status != css_err_none) {
        LOGE("pipe log Could not get width for <%s>", NODE_NAME(node));
        return UNKNOWN_ERROR;
    }

    if (width == 0) {
        LOGE("pipe log Could not get width for <%s>", NODE_NAME(node));
        return UNKNOWN_ERROR;
    }

    status = node->getValue(GCSS_KEY_HEIGHT, *height);
    if (status != css_err_none) {
        LOGE("pipe log Could not get height for <%s>", NODE_NAME(node));
        return UNKNOWN_ERROR;
    }

    if (height == 0) {
        LOGE("pipe log Could not get height for <%s>", NODE_NAME(node));
        return UNKNOWN_ERROR;
    }

    return status;
}

void RKISP2GraphConfig::limitPathOutputSize(uint32_t& path_out_w, uint32_t& path_out_h) {
    uint32_t limit_w;

    limit_w = path_out_w > PP_MAX_WIDTH ? PP_MAX_WIDTH : path_out_w;
    if (limit_w < path_out_w) {
        path_out_h = limit_w * path_out_h / path_out_w;
        path_out_w = limit_w;
    }
}

void RKISP2GraphConfig::isNeedPathCrop(uint32_t path_input_w,
                             uint32_t path_input_h,
                             bool sp_enabled,
                             std::vector<camera3_stream_t*>& outputStream,
                             bool& mp_need_crop,
                             bool& sp_need_crop) {
    // filter the same size
    std::vector<camera3_stream_t*> filterStream;
    for (auto s : outputStream) {
        bool filter = false;
        for (auto f : filterStream) {
            if (s->width == f->width &&
                s->height == f->height) {
                filter = true;
                break;
            }
        }

        if (!filter)
            filterStream.push_back(s);
    }

    std::sort(filterStream.begin(), filterStream.end(),
              [] (camera3_stream_t* s1, camera3_stream_t* s2) {
                return s1->width > s2->width;
              });

    float source_ratio = (float)path_input_w / path_input_h;
    std::set<float> str_ratiov;

    for (auto s : filterStream)
        str_ratiov.insert((float)(s->width) / s->height);

    if (!sp_enabled) {
        if (str_ratiov.size() > 1) {
            mp_need_crop = false;
            sp_need_crop = false;
        } else {
            mp_need_crop = true;
            sp_need_crop = false;
        }
    } else {
        if (str_ratiov.size() > 2) {
            mp_need_crop = false;
            sp_need_crop = false;
        } else {
            mp_need_crop = true;
            sp_need_crop = true;
        }
    }

    for (auto dst_ratio : str_ratiov) {
        LOGD("@ %s : stream ratios: %f", __FUNCTION__, dst_ratio);
        if (source_ratio > dst_ratio) {
            LOGW("width may be cropped, may have FOV issue,"
                 "source_ratio %f, dst_ratio %f!",
                 source_ratio, dst_ratio);
            break;
        }
    }

    LOGD("@ %s : mp_need_crop %d, sp_need_crop %d, sp_enabled %d",
         __FUNCTION__, mp_need_crop, sp_need_crop, sp_enabled);
}

void RKISP2GraphConfig::cal_crop(uint32_t &src_w, uint32_t &src_h, uint32_t &dst_w, uint32_t &dst_h) {

    float ratio_src = src_w * 1.0 / src_h;
    float ratio_dst = dst_w * 1.0 / dst_h;
    if(ratio_src > ratio_dst)
        src_w = (uint32_t)(src_h * ratio_dst);
    if(ratio_src < ratio_dst)
        src_h = (uint32_t)(src_w / ratio_dst);
    LOGD("@%s : src_ratio:%f, dst_ratio:%f, src(%dx%d), dst(%dx%d)", __FUNCTION__,
         ratio_src, ratio_dst,src_w, src_h, dst_w, dst_h);
}

status_t RKISP2GraphConfig::selectSensorOutputFormat(int32_t cameraId, int &w, int &h, uint32_t &format) {
    camera3_stream_t* stream = NULL;
    w = h = 0;

    for (auto it = mStreamToSinkIdMap.begin(); it != mStreamToSinkIdMap.end(); ++it) {
        //dump raw case: sensor output should satisfy rawStream first
        if (it->second == GCSS_KEY_IMGU_RAW) {
            stream = it->first;
            // setprop persist.vendor.camera.dump 16 will produce this case
            if(stream->width == 0 || stream->height == 0)
                continue;
            break;
        }

        //normal case: get the app stream which map to mp, this stream size decide the
        //sensor output
        if (it->second == GCSS_KEY_IMGU_VIDEO) {
            stream = it->first;
        }
    }
    if (!stream) {
        LOGE("@%s : App stream is Null", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    if (0 == mAvailableSensorFormat.size()) {
        LOGE("@%s : Emum sensor frame size may failed", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    // default sensor Mbus code
    /* TODO  add format select logic, now just pick the first one*/
    format = mAvailableSensorFormat.begin()->first;

    const RKISP2CameraCapInfo *cap = getRKISP2CameraCapInfo(cameraId);
    const std::vector<struct FrameSize_t>& tuningSupportSize = cap->getSupportTuningSizes();

    std::vector<struct SensorFrameSize> &frameSize = mAvailableSensorFormat[format];
    // travel the frameSize from small to large to get the suitable sensor output
    for (auto it = frameSize.begin(); it != frameSize.end(); ++it) {
        if((*it).max_width >= stream->width &&
           (*it).max_height >= stream->height) {
            //for SOC Sensor
            if(cap->sensorType() == SENSOR_TYPE_SOC) {
                w = (*it).max_width;
                h = (*it).max_height;
                LOGD("@%s Select sensor format: code 0x%x:%s,  Res(%dx%d)", __FUNCTION__,
                     format, gcu::pixelCode2String(format).c_str(), (*it).max_width, (*it).max_height);
                break;
            }
            //for RAW Sensor
            // travel the tuningSupportSize to check the sensor output size is supported
            for (auto iter = tuningSupportSize.begin(); iter != tuningSupportSize.end(); ++iter) {
                LOGD("@%s : tuningSupportSize: %dx%d", __FUNCTION__, (*iter).width, (*iter).height);
                if((*it).max_width == (*iter).width &&
                   (*it).max_height == (*iter).height) {
                    LOGD("@%s Select sensor format: code 0x%x:%s,  Res(%dx%d)", __FUNCTION__,
                         format, gcu::pixelCode2String(format).c_str(), (*it).max_width, (*it).max_height);
                    w = (*it).max_width;
                    h = (*it).max_height;
                    break;
                }
                if(iter == tuningSupportSize.end())
                    LOGD("@%s : Tuning not supprt the sensor output size %dx%d", __FUNCTION__,
                         (*it).max_width, (*it).max_height);
            }
            // sensor fmt found
            if(w != 0 && h != 0)
                break;
        }
    }

    if(frameSize.back().max_width < stream->width ||
       frameSize.back().max_height < stream->height) {
        LOGE("@%s : App stream size(%dx%d) larger than Sensor full size(%dx%d), Check camera3_profiles.xml",
             __FUNCTION__, stream->width, stream->height, frameSize.back().max_width, frameSize.back().max_height);
        return UNKNOWN_ERROR;
    }
    if((w == 0 || h == 0) && frameSize.size() != 0) {
        w = frameSize.back().max_width;
        h = frameSize.back().max_height;
        LOGD("@%s : Can't find the tuning support sensor size, select sensor full size(%dx%d)",
             __FUNCTION__, w, h);
    }

    return OK;
}

string RKISP2GraphConfig::getSinkEntityName(std::shared_ptr<MediaEntity> entity, int port) {
    std::vector<media_link_desc> links;
    entity->getLinkDesc(links);
    if (links.size()) {
        struct media_pad_desc* pad = &links[port].sink;
        struct media_entity_desc entityDesc;
        mMediaCtl->findMediaEntityById(pad->entity, entityDesc);
        return entityDesc.name;
    }
    return "none";
}

status_t RKISP2GraphConfig::getSensorMediaCtlConfig(int32_t cameraId,
                                          int32_t testPatternMode,
                                          MediaCtlConfig *mediaCtlConfig) {
    status_t ret = OK;

    string sensorEntityName = "none";
    ret = PlatformData::getCameraHWInfo()->getSensorEntityName(cameraId, sensorEntityName);
    if (ret != NO_ERROR)
        return UNKNOWN_ERROR;

    ret = PlatformData::getCameraHWInfo()->getAvailableSensorOutputFormats(cameraId, mAvailableSensorFormat);
    if (ret != NO_ERROR) {
        LOGE("@%s : Can't enum sensor(%s) frame sizes", __FUNCTION__, sensorEntityName.c_str());
        return UNKNOWN_ERROR;
    }

    std::shared_ptr<MediaEntity> sensorEntity = nullptr;
    ret = mMediaCtl->getMediaEntity(sensorEntity, sensorEntityName.c_str());
    if (ret != NO_ERROR) {
        LOGE("@%s, fail to get sensor(%s) MediaEntity", __FUNCTION__, sensorEntityName.c_str());
        return UNKNOWN_ERROR;
    }
    std::vector<media_link_desc> links;
    sensorEntity->getLinkDesc(links);
    if (links.size()) {
        struct media_pad_desc* pad = &links[0].sink;
        struct media_entity_desc entityDesc;
        mMediaCtl->findMediaEntityById(pad->entity, entityDesc);
        string name = entityDesc.name;
        // check if mipi or DVP interface
        if (name.find("cif") != std::string::npos)
            mSensorLinkedToCIF = true;
        if (name.find("dphy") != std::string::npos) {
            mIsMipiInterface = true;
            mSnsLinkedPhyEntNm = name;
            //check sensor->mipi->cif case
            std::shared_ptr<MediaEntity> phyEntity = nullptr;
            ret = mMediaCtl->getMediaEntity(phyEntity, name.c_str());
            CheckError(ret != NO_ERROR, UNKNOWN_ERROR, "@%s,  failed to get csi(%s) MediaEntity",
                           __FUNCTION__, name.c_str());

            string ispname = getSinkEntityName(phyEntity, 0);
            if(ispname.find("cif") != std::string::npos)
                mSensorLinkedToCIF = true;
        }
        //link sensor --> nextEntity
        //if it's mipi interface, the nextEntity is mipi_dphy
        //if it's dvp interface, the nextEntity is isp
        addLinkParams(sensorEntityName, links[0].source.index, name, links[0].sink.index, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);

        int width, height;
        uint32_t format;
        ret = selectSensorOutputFormat(cameraId, width, height, format);
        if (ret != OK)
            return UNKNOWN_ERROR;
        addFormatParams(sensorEntityName, width, height, links[0].source.index, format, 0, 0, mediaCtlConfig);
        mCurSensorFormat = mediaCtlConfig->mFormatParams.back();
        addSelectionParams(sensorEntityName, width, height, 0, 0, V4L2_SEL_TGT_CROP, links[0].source.index, mediaCtlConfig);
    }

    // Csi srcPad and sinkPad format and selection need config ?
    // mipi phy driver get csi format from sensor rather than ioctls called by
    // app, so it's no need to set csi pads format and selection

    return OK;
}

status_t RKISP2GraphConfig::getImguMediaCtlConfig(int32_t cameraId,
                                          int32_t testPatternMode,
                                          MediaCtlConfig *mediaCtlConfig,
                                          std::vector<camera3_stream_t*>& outputStream)
{
    //CIF isp
    if (mSensorLinkedToCIF) {
        LOGI("@%s : sensor link to cif", __FUNCTION__);
        //do not support crop now, don't set selection
        addImguVideoNode(IMGU_NODE_VIDEO, MEDIACTL_VIDEONAME_CIF, mediaCtlConfig);
        addFormatParams(MEDIACTL_VIDEONAME_CIF, mCurSensorFormat.width, mCurSensorFormat.height,
                        0, V4L2_PIX_FMT_NV12, 0, 0, mediaCtlConfig);
        return OK;
    }

    int mipSrcPad = 1;
    int csiSrcPad = 1;
    int csiSinkPad = 0;

    int ispSinkPad = 0;
    int ispParamPad = 1;
    int ispSrcPad = 2;
    int ispStatsPad = 3;

    int mpSinkPad = 0;
    int spSinkPad = 0;
    int rpSinkPad = 0;
    int statsSinkPad = 0;
    int paramSrcPad = 0;

    string mipName = "none";
    string mipName2 = "none";
    string cif_mipiName = "none";
    string csiName = "none";
    string IspName = "none";
    string mpName = "none";
    string spName = "none";
    string rpName = "none";
    string statsName = "none";
    string paramName = "none";
    std::vector<std::string> elementNames;
    PlatformData::getCameraHWInfo()->getMediaCtlElementNames(elementNames);
    for (auto &it: elementNames) {
        LOGD("elementNames:%s",it.c_str());
        if (it.find("dphy") != std::string::npos &&
            mSnsLinkedPhyEntNm == it)
            mipName = it;
        if (it.find("mipi-csi") != std::string::npos)
            mipName2 = it;
        if (it.find("cif_mipi") != std::string::npos)
            cif_mipiName = it;
        if (it.find("csi-subdev") != std::string::npos)
            csiName = it;
        if (it.find("isp-subdev") != std::string::npos)
            IspName = it;
        if (it.find("mainpath") != std::string::npos)
            mpName = it;
        if (it.find("selfpath") != std::string::npos)
            spName = it;
        if (PlatformData::getCameraHWInfo()->isIspSupportRawPath() &&
            it.find("rawpath") != std::string::npos)
            rpName = it;
        if (it.find("statistics") != std::string::npos)
            statsName = it;
        if (it.find("input-params") != std::string::npos)
            paramName = it;
    }
    LOGD("%s: mipName = %s", __FUNCTION__, mipName.c_str());
    LOGD("%s: mipName2 = %s", __FUNCTION__, mipName2.c_str());
    LOGD("%s: csiName = %s", __FUNCTION__, csiName.c_str());
    LOGD("%s: IspName = %s", __FUNCTION__, IspName.c_str());
    LOGD("%s: mpName = %s", __FUNCTION__, mpName.c_str());
    LOGD("%s: spName = %s", __FUNCTION__, spName.c_str());
    LOGD("%s: rpName = %s", __FUNCTION__, rpName.c_str());
    LOGD("%s: statsName = %s", __FUNCTION__, statsName.c_str());
    LOGD("%s: paramName = %s", __FUNCTION__, paramName.c_str());

    int ispOutWidth, ispInWidth ,ispOutHeight, ispInHeight;
    uint32_t ispOutFormat ,ispInFormat, videoOutFormat;
    ispOutWidth = ispInWidth = mCurSensorFormat.width;
    ispOutHeight = ispInHeight = mCurSensorFormat.height;
    ispInFormat = mCurSensorFormat.formatCode;
    ispOutFormat = ISP_DEFAULT_OUTPUT_FORMAT;
    videoOutFormat = VIDEO_DEFAULT_OUTPUT_FORMAT;
    mMpOutputRaw = false;

    camera3_stream_t* mpStream = NULL;
    camera3_stream_t* spStream = NULL;
    camera3_stream_t* rawStream = NULL;
    for (auto it = mStreamToSinkIdMap.begin(); it != mStreamToSinkIdMap.end(); ++it) {
        if (it->second == GCSS_KEY_IMGU_VIDEO)
            mpStream = it->first;
        if (it->second == GCSS_KEY_IMGU_PREVIEW)
            spStream = it->first;
        if (it->second == GCSS_KEY_IMGU_RAW)
            rawStream = it->first;
    }

    //Dvp doesn't need this link
    if(mIsMipiInterface){
        if ((mipName.find("dphy2") != std::string::npos) && (mipName2.find("mipi") != std::string::npos)) {
            //for dual camera
            if(PlatformData::supportDualVideo()) {
                addLinkParams(mipName, mipSrcPad, mipName2, csiSinkPad, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
                addLinkParams(mipName2, 1, "stream_cif_mipi_id0", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
                addLinkParams(mipName2, 2, "stream_cif_mipi_id1", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
                addLinkParams(mipName2, 3, "stream_cif_mipi_id2", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
                addLinkParams(mipName2, 4, "stream_cif_mipi_id3", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);

                addLinkParams("rkisp-csi-subdev", 2, "rkisp_rawwr0", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
                addLinkParams("rkisp-csi-subdev", 4, "rkisp_rawwr2", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
                addLinkParams("rkisp-csi-subdev", 5, "rkisp_rawwr3", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);

                addLinkParams("rkisp-isp-subdev", 2, "rkisp_mainpath", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
                addLinkParams("rkisp-isp-subdev", 2, "rkisp_selfpath", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            }
	    } else if(mipName2.find("mipi") != std::string::npos) {
            addLinkParams(mipName, mipSrcPad, mipName2, csiSinkPad, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            addLinkParams(mipName2, 1, "stream_cif_mipi_id0", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            addLinkParams(mipName2, 2, "stream_cif_mipi_id1", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            addLinkParams(mipName2, 3, "stream_cif_mipi_id2", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            addLinkParams(mipName2, 4, "stream_cif_mipi_id3", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            mSensorLinkedToCIF = true;
	    } else {
            addLinkParams(mipName, mipSrcPad, csiName, csiSinkPad, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            addLinkParams(csiName, csiSrcPad, IspName, ispSinkPad, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            addLinkParams(csiName, 2, "rkisp_rawwr0", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            addLinkParams(csiName, 4, "rkisp_rawwr2", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            addLinkParams(csiName, 5, "rkisp_rawwr3", 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
	    }
    }
    // isp input pad format and selection config
    addFormatParams(IspName, ispInWidth, ispInHeight, ispSinkPad, ispInFormat, 0, 0, mediaCtlConfig);
    addSelectionParams(IspName, ispInWidth, ispInHeight, 0, 0, V4L2_SEL_TGT_CROP, ispSinkPad, mediaCtlConfig);
    if(mSensorLinkedToCIF){
		addImguVideoNode(IMGU_NODE_VIDEO, MEDIACTL_VIDEONAME_CIF_MIPI_ID0, mediaCtlConfig);
		addFormatParams(MEDIACTL_VIDEONAME_CIF_MIPI_ID0, mCurSensorFormat.width, mCurSensorFormat.height,
				0, V4L2_PIX_FMT_NV12, 0, 0, mediaCtlConfig);
        return OK;
    }


    // if enable raw but isp doesn't support rawPath, use mainPath to output raw
    if ((rawStream != NULL || LogHelper::isDumpTypeEnable(CAMERA_DUMP_RAW)) &&
        rpName == "none") {
        LOGI("@%s : MainPath outputs raw data for isp doesn't support rawPath", __FUNCTION__);
        mMpOutputRaw = true;
    }

    // if mainPath output raw, ispOutput format should be set to raw format
    if(mMpOutputRaw) {
        // conversion between V4L2_MBUS_FMT_xx formats and V4L2_PIX_FMT_xx formats
        ispOutFormat = mCurSensorFormat.formatCode;
        videoOutFormat = gcu::getV4L2Format(gcu::pixelCode2fourcc(mCurSensorFormat.formatCode));
    }
    // isp output pad format and selection config
    addSelectionParams(IspName, ispOutWidth, ispOutHeight, 0, 0, V4L2_SEL_TGT_CROP, ispSrcPad, mediaCtlConfig);
    addFormatParams(IspName, ispOutWidth, ispOutHeight, ispSrcPad, ispOutFormat, 0, 0, mediaCtlConfig);

    // isp stats and param link enable
    addLinkParams(IspName, ispStatsPad, statsName,  statsSinkPad,  1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
    addLinkParams(paramName, paramSrcPad, IspName, ispParamPad, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);

    bool mp_need_crop = true;
    bool sp_need_crop = true;

    isNeedPathCrop(ispOutWidth, ispOutHeight,
                   spStream && spName != "none" ,
                   outputStream, mp_need_crop,
                   sp_need_crop);

    struct v4l2_selection select;
    CLEAR(select);
    if(mpStream) {
        /* videodev2.h says don't use *_MPLAEN */
        uint32_t mpInWidth = ispOutWidth;
        uint32_t mpInHeight = ispOutHeight;
        if(mpStream->width > MP_MAX_WIDTH && mpStream->height > MP_MAX_HEIGHT) {
            LOGE("@%s APP Stream size(%dx%d) can't beyond MP cap(%dx%d)", __FUNCTION__,
                 mpStream->width, mpStream->height, MP_MAX_WIDTH, MP_MAX_HEIGHT);
            return UNKNOWN_ERROR;
        }

        if (mp_need_crop)
            cal_crop(mpInWidth, mpInHeight, mpStream->width, mpStream->height);
        else
            limitPathOutputSize(mpInWidth, mpInHeight);
        select.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        select.target = V4L2_SEL_TGT_CROP;
        select.flags = 0;
        select.r.left = (ispOutWidth - mpInWidth) / 2;
        select.r.top = (ispOutHeight - mpInHeight) / 2;
        select.r.width = mpInWidth;
        select.r.height = mpInHeight;
        if(!mMpOutputRaw) {
            //for the case: isp output size < app stream size, select isp output
            //size as the vidoe node out output size, may happen in tuning dump raw case
            uint32_t videoWidth = mpInWidth;
            uint32_t videoHeight = mpInHeight;

            if (mp_need_crop) {
                videoWidth = mpStream->width > mpInWidth ? mpInWidth : mpStream->width ;
                videoHeight = mpStream->height > mpInHeight ? mpInHeight : mpStream->height ;
            }
            addSelectionVideoParams(mpName, select, mediaCtlConfig);
            addFormatParams(mpName, videoWidth, videoHeight, mpSinkPad, videoOutFormat, 0, 0, mediaCtlConfig);
        } else {
            select.r.left = 0;
            select.r.top = 0;
            select.r.width = ispOutWidth;
            select.r.height = ispOutHeight;
            addSelectionVideoParams(mpName, select, mediaCtlConfig);
            addFormatParams(mpName, ispOutWidth, ispOutHeight, mpSinkPad, videoOutFormat, 0, 0, mediaCtlConfig);
        }
        addImguVideoNode(IMGU_NODE_VIDEO, mpName, mediaCtlConfig);
        addLinkParams(IspName, ispSrcPad, mpName,  mpSinkPad,  1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
    } else {
        LOGE("@%s : No app stream map to mainPath", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    //if mainPath is used to output raw, disable selfPath
    if(spStream && spName != "none" && !mMpOutputRaw) {
        uint32_t spInWidth = ispOutWidth;
        uint32_t spInHeight = ispOutHeight;
        // if SP output size beyond its capacity, the out frame is bad
        if(spStream->width > SP_MAX_WIDTH && spStream->height > SP_MAX_HEIGHT) {
            LOGW("@%s Stream %p size(%dx%d) beyond SP cap(%dx%d), should attach to MP", __FUNCTION__,
                 spStream, spStream->width, spStream->height, SP_MAX_WIDTH, SP_MAX_HEIGHT);
        } else {
            if (sp_need_crop)
                cal_crop(spInWidth, spInHeight, spStream->width, spStream->height);
            else
                limitPathOutputSize(spInWidth, spInHeight);

            select.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            select.target = V4L2_SEL_TGT_CROP;
            select.flags = 0;
            select.r.left = (ispOutWidth - spInWidth) / 2;
            select.r.top = (ispOutHeight - spInHeight) / 2;
            select.r.width = spInWidth;
            select.r.height = spInHeight;
            //for the case: isp output size < app stream size, select isp output
            //size as the vidoe node out output size, may happen in tuning dump raw case
            uint32_t videoWidth = spInWidth;
            uint32_t videoHeight = spInHeight;

            if (sp_need_crop) {
                videoWidth = spStream->width > spInWidth ? spInWidth : spStream->width ;
                videoHeight = spStream->height > spInHeight ? spInHeight : spStream->height ;
            }
            addSelectionVideoParams(spName, select, mediaCtlConfig);
            addFormatParams(spName, videoWidth, videoHeight, spSinkPad, videoOutFormat, 0, 0, mediaCtlConfig);
            addImguVideoNode(IMGU_NODE_VF_PREVIEW, spName, mediaCtlConfig);
            addLinkParams(IspName, ispSrcPad, spName,  spSinkPad,  1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        }
    } else {
        //disable isp --> selfPath link
        /* addLinkParams(IspName, ispSrcPad, spName,  spSinkPad,  1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig); */
        LOGI("@%s : No need for selfPath", __FUNCTION__);
    }

    //if isp support rawPath and sensor is raw sensor
    if(rpName != "none" && graphconfig::utils::isRawFormat(mCurSensorFormat.formatCode)) {
        addFormatParams(rpName, mCurSensorFormat.width, mCurSensorFormat.height, rpSinkPad,
                        gcu::getV4L2Format(gcu::pixelCode2fourcc(mCurSensorFormat.formatCode)),
                        0, 0, mediaCtlConfig);
        addImguVideoNode(IMGU_NODE_RAW, rpName, mediaCtlConfig);
        addLinkParams(IspName, ispSrcPad, rpName,  rpSinkPad,  1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
    }

    //dumpMediaCtlConfig(*mediaCtlConfig);

    return OK;
}

/*
 * Imgu specific function
 */
status_t RKISP2GraphConfig::getImguMediaCtlData(int32_t cameraId,
                                          int32_t testPatternMode,
                                          MediaCtlConfig *mediaCtlConfig,
                                          MediaCtlConfig *mediaCtlConfigVideo,
                                          MediaCtlConfig *mediaCtlConfigStill)
{
    CheckError((!mediaCtlConfig || !mediaCtlConfigVideo || !mediaCtlConfigStill), \
               BAD_VALUE, "@%s null ptr\n", __FUNCTION__);

    int ret;

    Node *imgu = nullptr;
    Node *preview = nullptr, *video = nullptr, *still = nullptr, *output = nullptr, *input = nullptr;
    int width = 0, height = 0, format = 0;
    int enabled = 1;
    std::string kImguName = "rkisp1-isp-subdev";

    ret = mSettings->getDescendant(GCSS_KEY_IMGU, &imgu);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get imgu node");
        return UNKNOWN_ERROR;
    }

    // TODO: not done
    mSettings->getDescendant(GCSS_KEY_IMGU_PREVIEW, &preview);
    mSettings->getDescendant(GCSS_KEY_IMGU_VIDEO, &video);
    mSettings->getDescendant(GCSS_KEY_IMGU_STILL, &still);
    imgu->getDescendant(GCSS_KEY_OUTPUT, &output);
    imgu->getDescendant(GCSS_KEY_INPUT, &input);

    struct lut {
        uint32_t uid;
        string name;
        int ipuNodeName;
        Node *pipe;
        int pad;
    };

    vector<lut> uids;
    uids.clear();

    if (!mSensorLinkedToCIF)
        uids = {
            { GCSS_KEY_IMGU_STILL, MEDIACTL_STILLNAME, IMGU_NODE_STILL, still, -1 },
            { GCSS_KEY_INPUT, kImguName, IMGU_NODE_INPUT,input, 0 },
            { GCSS_KEY_OUTPUT, kImguName, -1, output, MEDIACTL_PAD_OUTPUT_NUM },
            { GCSS_KEY_IMGU_VIDEO, MEDIACTL_VIDEONAME, IMGU_NODE_VIDEO, video, 0 },
            { GCSS_KEY_IMGU_PREVIEW, MEDIACTL_PREVIEWNAME, IMGU_NODE_VF_PREVIEW, preview, 0 },
        };
    else
        uids = {
            { GCSS_KEY_IMGU_VIDEO, MEDIACTL_VIDEONAME_CIF, IMGU_NODE_VIDEO, video, 0 }
        };

    int ispOutWidth = 0, ispOutHeight = 0;
    for (int i = 0; i < uids.size(); i++) {
        string name = uids[i].name;
        Node *pipe = uids[i].pipe;

        if (pipe == nullptr) {
            LOGD("<%u> node is not present in graph (descriptor or settings) - continuing.", uids[i].uid);
            continue;
        }

        // Assume enabled="1" by default. Explicitly set to 0 in settings, if necessary.
        enabled = 1;
        ret = pipe->getValue(GCSS_KEY_ENABLED, enabled);
        if (ret != css_err_none) {
            LOGI("Attribute 'enabled' not present in <%s>. Assuming enabled=\"1\"", NODE_NAME(pipe));
        }

        if (!enabled) {
            LOGI("Node <%s> not enabled - continuing", NODE_NAME(pipe));
            continue;
        }

        ret = pipe->getValue(GCSS_KEY_WIDTH, width);
        if (ret != css_err_none) {
            LOGE("Could not get width for <%s>", NODE_NAME(pipe));
            return UNKNOWN_ERROR;
        }

        if (width == 0)
            continue;

        ret = pipe->getValue(GCSS_KEY_HEIGHT, height);
        if (ret != css_err_none) {
            LOGE("Could not get height for <%s>", NODE_NAME(pipe));
            return UNKNOWN_ERROR;
        }
        string fourccFormat = "";
        ret = pipe->getValue(GCSS_KEY_FORMAT, fourccFormat);
        if (ret != css_err_none) {
            LOGE("Could not get format for <%s>", NODE_NAME(pipe));
            return UNKNOWN_ERROR;
        }

        format = gcu::getV4L2Format(get_fourcc(fourccFormat[0],
                                               fourccFormat[1],
                                               fourccFormat[2],
                                               fourccFormat[3]));

        if (GCSS::ItemUID::key2str(uids[i].uid) == GC_PREVIEW ||
            GCSS::ItemUID::key2str(uids[i].uid) == GC_STILL ||
            GCSS::ItemUID::key2str(uids[i].uid) == GC_VIDEO) {
            int nodeWidth = 0, nodeHeight = 0;

            /* Use BGGR as bayer format when specific sensor receives test pattern request */
            if (testPatternMode != ANDROID_SENSOR_TEST_PATTERN_MODE_OFF) {
                const RKISP2CameraCapInfo* capInfo = getRKISP2CameraCapInfo(cameraId);
                CheckError(capInfo == nullptr, UNKNOWN_ERROR, "@%s: failed to get cameraCapInfo", __FUNCTION__);
                if (name.compare(MEDIACTL_INPUTNAME) == 0 &&
                        !capInfo->getTestPatternBayerFormat().empty()) {
                    format = gcu::getV4L2Format(capInfo->getTestPatternBayerFormat());
                }
            }
            addFormatParams(name, width, height, uids[i].pad,
                            format, 0, 0, mediaCtlConfig);

            // Get path crop  info
            ret = getNodeInfo(GCSS_KEY_IMGU_PCRP, *pipe, &nodeWidth, &nodeHeight);
            if (ret != OK) {
                LOGE("pipe log name: %s can't get info!", name.c_str());
                return UNKNOWN_ERROR;
            } else if (!mSensorLinkedToCIF) {
                struct v4l2_selection select;
                CLEAR(select);
                /* videodev2.h says don't use *_MPLAEN */
                select.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                select.target = V4L2_SEL_TGT_CROP;
                select.flags = 0;
                select.r.left = (ispOutWidth - nodeWidth) / 2;
                select.r.top = (ispOutHeight - nodeHeight) / 2;
                select.r.width = nodeWidth;
                select.r.height = nodeHeight;
                addSelectionVideoParams(name.c_str(), select, mediaCtlConfig);
                LOGD("pipe log name: %s  crop size %dx%d", name.c_str(), nodeWidth, nodeHeight);
            }

            LOGI("Adding video node: %s", NODE_NAME(pipe));
            addImguVideoNode(uids[i].ipuNodeName, name, mediaCtlConfig);
            if (!mSensorLinkedToCIF)
                addLinkParams(kImguName, MEDIACTL_PAD_OUTPUT_NUM, name, 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            else
                addLinkParams(mCSIBE, 0, name, 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        } else if (GCSS::ItemUID::key2str(uids[i].uid) == GC_INPUT) {
            int nodeWidth = 0, nodeHeight = 0;
            int32_t iMbusFormat = gcu::getMBusFormat(get_fourcc(fourccFormat[0],
                                                                fourccFormat[1],
                                                                fourccFormat[2],
                                                                fourccFormat[3]));
            addFormatParams(name, width, height, uids[i].pad,
                            iMbusFormat, 0, 0, mediaCtlConfig);

            // Get isp input crop  info
            ret = getNodeInfo(GCSS_KEY_IMGU_IAC, *pipe, &nodeWidth, &nodeHeight);
            if (ret != OK) {
                LOGW("pipe log name: %s can't get info!", name.c_str());
                return UNKNOWN_ERROR;
            } else
                addSelectionParams(name, nodeWidth, nodeHeight, 0, 0, V4L2_SEL_TGT_CROP, uids[i].pad, mediaCtlConfig);
            /* TODO: related to sensor interface, see getMediaCtlData*/
            if (mCSIBE.find("mipi") != std::string::npos)
                addLinkParams(mCSIBE, 1, kImguName, 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
            else // dvp sensor
                addLinkParams(mCSIBE, 0, kImguName, 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
        } else if (GCSS::ItemUID::key2str(uids[i].uid) == GC_OUTPUT) {
            int nodeWidth = 0, nodeHeight = 0;
            int32_t oMbusFormat = gcu::getMBusFormat(get_fourcc(fourccFormat[0],
                                                                fourccFormat[1],
                                                                fourccFormat[2],
                                                                fourccFormat[3]));
            /* use limit range if test pattern mode is selected */
            int quantization = testPatternMode != ANDROID_SENSOR_TEST_PATTERN_MODE_OFF ?
                V4L2_QUANTIZATION_LIM_RANGE : V4L2_QUANTIZATION_DEFAULT;
            addFormatParams(name, width, height, uids[i].pad,
                            oMbusFormat, 0, quantization, mediaCtlConfig);
            // Get isp output crop  info
            ret = getNodeInfo(GCSS_KEY_IMGU_ISM, *pipe, &nodeWidth, &nodeHeight);
            if (ret != OK) {
                LOGW("pipe log name: %s can't get info!", name.c_str());
                return UNKNOWN_ERROR;
            } else
                addSelectionParams(name, nodeWidth, nodeHeight, 0, 0, V4L2_SEL_TGT_CROP, uids[i].pad, mediaCtlConfig);
            ispOutWidth = nodeWidth;
            ispOutHeight = nodeHeight;
        } else {
            LOGE("pipe log name: wrong node %s !", GCSS::ItemUID::key2str(uids[i].uid));
            return UNKNOWN_ERROR;
        }
    }

    if (!mSensorLinkedToCIF) {
        LOGI("Adding stats node");
        /* addImguVideoNode(IMGU_NODE_STAT, MEDIACTL_STATNAME, mediaCtlConfig); */
        addLinkParams(kImguName, 3, MEDIACTL_STATNAME, 0, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);

        LOGI("Adding parameter node");
        /* addImguVideoNode(IMGU_NODE_PARAM, MEDIACTL_PARAMETERNAME, mediaCtlConfig); */
        addLinkParams(MEDIACTL_PARAMETERNAME, 0, kImguName, 1, 1, MEDIA_LNK_FL_ENABLED, mediaCtlConfig);
    }

    return ret;
}

void RKISP2GraphConfig::setMediaCtlConfig(std::shared_ptr<MediaController> sensorMediaCtl,std::shared_ptr<MediaController> imgMediaCtl,
                                    bool swapVideoPreview,
                                    bool enableStill)
{
    mMediaCtl = sensorMediaCtl;
    mImgMediaCtl = imgMediaCtl;

}

/*
 * Imgu helper function
 */
bool RKISP2GraphConfig::doesNodeExist(string nodeName)
{
    int ret;

    Node *node = nullptr;

    ret = mSettings->getDescendantByString(nodeName, &node);
    if (ret != css_err_none) {
        LOGD("Node <%s> was not found.", nodeName.c_str());
        return false;
    }

    /* There is no good way to search if node exists or not.
     * Because mSettings has both descriptor and settings combined we
     * need to ask for specific value see if node really exists in the
     * settings side. */
    int width;
    ret = node->getValue(GCSS_KEY_WIDTH, width);
    if (ret != css_err_none) {
        LOGD("Node <%s> was not found.", nodeName.c_str());
        return false;
    }

    return true;
}

/*
 * Get values for MediaCtlConfig control params.
 * \Node sensorNode    pointer to sensor node
 */
status_t RKISP2GraphConfig::addControls(const Node *sensorNode,
                                  const SourceNodeInfo &sourceInfo,
                                  MediaCtlConfig* config)
{
    css_err_t ret = css_err_none;
    std::string value;
    std::string entityName;

    if (!sourceInfo.pa.name.empty()) {
        entityName = sourceInfo.pa.name;
    } else if (!sourceInfo.tpg.name.empty()) {
        entityName = sourceInfo.tpg.name;
    } else {
        LOGE("Empty entity name");
        return UNKNOWN_ERROR;
    }

    ret = sensorNode->getValue(GCSS_KEY_EXPOSURE, value);
    if (ret == css_err_none) {
        addCtlParams(entityName, GCSS_KEY_EXPOSURE, V4L2_CID_EXPOSURE,
                     value, config);
    }

    ret = sensorNode->getValue(GCSS_KEY_GAIN, value);
    if (ret == css_err_none) {
        addCtlParams(entityName, GCSS_KEY_GAIN, V4L2_CID_ANALOGUE_GAIN,
                     value, config);
    }
    return OK;
}

/**
 * Add video nodes into mediaCtlConfig
 *
 * \param csiBESocOutput[in]  use to determine whether csi be soc is enabled
 * \param mediaCtlConfig[out] populate this struct with given values
 */
void RKISP2GraphConfig::addVideoNodes(const Node* csiBESocOutput,
                                MediaCtlConfig* config)
{
    CheckError((!config), VOID_VALUE, "@%s null ptr\n", __FUNCTION__);

    MediaCtlElement mediaCtlElement;

    // Imgu support
    mediaCtlElement.isysNodeName = ISYS_NODE_RAW;
    mediaCtlElement.name = mCSIBE;
    config->mVideoNodes.push_back(mediaCtlElement);
}

void RKISP2GraphConfig::addImguVideoNode(int nodeType, const string& nodeName, MediaCtlConfig *config)
{
    CheckError((!config), VOID_VALUE, "@%s null ptr\n", __FUNCTION__);
    CheckError((nodeType == IMGU_NODE_NULL), VOID_VALUE, "@%s null ipu node name\n", __FUNCTION__);
    MediaCtlElement mediaCtlElement;
    mediaCtlElement.name = nodeName;
    mediaCtlElement.isysNodeName = nodeType;
    config->mVideoNodes.push_back(mediaCtlElement);
}

/*
 * Imgu helper function
 */
status_t RKISP2GraphConfig::getValue(string &nodeName, uint32_t id, int &value)
{
    Node *node;
    int ret = mSettings->getDescendantByString(nodeName, &node);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get %s node", nodeName.c_str());
        return UNKNOWN_ERROR;
    }

    GCSS::GraphConfigAttribute *attr;
    ret = node->getAttribute(id, &attr);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get attribute '%s' for node: %s", GCSS::ItemUID::key2str(id), NODE_NAME(node));
        return UNKNOWN_ERROR;
    }
    string valueString = "-2";
    ret = attr->getValue(valueString);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get value of '%s' for node: %s", GCSS::ItemUID::key2str(id), NODE_NAME(node));
        return UNKNOWN_ERROR;
    }
    value = atoi(valueString.c_str());

    return OK;
}

/**
 * Dump contents of mediaCtlConfig struct
 */
void RKISP2GraphConfig::dumpMediaCtlConfig(const MediaCtlConfig &config) const {

    size_t i = 0;
    LOGD("MediaCtl config w=%d ,height=%d"
        ,config.mCameraProps.outputWidth,
        config.mCameraProps.outputHeight);
    for (i = 0; i < config.mLinkParams.size() ; i++) {
       LOGD("Link Params srcName=%s  srcPad=%d ,sinkName=%s, sinkPad=%d enable=%d"
            ,config.mLinkParams[i].srcName.c_str(),
            config.mLinkParams[i].srcPad,
            config.mLinkParams[i].sinkName.c_str(),
            config.mLinkParams[i].sinkPad,
            config.mLinkParams[i].enable);
    }
    for (i = 0; i < config.mFormatParams.size() ; i++) {
       LOGD("Format Params entityName=%s  pad=%d ,width=%d, height=%d formatCode=%x"
            ,config.mFormatParams[i].entityName.c_str(),
            config.mFormatParams[i].pad,
            config.mFormatParams[i].width,
            config.mFormatParams[i].height,
            config.mFormatParams[i].formatCode);
    }
    for (i = 0; i < config.mSelectionVideoParams.size() ; i++) {
       LOGD("Selection video Params entityName=%s  type=%d ,target=%d, flag=%d"
            ,config.mSelectionVideoParams[i].entityName.c_str(),
            config.mSelectionVideoParams[i].select.type,
            config.mSelectionVideoParams[i].select.target,
            config.mSelectionVideoParams[i].select.flags);
    }
    for (i = 0; i < config.mSelectionParams.size() ; i++) {
       LOGD("Selection Params entityName=%s  pad=%d ,target=%d, top=%d left=%d width=%d, height=%d"
            ,config.mSelectionParams[i].entityName.c_str(),
            config.mSelectionParams[i].pad,
            config.mSelectionParams[i].target,
            config.mSelectionParams[i].top,
            config.mSelectionParams[i].left,
            config.mSelectionParams[i].width,
            config.mSelectionParams[i].height);
    }
    for (i = 0; i < config.mControlParams.size() ; i++) {
       LOGD("Control Params entityName=%s  controlId=%x ,value=%d, controlName=%s"
            ,config.mControlParams[i].entityName.c_str(),
            config.mControlParams[i].controlId,
            config.mControlParams[i].value,
            config.mControlParams[i].controlName.c_str());
    }
}

/**
 * Get binning factor values from the given node
 * \param[in] node the node to read the values from
 * \param[out] hBin horizontal binning factor
 * \param[out] vBin vertical binning factor
 */
status_t RKISP2GraphConfig::getBinningFactor(const Node *node, int &hBin, int &vBin) const
{
    css_err_t ret = css_err_none;
    string value;
    ret = node->getValue(GCSS_KEY_BINNING_H_FACTOR, hBin);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get horizontal binning factor");
        return UNKNOWN_ERROR;
    }

    ret = node->getValue(GCSS_KEY_BINNING_V_FACTOR, vBin);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get vertical binning factor");
        return UNKNOWN_ERROR;
    }

    return OK;
}

/**
 * Get scaling factor values from the given node
 * \param[in] node the node to read the values from
 * \param[out] scalingNum scaling ratio
 * \param[out] scalingDenom scaling ratio
 */
status_t RKISP2GraphConfig::getScalingFactor(const Node *node,
                                       int32_t &scalingNum,
                                       int32_t &scalingDenom) const
{
    css_err_t ret = css_err_none;
    string value;
    ret = node->getValue(GCSS_KEY_SCALING_FACTOR_NUM, scalingNum);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get width scaling num ratio");
        return UNKNOWN_ERROR;
    }

    ret = node->getValue(GCSS_KEY_SCALING_FACTOR_DENOM, scalingDenom);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get width scaling num ratio");
        return UNKNOWN_ERROR;
    }

    return OK;
}

/**
 * Get width and height values from the given node
 * \param[in] node the node to read the values from
 * \param[out] w width
 * \param[out] h height
 */
status_t RKISP2GraphConfig::getDimensions(const Node *node, int &w, int &h) const
{
    css_err_t ret = css_err_none;
    ret = node->getValue(GCSS_KEY_WIDTH, w);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get width");
        return UNKNOWN_ERROR;
    }
    ret = node->getValue(GCSS_KEY_HEIGHT, h);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get height");
        return UNKNOWN_ERROR;
    }

    return OK;
}

/**
 * Get width, height and cropping values from the given node
 * \param[in] node the node to read the values from
 * \param[out] w width
 * \param[out] h height
 * \param[out] l left crop
 * \param[out] t top crop
 */
status_t RKISP2GraphConfig::getDimensions(const Node *node, int32_t &w, int32_t &h,
                                    int32_t &l, int32_t &t) const
{
    status_t retErr = getDimensions(node, w, h);
    if (retErr != OK)
        return UNKNOWN_ERROR;

    css_err_t ret = node->getValue(GCSS_KEY_LEFT, l);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get left crop");
        return UNKNOWN_ERROR;
    }
    ret = node->getValue(GCSS_KEY_TOP, t);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get top crop");
        return UNKNOWN_ERROR;
    }

    return OK;
}

/**
 * Add format params to config
 *
 * \param[in] entityName
 * \param[in] width
 * \param[in] height
 * \param[in] pad
 * \param[in] format
 * \param[in] filed
 * \param[in] quantization
 * \param[out] config populate this struct with given values
 */
void RKISP2GraphConfig::addFormatParams(const string &entityName,
                                  int width,
                                  int height,
                                  int pad,
                                  int format,
                                  int field,
                                  int quantization,
                                  MediaCtlConfig *config)
{
    if (!entityName.empty() && config) {
        MediaCtlFormatParams mediaCtlFormatParams;
        mediaCtlFormatParams.entityName = entityName;
        mediaCtlFormatParams.width = width;
        mediaCtlFormatParams.height = height;
        mediaCtlFormatParams.pad = pad;
        mediaCtlFormatParams.formatCode = format;
        mediaCtlFormatParams.stride = 0;
        mediaCtlFormatParams.field = field;
        mediaCtlFormatParams.quantization= quantization;
        config->mFormatParams.push_back(mediaCtlFormatParams);
        MediaCtlParamsOrder order;
        order.index = config->mFormatParams.size() - 1;
        order.type = MEDIACTL_PARAMS_TYPE_FMT;
        config->mParamsOrder.push_back(order);
        LOGI("@%s, entityName:%s, width:%d, height:%d, pad:%d, format:0x%x:%s, field:%d",
            __FUNCTION__, entityName.c_str(), width, height, pad, format, gcu::pixelCode2String(format).c_str(), field);
    }
}

/**
 * Add control params into config
 * \param[in] entityName
 * \param[in] controlName
 * \param[in] controlId
 * \param[in] strValue
 * \param[out] config populate this struct with given values
 */
void RKISP2GraphConfig::addCtlParams(const string &entityName,
                               uint32_t controlName,
                               int controlId,
                               const string &strValue,
                               MediaCtlConfig *config)
{
    if (!entityName.empty() && config) {
        int value = atoi(strValue.c_str());
        string controlNameStr = GCSS::ItemUID::key2str(controlName);

        MediaCtlControlParams mediaCtlControlParams;
        mediaCtlControlParams.entityName = entityName;
        mediaCtlControlParams.controlName = controlNameStr;
        mediaCtlControlParams.controlId = controlId;
        mediaCtlControlParams.value = value;
        config->mControlParams.push_back(mediaCtlControlParams);
        MediaCtlParamsOrder order;
        order.index = config->mControlParams.size() - 1;
        order.type = MEDIACTL_PARAMS_TYPE_CTL;
        config->mParamsOrder.push_back(order);
        LOGI("@%s, entityName:%s, controlNameStr:%s, controlId:%d, value:%d",
            __FUNCTION__, entityName.c_str(), controlNameStr.c_str(), controlId, value);
    }
}

/**
 * Add selection params into config
 *
 * \param[in] entityName
 * \param[in] width
 * \param[in] height
 * \param[in] left
 * \param[in] top
 * \param[in] target
 * \param[in] pad
 * \param[out] config populate this struct with given values
 */
void RKISP2GraphConfig::addSelectionParams(const string &entityName,
                                     int width,
                                     int height,
                                     int left,
                                     int top,
                                     int target,
                                     int pad,
                                     MediaCtlConfig *config)
{
    if (!entityName.empty() && config) {
        MediaCtlSelectionParams mediaCtlSelectionParams;
        mediaCtlSelectionParams.width = width;
        mediaCtlSelectionParams.height = height;
        mediaCtlSelectionParams.left = left;
        mediaCtlSelectionParams.top = top;
        mediaCtlSelectionParams.target = target;
        mediaCtlSelectionParams.pad = pad;
        mediaCtlSelectionParams.entityName = entityName;
        config->mSelectionParams.push_back(mediaCtlSelectionParams);
        MediaCtlParamsOrder order;
        order.index = config->mSelectionParams.size() - 1;
        order.type = MEDIACTL_PARAMS_TYPE_CTLSEL;
        config->mParamsOrder.push_back(order);
        LOGI("@%s, width:%d, height:%d, left:%d, top:%d, target:%d, pad:%d, entityName:%s",
            __FUNCTION__, width, height, left, top, target, pad, entityName.c_str());
    }
}

void RKISP2GraphConfig::addSelectionVideoParams(const string &entityName,
                                     const struct v4l2_selection &select,
                                     MediaCtlConfig* config)
{
    if (entityName.empty() || !config) {
        LOGE("The config or entity <%s> is empty!", entityName.c_str());
        return;
    }

    MediaCtlSelectionVideoParams mediaCtlSelectionVideoParams;
    mediaCtlSelectionVideoParams.entityName = entityName;
    mediaCtlSelectionVideoParams.select = select;
    config->mSelectionVideoParams.push_back(mediaCtlSelectionVideoParams);
    MediaCtlParamsOrder order;
    order.index = config->mSelectionVideoParams.size() - 1;
    order.type = MEDIACTL_PARAMS_TYPE_VIDSEL;
    config->mParamsOrder.push_back(order);
    LOGI("@%s, width:%d, height:%d, left:%d, top:%d, target:%d, type:%d, flags:%d entityName:%s",
        __FUNCTION__, select.r.width, select.r.height, select.r.left, select.r.top,
        select.target, select.type, select.flags, entityName.c_str());
}

/**
 * Add link params into config
 *
 * \param[in] srcName
 * \param[in] srcPad
 * \param[in] sinkName
 * \param[in] sinkPad
 * \param[in] enable
 * \param[in] flags
 * \param[out] config populate this struct with given values
 */
void RKISP2GraphConfig::addLinkParams(const string &srcName,
                                int srcPad,
                                const string &sinkName,
                                int sinkPad,
                                int enable,
                                int flags,
                                MediaCtlConfig *config)
{
    if (!srcName.empty() && !sinkName.empty() && config) {
        MediaCtlLinkParams mediaCtlLinkParams;
        mediaCtlLinkParams.srcName = srcName;
        mediaCtlLinkParams.srcPad = srcPad;
        mediaCtlLinkParams.sinkName = sinkName;
        mediaCtlLinkParams.sinkPad = sinkPad;
        mediaCtlLinkParams.enable = enable;
        mediaCtlLinkParams.flags = flags;
        config->mLinkParams.push_back(mediaCtlLinkParams);
        LOGI("@%s, srcName:%s, srcPad:%d, sinkName:%s, sinkPad:%d, enable:%d, flags:%d",
            __FUNCTION__, srcName.c_str(), srcPad, sinkName.c_str(), sinkPad, enable, flags);
    }
}

/**
 * Gets all stream id's and generates kernel list for each of those.
 * Generated kernel lists are stored inside a mKernels map, from where
 * they can be retrieved with streamId.
 *
 */
status_t RKISP2GraphConfig::generateKernelListsForStreams()
{
    return OK;
}

void RKISP2GraphConfig::dumpSettings()
{
    mSettings->dumpNodeTree(mSettings, 2);
}

void RKISP2GraphConfig::dumpKernels(int32_t streamId)
{
}

RKISP2GraphConfig::Rectangle::Rectangle(): w(0),h(0),t(0),l(0) {}
RKISP2GraphConfig::SubdevPad::SubdevPad(): Rectangle(), mbusFormat(0){}
RKISP2GraphConfig::SourceNodeInfo::SourceNodeInfo() : metadataEnabled(false),
                                                interlaced(0) {}
                                                
} // namespace rkisp2
} // namespace camera2
} // namespace android
