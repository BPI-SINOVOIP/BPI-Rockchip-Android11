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

#include <assert.h>
#include "LogHelper.h"
#include "graph_query_manager.h"

using namespace GCSS;
using namespace GCSS::RelayControl;
using namespace std;
using namespace android::camera2;

void GraphQueryManager::setGraphSettings(const IGraphConfig *graphSettings)
{
    // \todo cast while the memberi is not IGraphConfig type
    mGraphSettings = static_cast<const GraphConfigNode*>(graphSettings);
}
void GraphQueryManager::setGraphDescriptor(const IGraphConfig *graphDescriptor)
{
    // \todo cast while the memberi is not IGraphConfig type
    mGraphDescriptor = static_cast<const GraphConfigNode*>(graphDescriptor);
}

css_err_t GraphQueryManager::queryGraphs(const GraphQuery& query,
                                        GraphQueryResult& ret_vec,
                                        bool strict)
{
    css_err_t ret;
    strictQuery = strict;

    // Get all settings level nodes
    GraphConfigNode::gcss_node_vector settingsVector;
    ret = mGraphSettings->getAllDescendants(settingsVector, GCSS_KEY_SETTINGS);
    if (ret != css_err_none) {
        LOGD("Invalid XML. Settings tag has no children.");
        return ret;
    }

    ret = goThroughSettings(query, settingsVector, ret_vec);
    return ret;
}

/**
 * Searches only from the provided search nodes
 * \param query std::map of items to search for
 * \param baseResults search only from inside these settings
 * \param ret this vector is populated with references to matched settings
 * \return css_err_t
 */
css_err_t GraphQueryManager::queryGraphs(const GraphQuery& query,
                                        const GraphQueryResult& baseResults,
                                        GraphQueryResult& ret_vec,
                                        bool strict)
{
    strictQuery = strict;
    return goThroughSettings(query, baseResults, ret_vec);
}

/**
 * Go through every <settings>, and look for matches in search criteria
 */
css_err_t GraphQueryManager::goThroughSettings(const GraphQuery &query,
                                              const GraphQueryResult& settingsNodes,
                                              GraphQueryResult& ret_vec)
{
    css_err_t ret;
    for (uint16_t i = 0; i < settingsNodes.size(); i++) {

        // Search hit counter
        uint16_t result = 0;

        // Loop through search items and put hit count to result
        ret = goThroughSearchItems(query, settingsNodes[i], result);

        if (ret != css_err_none) {
            return ret;
        }
        if (!strictQuery && result > 0) {
            ret_vec.push_back(settingsNodes[i]);
        }
        else if (result == query.size()) {
            ret_vec.push_back(settingsNodes[i]);
        }
    }

    if (ret_vec.size() == 0) {
        LOGE("No settings matching the query found");
        return css_err_general;
    }
    return css_err_none;
}

/**
 * Checks every search item against settings data. If search is strict, we
 * escape as soon as some condition is not found.
 * \param query map of search items
 * \param settingsNode the settings node to search from
 * \param result count of matches
 * \return css_err_t
 */
css_err_t GraphQueryManager::goThroughSearchItems(const GraphQuery &query,
                                                 GraphConfigNode *settingsNode,
                                                 uint16_t &result)
{
    css_err_t ret;
    GraphQuery::const_iterator ite;
    bool allFound = true;
    std::string str_val;
    int int_val = -1;
    for (ite = query.begin(); ite != query.end(); ++ite) {

        GraphConfigNode* graphNode = settingsNode;
        /**
         * Loop through item uids in the search query. Traverse the tree
         * node by node. Return back to root node when there's dead end or
         * match is found.
         */
        for (uint16_t ii = 0; ii < ite->first.size(); ii++) {

            // If at the last uid in vector, check for attribute
            if (ii == ite->first.size() - 1) {

                // Try to get int
                ret = graphNode->getValue(ite->first[ii], int_val);
                if (ret == css_err_none) {
                    str_val = std::to_string(int_val);
                } else {
                    // try to get string
                    ret = graphNode->getValue(ite->first[ii], str_val);
                    if (ret != css_err_none) {
                        if (strictQuery)
                            allFound = false;
                    }
                }

                // If searched string matches, increment result counter
                if (str_val == ite->second)
                    result++;

                break;
            } else {
                // Get next level node
                ret = graphNode->getDescendant(ite->first[ii], &graphNode);

                if (ret != css_err_none) {
                    // Dead end
                    if (strictQuery)
                        allFound = false;
                    break;
                }
            }
        }
        if (allFound != true)
            break;
    }
    return css_err_none;
}

/**
 * Builds a graph which is a combination of data in graph descriptor and
 * graph settings. Resulting graph is based on connections defined in graph
 * descriptor's graph node. This function then applies settings for those
 * connections from the given settingsGraph.
 *
 * \param settingsGraph graph node containing the settings data
 * \param results  the result graph to return
 */
