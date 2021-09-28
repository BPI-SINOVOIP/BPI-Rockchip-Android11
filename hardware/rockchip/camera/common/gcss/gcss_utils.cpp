/*
 * Copyright (C) 2015-2017 Intel Corporation
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

#define LOG_TAG "GCSS"

#include "LogHelper.h"

#include "cipf_css/ia_cipf_css.h"
#include "gcss.h"
#include "gcss_item.h"
#include "gcss_utils.h"

using namespace GCSS;
using namespace std;
using namespace android::camera2;

IGraphConfig*
GraphCameraUtil::nodeGetPortById(const IGraphConfig *node, uint32_t id)
{
    const GraphConfigNode *gcNode;
    GraphConfigNode *portNode;
    int portId;
    css_err_t ret;

    if (!node)
        return nullptr;

    gcNode = static_cast<const GraphConfigNode*>(node);

    GraphConfigItem::const_iterator it = gcNode->begin();
    while (it != gcNode->end()) {
        ret = gcNode->getDescendant(GCSS_KEY_TYPE,
                                    "port",
                                    it,
                                    &portNode);
        if (ret != css_err_none)
            continue;

        ret = portNode->getValue(GCSS_KEY_ID, portId);
        if (ret == css_err_none && (uint32_t)portId == id)
            return portNode;
    }

    return nullptr;
}

css_err_t
GraphCameraUtil::portGetPeer(const IGraphConfig *port, IGraphConfig **peer)
{
    css_err_t ret = css_err_none;
    IGraphConfig *root;
    int enabled = 1;
    string peerName;

    if (port == nullptr || peer == nullptr) {
       LOGE("Invalid Node, cannot get the peer port");
       return css_err_argument;
    }

    ret = port->getValue(GCSS_KEY_ENABLED, enabled);
    if (ret == css_err_none && !enabled)
        return css_err_noentry;

    root = port->getRoot();
    if (root == nullptr) {
        LOGE("Failed to get root");
        return css_err_internal;
    }

    ret = port->getValue(GCSS_KEY_PEER, peerName);
    if (ret != css_err_none) {
        LOGE("Couldn't find peer value");
        return css_err_argument;
    }

    *peer = root->getDescendantByString(peerName);
    if (*peer == nullptr) {
       LOGE("Failed to find peer by name %s", peerName.c_str());
       return css_err_argument;
    }
    return css_err_none;
}

css_err_t
GraphCameraUtil::portGetFourCCInfo(const IGraphConfig *portNode,
                                   ia_uid &stageId, uint32_t &terminalId)
{
    IGraphConfig *pgNode; // The Program group node
    css_err_t ret = css_err_none;
    int32_t pgId, portId;
    string type;

    if (portNode == nullptr)
        return css_err_argument;

    ret = portNode->getValue(GCSS_KEY_ID, portId);
    if (ret != css_err_none) {
        LOGE("Failed to get port's id");
        return css_err_argument;
    }

    pgNode = portNode->getAncestor();
    if (ret != css_err_none || pgNode == nullptr) {
       LOGE("Failed to get port ancestor");
       return css_err_argument;
    }

    ret = pgNode->getValue(GCSS_KEY_TYPE, type);
    if (ret != css_err_none) {
        LOGE("Failed to get port's ancestor type ");
        return css_err_argument;
    }

    ret = pgNode->getValue(GCSS_KEY_PG_ID, pgId);
    if (ret == css_err_none) {
        stageId = psys_2600_pg_uid(pgId);
        terminalId = stageId + (uint32_t)portId;
    } else {
        stageId = 0;
        terminalId = (uint32_t)portId;
    }
    return css_err_none;
}

int32_t
GraphCameraUtil::portGetDirection(const IGraphConfig *port)
{
    int32_t direction = 0;
    css_err_t ret = css_err_none;
    ret = port->getValue(GCSS_KEY_DIRECTION, direction);
    if (ret != css_err_none) {
        LOGE("Failed to retrieve port direction, default to input");
    }

    return direction;
}

bool GraphCameraUtil::portIsVirtual(const IGraphConfig *port)
{
    string type;
    css_err_t ret = css_err_none;
    if (!port)
        return false;
    ret = port->getValue(GCSS_KEY_TYPE, type);
    if (ret != css_err_none) {
        LOGE("Failed to retrieve port type, default to input");
    }

    return (type == string("sink"));
}

bool GraphCameraUtil::isEdgePort(const IGraphConfig *port)
{
    css_err_t ret = css_err_none;
    IGraphConfig *peer = nullptr;
    IGraphConfig *peerAncestor = nullptr;
    int32_t portDirection;
    int32_t execCtxId = -1;
    int32_t peerExecCtxId = -1;
    string peerType;

    portDirection = GraphCameraUtil::portGetDirection(port);

    ret = portGetPeer(port, &peer);
    if (ret != css_err_none) {
        if (ret != css_err_noentry) {
            LOGE("Failed to create fourcc info for source port");
        }
        return false;
    }

    execCtxId = GraphCameraUtil::portGetExecCtxId(port);
    if (execCtxId < 0) {
        // Fall back to stream id
        execCtxId = GraphCameraUtil::portGetStreamId(port);
            if (execCtxId < 0)
                return false;
    }
    /*
     * get the execCtx id of the peer port
     * we also check the ancestor for that. If the peer is a virtual sink then
     * it does not have ancestor.
     */
    if (!GraphCameraUtil::portIsVirtual(peer)) {
        peerAncestor = peer->getAncestor();
        if (peerAncestor == nullptr) {
            LOGE("Failed to get peer's ancestor");
            return false;
        }

        ret = peerAncestor->getValue(GCSS_KEY_EXEC_CTX_ID, peerExecCtxId);
        if (ret != css_err_none) {
            LOGD("Failed to get exec ctx ID of peer PG. Trying to use stream id");
            // Fall back to stream id
            ret = peerAncestor->getValue(GCSS_KEY_STREAM_ID, peerExecCtxId);
            if (ret != css_err_none) {
                LOGE("Failed to get stream ID of peer PG %s",
                        print(peerAncestor).c_str());
                return false;
            }
        }
        /*
         * Retrieve the type of the peer ancestor. It could be it is not a
         * program group node but a sink or hw block
         */
        peerAncestor->getValue(GCSS_KEY_TYPE, peerType);
    }

    if (portDirection == PORT_DIRECTION_INPUT) {
        /*
         *  input port,
         *  The port is on the edge, if the peer is a hw block, or has a
         *  different execCtx id
         */
        if ((execCtxId != peerExecCtxId) || (peerType == string("hw"))) {
            return true;
        }
    } else {
        /*
         *  output port,
         *  The port is on the edge if the peer is a virtual port, or has a
         *  different execCtx id
         */
        if (portIsVirtual(peer) || (execCtxId != peerExecCtxId)) {
            return true;
        }
    }
    return false;
}

