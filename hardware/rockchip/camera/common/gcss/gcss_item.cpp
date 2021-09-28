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
#include "PlatformData.h"
#include "gcss_item.h"
#include "graph_query_manager.h"
using namespace GCSS;
using namespace std;
using namespace android::camera2;

/**
 * function to copy GraphConfigNodes
 *
 * @return copied node or nullptr
 */
GraphConfigNode* GraphConfigNode::copy()
{
    GraphConfigNode *ret = new GraphConfigNode;
    if (ret == nullptr) {
        return nullptr;
    }
    gcss_item_map::const_iterator it = this->item.begin();
    while(it != this->item.end()) {

        if (it->second->type == INT_ATTRIBUTE) {
            GraphConfigIntAttribute * new_item =
                    static_cast<GraphConfigIntAttribute*>(it->second)->copy();
            if (new_item == nullptr) {
                delete ret;
                return nullptr;
            }
            ret->item.insert(std::make_pair(it->first, new_item));
        }
        else if (it->second->type == STR_ATTRIBUTE) {
            GraphConfigStrAttribute * new_item =
                    static_cast<GraphConfigStrAttribute*>(it->second)->copy();
            if (new_item == nullptr) {
                delete ret;
                return nullptr;
            }
            ret->item.insert(std::make_pair(it->first, new_item));
        } else {
            GraphConfigNode * new_item =
                    static_cast<GraphConfigNode*>(it->second)->copy();
            if (new_item == nullptr) {
                delete ret;
                return nullptr;
            }
            new_item->mAncestor = ret;
            ret->item.insert(std::make_pair(it->first, new_item));
        }
        ++it;
    }
    return ret;
}

GraphConfigIntAttribute * GraphConfigIntAttribute::copy() {
    GraphConfigIntAttribute * result = new GraphConfigIntAttribute(*this);
    return result;
}
GraphConfigStrAttribute * GraphConfigStrAttribute::copy() {
    GraphConfigStrAttribute * result = new GraphConfigStrAttribute(*this);
    return result;
}

ia_uid ItemUID::str2key(const std::string &key_str)
{
    GcssKeyMap *gcssKeMap = PlatformData::getGcssKeyMap();
    return gcssKeMap->str2key(key_str);
}

const char* ItemUID::key2str(const ia_uid key)
{
    GcssKeyMap *gcssKeMap = PlatformData::getGcssKeyMap();
    return gcssKeMap->key2str(key);
}

ia_uid ItemUID::generateKey(const std::string& str) {
    GcssKeyMap *gcssKeMap = PlatformData::getGcssKeyMap();
    ia_uid uuid = (ia_uid)gcssKeMap->gcssKeyMapSize();

    std::map<std::string, ia_uid> map;
    map.insert(std::make_pair(str.c_str(), uuid));
    gcssKeMap->gcssKeyMapInsert(map);

    return uuid;
}

void ItemUID::addCustomKeyMap(std::map<std::string, ia_uid> &customMap)
{
    if (customMap.size() == 0)
       return;

    GcssKeyMap *gcssKeMap = PlatformData::getGcssKeyMap();
    gcssKeMap->gcssKeyMapInsert(customMap);
}

std::string ItemUID::toString() const
{
    std::string log;
    for (size_t i = 0; i < mUids.size(); i++) {
        log.append(key2str(mUids[i]));
        if ((i + 1) != mUids.size()) {
            log.append(".");
        }
    }
    return log;
}

/**
 * Helper functions
 */
/**
 * Dumps the whole tree of nodes and attributes. Everytime a node is found
 * calls itself recursively.
 * @param node Node to dump
 * @param depth keeps record on how deep in tree we are. Default should be 0
 */