css_err_t GraphQueryManager::getGraph(GraphConfigNode* settingsGraph,
                                      GraphConfigNode* results)
{
    css_err_t ret;
    int settingsKey = -1, graphID = -1;

    if (settingsGraph == nullptr || mGraphDescriptor == nullptr || results == nullptr)
        return css_err_argument;

    // Get key from the settings and add that to the result graph
    ret = settingsGraph->getValue(GCSS_KEY_KEY, settingsKey);
    if (ret != css_err_none)
        return ret;

    ret = results->addValue(GCSS_KEY_KEY, settingsKey);
    if (ret != css_err_none)
        return ret;

    /* Get graph id from the settings and find corresponding graph node from the
     * graph descriptor */
    ret = settingsGraph->getValue(GCSS_KEY_ID, graphID);
    if (ret != css_err_none)
        return ret;

    /* Get "graphs" node from the descriptor. Use graph id, and enumerate
    through children to find graph that is associated with selected settings.*/
    const IGraphConfig *gcDescriptor = static_cast<const IGraphConfig*>(mGraphDescriptor);

    IGraphConfig *graphs = gcDescriptor->getDescendant(GCSS_KEY_GRAPHS);
    if (graphs == nullptr)
        return css_err_data;

    uint32_t descCount = graphs->getDescendantCount();
    IGraphConfig *graphNode = nullptr;
    for (uint32_t i = 0; i < descCount && graphNode == nullptr; i++) {
        graphNode = graphs->iterateDescendantByIndex(GCSS_KEY_ID, ia_uid(graphID), i);
    }
    if (graphNode == nullptr) {
        LOGE("could not find graph with id %d from graph descriptor",
                       graphID);
        return css_err_data;
    }
    // \todo casting needed
    GraphConfigNode *gc_graph_node = static_cast<GraphConfigNode*>(graphNode);

    // Copy graph config graph node
    GraphConfigNode *gc_graph_node_copy = gc_graph_node->copy();

    if (gc_graph_node_copy == nullptr) {
        LOGE("Error creating GraphConfigNode: No memory");
        return css_err_nomemory;
    }

    // Add graph node with connections to return tree
    ret = results->insertDescendant(gc_graph_node_copy, GCSS_KEY_GRAPH);
    if (ret != css_err_none)
        return ret;

    /**
     * Loop through all connections and add associated nodes from descriptor
     *
     * \todo if graph has other children than connections, a type attribute check
     * is needed using getDescendant iterator
     */
    GraphConfigItem::const_iterator it = gc_graph_node->begin();
    for (;it != gc_graph_node->end(); ++it) {
        if (it->second->type != NODE || it->first != GCSS_KEY_CONNECTION)
            continue;
        GraphConfigNode * connection_node = static_cast<GraphConfigNode*>(it->second);

        // Insert connected nodes to result graph and add the settings
        std::string source_connection;
        std::string sink_connection;
        ret = connection_node->getValue(GCSS_KEY_SOURCE, source_connection);
        if (ret != css_err_none)
            return ret;
        ret = connection_node->getValue(GCSS_KEY_SINK, sink_connection);
        if (ret != css_err_none)
            return ret;
        string staticConnection;
        ret = connection_node->getValue(GCSS_KEY_STATIC, staticConnection);
        if (ret != css_err_none || staticConnection == "false") {
            ret = getConnectionData(source_connection,
                                    sink_connection,
                                    settingsGraph,
                                    results);
            if (ret != css_err_none)
                return ret;
        } else {
            ret = getStaticConnectionData(source_connection,
                                          sink_connection,
                                          results);
            if (ret != css_err_none)
                return ret;
        }

    }

    /* apply everything from settings */
    ret = addDescendantsFromNode(results, settingsGraph,
              RELAY_RULE_ADD_NODES
            | RELAY_RULE_PROPAGATE
            | RELAY_RULE_OVERWRITE);
    if (ret != css_err_none) {
        LOGE("Failed to add settings to the result graph");
        return ret;
    }

    // Get sensor node from settings and get its id
    /** \todo get rid of SENSOR specification in getGraph(), nesting
     * sensor mode data into sensor node based on mode attribute in settings
     * could re-use the common option-list logic. We just have to solve how
     * to provide option lists also via the settings file as option lists are
     * part of descriptor file atm.
     * Now just considering 'sensor' not mandatory node to allow describing
     * partial graphs or graphs with different types of sources.
     */
    GraphConfigNode * settingsSensorNode;
    ret = settingsGraph->getDescendant(GCSS_KEY_SENSOR, &settingsSensorNode);
    if (ret != css_err_none) {
        LOGW("getGraph didn't find sensor, ignoring sensor modes.");
        return css_err_none;
    }

    std::string sensorModeID;
    ret = settingsSensorNode->getValue(GCSS_KEY_MODE_ID, sensorModeID);
    if (ret != css_err_none) {
        LOGW("GetGraph failed to set sensor mode. Sensor lacks mode_id");
        return ret;
    }

    GraphConfigNode * descSensorNode;
    ret = results->getDescendant(GCSS_KEY_SENSOR, &descSensorNode);
    if (ret != css_err_none)
        return ret;

    /**
     * Find sensor modes from settings xml and apply contents from matching
     * sensor mode to the sensor node.
     */
    GraphConfigNode *sensorModesNode;
    ret = mGraphSettings->getDescendant(GCSS_KEY_SENSOR_MODES, &sensorModesNode);
    if (ret != css_err_none) {
        LOGW("Settings file is missing sensor_modes");
        return ret;
    }

    ret = addSensorModeData(descSensorNode, sensorModesNode, sensorModeID);
    if (ret != css_err_none)
        return ret;

    return css_err_none;
}
/**
 * Populate sensor node with data from sensor mode settings based on id.
 * \param sensorModesNode [in] Node that contains sensor modes
 * \param sensorModeID [in] id of the sensor mode to get
 * \return css_err_t
 */