int GraphCameraUtil::portGetStreamId(const IGraphConfig *port)
{
    int i = portGetKey(port, GCSS_KEY_STREAM_ID);
    if (i < 0)
        LOGE("Failed to get %s", ItemUID::key2str(GCSS_KEY_STREAM_ID));
    return i;
}

/**
 * Retrieve the existing ctx id's of the program group nodes
 * in the graph settings passed as parameter.
 *
 * \param[in] settings Head node of the graph config settings
 * \param[out] execCtxIds Set with the found executions context ids
 *
 * \return css_err_none
 * \return other if a node of type program group did not have an execution
 * context associated with it
 */
css_err_t GraphCameraUtil::getExecCtxIds(const IGraphConfig &settings,
                                         std::set<int32_t> &execCtxIds)
{
    css_err_t ret(css_err_none);
    uint32_t descendentCount = settings.getDescendantCount();
    IGraphConfig* nodePtr(nullptr);

    for (uint32_t i = 0; i < descendentCount; i++) {
        nodePtr = settings.iterateDescendantByIndex(GCSS_KEY_TYPE,
                                                    GCSS_KEY_PROGRAM_GROUP,
                                                    i);
        if (nodePtr != nullptr) {
            int32_t execCtxId(-1);
            ret = nodePtr->getValue(GCSS_KEY_EXEC_CTX_ID, execCtxId);
            if (ret == css_err_none)
                execCtxIds.insert(execCtxId);
        }
    }
    return ret;
}

