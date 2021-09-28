#ifndef RELEASE_TOOLS_CONFIG_PARSER_H_
#define RELEASE_TOOLS_CONFIG_PARSER_H_

#include <iostream>
#include "fstream"
#include <dirent.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <vector>

#include "release_utils.h"
#include "ParserPoint.h"

using namespace std;

enum class TitleType:int {
    Filter,
    ForceUpdate,
    Remote,
    Platform,
    Unknow
};


vector<ParserPoint> openConfigFile(string fileName);

#endif //RELEASE_TOOLS_CONFIG_PARSER_H_