css_err_t
GraphQueryManager::addSensorModeData(GraphConfigNode *sensorNode,
                                     GraphConfigNode *sensorModesNode,
                                     const std::string &sensorModeID)
{
    css_err_t ret;

    // Add globals from sensor modes node
    ret = addDescendantsFromNode(sensorNode,
                                 sensorModesNode,
                                 RELAY_RULE_OVERWRITE);
    if (ret != css_err_none) {
        LOGE("couldn't add settings from sensor modes node");
        return ret;
    }
    GraphConfigNode * sensorModeNode;
    GraphConfigItem::const_iterator it = sensorModesNode->begin();

    ret = sensorModesNode->getDescendant(GCSS_KEY_NAME,
                                         sensorModeID,
                                         it,
                                         &sensorModeNode);

    if (ret != css_err_none)
        return ret;

    // Add contents from sensor mode to sensor
    return addDescendantsFromNode(sensorNode, sensorModeNode);
}

GraphConfigNode* GraphQueryManager::getPortPeer(GraphConfigNode *portNode)
{
    GraphConfigNode *root, *retNode = nullptr;
    css_err_t ret;

    /** \todo check the type once type is of integer attribute */
    /** \todo peer attribute should be of reference type */
    GraphConfigAttribute * peerAttr = nullptr;
    ret = portNode->getAttribute(GCSS_KEY_PEER, &peerAttr);
    if (ret != css_err_none) {
        /* no peer, nowhere to propagate */
        return nullptr;
    }
    assert(peerAttr);

    std::string peerPortStr;
    ret = peerAttr->getValue(peerPortStr);
    if (ret != css_err_none)
        return nullptr;

    root = portNode->getRootNode();
    assert(root);
    size_t del = peerPortStr.find(":");
    string peerNode, peerPort;
    if (del == string::npos) {
        /* no delimiter -> virtual sink */
        peerNode = peerPortStr;
        ret = root->getDescendant(ItemUID::str2key(peerNode), &retNode);
        if (ret != css_err_none)
            return nullptr;
    } else {
        peerNode = peerPortStr.substr(0, del);
        peerPort = peerPortStr.substr(del+1);
        ret = root->getDescendant(ItemUID::str2key(peerNode), &retNode);
        if (ret != css_err_none)
            return nullptr;
        ret = retNode->getDescendant(ItemUID::str2key(peerPort), &retNode);
        if (ret != css_err_none)
            return nullptr;
    }
    return retNode;
}
/**
 * copy node from descriptor to result node during getConnectionData
 *
 * One of the operations done several times by get graph is copying nodes from
 * the descriptor tree to the new result node tree that we are building
 *
 * This operation checks that if the node is already present in the result node
 * tree we do not add it more than once.
 *
 * \param[in] descriptorNodes Root node from the graph descriptor where all the
 *                             nodes in the graph are listed. This is the source.
 * \param[in] nodeId Unique id of the node to to copy
 * \param[out] resultNode Root node of the result settings being constructed by
 *                         getGraph. This is the destination of the copy
 * \return Pointer to the copied node in the result tree
 */
IGraphConfig *GraphQueryManager::copyNodeToResult(GraphConfigNode* descriptorNodes,
                                                  ia_uid nodeId,
                                                  GraphConfigNode* resultNode)
{
    css_err_t ret = css_err_none;
    IGraphConfig *descNode = descriptorNodes->getDescendant(nodeId);
    if (!descNode) {
        LOGE("Node(%s) not found from descriptor",
                       ItemUID::key2str(nodeId));
        return nullptr;
    }
    IGraphConfig *outNode = resultNode->getDescendant(nodeId);
    if (!outNode) {
        ret = resultNode->insertDescendant(
                static_cast<GraphConfigNode*>(descNode)->copy(),
                nodeId);
        if (ret != css_err_none)
            return nullptr;
        outNode = resultNode->getDescendant(nodeId);
        assert(outNode);
    }
    return outNode;
}
/**
 * Takes source and destination strings for a connection and looks for the
 * named nodes from the graph descriptor, it then copies the nodes to the
 * result tree and populates the corresponding values from the settings.
 *
 * This method is mainly used during get graph to construct the combined
 * settings + graph information.
 *
 * \param[in] source_connection Contains the connection source string
 * \param[in] sink_connection Contains the connection sink string
 * \param[in] settings It has the container node for settings
 * \param[out] ret_node The root node with the results of get graph that we are
 *                       building
 * \return css_err_t
 */
