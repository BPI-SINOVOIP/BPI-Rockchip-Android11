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

#ifndef GCSS_ITEM_H_
#define GCSS_ITEM_H_

#include "gcss.h"

namespace GCSS {
/**
 * Holds type of the item. It can be Node or Attribute (int or string)
 */
enum Type {
    NA               = (1 << 0),
    STR_ATTRIBUTE    = (1 << 1),
    INT_ATTRIBUTE    = (1 << 2),
    NODE             = (1 << 3)
};
/**
 * \class GraphConfigItem
 * Base class that holds attributes or nodes inside
 */
class GraphConfigItem
{
public:
    typedef std::multimap<ia_uid, GraphConfigItem*> gcss_item_map;
    typedef gcss_item_map::const_iterator const_iterator;

    /** Getter for int value of attribute. Return error if getValue is being
     * used directly or if child class doesn't implement getValue
     * \param[out] value
     */

    virtual css_err_t getValue(int&) {return css_err_noentry;}
    /** Getter for string value of attribute. Return error if getValue is being
     * used directly or if child class doesn't implement getValue
     * \param[out] value
     */

    virtual css_err_t getValue(std::string&) {return css_err_noentry;}
    /** Setter for string value of attribute.
     * \param[in] new value
     */

    virtual css_err_t setValue(const std::string&) {return css_err_noentry;}
    /** Setter for int value of attribute.
     * \param[in] new value
     */
    virtual css_err_t setValue(int) {return css_err_noentry;}

    Type type;
    GraphConfigItem(Type type) : type(type) {}

    virtual ~GraphConfigItem() {}
};


inline bool operator<(const ItemUID &r, const ItemUID &l)
{
    return r.mUids < l.mUids;
}

inline bool operator>(const ItemUID &r, const ItemUID &l)
{
    return r.mUids > l.mUids;
}

/**
 * \class GraphConfigAttribute
 * Base class for graph config attributes
 */
class GraphConfigAttribute : public GraphConfigItem {
public:
    GraphConfigAttribute() : GraphConfigItem(NA){}
    GraphConfigAttribute(Type t) : GraphConfigItem(t){}
    ~GraphConfigAttribute() {}
};

/**
 * \class GraphConfigIntAttribute
 * Container for integer type attributes
 */
class GraphConfigIntAttribute : public GraphConfigAttribute
{
public:
    GraphConfigIntAttribute() : GraphConfigAttribute(INT_ATTRIBUTE), mInteger(-1){}
    GraphConfigIntAttribute* copy();
    css_err_t getValue(int& intVal){intVal = mInteger; return css_err_none;}
    css_err_t setValue(int intVal) { return insertInteger(intVal);}
    ~GraphConfigIntAttribute() {};
private:
    css_err_t insertInteger(int);
    int mInteger;
};

/**
 * \class GraphConfigStrAttribute
 * Container for string type attributes
 */
class GraphConfigStrAttribute : public GraphConfigAttribute
{
public:
    GraphConfigStrAttribute() : GraphConfigAttribute(STR_ATTRIBUTE){}
    GraphConfigStrAttribute* copy();
    css_err_t getValue(std::string& str){str = mString; return css_err_none;}
    css_err_t setValue(const std::string& str) { return insertString(str);}
    ~GraphConfigStrAttribute() {};
private:
    css_err_t insertString(const std::string&);
    std::string mString;
};

/**
 * \class GraphConfigNode
 * Provides methods for manipulating nodes in the graph
 */
class GraphConfigNode : public GraphConfigItem, public IGraphConfig
{
public:
    typedef std::vector<GraphConfigNode*> gcss_node_vector;

    GraphConfigNode(const GraphConfigNode&);
    GraphConfigNode() : GraphConfigItem(NODE), mAncestor(nullptr){};
    ~GraphConfigNode();

    GraphConfigNode* copy();
    gcss_item_map item;
    void dumpNode();
    static void dumpNodeTree(GraphConfigNode*, int depth  = 0);
    const_iterator begin() const { return item.begin(); }
    const_iterator end() const { return item.end(); }