void GraphConfigNode::dumpNodeTree(GraphConfigNode* node, int depth)
{
    string indent;

    for (int d = 0; d < depth; d++)
        indent += "  ";

    /* enumerate items using API instead of direct iterator access */
    multimap<ia_uid, GraphConfigItem*>::const_iterator it;
    it = node->item.begin();
    for (; it != node->item.end(); ++it) {
        if (it->second->type == INT_ATTRIBUTE) {

            int int_val = -1;

            if (css_err_none == static_cast<GraphConfigIntAttribute*>(it->second)
                ->getValue(int_val)) {

                LOGE("%s'%s'(%d) int [Attr] '%d'", indent.c_str(),
                            ItemUID::key2str(it->first),
                            it->first,
                            int_val);
            }
        }
        else if (it->second->type == STR_ATTRIBUTE) {
            string str_val = "";

            static_cast<GraphConfigStrAttribute*>(it->second)
                ->getValue(str_val);
            LOGE("%s'%s' str [Attr] '%s'", indent.c_str(),
                        ItemUID::key2str(it->first),
                        str_val.c_str());

        }
        else if (it->second->type == NODE) {
            LOGE("%s'%s'(%d) [Node]",
                        indent.c_str(),
                        ItemUID::key2str(it->first),
                        it->first);
            dumpNodeTree(static_cast<GraphConfigNode*>(it->second), depth + 1);
        } else {
            LOGE("type?");
        }
    }
}

void GraphConfigNode::dumpNode()
{
    LOGE("Node %p type: %s ancestor %p map size %zu",
                                      this,
                                      type == NA ? "NA":
                                      type == STR_ATTRIBUTE ? "STRING":
                                      type == INT_ATTRIBUTE ? "INT": " NODE",
                                      mAncestor,
                                      item.size());
    if (type == NODE) {
        gcss_node_vector descendants;
        getAllDescendants(descendants);
        LOGE("Node number of descendants : %zu", descendants.size());
    }
}

// END HELPERS

/**
 * Insert integer value to attribute
 */
css_err_t GraphConfigIntAttribute::insertInteger(int val)
{

    mInteger = val;
    return css_err_none;
}

/**
 * Insert string value to attribute
 */
css_err_t GraphConfigStrAttribute::insertString(const std::string& val)
{

    mString = val;
    return css_err_none;
}

css_err_t GraphConfigNode::getAncestor(GraphConfigNode** ret)
{
    if (ret == nullptr)
        return css_err_argument;
    *ret = mAncestor;
    return css_err_none;
}
/**
 * Inserts Node inside another node
 */
css_err_t GraphConfigNode::insertDescendant(GraphConfigItem* child,
                                           ia_uid iuid)
{
    if (child == nullptr) {
        return css_err_general;
    }

    item.insert(std::make_pair(iuid, child));
    if (child->type == NODE)
        ((GraphConfigNode*)child)->mAncestor = this;
    return css_err_none;
}

css_err_t GraphConfigNode::getAttribute(const ia_uid iuid,
                                       GraphConfigAttribute** ret) const {

   GraphConfigItem::const_iterator it = item.find(iuid);
   if (it != item.end()) {
       if (it->second->type == INT_ATTRIBUTE) {
           *ret = static_cast<GraphConfigIntAttribute*>(it->second);
       } else {
           *ret = static_cast<GraphConfigStrAttribute*>(it->second);
       }
       return css_err_none;
   }
   return css_err_general;
}

bool GraphConfigNode::hasItem(const ia_uid iuid) const {
   GraphConfigItem::const_iterator it = item.find(iuid);
   return (it != item.end());
}


/**
 * Gets attribute inside a node
 */
css_err_t GraphConfigNode::getIntAttribute(ia_uid iuid,
                                          GraphConfigIntAttribute& ret)
{
    gcss_item_map::const_iterator it = item.find(iuid);

    if (it != item.end()) {
        if (it->second->type != INT_ATTRIBUTE)
            return css_err_argument;
        ret = static_cast<GraphConfigIntAttribute&>(*it->second);

        return css_err_none;
    }
    return css_err_general;
}
/**
 * Gets attribute inside a node
 */
css_err_t GraphConfigNode::getStrAttribute(ia_uid iuid,
                                          GraphConfigStrAttribute& ret)
{
    gcss_item_map::const_iterator it = item.find(iuid);

    if (it != item.end()) {
        if (it->second->type != STR_ATTRIBUTE)
            return css_err_argument;
        ret = static_cast<GraphConfigStrAttribute&>(*it->second);

        return css_err_none;
    }
    return css_err_general;
}
/**
 * Gets node inside a node
 */
css_err_t GraphConfigNode::getDescendant(ia_uid iuid,
                                        GraphConfigNode** ret) const
{
    gcss_item_map::const_iterator it = item.find(iuid);
    if (it != item.end()) {
        // type has to be node
        if (it->second->type != NODE) {
            return css_err_argument;
        }

        *ret = static_cast<GraphConfigNode*>(it->second);

        return css_err_none;
    }
    else {
        // no child to return
        *ret = nullptr;
        return css_err_general;
    }
}