css_err_t
GraphQueryManager::getConnectionData(
        const std::string& source_connection,
        const std::string& sink_connection,
        GraphConfigNode *settings,
        GraphConfigNode *ret_node)
{
    GraphConfigNode *nodes = nullptr;
    css_err_t ret = css_err_general;
    string set_sink_connection = sink_connection;

    if (ret_node == nullptr)
        return css_err_argument;

    ret = mGraphDescriptor->getDescendant(GCSS_KEY_NODES, &nodes);
    if (ret != css_err_none) {
        LOGE("Error, graph_descriptor does not have a 'nodes' node");
        return ret;
    }

    /*
     *  Split sink/source values (node:port) to find the node name and
     *  the port name.
     *  Handle special cases: virtual sinks and sources.
     *  Virtual sink is one end point of the graph.
     *  Virtual source is a buffer source that inject buffers to the graph.
     *  In those cases the port name is left empty and the uid set to
     *  GCSS_KEY_NA
     */
    std::string src_node_name;
    std::string src_port_name;
    std::size_t pos = source_connection.find(":");
    if (pos == string::npos) {
        src_node_name = source_connection;
    } else {
        src_node_name = source_connection.substr(0, pos);
        src_port_name = source_connection.substr(pos+1);
    }

    pos = sink_connection.find(":");
    std::string dst_node_name;
    std::string dst_port_name;
    if (pos == string::npos) {
        dst_node_name = sink_connection;
    } else {
        dst_node_name = sink_connection.substr(0, pos);
        dst_port_name = sink_connection.substr(pos+1);
    }

    ia_uid src_node_uid = ItemUID::str2key(src_node_name);
    ia_uid dst_node_uid = ItemUID::str2key(dst_node_name);
    ia_uid src_port_uid = ItemUID::str2key(src_port_name);
    ia_uid dst_port_uid = ItemUID::str2key(dst_port_name);

    /* Handle source : */
    /* source settings missing implicitly mean that source is not part of
     * the active graph or does not need settings */
    IGraphConfig *setSrcNode = settings->getDescendant(src_node_uid);
    if (!setSrcNode) {
        LOGW("Node(%s) not found from settings, ignoring connection",
                src_node_name.c_str());
        return css_err_none;
    }

    IGraphConfig *setSrcPort = nullptr;
    if (src_port_uid == GCSS_KEY_NA) {
        // in case of virtual source the node is treated as a port
        setSrcPort = setSrcNode;
    } else {
        setSrcPort = setSrcNode->getDescendant(src_port_uid);
        if (!setSrcPort) {
            LOGW("Node(%s) port %s not found from settings",
                           src_node_name.c_str(), src_port_name.c_str());
            return css_err_none;
        }
    }

    /* Copy src node from descriptor nodes to result */
    IGraphConfig *outSrcNode = copyNodeToResult(nodes, src_node_uid, ret_node);
    if (outSrcNode == nullptr) {
        LOGE("Failed to copy src node(%s) from descriptor to settings",
                                  src_node_name.c_str());
        return css_err_general;
    }
    /*
     * A src port is disabled only if the attribute enabled is present and set
     * to 0, if the attribute is not present it is assumed that the port is
     * enabled.
     */
    int32_t is_src_port_enabled;
    ret = setSrcPort->getValue(GCSS_KEY_ENABLED, is_src_port_enabled);
    if (ret == css_err_none && !is_src_port_enabled) {
        // src port disabled skip connection processing
        LOGD("Src port %s disabled, skip dst and peer processing",
                      src_node_name.c_str());
        return css_err_none;
    }

    /* For virtual sinks check if source settings explicitly redefine the
     * connection to sink. If not just take the sink defined by graph.
     * If virtual sink is not in settings then ignore. All active virtual
     * sinks should appear in the settings
     */
    if (dst_port_uid == GCSS_KEY_NA) {

        ret = setSrcPort->getValue(GCSS_KEY_PEER, set_sink_connection);
        if (ret != css_err_none) {
            LOGD("Using default connection %s",
                         source_connection.c_str());
        } else {
            // override the destination node uid
            dst_node_uid = ItemUID::str2key(set_sink_connection);
            LOGD("Overriding destination node %s with %s",
                    dst_node_name.c_str(),set_sink_connection.c_str());
        }
        IGraphConfig *setDstNode = settings->getDescendant(dst_node_uid);
        if (setDstNode == nullptr) {
            LOGD("Ignoring node %s for not being in settings",
                        dst_node_name.c_str());
            return css_err_none;
        }
        //add for rockchip, zyc
        int32_t is_dst_port_enabled;
        ret = setDstNode->getValue(GCSS_KEY_ENABLED, is_dst_port_enabled);
        if (ret == css_err_none && !is_dst_port_enabled) {
            // dst port disabled skip connection processing
            LOGD("Dst port %s disabled, skip dst and peer processing",
                          dst_node_name.c_str());
            return css_err_none;
        }
    }

    /* Handle sink : copy sink node to results */
    IGraphConfig *outDstNode = copyNodeToResult(nodes, dst_node_uid, ret_node);
    if (outDstNode == nullptr) {
        LOGE("Failed to copy dst node(%s) from descriptor to settings",
                                  dst_node_name.c_str());
        return css_err_general;
    }

    if (dst_port_uid == GCSS_KEY_NA) {
        /* for virtual sinks we add direction input as if they were ports */
        ret = static_cast<GraphConfigNode*>(outDstNode)
            ->addValue(GCSS_KEY_DIRECTION, 0);
        if (ret != css_err_none)
            return ret;
    }

    IGraphConfig *outSrcPort;
    if (src_port_uid == GCSS_KEY_NA) {
        // in case of virtual source the node is treated as a port
        outSrcPort = outSrcNode;
    } else {
        outSrcPort = outSrcNode->getDescendant(src_port_uid);
        if (!outSrcPort) {
            LOGE("Node(%s) has no port named '%s'",
                          src_node_name.c_str(),
                          src_port_name.c_str());
            return css_err_general;
        }
    }

    IGraphConfig *outDstPort;
    if (dst_port_uid == GCSS_KEY_NA) {
        /* in case of virtual sink, the node is considered as a port */
        outDstPort = outDstNode;

        string type;
        ret = outDstPort->getValue(GCSS_KEY_TYPE, type);
        if (ret != css_err_none) {
            LOGE("No type for connected peer node");
            return ret;
        }

        if (type.compare("sink") != 0) {
            LOGE("sink connection attribute without port '%s' not pointing to virtual sink", type.c_str());
            return css_err_general;
        }

        /** \todo WORKAROUND[START]: add stream_id's to every 'sink' */
        int32_t stream_id;
        /* check if already set */
        ret = outDstPort->getValue(GCSS_KEY_STREAM_ID, stream_id);
        if (ret != css_err_none) {
            /* read stream id from ancestor of the port that we are propagating */
            ret = outSrcNode->getValue(GCSS_KEY_STREAM_ID, stream_id);
            if (ret != css_err_none) {
                LOGE("No stream_id set for connected peer");
                return ret;
            }
            ret = static_cast<GraphConfigNode*>(outDstPort)
                    ->addValue(GCSS_KEY_STREAM_ID, stream_id);
            if (ret != css_err_none) {
                LOGE("Failed to add stream ID to sink peer");
                return ret;
            }
        }
        /* WORKAROUND[END] */
    } else {
        outDstPort = outDstNode->getDescendant(dst_port_uid);
        if (!outDstPort) {
            LOGE("Node(%s) has no port named '%s'",
                          dst_node_name.c_str(),
                          dst_port_name.c_str());
            return css_err_general;
        }
    }

    /*
     * Source and destination are valid.
     * Set the peer attribute for each of the ends of the connection.
     */
    std::string peerStr;
    GraphConfigStrAttribute * peerAttribute;
    ret = outSrcPort->getValue(GCSS_KEY_PEER, peerStr);
    if (ret == css_err_none) {
        LOGD("Node(%s) port '%s' already connected once to '%s'",
                     src_node_name.c_str(),
                     src_port_name.c_str(),
                     peerStr.c_str());
    }
    // even the src port is already in another connection,
    // Allow port to have multiple connections.
    peerAttribute = new GraphConfigStrAttribute;
    if (peerAttribute == nullptr)
        return css_err_nomemory;
    peerAttribute->setValue(set_sink_connection);
    ret = static_cast<GraphConfigNode*>(outSrcPort)
            ->insertDescendant(peerAttribute, GCSS_KEY_PEER);
    if (ret != css_err_none) {
        delete peerAttribute;
        return ret;
    }

    ret = outDstPort->getValue(GCSS_KEY_PEER, peerStr);
    if (ret != css_err_none) {
        ret = static_cast<GraphConfigNode*>(outDstPort)
                ->addValue(GCSS_KEY_PEER, source_connection);
        if (ret != css_err_none) {
            return ret;
        }
    } else {
        LOGD("Node(%s) port '%s' already connected once to '%s' adding new peer",
                      dst_node_name.c_str(),
                      dst_port_name.c_str(),
                      peerStr.c_str());
        // even the dst port is already in another connection,
        // Allow port to have multiple connections.
        peerAttribute = new GraphConfigStrAttribute;
        if (peerAttribute == nullptr)
            return css_err_nomemory;
        peerAttribute->setValue(source_connection);
        ret = static_cast<GraphConfigNode*>(outDstPort)
                ->insertDescendant(peerAttribute, GCSS_KEY_PEER);
        if (ret != css_err_none) {
            delete peerAttribute;
            return ret;
        }
    }

    return css_err_none;
}

