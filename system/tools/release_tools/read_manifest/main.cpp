#include <iostream>
#include <cstring>
#include <fstream>
#include <string>

#include "../release_utils.h"

using namespace std;

void printRepoName(string manifest_file, string project, string key) {

    ifstream fin(manifest_file.data());
    string tempStr;
    while (getline(fin, tempStr)) {
        string result;
        // line filter, only change project line
        if (HasKeyWordInString(tempStr, project)) {
            string node_name;
            if (!FindKeyName(tempStr, key + "=\"", "\"", &node_name)) {
                continue;
            } else {
                cout << node_name << endl;
                break;
            }
        }
    }
}

int main(int argc, char** argv) {

    string file_name, key = "", project = "";
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
        } else if (strcmp(s, "--project") == 0) {
            project = the_rest;
        } else if (strcmp(s, "--key") == 0) {
            key = the_rest;
        } else {
            goto FAIL_PRINT;
        }
    }
    printRepoName(file_name, project, key);
    return 0;

    FAIL_PRINT:
    cout << "Invalid argument!" << endl;
    cout << "use -i manifest_file.xml" << endl;
    return -1;
}