/**
 *  Enumerate through nodes, starting from the given iterator. Searches
 *  attributes that match the given string and returns the iterator to next.
 * \param attribute the attribute by which we want to search, ie. type
 * \param value the value of the attribute that we want, ie. kernel
 * \param it the iterator to start searching from. Iterator to matched node
 * is returned.
 * \param retNode return the current node
 * \return css_err_t
 */
css_err_t GraphConfigNode::getDescendant(const ia_uid attribute,
                                        const std::string& searchAttributeValue,
                                        const_iterator& it,
                                        GraphConfigNode** retNode) const
{
    for (;it != item.end(); ++it) {
        if (it->second->type != NODE)
            continue;
        css_err_t ret = iterateAttributes(attribute, searchAttributeValue, it);
        if (ret == css_err_none) {
            *retNode = static_cast<GraphConfigNode*>(it->second);
            it++;
            return css_err_none;
        }
    }
    return css_err_end;
}

/**
 *  Enumerate through nodes, starting from the given iterator. Searches
 *  attributes that match the given int and returns the iterator to next.
 * \param attribute the attribute by which we want to search, ie. type
 * \param value the int value of the attribute that we want
 * \param it the iterator to start searching from. Iterator to next is returned
 * \param retNode return the current node
 * \return css_err_t
 */
css_err_t GraphConfigNode::getDescendant(const ia_uid attribute,
                                        const int searchAttributeValue,
                                        const_iterator& it,
                                        GraphConfigNode** retNode) const
{
    for (;it != item.end(); ++it) {
        if (it->second->type != NODE)
            continue;
        css_err_t ret = iterateAttributes(attribute, searchAttributeValue, it);
        if (ret == css_err_none) {
            *retNode = static_cast<GraphConfigNode*>(it->second);
            it++;
            return css_err_none;
        }
    }
    return css_err_end;
}

IGraphConfig*
GraphConfigNode::iterateDescendantByIndex(const ia_uid attribute,
                                          const ia_uid value,
                                          uint32_t &index) const
{
    int32_t intValue = -1;
    std::string strValue;
    if (attribute < GCSS_KEY_NUMERICAL_START || attribute > GCSS_KEY_NUMERICAL_END) {
        strValue = ItemUID::key2str(value); // convert gcss key to str
    } else {
        intValue = int32_t(value);
    }
    const GraphConfigNode *node = static_cast<const GraphConfigNode*>(this);
    GraphConfigItem::const_iterator it = node->item.begin();
    std::advance(it, (int32_t)index); // move iterator to index position

    for (;it != node->item.end(); ++it) {
        if (it->second->type == NODE) {
            css_err_t ret = css_err_none;
            if (intValue != -1) {
                ret = iterateAttributes(attribute, intValue, it);
            } else {
                ret = iterateAttributes(attribute, strValue, it);
            }
            if (ret == css_err_none)
                return static_cast<GraphConfigNode*>(it->second);
        }
        index++;
    }
    return nullptr;
}

IGraphConfig*
GraphConfigNode::iterateDescendantByIndex(const ia_uid attribute,
                                          uint32_t &index) const
{
    const GraphConfigNode *node = static_cast<const GraphConfigNode*>(this);
    GraphConfigItem::const_iterator it = node->item.begin();
    std::advance(it, (int32_t)index); // move iterator to index position

    for (;it != node->item.end(); ++it) {
        if ((it->first != attribute) || (it->second->type != NODE)) {
            index++;
            continue;
        }
        return static_cast<GraphConfigNode*>(it->second);
    }
    return nullptr;
}

uint32_t
GraphConfigNode::getDescendantCount() const
{
    const GraphConfigNode *node = static_cast<const GraphConfigNode*>(this);
    return (uint32_t)node->item.size();
}

/**
 * Gets the last node in a colon separated representation of a tree.
 * ie. isa:scaled_output returns scaled_output node.
 *
 * \param str input string of colon separated nodes
 * \param retNode the found node
 * \return
 */