/**
 * Handler of static connections
 *
 * Static connections propagate the nodes to the combined settings (output of
 * getGraph, here also referred as the result) no matter what is the content of
 * the settings.
 *
 * For connections with sinks as destination, it propagates the following
 * properties from the source port to the sink:
 * - EXEC_CTX_ID
 * - CONTENT_TYPE
 *
 * Like regular connections it also sets the PEER attribute correctly in each
 * end-node of the connection.
 */
css_err_t
GraphQueryManager::getStaticConnectionData(
        const std::string& source_connection,
        const std::string& sink_connection,
        GraphConfigNode *ret_node)
{
    GraphConfigNode *nodes = nullptr;
    css_err_t ret = css_err_general;
    string set_sink_connection = sink_connection;

    if (ret_node == nullptr)
        return css_err_argument;

    ret = mGraphDescriptor->getDescendant(GCSS_KEY_NODES, &nodes);
    if (ret != css_err_none) {
        LOGE("Error, graph_descriptor does not have a 'nodes' node");
        return ret;
    }

    /*
     *  Split sink/source values (node:port) to find the node name and
     *  the port name.
     *  Handle special cases: virtual sinks and sources.
     *  Virtual sink is one end point of the graph.
     *  Virtual source is a buffer source that inject buffers to the graph.
     *  In those cases the port name is left empty and the uid set to
     *  GCSS_KEY_NA
     */
    std::string src_node_name;
    std::string src_port_name;
    std::size_t pos = source_connection.find(":");
    if (pos == string::npos) {
        src_node_name = source_connection;
    } else {
        src_node_name = source_connection.substr(0, pos);
        src_port_name = source_connection.substr(pos+1);
    }

    pos = sink_connection.find(":");
    std::string dst_node_name;
    std::string dst_port_name;
    if (pos == string::npos) {
        dst_node_name = sink_connection;
    } else {
        dst_node_name = sink_connection.substr(0, pos);
        dst_port_name = sink_connection.substr(pos+1);
    }

    ia_uid src_node_uid = ItemUID::str2key(src_node_name);
    ia_uid dst_node_uid = ItemUID::str2key(dst_node_name);
    ia_uid src_port_uid = ItemUID::str2key(src_port_name);
    ia_uid dst_port_uid = ItemUID::str2key(dst_port_name);

    /* Handle source : */

    /* Copy src node from descriptor nodes to result */
    IGraphConfig *outSrcNode = copyNodeToResult(nodes, src_node_uid, ret_node);
    if (outSrcNode == nullptr) {
        LOGE("Failed to copy src node(%s) from descriptor to settings",
                                  src_node_name.c_str());
        return css_err_general;
    }

    /* Handle sink : copy sink node to results */
    IGraphConfig *outDstNode = copyNodeToResult(nodes, dst_node_uid, ret_node);
    if (outDstNode == nullptr) {
        LOGE("Failed to copy dst node(%s) from descriptor to settings",
                                  dst_node_name.c_str());
        return css_err_general;
    }

    if (dst_port_uid == GCSS_KEY_NA) {
        /* for virtual sinks we add direction input as if they were ports */
        ret = static_cast<GraphConfigNode*>(outDstNode)
            ->addValue(GCSS_KEY_DIRECTION, 0);
        if (ret != css_err_none)
            return ret;
    }

    IGraphConfig *outSrcPort;
    if (src_port_uid == GCSS_KEY_NA) {
        // in case of virtual source the node is treated as a port
        outSrcPort = outSrcNode;
    } else {
        outSrcPort = outSrcNode->getDescendant(src_port_uid);
        if (!outSrcPort) {
            LOGE("Node(%s) has no port named '%s'",
                          src_node_name.c_str(),
                          src_port_name.c_str());
            return css_err_general;
        }
    }

    IGraphConfig *outDstPort;
    if (dst_port_uid == GCSS_KEY_NA) {
        /* in case of virtual sink, the node is considered as a port */
        outDstPort = outDstNode;
        /* Propagate the execution context of the src node into the sink */
        propagateIntAttribute(outSrcNode, outDstNode, GCSS_KEY_EXEC_CTX_ID);

        /* Propagate the content_type of the src node into the sink */
        propagateStrAttribute(outSrcNode, outDstNode, GCSS_KEY_CONTENT_TYPE);
    } else {
        outDstPort = outDstNode->getDescendant(dst_port_uid);
        if (!outDstPort) {
            LOGE("Node(%s) has no port named '%s'",
                          dst_node_name.c_str(),
                          dst_port_name.c_str());
            return css_err_general;
        }
    }

    /*
     * Source and destination are valid.
     * Set the peer attribute for each of the ends of the connection.
     */
    std::string peerStr;
    GraphConfigStrAttribute * peerAttribute;
    ret = outSrcPort->getValue(GCSS_KEY_PEER, peerStr);
    if (ret == css_err_none) {
        LOGD("Node(%s) port '%s' already connected once to '%s'",
                     src_node_name.c_str(),
                     src_port_name.c_str(),
                     peerStr.c_str());
    }
    // even the src port is already in another connection,
    // Allow port to have multiple connections.
    peerAttribute = new GraphConfigStrAttribute;
    if (peerAttribute == nullptr)
        return css_err_nomemory;
    peerAttribute->setValue(set_sink_connection);
    ret = static_cast<GraphConfigNode*>(outSrcPort)
            ->insertDescendant(peerAttribute, GCSS_KEY_PEER);
    if (ret != css_err_none) {
        delete peerAttribute;
        return ret;
    }

    ret = outDstPort->getValue(GCSS_KEY_PEER, peerStr);
    if (ret != css_err_none) {
        ret = static_cast<GraphConfigNode*>(outDstPort)
                ->addValue(GCSS_KEY_PEER, source_connection);
        if (ret != css_err_none) {
            return ret;
        }
    } else {
        LOGD("Node(%s) port '%s' already connected once to '%s' adding new peer",
                      dst_node_name.c_str(),
                      dst_port_name.c_str(),
                      peerStr.c_str());
        // even the dst port is already in another connection,
        // Allow port to have multiple connections.
        peerAttribute = new GraphConfigStrAttribute;
        if (peerAttribute == nullptr)
            return css_err_nomemory;
        peerAttribute->setValue(source_connection);
        ret = static_cast<GraphConfigNode*>(outDstPort)
                ->insertDescendant(peerAttribute, GCSS_KEY_PEER);
        if (ret != css_err_none) {
            delete peerAttribute;
            return ret;
        }
    }

    return css_err_none;
}
css_err_t GraphQueryManager::handleAttributeOptions(GraphConfigNode *node,
                                                   ia_uid attribute_key,
                                                   const string &newValue)
{
    css_err_t ret;

    /* find if ite->second has an option list */
    GraphConfigNode *opNode = nullptr;
    GraphConfigNode::const_iterator op_ite;
    for (op_ite = node->begin(); op_ite != node->end(); ++op_ite) {
        /** \todo cannot use generic getDescendant overloads for searh because
         * need to ensure that returned node is one of options */
        if (op_ite->second->type != NODE)
            continue;
        if (op_ite->first != GCSS_KEY_OPTIONS)
            continue;
        css_err_t ret = node->iterateAttributes(GCSS_KEY_ATTRIBUTE,
                                               (int)attribute_key,
                                               op_ite);
        if (ret == css_err_none) {
            opNode = static_cast<GraphConfigNode*>(op_ite->second);
            break;
        }
    }
    if (!opNode)
        return css_err_noentry;
    // select option where value matches newValue
    op_ite = opNode->begin();
    ret = opNode->getDescendant(GCSS_KEY_VALUE, newValue, op_ite, &opNode);
    if (ret != css_err_none) {
        LOGE("Failed to find attribute value '%s' from its option list", newValue.c_str());
        return ret;
    }
    ret = opNode->getDescendant(GCSS_KEY_APPLY, &opNode);
    if (ret != css_err_none)
        return css_err_none;

    ret = addDescendantsFromNode(node->getRootNode(),
                                 opNode,
                                 RELAY_RULE_OVERWRITE);
    if (ret != css_err_none) {
        LOGE("Failed to apply option attributes");
        return ret;
    }
    return css_err_none;
}

