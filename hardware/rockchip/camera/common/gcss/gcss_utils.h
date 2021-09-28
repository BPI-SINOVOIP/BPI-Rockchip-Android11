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

#ifndef GCSS_UTILS_H_
#define GCSS_UTILS_H_

#include "gcss.h"
#include <set>

namespace GCSS {

/**
 * \class GraphCameraUtil
 * Class that holds utility functions to derive information from
 * GraphConfig container for Camera runtime.
 *
 * Utilities are separated from GraphConfig interface in order to
 * specialize the XML-schema that Camera runtime is dependent of
 * from the generic concept of graph information.
 *
 * These specializations include execCtxs, ports, execCtx edges
 * as well as sensor and imaging kernel details that are nested
 * in generic graph elements hierarchy.
 */
class GraphCameraUtil
{
public:
    virtual ~GraphCameraUtil() {}

    static const int32_t PORT_DIRECTION_INPUT = 0;
    static const int32_t PORT_DIRECTION_OUTPUT = 1;

    /*
     * Generic Dimensions prototype:
     *
     * Port-elements, kernels as well as sensor entities input and output
     * elements reuse the common dimensions prototype including
     *  GCSS_KEY_WIDTH, GCSS_KEY_HEIGHT, GCSS_KEY_BYTES_PER_LINE,
     *  GCSS_KEY_LEFT, GCSS_KEY_TOP, GCSS_KEY_RIGHT, GCSS_KEY_BOTTOM
     */

    /**
     * Get width, height, bpl and cropping values from the given element
     *
     * \ingroup gcss
     *
     * \param[in] node the node to read the values from
     * \param[out] w width
     * \param[out] h height
     * \param[out] bpl bytes per line
     * \param[out] l left crop
     * \param[out] t top crop
     * \param[out] r right crop
     * \param[out] b bottom crop
     */
    static css_err_t getDimensions(const IGraphConfig *node,
                                  int32_t *w = nullptr,
                                  int32_t *h = nullptr,
                                  int32_t *bpl = nullptr,
                                  int32_t *l = nullptr,
                                  int32_t *t = nullptr,
                                  int32_t *r = nullptr,
                                  int32_t *b = nullptr);

    /*
     * NODE-specialization
     */
    static IGraphConfig* nodeGetPortById(const IGraphConfig *node, uint32_t id);

    /*
     * PORT-specialization
     */

    /**
     * Check if port is at the edge
     *
     * \ingroup gcss
     *
     * A port is at the edge of the video execCtx (pipeline) if its peer is in a PG
     * that has a different execCtxID (a.k.a. pipeline id) or if its peer is a
     * virtual sink.
     *
     * Here we check for both conditions and return true if this port is at either
     * edge of a pipeline
     * \param[in] port Reference to port Graph node
     * \return true if it is edge port
     */
    static bool isEdgePort(const IGraphConfig* port);

    /**
     * Check if port is virtual
     *
     * \ingroup gcss
     *
     * Check if the port is a virtual port. this is the end point
     * of the graph. Virtual ports are the nodes of type sink.
     *
     * \param[in] port Reference to port Graph node
     * \return true if it is a virtual port
     * \return false if it is not a virtual port
     */
    static bool portIsVirtual(const IGraphConfig* port);

    /**
     * Return the port direction
     *
     * \ingroup gcss
     *
     * \param[in] port
     * \return 0 if it is an input port
     * \return 1 if it is an output port
     */
    static int portGetDirection(const IGraphConfig* port);

    /**
     * For a given port node it constructs the fourCC code used in the connection
     * object. This is constructed from the program group id.
     *
     * \ingroup gcss
     *
     * \param[in] portNode
     * \param[out] stageId Fourcc code that describes the PG where this node is
     *                     contained
     * \param[out] terminalID Fourcc code that describes the port, in FW jargon,
     *                        this is a PG terminal.
     * \return css_err_none in case of no error
     * \return css_err_argument in case some error is found
     */
    static css_err_t portGetFourCCInfo(const IGraphConfig *portNode,
                                      ia_uid& stageId,
                                      uint32_t& terminalId);