int GraphCameraUtil::portGetExecCtxId(const IGraphConfig *port)
{
    return portGetKey(port, GCSS_KEY_EXEC_CTX_ID);
}

int GraphCameraUtil::portGetKey(const IGraphConfig *port, ia_uid uid)
{
    css_err_t ret = css_err_none;
    const IGraphConfig *ancestor;
    int32_t keyValue = -1;
    string type;

    if (port == nullptr) {
        LOGE("Invalid Node, cannot get the port execCtx id");
        return -1;
    }

    ret = port->getValue(GCSS_KEY_TYPE, type);
    if (ret != css_err_none) {
        LOGE("Failed to get Node Type");
        return -1;
    }

    /* Virtual sinks do not have nested ports, but instead the
     * peer attributes point to sink node itself. Therefore,
     * with sinks we do not need to traverse to anchestor.*/
    if (type == "sink") {
        ancestor = port;
    } else {
        ancestor = port->getAncestor();
        if (ancestor == nullptr) {
            LOGE("Failed to get port's ancestor");
            return -1;
        }
    }

    ret = ancestor->getValue(uid, keyValue);
    if (ret != css_err_none) {
        return -1;
    }
    return keyValue;
}

css_err_t GraphCameraUtil::getDimensions(const IGraphConfig *node,
                                        int32_t *w,
                                        int32_t *h,
                                        int32_t *bpl,
                                        int32_t *l,
                                        int32_t *t,
                                        int32_t *r,
                                        int32_t *b)
{
    css_err_t ret;

    if (!node)
        return css_err_argument;

    if (w) {
        ret = node->getValue(GCSS_KEY_WIDTH, *w);
        if (ret != css_err_none) {
            LOGE("Error: Couldn't get width");
            return css_err_noentry;
        }
    }

    if (h) {
        ret = node->getValue(GCSS_KEY_HEIGHT, *h);
        if (ret != css_err_none) {
            LOGE("Error: Couldn't get height");
            return css_err_noentry;
        }
    }

    if (bpl) {
        ret = node->getValue(GCSS_KEY_BYTES_PER_LINE, *bpl);
        if (ret != css_err_none) {
            LOGE("Error: Couldn't get bpl");
            return css_err_noentry;
        }
    }

    if (l) {
        ret = node->getValue(GCSS_KEY_LEFT, *l);
        if (ret != css_err_none) {
            LOGE("Error: Couldn't get left crop");
            return css_err_noentry;
        }
    }

    if (t) {
        ret = node->getValue(GCSS_KEY_TOP, *t);
        if (ret != css_err_none) {
            LOGE("Error: Couldn't get top crop");
            return css_err_noentry;
        }
    }

    if (r) {
        ret = node->getValue(GCSS_KEY_RIGHT, *r);
        if (ret != css_err_none) {
            LOGE("Error: Couldn't get right crop");
            return css_err_noentry;
        }
    }

    if (b) {
        ret = node->getValue(GCSS_KEY_BOTTOM, *b);
        if (ret != css_err_none) {
            LOGE("Error: Couldn't get bottom crop");
            return css_err_noentry;
        }
    }

    return css_err_none;
}


css_err_t GraphCameraUtil::sensorGetBinningFactor(const IGraphConfig *node,
                                                 int &hBin,
                                                 int &vBin)
{
    css_err_t ret = css_err_none;

    if (!node)
        return css_err_argument;

    ret = node->getValue(GCSS_KEY_BINNING_H_FACTOR, hBin);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get horizontal binning factor");
        return ret;
    }

    ret = node->getValue(GCSS_KEY_BINNING_V_FACTOR, vBin);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get vertical binning factor");
        return ret;
    }
    return css_err_none;
}

