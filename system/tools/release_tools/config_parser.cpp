#include "config_parser.h"

vector<ParserPoint> openConfigFile(string fileName) {
    ifstream fin(fileName.data());
    string tempStr;
    string type;
    int typeIndex = -1;
    int line = 0;
    vector<ParserPoint> resultArray;
    while (getline(fin, tempStr)) {
        if (FindKeyName(tempStr, "[", "]", &type)) {
            ParserPoint point(type, line);
            typeIndex++;
            resultArray.push_back(point);
        } else {
            if (typeIndex >= 0) {
                resultArray[typeIndex].addPoint(tempStr);
            }
        }
        line++;
    }
    fin.close();
    return resultArray;
}

/*
int main(int argc, char** argv) {
    vector<ParserPoint> result = openConfigFile(argv[1]);

    for (int i = 0; i < result.size(); i++){
        result[i].printContent();
    }

}


TitleType string2Type(string name) {
    if (name == "Filter")
        return TitleType::Filter;
    else if (name == "ForceUpdate")
        return TitleType::ForceUpdate;
    else if (name == "Remote")
        return TitleType::Remote;
    else if (name == "Platform")
        return TitleType::Platform;
    else
        return TitleType::Unknow;
}

string type2String(TitleType type) {
    switch (type):
    case TitleType::Filter:
        return "Filter";
    case TitleType::ForceUpdate:
        return "ForceUpdate";
    case TitleType::Remote:
        return "Remote";
    case TitleType::Platform:
        return "Platform";
    default:
        return "Unknow";
}


// get title name
int findTypeName(string stringIn, string *){

}
*/
// add to array