    /**
     * Retrieve the graph config node of the port that is connected to a given port.
     *
     * \ingroup gcss
     *
     * \param[in] port Node with the info of the port that we want to find its peer.
     * \param[out] peer Pointer to a node where the peer node reference will be
     *                  stored
     * \return css_err_none
     * \return css_err_argument if any of the graph settings is incorrect.
     */
    static css_err_t portGetPeer(const IGraphConfig* port, IGraphConfig** peer);

    /**
     * Finds the execCtx/execxt id of the program group that the port is in.
     *
     * \ingroup gcss
     *
     * \param[in] port The port whose execCtx/execxt id is being returned
     * \return valid execCtx id, or -1 in case of error.
     */
    static int portGetStreamId(const IGraphConfig *port);
    static int portGetExecCtxId(const IGraphConfig *port);
    static int getExecCtxIds(const IGraphConfig &setting,
                             std::set<int32_t> &execCtxIds);
    static int portGetKey(const IGraphConfig *port, ia_uid uid);

    /**
     * SENSOR-specialization
     */

    /**
     * Get binning factor values from the given node
     *
     * \ingroup gcss
     *
     * \param[in] node the node to read the values from
     * \param[out] hBin horizontal binning factor
     * \param[out] vBin vertical binning factor
     */
    static css_err_t sensorGetBinningFactor(const IGraphConfig *node,
                                           int &hBin, int &vBin);

    /**
     * Get scaling factor values from the given node
     *
     * \ingroup gcss
     *
     * \param[in] node the node to read the values from
     * \param[out] scalingNum scaling ratio
     * \param[out] scalingDenom scaling ratio
     */
    static css_err_t sensorGetScalingFactor(const IGraphConfig *node,
                                           int &scalingNum,
                                           int &scalingDenom);

    /**
     * Finds input port for the given execCtx or stream id
     *
     * \ingroup gcss
     *
     * \param[in] uid key identifying whether the next value is a stream id or
     *                a exec ctx id.
     * \param[in] execCtxId id of the execCtx OR stream id
     * \param[in] graphHandle pointer to any node in the graph
     * \param[out] port input port
     */
    static css_err_t getInputPort(ia_uid uid,
                                  int32_t execCtxId,
                                  const IGraphConfig *graphHandle,
                                  IGraphConfig **port);

    /**
     * DEPRECATED see above
     */
    static css_err_t streamGetInputPort(int32_t execCtxId,
                                       const IGraphConfig *graphHandle,
                                       IGraphConfig **port);

    /**
     *
     * Retrieve a list of program groups that belong to a given execCtx id or
     * stream id.
     * Iterates through the graph configuration storing the program groups
     * that match this execCtx id into the provided vector.
     *
     * \param[in] uid Used to determine if we search the PG per stream-id or exec
     *                ctx-id
     * \param[in] value Depending on the key parameter this is the value of the
     *                  stream-id or execCtx-id to match.
     * \param[in] GCHandle Handle to get graph result.
     * \param[out] programGroups Vector with the nodes that match the criteria.
     */
    static css_err_t getProgramGroups(ia_uid uid,
                                     int32_t value,
                                     const GCSS::IGraphConfig *GCHandle,
                                     std::vector<IGraphConfig*> &pgs);

    /**
     * Helper function to get values from the kernel settings
     * \todo Moved to bxt aic utils, will be removed from here after hal has
     *       adapted to changes.
     * \ingroup gcss
     *
     * \param[in] kernelNode
     * \param[out] palUuid
     * \param[out] kernelId
     * \param[out] metadata
     * \param[out] enable
     * \param[out] rcb
     * \param[out] branchPoint
     */
    static css_err_t kernelGetValues(const IGraphConfig *kernelNode,
                                    int32_t *palUuid = nullptr,
                                    int32_t *kernelId = nullptr,
                                    uint32_t *metadata = nullptr,
                                    int32_t *enable = nullptr,
                                    int32_t *rcb = nullptr,
                                    int32_t *branchPoint = nullptr,
                                    int32_t *sinkPort = nullptr);
    /**
     * Debug utils
     */

    /**
     * Pretty print any recognized element: node, port, kernel
     *
     * \ingroup gcss
     *
     * \param[in] node of which name to print
     */
    static std::string print(const IGraphConfig *node);

};
} // namespace
#endif /* GCSS_UTILS_H_ */