css_err_t GraphCameraUtil::sensorGetScalingFactor(const IGraphConfig *node,
                                                 int &scalingNum,
                                                 int &scalingDenom)
{
    css_err_t ret = css_err_none;

    if (!node)
        return css_err_argument;

    ret = node->getValue(GCSS_KEY_SCALING_FACTOR_NUM, scalingNum);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get width scaling num ratio");
        return ret;
    }
    ret = node->getValue(GCSS_KEY_SCALING_FACTOR_DENOM, scalingDenom);
    if (ret != css_err_none) {
        LOGE("Error: Couldn't get width scaling num ratio");
        return ret;
    }
    return css_err_none;
}
/**
 * DEPRECATED TO BE REMOVED.
 * Kept here to allow a backward compatible change.
 * XOS tests would need to move to getInputPort
 * Then it can be deleted.
 */
css_err_t GraphCameraUtil::streamGetInputPort(int32_t execCtxId,
                                             const IGraphConfig *graphHandle,
                                             IGraphConfig **port)
{
    return getInputPort(GCSS_KEY_STREAM_ID, execCtxId, graphHandle, port);
}

css_err_t GraphCameraUtil::getInputPort(ia_uid uid,
                                       int32_t execCtxId,
                                       const IGraphConfig *graphHandle,
                                       IGraphConfig **port)
{
    css_err_t ret = css_err_none;
    GraphConfigNode *tmp = nullptr;
    GraphConfigNode *pgNode = nullptr;
    IGraphConfig *result = nullptr;
    int32_t execCtxIdFound = -1;

    if (graphHandle == nullptr || port == nullptr) {
        LOGE("nullptr pointer given");
        return css_err_argument;
    }

    *port = nullptr;

    // Use the handle to get pointer to the root of the graph
    GraphConfigNode *root = static_cast<GraphConfigNode*>(graphHandle->getRoot());
    GraphConfigItem::const_iterator it = root->begin();

    while (it != root->end()) {

        ret = root->getDescendant(GCSS_KEY_TYPE, "program_group",
                                       it, &pgNode);
        if (ret != css_err_none)
            continue;

        ret = pgNode->getValue(uid, execCtxIdFound);
        if ((ret == css_err_none) && (execCtxIdFound == execCtxId)) {

            GraphConfigItem::const_iterator it = pgNode->begin();
            while (it != pgNode->end()) {
                ret = pgNode->getDescendant(GCSS_KEY_TYPE, "port",
                                                it, &tmp);
                if (ret != css_err_none)
                    continue;
                result = tmp;
                int32_t direction = portGetDirection(result);
                if (direction == PORT_DIRECTION_INPUT) {

                    GCSS::IGraphConfig *peer;
                    ret = portGetPeer(result, &peer);
                    if (ret != css_err_none) {
                        LOGD("%s: no peer", __FUNCTION__);
                        continue;
                    }
                    peer = peer->getAncestor();
                    if (ret != css_err_none)
                        continue;
                    int32_t peerExecCtxId;

                    ret = peer->getValue(uid, peerExecCtxId);
                    if (ret != css_err_none) {
                        LOGD("%s: no %s for peer %s",
                                __FUNCTION__,
                                ItemUID::key2str(uid),
                                print(peer).c_str());
                        continue;
                    }
                    std::string str;
                    ret = peer->getValue(GCSS_KEY_TYPE, str);
                    if (ret != css_err_none) {
                        continue;
                    }
                    /* If the ancestor is not a PG, we've reached the end */
                    if (str != "program_group") {
                        *port = result;
                        return css_err_none;
                    }
                    if (peerExecCtxId == execCtxId)
                        continue;
                    /* assuming only one input per execCtx */
                    *port = result;
                    return css_err_none;
                }
            }
        }
    }
    return (*port == nullptr) ? css_err_argument : css_err_none;
}