css_err_t GraphConfigNode::getDescendantByString(const std::string &str,
                                                GraphConfigNode **retNode)
{
    css_err_t ret = css_err_argument;
    std::size_t pos = 0;
    std::string resultStr, remainingStr = str;
    GraphConfigNode * foundNode = this;
    bool split = true;

    while (split) {
        if ((pos = remainingStr.find(":")) == string::npos) {
            // Reached last node
            resultStr = remainingStr;
            ret = foundNode->getDescendant(ItemUID::str2key(resultStr), retNode);
            split = false;
        }
        else {
            resultStr = remainingStr.substr(0, pos);
            remainingStr = remainingStr.substr(pos + 1);

            // Get next level of nodes
            ret = foundNode->getDescendant(ItemUID::str2key(resultStr), &foundNode);
        }
        if (ret != css_err_none) {
            LOGD("Error getting descendant %s", resultStr.c_str());
            return ret;
        }
    }
    return css_err_none;
}

/**
 * Gets iterator to next attribute inside the node.
 * \param it the iterator for the attributes inside node
 * \param retAttr save found attribute here
 * \return
 */

GraphConfigItem::const_iterator GraphConfigNode::getNextAttribute(
        GraphConfigItem::const_iterator& it) const
{
    it++;

    while (it != item.end()) {
        if (it->second->type == STR_ATTRIBUTE || it->second->type == INT_ATTRIBUTE) {
            return it;
        }
        it++;
    }
    return it;
}

/**
 * Gets all nodes inside a node and puts them into a vector. Optional parameter
 * can be used to include only descendants with given item uid.
 *
 * \param ret [out] store descendants to this vector
 * \param iuid [optional] Only get nodes with certain item uid
 * \return css_err_none if at least one descendant added to vector
 * \return css_err_general if no descendants to add
 */
css_err_t GraphConfigNode::getAllDescendants(gcss_node_vector& ret,
                                            ia_uid iuid) const
{
    gcss_item_map::const_iterator it;

    for (it = item.begin(); it != item.end(); ++it) {

        // We are only interested in nodes, not attributes
        if (it->second->type != NODE)
            continue;

        if ((!iuid) || (it->first == iuid))
            ret.push_back(static_cast<GraphConfigNode*>(it->second));
    }
    if (ret.size() > 0)
        return css_err_none;

    return css_err_general;
}

GraphConfigNode::~GraphConfigNode()
{
    gcss_item_map::iterator it = item.begin();
    while (it != item.end()) {
        delete it->second;
        it->second = nullptr;
        ++it;
    }
}

GraphConfigNode* GraphConfigNode::getRootNode(void) const
{
    GraphConfigNode *gcn = const_cast<GraphConfigNode*>(this);
    while (gcn->mAncestor != nullptr)
        gcn = gcn->mAncestor;
    return gcn;
}

IGraphConfig* GraphConfigNode::getRoot(void) const
{
    return static_cast<IGraphConfig*>(getRootNode());
}

IGraphConfig* GraphConfigNode::getAncestor() const
{
    return static_cast<IGraphConfig*>(mAncestor);
}

IGraphConfig* GraphConfigNode::getDescendant(const ItemUID &iuid) const
{
    GraphConfigNode *gcn_current = nullptr,
                    *gcn;
    css_err_t ret;
    size_t depth;
    for (depth=0; depth < iuid.size(); depth++) {
        if (gcn_current == nullptr)
            ret = getDescendant(iuid[depth], &gcn);
        else
            ret = gcn_current->getDescendant(iuid[depth], &gcn);
        if (ret != css_err_none) {
            /* return parent in case the last key is attribute */
            if (depth != 0 && depth == iuid.size() -1)
                return gcn_current;
            return nullptr;
        }
        gcn_current = gcn;
    }
    return gcn_current;
}

IGraphConfig* GraphConfigNode::getDescendant(ia_uid uid) const
{
    GraphConfigNode *gcn;
    css_err_t ret;
    ret = getDescendant(uid, &gcn);
    if (ret != css_err_none)
        return nullptr;
    return static_cast<IGraphConfig*>(gcn);
}

/**
 * Gets the last node in a colon separated representation of a tree.
 * ie. isa:scaled_output returns scaled_output node.
 *
 * \param str input string of colon separated nodes
 * \param retNode the found node
 * \return
 */
IGraphConfig*
GraphConfigNode::getDescendantByString(const std::string &str) const
{
    IGraphConfig* node = nullptr;
    std::size_t pos = 0;
    std::string resultStr, remainingStr = str;
    bool split = true;

    while (split) {
        if ((pos = remainingStr.find(":")) == string::npos) {
            // Reached last node
            resultStr = remainingStr;
            split = false;
        }
        else {
            resultStr = remainingStr.substr(0, pos);
            remainingStr = remainingStr.substr(pos + 1);
        }
        if (node == nullptr) {
            node = getDescendant(ItemUID::str2key(resultStr));
        } else {
            node = node->getDescendant(ItemUID::str2key(resultStr));
        }
        if (node == nullptr) {
            LOGD("Error getting descendant %s", resultStr.c_str());
            return nullptr;
        }
    }
    return node;
}

