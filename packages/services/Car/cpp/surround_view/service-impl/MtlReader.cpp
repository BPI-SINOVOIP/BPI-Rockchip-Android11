/*
 * Copyright 2020 The Android Open Source Project
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

#include "MtlReader.h"

#include <android-base/logging.h>
#include <cstdio>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

namespace {

constexpr int kCharBufferSize = 128;

void ReadFloat3(FILE* file, float* value) {
    float temp[3];
    int res = fscanf(file, "%f %f %f", &temp[0], &temp[1], &temp[2]);
    3 == res ? std::memcpy(value, temp, 3 * sizeof(float)) : nullptr;
}

void ReadFloat(FILE* file, float* value) {
    float temp;
    int res = fscanf(file, "%f", &temp);
    *value = res > 0 ? temp : -1;
}

void ReadInt(FILE* file, int* value) {
    int temp;
    int res = fscanf(file, "%d", &temp);
    *value = res > 0 ? temp : -1;
}

void ReadString(FILE* file, std::string* value) {
    char temp[kCharBufferSize];
    fscanf(file, "%s", temp);
    *value = temp;
}
}  // namespace

bool ReadMtlFromFile(const std::string& mtlFilename,
                     std::map<std::string, MtlConfigParams>* params) {
    FILE* file = fopen(mtlFilename.c_str(), "r");
    if (!file) {
        LOG(ERROR) << "Failed to open mtl file: " << mtlFilename;
        return false;
    }

    std::string currentConfig;
    while (true) {
        char lineHeader[kCharBufferSize];
        // read the first word of the line
        int res = fscanf(file, "%s", lineHeader);

        if (res == EOF) {
            break;  // EOF = End Of File. Quit the loop.
        }

        if (strcmp(lineHeader, "#") == 0) {
            fgets(lineHeader, sizeof(lineHeader), file);
            continue;
        }
        if (strcmp(lineHeader, "newmtl") == 0) {
            res = fscanf(file, "%s", lineHeader);
            if (params->find(lineHeader) != params->end()) {
                fclose(file);
                LOG(ERROR) << "Duplicated params of : " << lineHeader[0];
                return false;
            }
            currentConfig = lineHeader;
            continue;
        }

        if (strcmp(lineHeader, "Ns") == 0) {
            ReadFloat(file, &((*params)[currentConfig].ns));
            continue;
        }
        if (strcmp(lineHeader, "Ni") == 0) {
            ReadFloat(file, &((*params)[currentConfig].ni));
            continue;
        }
        if (strcmp(lineHeader, "d") == 0) {
            ReadFloat(file, &((*params)[currentConfig].d));
            continue;
        }
        if (strcmp(lineHeader, "Tr") == 0) {
            ReadFloat(file, &((*params)[currentConfig].tr));
            continue;
        }
        if (strcmp(lineHeader, "Tf") == 0) {
            ReadFloat3(file, (*params)[currentConfig].tf);
            continue;
        }
        if (strcmp(lineHeader, "illum") == 0) {
            ReadInt(file, &((*params)[currentConfig].illum));
            continue;
        }
        if (strcmp(lineHeader, "Ka") == 0) {
            ReadFloat3(file, (*params)[currentConfig].ka);
            continue;
        }
        if (strcmp(lineHeader, "Kd") == 0) {
            ReadFloat3(file, (*params)[currentConfig].kd);
            continue;
        }
        if (strcmp(lineHeader, "Ks") == 0) {
            ReadFloat3(file, (*params)[currentConfig].ks);
            continue;
        }
        if (strcmp(lineHeader, "Ke") == 0) {
            ReadFloat3(file, (*params)[currentConfig].ke);
            continue;
        }
        if (strcmp(lineHeader, "map_bump") == 0) {
            ReadString(file, &((*params)[currentConfig].mapBump));
            continue;
        }
        if (strcmp(lineHeader, "bump") == 0) {
            ReadString(file, &((*params)[currentConfig].bump));
            continue;
        }
        if (strcmp(lineHeader, "map_Ka") == 0) {
            ReadString(file, &((*params)[currentConfig].mapKa));
            continue;
        }
        if (strcmp(lineHeader, "map_Kd") == 0) {
            ReadString(file, &((*params)[currentConfig].mapKd));
            continue;
        }
        if (strcmp(lineHeader, "map_Ks") == 0) {
            ReadString(file, &((*params)[currentConfig].mapKs));
            continue;
        } else {
            LOG(WARNING) << "Unknown tag " << lineHeader << ". Skipped";
            fgets(lineHeader, sizeof(lineHeader), file);
            continue;
        }
    }

    fclose(file);
    return true;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