css_err_t
GraphCameraUtil::getProgramGroups(ia_uid uid,
                                  int32_t value,
                                  const GCSS::IGraphConfig *GCHandle,
                                  std::vector<IGraphConfig*> &pgs)
{
    css_err_t ret = css_err_none;
    GraphConfigNode *result;
    std::vector<GraphConfigNode*> allProgramGroups;
    int32_t idFound = -1;

    const GraphConfigNode *temp = static_cast<const GraphConfigNode*>(GCHandle);
    GraphConfigNode::const_iterator it = temp->begin();

    while (it != temp->end()) {

        ret = temp->getDescendant(GCSS_KEY_TYPE, "program_group", it, &result);
        if (ret == css_err_none)
            allProgramGroups.push_back(result);
    }

    if (allProgramGroups.empty()) {
        LOGE("Failed to find any PG's for value id %d"
             " BUG(check graph config file)", value);
        return css_err_general;
    }

    for (size_t i = 0; i < allProgramGroups.size(); i++) {
        ret = allProgramGroups[i]->getValue(uid, idFound);
        if ((ret == css_err_none) && (idFound == value)) {

            pgs.push_back(static_cast<IGraphConfig*>(allProgramGroups[i]));
        }
    }
    return css_err_none;
}

css_err_t GraphCameraUtil::kernelGetValues(const IGraphConfig *kernelNode,
                                          int32_t *palUuid,
                                          int32_t *kernelId,
                                          uint32_t *metadata,
                                          int32_t *enable,
                                          int32_t *rcb,
                                          int32_t *branchPoint,
                                          int32_t *sinkPort)
{
    css_err_t ret = css_err_none;
    std::string metadataStr;

    if (!kernelNode)
        return css_err_argument;

    // Metadata (optional)
    if (metadata) {
        ret = kernelNode->getValue(GCSS_KEY_METADATA, metadataStr);

        if (ret == css_err_none) {
            std::size_t pos = 0;
            std::string resultStr, remainingStr = metadataStr;

            bool split = true;
            uint32_t loops = 0;

            unsigned int val;
            while (split) {
                if ((pos = remainingStr.find(",")) == string::npos) {
                    val = (uint32_t)std::stoi(remainingStr);
                    split = false;
                } else {
                    resultStr = remainingStr.substr(0, pos);
                    remainingStr = remainingStr.substr(pos + 1);
                    val = (uint32_t)std::stoi(resultStr);
                }
                metadata[loops] = val;
                loops++;
            }
        }
    }

    // Resolution changing block flag (optional)
    if (rcb) {
        ret = kernelNode->getValue(GCSS_KEY_RCB, *rcb);
        if (css_err_none != ret)
            *rcb = 0;
    }

    if (branchPoint) {
        ret = kernelNode->getValue(GCSS_KEY_BRANCH_POINT, *branchPoint);
        if (css_err_none != ret)
            *branchPoint = 0;
    }
    // Enabled (optional)
    if (enable) {
       ret = kernelNode->getValue(GCSS_KEY_ENABLED, *enable);
        if (ret != css_err_none)
            *enable = 1;
    }
    // Pal Uuid
    if (palUuid) {
        ret = kernelNode->getValue(GCSS_KEY_PAL_UUID, *palUuid);
        if (css_err_none != ret) {
            LOGE("ERROR: Couldn't get pal_uuid");
            return ret;
        }
    }
    // Kernel id
    if (kernelId) {
        ret = kernelNode->getValue(GCSS_KEY_ID, *kernelId);
        if (css_err_none != ret) {
            LOGE("Couldn't get kernel id");
            return ret;
        }
    }

    // Sink Port
    if (sinkPort) {
        ret = kernelNode->getValue(GCSS_KEY_SINK_PORT, *sinkPort);
        if (css_err_none != ret)
            *sinkPort = -1;
    }

    return css_err_none;
}

std::string
GraphCameraUtil::print(const IGraphConfig *node)
{
    string retstr = "";
    string value;
    css_err_t ret;

    ret = node->getValue(GCSS_KEY_TYPE, value);
    if (ret != css_err_none) {
        value = "NODE";
    }
    retstr += value + "[";

    ret = node->getValue(GCSS_KEY_NAME, value);
    if (ret != css_err_none) {
        value = "NA"; /** \todo fetch node key and key2str */
    }
    retstr += value + "]";

    /** \todo based on type port, kernel, also fetch ancestor info */
    return retstr;
}
