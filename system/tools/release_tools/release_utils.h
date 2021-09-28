#ifndef RELEASE_TOOLS_RELEASE_UTILS_H_
#define RELEASE_TOOLS_RELEASE_UTILS_H_

#include <iostream>
#include "fstream"
#include <dirent.h>
#include <stdio.h>
#include <string>
#include <cstring>

#define DEBUG 0

#define LogE(format)\
cout << "[ERROR] " << __func__ <<"() at line "<< __LINE__  << ":" << format << endl;

#define LogW(format)\
cout << "[WARNING] " << __func__ <<"() at line "<< __LINE__  << ":" << format << endl;

#define LogD(format)\
if (DEBUG) cout << "[DEBUG] " << __func__ <<"() at line "<< __LINE__  << ":" << format << endl;

using namespace std;

string getPath(string param);

bool FindKeyWordEndFix(string originStr, string keyword);
bool HasKeyWordInString(string originStr, string keyword);
bool FindDefaultRevision(string hash_file_name, string *revision);
bool FindKeyName(string originStr, string keyword1, string keyword2, string *result);
bool FindHashOfKeyName(string hash_file_name, string keyName, string *hash, bool usePath);
bool FindUpstreamOfKeyName(string hash_file_name, string keyName, string *upstream, bool usePath);
bool FindHashFromLine(string stringLine, string *hash);
bool FindHashFromFile(string fileName, string *hash);

void ReplaceString(string toBeReplaced, string *originStr, string replacedStr);
void InsertString(string insertStr, string *originStr, string afterWord);

#endif //RELEASE_TOOLS_RELEASE_UTILS_H_