/**
 * Populate object with children of the param node
 */
css_err_t GraphQueryManager::addDescendantsFromNode(
        GraphConfigNode* to,
        GraphConfigNode* from,
        Rule rr)
{
    GraphConfigItem::gcss_item_map::const_iterator ite;
    css_err_t ret;

    if (!to || ! from)
        return css_err_argument;

    for (ite = from->item.begin(); ite != from->item.end(); ++ite) {

        GraphConfigNode * gc_settings_node = nullptr;
        // traverse to descendant if it already exists
        ret = to->getDescendant(ite->first, &gc_settings_node);
        if (ret == css_err_none) {
            /* never overwrite options */
            if (!(rr & RELAY_RULE_OVERWRITE)
                || ite->first == GCSS_KEY_OPTIONS)
                continue;
            ret = addDescendantsFromNode(
                    gc_settings_node,
                    static_cast<GraphConfigNode*>(ite->second),
                    rr);
            if (ret != css_err_none)
                return ret;
            continue;
        }

        /* If attribute exists, update descriptor value from the settings,
         * otherwise copy the node from descriptor to the results. */
        std::string newValueStr;
        GraphConfigAttribute * resultsAttr = nullptr;
        if(ite->second->type == INT_ATTRIBUTE) {
            int newValue = 0;
            ret = to->getAttribute(ite->first, &resultsAttr);
            if (ret == css_err_none) {
                // Do not overwrite if id
                if (!(rr & RELAY_RULE_OVERWRITE)
                    || ite->first == GCSS_KEY_ID
                    || ite->first == GCSS_KEY_DIRECTION)
                    continue;
                ite->second->getValue(newValue);
                resultsAttr->setValue(newValue);
                if (rr & RELAY_RULE_HANDLE_OPTIONS) {
                    newValueStr = to_string(newValue);
                    ret = handleAttributeOptions(to, ite->first, newValueStr);
                    if (ret != css_err_none && ret != css_err_noentry)
                        return ret;
                }
                continue;
            }
            resultsAttr =
                static_cast<GraphConfigIntAttribute*>(ite->second)->copy();
            if (resultsAttr == nullptr) {
                LOGE("Error creating GraphConfigItem: No memory");
                return css_err_nomemory;
            }
            if (rr & RELAY_RULE_HANDLE_OPTIONS) {
                ret = resultsAttr->getValue(newValue);
                if (ret != css_err_none) {
                    delete resultsAttr;
                    return ret;
                }
                newValueStr = to_string(newValue);
                ret = handleAttributeOptions(to, ite->first, newValueStr);
                if (ret != css_err_none && ret != css_err_noentry) {
                    delete resultsAttr;
                    return ret;
                }
            }
            to->insertDescendant(resultsAttr, ite->first);
        } else if (ite->second->type == STR_ATTRIBUTE) {
            ret = to->getAttribute(ite->first, &resultsAttr);
            if(ret == css_err_none) {
                // Do not overwrite if name or type
                if (!(rr & RELAY_RULE_OVERWRITE)
                    || ite->first == GCSS_KEY_NAME
                    || ite->first == GCSS_KEY_TYPE
                    || ite->first == GCSS_KEY_PEER)
                    continue;
                ite->second->getValue(newValueStr);
                resultsAttr->setValue(newValueStr);
                if (rr & RELAY_RULE_HANDLE_OPTIONS) {
                    ret = handleAttributeOptions(to, ite->first, newValueStr);
                    if (ret != css_err_none && ret != css_err_noentry)
                        return ret;
                }
                continue;
            }

            resultsAttr =
                static_cast<GraphConfigStrAttribute*>(ite->second)->copy();
            if (resultsAttr == nullptr) {
                LOGE("Error creating GraphConfigItem: No memory");
                return css_err_nomemory;
            }
            if (rr & RELAY_RULE_HANDLE_OPTIONS) {
                ret = resultsAttr->getValue(newValueStr);
                if (ret != css_err_none) {
                    delete resultsAttr;
                    return ret;
                }
                ret = handleAttributeOptions(to, ite->first, newValueStr);
                if (ret != css_err_none && ret != css_err_noentry) {
                    delete resultsAttr;
                    return ret;
                }
            }
            to->insertDescendant(resultsAttr, ite->first);
        } else if (rr & RELAY_RULE_ADD_NODES) {
            GraphConfigNode * results_node =
                static_cast<GraphConfigNode*>(ite->second)->copy();
            if (results_node == nullptr) {
                LOGE("Error creating GraphConfigItem: No memory");
                return css_err_nomemory;
            }
            to->insertDescendant(results_node, ite->first);
        }
    }

    /* propagate between connected ports */
    if (rr & RELAY_RULE_PROPAGATE) {
        /* only propagate to downstream when mandatory direction attribute is
         * set */
        int32_t direction;
        ret = to->getValue(GCSS_KEY_DIRECTION, direction);
        if (ret != css_err_none)
            return css_err_none;
        else if (direction != 1)
            return css_err_none;

        GraphConfigNode* peerNode = GraphQueryManager::getPortPeer(to);
        if (!peerNode)
            return css_err_none;

        /** \todo WORKAROUND: don't overwrite when propagating to  virtual sinks
         * Currently settings are missing explicit way to tell difference
         * between queried dimension and actual buffer dimenssions.
         */
        string type;
        Rule rrPropagate = RELAY_RULE_ADD_NODES;
        ret = peerNode->getValue(GCSS_KEY_TYPE, type);
        if (ret != css_err_none || type.compare("sink") != 0)
            rrPropagate = RELAY_RULE_OVERWRITE;

        /* Note: using 'to' as source instead of 'from' (settings) in order to
         * propagate also the option lists */
        ret = addDescendantsFromNode(peerNode, to, rrPropagate);
        if (ret != css_err_none)
            return ret;

    }

    return css_err_none;
}

