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


#define LOG_TAG "GCSS_PARSER"
#include "LogHelper.h"

#include <algorithm>
#include "GCSSParser.h"
#include <stdio.h>
#include <sstream>
#include <string.h>
#include <vector>
extern "C" {
#include "gcss.h"
}

using namespace std;
using namespace android::camera2;

namespace GCSS {

GCSSParser::GCSSParser() :
        mTopLevelNode(NA),
        mCurrentNode(nullptr)
{
}

GCSSParser::~GCSSParser()
{
    delete mCurrentNode;
}

/**
 * Function to return attribute type based on its tag
 */
void GCSSParser::getElementType(const ia_uid& att_uid, ia_uid& data_type)
{
    if (att_uid > GCSS_KEY_NUMERICAL_START && att_uid < GCSS_KEY_NUMERICAL_END) {
        // Numerical
        data_type = INT_ATTRIBUTE;
    }
    // todo: Everything but integer are handled as type NA for now
    else {
        data_type = GCSS_KEY_NA;
    }
}

/**
 * the callback function of the libexpat for handling of one element start
 *
 * When it comes to the start of one element. This function will be called.
 *
 * New nodes and attributes are added as children for the current
 * node. Attribute value is added with insert[String, Integer]Value
 *
 * \param userData: the pointer we set by the function XML_SetUserData.
 * \param name: the element's name.
 */

void GCSSParser::startElement(void *userData, const char *name, const char **atts)
{
    GCSSParser *gcParser = (GCSSParser *)userData;
    string tag_str = name;
    string att_str;
    string value_str;
    ia_uid data_type;
    ia_uid tag_uid, att_uid = 0;
    css_err_t ret = css_err_none;

    transform(tag_str.begin(), tag_str.end(), tag_str.begin(), ::tolower);

    // Generate gcss key for tags which does not exist in gcss_keys.h
    tag_uid = ItemUID::str2key(tag_str);
    if (tag_uid == GCSS_KEY_NA) {
        tag_uid = ItemUID::generateKey(tag_str);
    } else if (tag_uid == GCSS_KEY_GRAPH_SETTINGS
        || tag_uid == GCSS_KEY_GRAPH_DESCRIPTOR) {
        // Do not add top level nodes
        gcParser->mTopLevelNode = tag_uid;
        return;
    }

    GraphConfigNode* gcnode = new GraphConfigNode;
    if (gcnode == nullptr) {
        LOGE("Error creating node, No memory");
        return;
    }
    for (int i = 0; atts[i]; i += 2) {
        att_str = atts[i];
        value_str = atts[i + 1];
        transform(att_str.begin(), att_str.end(), att_str.begin(), ::tolower);
        att_uid = ItemUID::str2key(att_str);

        /* In graph descriptor, if attribute is "name", check if name value
         * exists in gcss_key map and generate a new key if it doesn't. Also
         * make tag name a type and use name as tag value
         */
        if (att_uid == GCSS_KEY_NAME
                && gcParser->mTopLevelNode == GCSS_KEY_GRAPH_DESCRIPTOR) {

            if (tag_uid != GCSS_KEY_NODE) {
                ret = gcnode->addValue(GCSS_KEY_TYPE, tag_str);
                if (ret != css_err_none) {
                    LOGE("Error adding name attribute");
                    delete gcnode;
                    return;
                }
            }

            // Add name value to gcss keys if not already
            tag_uid = ItemUID::str2key(value_str);
            if (tag_uid == GCSS_KEY_NA)
                tag_uid = ItemUID::generateKey(value_str);
        }
        /* Find out if content is string, integer etc... */
        gcParser->getElementType(att_uid, data_type);

        /* Store attribute inside node */

        if (data_type == INT_ATTRIBUTE) {
            int attr_int_value = atoi(value_str.c_str());
            ret = gcnode->addValue(att_uid, attr_int_value);
        } else if (att_uid == GCSS_KEY_ATTRIBUTE) {
            /* treat attribute named attribute as integer key */
            int attr_int_value = (int)ItemUID::str2key(value_str);
            if (tag_uid == GCSS_KEY_NA)
                attr_int_value = (int)ItemUID::generateKey(value_str);
            ret = gcnode->addValue(att_uid, attr_int_value);
        } else {
            ret = gcnode->addValue(att_uid, value_str);
        }
        /*
         * \todo allowing duplicate attributes (css_err_full)
         */
        if (ret != css_err_none && ret != css_err_full) {
            delete gcnode;
            LOGE("Error while adding attribute");
            return;
        }
    }

    if (!gcParser->mCurrentNode) {
        /* TODO: initialize root node for graph_descriptor and
         * graph_settings inside query manager and run parser for both
         * files to fetch the data */
        gcParser->mCurrentNode = new GraphConfigNode;
        if (gcParser->mCurrentNode == nullptr) {
            LOGE("Error creating node, No memory");
            delete gcnode;
            return;
        }
    }

    gcParser->mCurrentNode->insertDescendant(gcnode, tag_uid);
    gcParser->mCurrentNode = gcnode;
}

/**
 * the callback function of the libexpat for handling of one element end
 *
 * When it comes to the end of one element. This function will be called.
 *
 * \param userData: the pointer we set by the function XML_SetUserData.
 * \param name: the element's name.
 */

void GCSSParser::endElement(void *userData, const char*)
{
    GCSSParser *gcParser = (GCSSParser *)userData;

    GraphConfigNode *gcn;
    gcParser->mCurrentNode->getAncestor(&gcn);
    if (gcn)
        gcParser->mCurrentNode = gcn;

    return;
}
/**
 * Parse GCSS xml file
 *
 * \ingroup gcss
 *
 * The function first parses the graph descriptors. Then it will optionally
 * parse graph settings. Parsed data can be accessed using gcss_item utilities.
 *
 * \todo there should only be IGraphConfig version of this method. Move
 * implementation to this method when there are no more users for GraphConfiNode
 * version.
 *
 * \param[in] filename, path to xml file to parse
 * \param[out] ret, pointer's pointer to parsed data
 */
void GCSSParser::parseGCSSXmlFile(const char* fileName, IGraphConfig** ret)
{
    if (ret == nullptr) {
        LOGE("nullptr pointer given!");
        return;
    }
    GraphConfigNode *node = nullptr;
    parseGCSSXmlFile(fileName, &node);
    if (node == nullptr) {
        LOGE("Parser returned nullptr!");
        return;
    }
    *ret = static_cast<IGraphConfig*>(*node);
}
/**
 * Parse GCSS xml file
 *
 * The function first parses the graph descriptors.
 * Then it will optionally parse graph settings
 * The graph descriptor is stored in the GCSS GraphConfigTree containers
 *
 */
void GCSSParser::parseGCSSXmlFile(const char* fileName, GraphConfigNode** ret)
{
    if (mCurrentNode != nullptr) {
        // This is indication that some error happened in a previous parse
        // let's cleanup the allocated memory before parsing again
        delete mCurrentNode;
    }
    mCurrentNode = nullptr;
    void *pBuf = nullptr;

    // supported file types
    const std::string COMPRESSED_FILE_EXT = "gz";
    const std::string XML_FILE_EXT = "xml";

    /*
     * Check file extension
     * We support xmls and files compressed with gz
     */
    std::string fileNameStr = fileName;
    size_t pos = fileNameStr.rfind(".");
    std::string extension = fileNameStr.substr(pos + 1, fileNameStr.length() - pos);

    XML_Parser parser = ::XML_ParserCreate(nullptr);
    if (nullptr == parser) {
        LOGE("@%s, line:%d, parser is nullptr", __FUNCTION__, __LINE__);
        return;
    }

    ::XML_SetUserData(parser, this);
    ::XML_SetElementHandler(parser, startElement, endElement);

    pBuf = malloc(BUFFERSIZE);
    if (nullptr == pBuf) {
        LOGE("@%s, line:%d, pBuf is nullptr", __FUNCTION__, __LINE__);
        ::XML_ParserFree(parser);
        return;
    }

    if (extension == XML_FILE_EXT) {
        parseXML(parser, fileName, pBuf);
    } else {
        LOGE("file type (%s) not suppoted.", extension.c_str());
    }

    *ret = mCurrentNode;
    mCurrentNode = nullptr;

    if (parser)
        ::XML_ParserFree(parser);
    if (pBuf)
        free(pBuf);
}

/*
 * Parses xml file
 */
void GCSSParser::parseXML(XML_Parser &parser, const char* fileName, void* pBuf)
{
    int done = 0;
    FILE *fp = fopen(fileName, "r");
    if (nullptr == fp) {
        LOGE("@%s, line:%d, fp is nullptr", __FUNCTION__, __LINE__);
        return;
    }
    do {
        int len = (int)fread(pBuf, 1, BUFFERSIZE, fp);
        if (!len) {
            if (ferror(fp)) {
                clearerr(fp);
                break;
            }
        }

        done = len < BUFFERSIZE;
        if (XML_Parse(parser, (const char *)pBuf, len, done) == XML_STATUS_ERROR) {
            LOGE("@%s, line:%d, XML_Parse error %s", __FUNCTION__, __LINE__, (const char *)pBuf);
            break;
        }
    } while (!done);
    if (fp)
        fclose(fp);
    return;
}
} // namespace
