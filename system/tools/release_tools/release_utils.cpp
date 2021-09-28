#include "release_utils.h"

string getPath(string param) {
    if (FindKeyWordEndFix(param, "/")) {
        return param.substr(0, param.length()-1);
    } else {
        return param;
    }
}

bool FindKeyWordEndFix(string originStr, string keyword) {
    size_t where = originStr.rfind(keyword);
    if (where == string::npos) return false;
    else {
        if (where == originStr.length() - keyword.length())
            return true;
        else return false;
    }
}

// if keyword is child of originStr, return true
bool HasKeyWordInString(string originStr, string keyword) {
    size_t where = originStr.find(keyword);
    LogD("keyword:" + keyword);
    if (where == string::npos) return false;
    else {
        return true;
    }
}

// find the string between keyword1 & keyword2, example: revision="31234124", result is 31234124
bool FindKeyName(string inputOriginStr, string keyword1, string keyword2, string *result) {
    //keyword start pos, such as 'project path=' is 3
    string originStr(inputOriginStr);
    size_t start = originStr.find(keyword1);
    size_t keywordLength = keyword1.length();

    if (start != string::npos) {
        //find the next of '"' after keyword, such as <project path="build/blueprint"
        size_t end = originStr.find(keyword2, start + keywordLength);
        if (end == std::string::npos) return false;
        size_t length = end - start - keywordLength;
        if (length == 0) return false;

        *result = originStr.substr(start + keywordLength, length);
        return true;
    }
    return false;
}

bool FindDefaultRevision(string hash_file_name, string *revision) {
    ifstream fin(hash_file_name.data());
    string tempStr;
    while (getline(fin, tempStr)) {
        if (HasKeyWordInString(tempStr, "<default remote=")) {
            FindKeyName(tempStr, "revision=\"", "\"", revision);
            break;
        }
    }
}

bool FindValueOfNodeName(string hash_file_name, string lineTAG, string node, string *value, bool usePath) {
    ifstream fin(hash_file_name.data());
    string tempStr;
    bool found = false;
    string lineKeyWord;
    while (getline(fin, tempStr)) {
        if (usePath) lineKeyWord = "path=\"" + lineTAG + "\"";
        else lineKeyWord = "name=\"" + lineTAG + "\"";
        if (HasKeyWordInString(tempStr, lineKeyWord)) {
            //find, convert str and write
FOUND_LINE:
            string result;
            if (FindKeyName(tempStr, node, "\"", &result)) {
                *value = result;
                LogD("find " + node + " for line:" + tempStr + "," + lineTAG + ",value:" + *value);
                found = true;
                break;
            }
            //not find, write to file directly.
            else {
                //found name but not value
                LogW("missing \"" + node + " at line:" + lineTAG);
            }
        } else {
            //found nothing at path, try name
            if (usePath && HasKeyWordInString(tempStr, "name=\"" + lineTAG + "\"")) {
                goto FOUND_LINE;
            }
            LogD("Can't Find line for name:" + lineTAG + ", find it next line!");
        }
    }

    fin.close();
    return found;
}

// use projectName or projectPath to find its commitID.
bool FindHashOfKeyName(string hash_file_name, string keyName, string *hash, bool usePath) {
    return FindValueOfNodeName(hash_file_name, keyName, "revision=\"", hash, usePath);
}

bool FindUpstreamOfKeyName(string hash_file_name, string keyName, string *upstream, bool usePath) {
    return FindValueOfNodeName(hash_file_name, keyName, "upstream=\"", upstream, usePath);
}

bool FindHashFromLine(string stringLine, string *hash) {
    string result;
    if (FindKeyName(stringLine, "revision=\"", "\"", &result)) {
        *hash = result;
        return true;
    }
    return false;
}

bool FindHashFromFile(string fileName, string *hash) {
    ifstream fin(fileName.data());
    cout << fileName <<endl;
    string tempStr;
    bool found = false;
    while (getline(fin, tempStr)) {
        //find, convert str and write
        string result;
        if (HasKeyWordInString(tempStr, "commit")) {
           // cout << tempStr << endl;
            *hash = tempStr.substr(7, tempStr.length() - 6);
            //cout << *hash << endl;
            found = true;
            break;
        }
        //not find, write to file directly.
        else {
            //found name but not hash
        }
    }

    fin.close();
    return found;


}

// [Abandon] return true if keyword is end of originStr.
bool IsKeyWordEndFix(string originStr, string keyword) {
    size_t where = originStr.rfind(keyword);
    if (where == string::npos) return false;
    else {
        return true;
    }
}

void ReplaceString(string toBeReplaced, string *originStr, string replacedStr) {
    string::size_type pos = 0;
    pos = originStr -> find(toBeReplaced, pos);
    size_t toBeReplaceLength = toBeReplaced.size();

    //cout << "=========== "<< toBeReplaced << "," << *originStr <<"," << pos << toBeReplaceLength<<endl;

    if (pos != string::npos) {
        originStr -> replace(pos, toBeReplaceLength, replacedStr);
    }
}

void InsertString(string insertStr, string *originStr, string afterWord) {
    string::size_type pos = 0;
    pos = originStr -> rfind(afterWord);

    if (pos != string::npos) {
        originStr -> replace(pos, 0, insertStr);
    }
}