/**
 * Propagate an integer attribute from one node to another
 *
 * The method checks if the attribute is already present in the destination
 * node. If it is it just updates the value with the value from the source node.
 * If it is not present, it inserts a new attribute.
 *
 * \param[in] srcNode Source node to take the the attribute value
 * \param[in] dstNode Destination node that will receive the the attribute value
 * \param[in] attributeId The id for the attribute (like ex: GCSS_KEY_TYPE)
 *
 * \returns css_err_none if everything went fine.
 * \returns css_err_nomemory if the allocation of the new attribute failed.
 * \returns css_err_general if the attribute id is not of the expected type.
 */
css_err_t GraphQueryManager::propagateIntAttribute(IGraphConfig *srcNode,
                                                   IGraphConfig *dstNode,
                                                   ia_uid attributeId)
{
    int32_t value = -1;
    css_err_t ret = css_err_none;
    bool update = false;
    /* check first if the value is already present in the destination */
    ret = dstNode->getValue(attributeId, value);
    if (ret == css_err_none) {
        // we only need to update it
        update = true;
    }
    /* acquire the value from the src */
    ret = srcNode->getValue(attributeId, value);
    if (ret != css_err_none)
        return ret;

    /* update or insert depending */
    if (!update) {
        GraphConfigIntAttribute *attr = new GraphConfigIntAttribute;
        if (attr == nullptr)
            return css_err_nomemory;

        ret = attr->setValue(value);
        if (ret != css_err_none) {
            delete attr;
            return ret;
        }
        ret = static_cast<GraphConfigNode*>(dstNode)
                ->insertDescendant(attr, attributeId);
        if (ret != css_err_none)
            delete attr;
    } else {
        dstNode->setValue(attributeId, value);
    }
    return ret;
}

