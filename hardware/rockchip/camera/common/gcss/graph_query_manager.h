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

#ifndef GRAPH_QUERY_MANAGER_H
#define GRAPH_QUERY_MANAGER_H

#include "gcss.h"
#include "gcss_item.h"
#include "gcss_utils.h"

namespace GCSS {

/** RelayControl :
 * Controls of assigning GraphConfig container elements */
namespace RelayControl {
    typedef int8_t Rule;
    const Rule RELAY_RULE_ADD_NODES = 0x1;
    const Rule RELAY_RULE_HANDLE_OPTIONS = 1<<1;
    const Rule RELAY_RULE_PROPAGATE = 1<<2;
    const Rule RELAY_RULE_OVERWRITE = 1<<3;
}

class GraphQueryManager
{
public:
    ~GraphQueryManager(){};

    typedef std::vector<GraphConfigNode*> GraphQueryResult;
    typedef std::map<ItemUID, std::string> GraphQuery;

    css_err_t getGraph(GraphConfigNode*, GraphConfigNode*);
    css_err_t queryGraphs(const GraphQuery&,
                         GraphQueryResult&,
                         bool strict = true);
    css_err_t queryGraphs(const GraphQuery&,
                         const GraphQueryResult&,
                         GraphQueryResult&,
                         bool strict = true);
    void setGraphSettings(const GraphConfigNode* settings) {mGraphSettings = settings;}
    void setGraphDescriptor(const GraphConfigNode* descriptor) {mGraphDescriptor = descriptor;}
    void setGraphSettings(const IGraphConfig* settings);
    void setGraphDescriptor(const IGraphConfig* descriptor);

    static GraphConfigNode* getPortPeer(GraphConfigNode* portNode);
    static css_err_t handleAttributeOptions(GraphConfigNode *node,
                                           ia_uid attribute_key,
                                           const std::string &newValue);
private:
    // true = every search item has to match, false = at least one match
    bool strictQuery;
    // Store parsed data into these containers, instead of using a single node
    const GraphConfigNode *mGraphSettings;
    const GraphConfigNode *mGraphDescriptor;

    css_err_t goThroughSettings(const GraphQuery&,
                               const GraphQueryResult&,
                               GraphQueryResult&);
    css_err_t goThroughSearchItems(const GraphQuery&, GraphConfigNode*, uint16_t&);
    static css_err_t addDescendantsFromNode(
            GraphConfigNode *to,
            GraphConfigNode *from,
            RelayControl::Rule rr =   RelayControl::RELAY_RULE_ADD_NODES
                                    | RelayControl::RELAY_RULE_OVERWRITE);
    static css_err_t addSensorModeData(GraphConfigNode *sensorNode,
                                      GraphConfigNode *sensorModesNode,
                                      const std::string &sensorModeID);
    css_err_t getConnectionData(const std::string& source_connection,
                               const std::string& sink_connection,
                               GraphConfigNode *settings,
                               GraphConfigNode *ret_node);
    css_err_t getStaticConnectionData(const std::string& source_connection,
                                      const std::string& sink_connection,
                                      GraphConfigNode *ret_node);
    IGraphConfig *copyNodeToResult(GraphConfigNode* descriptorNodes,
                                   ia_uid nodeId,
                                   GraphConfigNode* resultNode);
    css_err_t propagateIntAttribute(IGraphConfig *srcNode,
                                    IGraphConfig *dstNode,
                                    ia_uid attributeId);
    css_err_t propagateStrAttribute(IGraphConfig *srcNode,
                                    IGraphConfig *dstNode,
                                    ia_uid attributeId);
};
} // namespace
#endif
