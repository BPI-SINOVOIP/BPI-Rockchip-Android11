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
            string node_name, path_name, groups_name, remote_name, revision_name;
            bool has_name=true, has_path=true, has_groups=true, has_remote=true, has_revision=true;
            //find 'name' in string line
            if (!FindKeyName(tempStr, "name=\"", "\"", &node_name)) {
                has_name=false;
            }
            if (!FindKeyName(tempStr, "path=\"", "\"", &path_name)) {
                has_path=false;
            }
            if (!FindKeyName(tempStr, "groups=\"", "\"", &groups_name)) {
                has_groups=false;
            }
            if (!FindKeyName(tempStr, "remote=\"", "\"", &remote_name)) {
                has_remote=false;
            }
            if (!FindKeyName(tempStr, "revision=\"", "\"", &revision_name)) {
                has_revision=false;
            }
            cout << "  <project ";
            if (has_path) cout << "path=\"" << path_name << "\" ";
            if (has_name) cout << "name=\"" << node_name << "\" ";
            if (has_groups) cout << "groups=\"" << groups_name << "\" ";
            if (has_remote) cout << "remote=\"" << remote_name << "\" ";
            if (has_revision) cout << "revision=\"" << revision_name << "\" ";
            cout << "/>" << endl;
        } else {
            cout << tempStr << endl;
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