/**
 * Propagate an string attribute from one node to another
 *
 * The method checks if the attribute is already present in the destination
 * node. If it is it just updates the value with the value from the source node.
 * If it is not present, it inserts a new attribute.
 *
 * \param[in] srcNode Source node to take the the attribute value
 * \param[in] dstNode Destination node that will receive the the attribute value
 * \param[in] attributeId The id for the attribute (like ex: GCSS_KEY_TYPE)
 *
 * \returns css_err_none if everything went fine.
 * \returns css_err_nomemory if the allocation of the new attribute failed.
 * \returns css_err_general if the attribute id is not of the expected type.
 */
css_err_t GraphQueryManager::propagateStrAttribute(IGraphConfig *srcNode,
                                                   IGraphConfig *dstNode,
                                                   ia_uid attributeId)
{
    string value;
    css_err_t ret = css_err_none;
    bool update = false;
    /* check first if the value is already present in the destination */
    ret = dstNode->getValue(attributeId, value);
    if (ret == css_err_none) {
        // we only need to update it
        update = true;
    }
    /* acquire the value from the src */
    ret = srcNode->getValue(attributeId, value);
    if (ret != css_err_none)
        return ret;

    /* update or insert depending */
    if (!update) {
        GraphConfigStrAttribute *attr = new GraphConfigStrAttribute;
        if (attr == nullptr)
            return css_err_nomemory;

        ret = attr->setValue(value);
        if (ret != css_err_none) {
            delete attr;
            return ret;
        }
        ret = static_cast<GraphConfigNode*>(dstNode)
                ->insertDescendant(attr, attributeId);
        if (ret != css_err_none)
            delete attr;
    } else {
        dstNode->setValue(attributeId, value);
    }
    return ret;
}