    css_err_t getAllDescendants(gcss_node_vector&, ia_uid iuid = 0) const;
    css_err_t getAncestor(GraphConfigNode**);
    css_err_t getAttribute(const ia_uid iuid, GraphConfigAttribute** ret) const;
    css_err_t getIntAttribute(const ia_uid, GraphConfigIntAttribute&);
    css_err_t getStrAttribute(const ia_uid, GraphConfigStrAttribute&);
    css_err_t getDescendant(const ia_uid, GraphConfigNode**) const;
    css_err_t getDescendant(const ia_uid attribute,
                           const int searchAttributeValue,
                           const_iterator& it,
                           GraphConfigNode** retNode) const;
    css_err_t getDescendant(const ia_uid attribute,
                           const std::string& searchAttributeValue,
                           const_iterator& it,
                           GraphConfigNode** retNode) const;
    css_err_t getDescendantByString(const std::string &str,
                                   GraphConfigNode **retNode);
    css_err_t insertDescendant(GraphConfigItem*, const ia_uid);

    GraphConfigNode* getRootNode(void) const;
    /**
     * Helper function which iterates through given node and search for
     * matching attribute. Return error if not found.
     * \param attribute the ItemUID of attribute to search
     * \param searchAttributeValue the value to search
     * \param it iterator for the node to search
     * \return css_err_t
     */
    template <typename T>
    css_err_t iterateAttributes(ia_uid attribute,
                               const T& searchAttributeValue,
                               const const_iterator& it) const
    {
        GraphConfigNode * node = static_cast<GraphConfigNode*>(it->second);
        const_iterator attrIterator = node->begin();

        while (attrIterator != node->end()) {
            if (attrIterator->first == attribute) {
                GraphConfigAttribute * retAttr =
                    static_cast<GraphConfigAttribute*>(attrIterator->second);
                T foundAttrValue;
                css_err_t ret = retAttr->getValue(foundAttrValue);

                if (ret != css_err_none)
                    return ret;
                if (foundAttrValue == searchAttributeValue)
                    return css_err_none;
            }
            attrIterator = node->getNext(attrIterator); // Get a next attribute
        }
        return css_err_end;
    }

    operator IGraphConfig*() { return this; }

private:
    GraphConfigItem::const_iterator getNext(const_iterator& it){
                                            return std::next(it, 1);}
    GraphConfigItem::const_iterator getNextAttribute(
            GraphConfigItem::const_iterator& it) const;
    GraphConfigNode* mAncestor;

    template <typename T>
    css_err_t getAttrValue(const ia_uid& uid, T& val) const {

        GraphConfigAttribute * retAttribute;
        css_err_t ret = getAttribute(uid, &retAttribute);

        if (css_err_none != ret)
            return ret;

        return retAttribute->getValue(val);
    }

    template <typename T>
    css_err_t setAttrValue(const ia_uid& uid, T& val) {

        GraphConfigAttribute * retAttribute;
        css_err_t ret = getAttribute(uid, &retAttribute);

        if (css_err_none != ret)
            return ret;

        return retAttribute->setValue(val);
    }

public: /* implements IGraphConfig */
    virtual IGraphConfig* getRoot(void) const;
    virtual IGraphConfig* getAncestor(void) const;
    virtual IGraphConfig* getDescendant(ia_uid uid) const;
    virtual IGraphConfig* getDescendant(const ItemUID&) const;
    virtual IGraphConfig* getDescendantByString(const std::string &str) const;
    virtual IGraphConfig* iterateDescendantByIndex(const ia_uid attribute,
                                                   uint32_t &index) const;
    virtual IGraphConfig* iterateDescendantByIndex(const ia_uid attribute,
                                                   const ia_uid value,
                                                   uint32_t &index) const;
    virtual uint32_t getDescendantCount() const;
    virtual css_err_t getValue(ia_uid, int&) const;
    virtual css_err_t getValue(const ItemUID&, int&) const;

    virtual css_err_t getValue(ia_uid, std::string&) const;
    virtual css_err_t getValue(const ItemUID&, std::string&) const;

    virtual css_err_t setValue(ia_uid, int);
    virtual css_err_t setValue(const ItemUID&, int);

    virtual css_err_t setValue(ia_uid, const std::string&);
    virtual css_err_t setValue(const ItemUID&, const std::string&);

    /* access not part of IGraphConfig */
    bool hasItem(const ia_uid iuid) const;
    css_err_t addValue(ia_uid uid, const std::string &val);
    css_err_t addValue(ia_uid uid, int val);

    // To silence possible compiler warning
    using GraphConfigItem::getValue;
    using GraphConfigItem::setValue;
};

} // namespace GCSS
#endif /* GCSS_ITEM_H_ */
