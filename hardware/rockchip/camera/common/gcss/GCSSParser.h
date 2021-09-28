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


#ifndef __GCSS_PARSER_H__
#define __GCSS_PARSER_H__

#include "graph_query_manager.h"
#include <expat.h>

namespace GCSS {

/**
 * \class GCSSParser
 *
 * This class is used to parse the Graph Configuration Subsystem graph
 * descriptor xml file. Uses the expat lib to do the parsing.
 */
class GCSSParser {
public:
    GCSSParser();
    ~GCSSParser();
    void parseGCSSXmlFile(const char*, GraphConfigNode**);
    void parseGCSSXmlFile(const char*, IGraphConfig**);

private: /* Constants*/
    static const int BUFFERSIZE = 4*1024;  // For xml file

private: /* Methods */
    static void startElement(void *userData, const char *name, const char **atts);
    static void endElement(void *userData, const char *name);

    void parseXML(XML_Parser &parser, const char* fileName, void* pBuf);
    void getElementType(const ia_uid& att_uid, ia_uid& data_type);
    void handleGraph(const char *name, const char **atts);
    void handleNode(const char *name, const char **atts);
    void checkField(GCSSParser *profiles, const char *name, const char **atts);

private:  /* Members */

    ia_uid mTopLevelNode;
    GCSS::GraphConfigNode *mCurrentNode; /* TODO: these should be of GraphConfig-iface type */
};
} // namespace
#endif
