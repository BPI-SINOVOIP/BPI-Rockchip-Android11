/*
 * Copyright (C) 2020 The Android Open Source Project
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
#include <fcntl.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#include <filesystem>

#include "golddata.pb.h"

using namespace std;

bool ConvertPbtxtToPb(const filesystem::path& pbtxtFile, const filesystem::path& pbOutDir) {
    // parse plain text from .pbtxt.
    android::net::GoldTest goldTest;

    int fd = open(pbtxtFile.c_str(), O_RDONLY);
    if (fd < 0) {
        cerr << "Failed to open " << pbtxtFile << ", " << strerror(errno) << endl;
        return false;
    }
    {
        google::protobuf::io::FileInputStream fileInput(fd);
        fileInput.SetCloseOnDelete(true);
        if (!google::protobuf::TextFormat::Parse(&fileInput, &goldTest)) {
            cerr << "Failed to parse " << pbtxtFile << endl;
            return false;
        }
    }

    // write marshalled message into .pb file
    filesystem::path pbFile = pbOutDir / pbtxtFile.filename();
    pbFile.replace_extension(".pb");
    fd = open(pbFile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0660);
    if (fd < 0) {
        cerr << "Failed to open " << pbFile << ", " << strerror(errno) << endl;
        return false;
    }
    {
        google::protobuf::io::FileOutputStream fileOutputStream(fd);
        fileOutputStream.SetCloseOnDelete(true);
        if (!goldTest.SerializeToZeroCopyStream(&fileOutputStream)) {
            cerr << "Failed to serialize " << pbFile << endl;
            filesystem::remove(pbFile);
            return false;
        }
    }

    cout << "Generate " << pbFile << " successfully" << endl;
    return true;
}

int main(int argc, char const* const* argv) {
    filesystem::path pbtxtFile;
    filesystem::path pbOutDir;
    const string arg_in = "--in_file=";
    const string arg_out = "--out_dir=";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.find(arg_in) == 0) {
            pbtxtFile = filesystem::path(arg.substr(arg_in.size()));
        } else if (arg.find(arg_out) == 0) {
            pbOutDir = filesystem::path(arg.substr(arg_out.size()));
        } else {
            cerr << "Unknown argument: " << arg << endl;
            exit(1);
        }
    }

    if (pbtxtFile.empty() || pbOutDir.empty()) {
        cerr << arg_in << " or " << arg_out << " is unassigned" << endl;
        exit(1);
    }
    if (!ConvertPbtxtToPb(pbtxtFile, pbOutDir)) {
        cerr << "Failed to convert " << pbtxtFile << endl;
        exit(1);
    }
}
