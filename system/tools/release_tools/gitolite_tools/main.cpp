#include <iostream>
#include <cstring>
#include <fstream>
#include <string>

#include "../release_utils.h"

using namespace std;

void printRepoName(string manifest_file, string prefix, string endfix) {

    ifstream fin(manifest_file.data());
    string tempStr;
    while (getline(fin, tempStr)) {
        string result;
        // line filter, only change project line
        if (HasKeyWordInString(tempStr, "<project")) {
            //find, convert str and write
            string node_name;
            //find 'name' in string line
            if (FindKeyName(tempStr, "name=\"", "\"", &node_name)) {
                cout << prefix << node_name << endfix << endl;
            } else {
                cout << "your manifest file missing path & name, please check!";
            }
        }
    }
}

int main(int argc, char** argv) {

    string file_name, prefix = "", endfix = "";
    if (argc == 1) goto FAIL_PRINT;

    // Parse flags, all of which start with '-'
    for (int i = 1; i < argc; ++i) {
        const size_t len = strlen(argv[i]);
        const char *s = argv[i];
        if (s[0] != '-') {
            continue;  // On to the positional arguments.
        }
        string the_rest = argv[i+1];
        if (s[1] == 'i') {
            file_name = the_rest;
        } else if (strcmp(s, "--prefix") == 0) {
            prefix = the_rest;
        } else if (strcmp(s, "--endfix") == 0) {
            endfix = the_rest;
        } else {
            goto FAIL_PRINT;
        }
    }
    printRepoName(file_name, prefix, endfix);
    return 0;

    FAIL_PRINT:
    cout << "Invalid argument!" << endl;
    cout << "use -i manifest_file.xml" << endl;
    return -1;
}