css_err_t
GraphConfigNode::getValue(const ItemUID &iuid, int &val) const
{
    const IGraphConfig *gc;
    if (iuid.size() > 1) {
        gc = getDescendant(iuid);
        if (!gc)
            return css_err_argument;
    } else {
        gc = this;
    }
    return gc->getValue(iuid[iuid.size()-1], val);
}

css_err_t
GraphConfigNode::getValue(const ItemUID &iuid, std::string &val) const
{
    const IGraphConfig *gc;
    if (iuid.size() > 1) {
        gc = getDescendant(iuid);
        if (!gc)
            return css_err_argument;
    } else {
        gc = this;
    }
    return gc->getValue(iuid[iuid.size()-1], val);
}

css_err_t
GraphConfigNode::getValue(ia_uid uid, int &val) const
{
    return getAttrValue(uid, val);
}

css_err_t
GraphConfigNode::getValue(ia_uid uid, string &val) const
{
    return getAttrValue(uid, val);
}

css_err_t
GraphConfigNode::setValue(const ItemUID &iuid, int val)
{
    IGraphConfig *gc;
    if (iuid.size() > 1) {
        gc = getDescendant(iuid);
        if (!gc)
            return css_err_noentry;
    } else {
        gc = this;
    }
    return gc->setValue(iuid[iuid.size()-1], val);
}

css_err_t
GraphConfigNode::setValue(const ItemUID &iuid, const string &val)
{
    IGraphConfig *gc;
    if (iuid.size() > 1) {
        gc = getDescendant(iuid);
        if (!gc)
            return css_err_noentry;
    } else {
        gc = this;
    }
    return gc->setValue(iuid[iuid.size()-1], val);
}

css_err_t
GraphConfigNode::setValue(ia_uid uid, int val)
{
    GraphConfigAttribute *attribute;
    css_err_t ret = getAttribute(uid, &attribute);
    if (ret != css_err_none)
        return css_err_noentry;

    if (attribute->type != INT_ATTRIBUTE) {
        LOGE("Attribute is of wrong type");
        return css_err_argument;
    }
    string valStr = to_string(val);
    ret = GraphQueryManager::handleAttributeOptions(this, uid, valStr);
    if (ret != css_err_none) {
        LOGE("setValue() restricted to attributes that have predefined options");
        return ret;
    }

    return attribute->setValue(val);
}

css_err_t
GraphConfigNode::setValue(ia_uid uid, const string &val)
{
    GraphConfigAttribute *attribute;
    css_err_t ret = getAttribute(uid, &attribute);
    if (ret != css_err_none)
        return css_err_noentry;

    if (attribute->type != STR_ATTRIBUTE) {
        LOGE("Attribute is of wrong type");
        return css_err_argument;
    }

    ret = GraphQueryManager::handleAttributeOptions(this, uid, val);
    if (ret != css_err_none) {
        LOGE("setValue() restricted to attributes that have predefined options");
        return ret;
    }

    return attribute->setValue(val);
}

css_err_t
GraphConfigNode::addValue(ia_uid uid, const string &val)
{
    GraphConfigStrAttribute *attribute;
    css_err_t ret;

    if (hasItem(uid))
        return css_err_full;

    attribute = new GraphConfigStrAttribute;
    if (attribute == nullptr)
        return css_err_nomemory;
    attribute->setValue(val);
    ret = insertDescendant(attribute, uid);
    if (ret != css_err_none) {
        delete attribute;
        return ret;
    }

    return css_err_none;
}

css_err_t
GraphConfigNode::addValue(ia_uid uid, int val)
{
    GraphConfigIntAttribute *attribute;
    css_err_t ret;

    if (hasItem(uid))
        return css_err_full;

    attribute = new GraphConfigIntAttribute;
    if (attribute == nullptr)
        return css_err_nomemory;
    attribute->setValue(val);
    ret = insertDescendant(attribute, uid);
    if (ret != css_err_none) {
        delete attribute;
        return ret;
    }

    return css_err_none;
}
