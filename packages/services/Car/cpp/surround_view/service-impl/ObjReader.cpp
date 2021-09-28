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

#include "ObjReader.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "MtlReader.h"
#include "core_lib.h"

#include <android-base/logging.h>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

using android_auto::surround_view::CarMaterial;
using android_auto::surround_view::CarVertex;

namespace {

constexpr int kNumberOfVerticesPerFace = 3;
constexpr int kNumberOfAxes = 3;
constexpr int kCharBufferSize = 128;

const std::array<float, 16> kMat4Identity = {
        /*row 0*/ 1, 0, 0, 0,
        /*row 1*/ 0, 1, 0, 0,
        /*row 2*/ 0, 0, 1, 0,
        /*row 3*/ 0, 0, 0, 1};

// Copies face vertices parsed from obj to car vertices.
void CopyFaceToCarVertex(const std::vector<std::array<float, kNumberOfAxes>>& currentVertices,
                         const std::vector<std::array<float, kNumberOfAxes>>& currentTextures,
                         const std::vector<std::array<float, kNumberOfAxes>>& currentNormals,
                         int vertexId, int textureId, int normalId, CarVertex* carVertex) {
    std::memcpy(carVertex->pos.data(), currentVertices[vertexId - 1].data(),
                currentVertices[vertexId - 1].size() * sizeof(float));

    if (textureId != -1) {
        std::memcpy(carVertex->tex_coord.data(), currentTextures[textureId - 1].data(),
                    2 * sizeof(float));
        // Set texture coodinates as invalid.
        carVertex->tex_coord = {-1.0, -1.0};
    }

    std::memcpy(carVertex->normal.data(), currentNormals[normalId - 1].data(),
                currentNormals[normalId - 1].size() * sizeof(float));
}

}  // namespace

bool ReadObjFromFile(const std::string& objFilename, std::map<std::string, CarPart>* carPartsMap) {
    return ReadObjFromFile(objFilename, ReadObjOptions(), carPartsMap);
}

bool ReadObjFromFile(const std::string& objFilename, const ReadObjOptions& option,
                     std::map<std::string, CarPart>* carPartsMap) {
    FILE* file = fopen(objFilename.c_str(), "r");
    if (!file) {
        LOG(ERROR) << "Failed to open obj file: " << objFilename;
        return false;
    }

    for (int i = 0; i < kNumberOfAxes; ++i) {
        if (option.coordinateMapping[i] >= kNumberOfAxes || option.coordinateMapping[i] < 0) {
            fclose(file);
            LOG(ERROR) << "coordinateMapping index must be less than 3 and greater or equal "
                          "to 0.";
            return false;
        }
    }

    std::vector<std::array<float, kNumberOfAxes>> currentVertices;
    std::vector<std::array<float, kNumberOfAxes>> currentNormals;
    std::vector<std::array<float, kNumberOfAxes>> currentTextures;
    std::map<std::string, MtlConfigParams> mtlConfigParamsMap;
    std::string currentGroupName;
    MtlConfigParams currentMtlConfig;

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

        // TODO(b/156558814): add object type support.
        // TODO(b/156559272): add document for supported format.
        // Only single group per line is supported.
        if (strcmp(lineHeader, "g") == 0) {
            res = fscanf(file, "%s", lineHeader);
            currentGroupName = lineHeader;
            currentMtlConfig = MtlConfigParams();

            if (carPartsMap->find(currentGroupName) != carPartsMap->end()) {
                LOG(WARNING) << "Duplicate group name: " << currentGroupName
                             << ". using car part name as: " << currentGroupName << "_dup";
                currentGroupName.append("_dup");
            }
            carPartsMap->emplace(
                    std::make_pair(currentGroupName,
                                   CarPart((std::vector<CarVertex>()), CarMaterial(), kMat4Identity,
                                           std::string(), std::vector<std::string>())));
            continue;
        }

        // no "g" case, assign it as default.
        if (currentGroupName.empty()) {
            currentGroupName = "default";
            currentMtlConfig = MtlConfigParams();
            carPartsMap->emplace(
                    std::make_pair(currentGroupName,
                                   CarPart((std::vector<CarVertex>()), CarMaterial(), kMat4Identity,
                                           std::string(), std::vector<std::string>())));
        }

        if (strcmp(lineHeader, "usemtl") == 0) {
            res = fscanf(file, "%s", lineHeader);

            // If material name not found.
            if (mtlConfigParamsMap.find(lineHeader) == mtlConfigParamsMap.end()) {
                carPartsMap->at(currentGroupName).material = CarMaterial();
                LOG(ERROR) << "Material not found: $0" << lineHeader;
                return false;
            }

            currentMtlConfig = mtlConfigParamsMap[lineHeader];

            carPartsMap->at(currentGroupName).material.ka = {currentMtlConfig.ka[0],
                                                             currentMtlConfig.ka[1],
                                                             currentMtlConfig.ka[2]};

            carPartsMap->at(currentGroupName).material.kd = {currentMtlConfig.kd[0],
                                                             currentMtlConfig.kd[1],
                                                             currentMtlConfig.kd[2]};

            carPartsMap->at(currentGroupName).material.ks = {currentMtlConfig.ks[0],
                                                             currentMtlConfig.ks[1],
                                                             currentMtlConfig.ks[2]};

            carPartsMap->at(currentGroupName).material.d = currentMtlConfig.d;

            carPartsMap->at(currentGroupName).material.textures.clear();

            continue;
        }

        if (strcmp(lineHeader, "mtllib") == 0) {
            res = fscanf(file, "%s", lineHeader);
            mtlConfigParamsMap.clear();
            std::string mtlFilename;
            if (option.mtlFilename.empty()) {
                mtlFilename = objFilename.substr(0, objFilename.find_last_of("/"));
                mtlFilename.append("/");
                mtlFilename.append(lineHeader);
            } else {
                mtlFilename = option.mtlFilename;
            }
            if (!ReadMtlFromFile(mtlFilename, &mtlConfigParamsMap)) {
                LOG(ERROR) << "Parse MTL file " << mtlFilename << " failed.";
                return false;
            }
            continue;
        }

        if (strcmp(lineHeader, "v") == 0) {
            std::array<float, kNumberOfAxes> pos;
            fscanf(file, "%f %f %f\n", &pos[option.coordinateMapping[0]],
                   &pos[option.coordinateMapping[1]], &pos[option.coordinateMapping[2]]);
            for (int i = 0; i < kNumberOfAxes; ++i) {
                pos[i] *= option.scales[i];
                pos[i] += option.offsets[i];
            }
            currentVertices.push_back(pos);
        } else if (strcmp(lineHeader, "vt") == 0) {
            std::array<float, kNumberOfAxes> texture;
            fscanf(file, "%f %f %f\n", &texture[0], &texture[1], &texture[2]);
            currentTextures.push_back(texture);
        } else if (strcmp(lineHeader, "vn") == 0) {
            std::array<float, kNumberOfAxes> normal;
            fscanf(file, "%f %f %f\n", &normal[option.coordinateMapping[0]],
                   &normal[option.coordinateMapping[1]], &normal[option.coordinateMapping[2]]);
            currentNormals.push_back(normal);
        } else if (strcmp(lineHeader, "f") == 0) {
            int vertexId[kNumberOfVerticesPerFace];
            int textureId[kNumberOfVerticesPerFace] = {-1, -1, -1};
            int normalId[kNumberOfVerticesPerFace];

            // Face vertices supported formats:
            // With texture:     pos/texture/normal
            // Without texture:  pos//normal

            // Scan first vertex position.
            int matches = fscanf(file, "%d/", &vertexId[0]);

            if (matches != 1) {
                LOG(WARNING) << "Face index error. Skipped.";
                fgets(lineHeader, sizeof(lineHeader), file);
                continue;
            }

            // Try scanning first two face 2 vertices with texture format present.
            bool isTexturePresent = true;
            matches = fscanf(file, "%d/%d %d/%d/%d", &textureId[0], &normalId[0], &vertexId[1],
                             &textureId[1], &normalId[1]);

            // If 5 matches not found, try scanning first 2 face vertices without
            // texture format.
            if (matches != 5) {
                matches = fscanf(file, "/%d %d//%d", &normalId[0], &vertexId[1], &normalId[1]);

                // If 3 matches not found return with error.
                if (matches != 3) {
                    LOG(WARNING) << "Face format not supported. Skipped.";
                    fgets(lineHeader, sizeof(lineHeader), file);
                    continue;
                }

                isTexturePresent = false;
            }

            // Copy first two face vertices to car vertices.
            std::array<CarVertex, kNumberOfVerticesPerFace> carVertices;
            CopyFaceToCarVertex(currentVertices, currentTextures, currentNormals, vertexId[0],
                                textureId[0], normalId[0], &carVertices[0]);
            CopyFaceToCarVertex(currentVertices, currentTextures, currentNormals, vertexId[1],
                                textureId[1], normalId[1], &carVertices[1]);

            // Add a triangle that the first two vertices make with every subsequent
            // face vertex 3 and onwards. Note this assumes the face is a convex
            // polygon.
            do {
                if (isTexturePresent) {
                    matches = fscanf(file, " %d/%d/%d", &vertexId[2], &textureId[2], &normalId[2]);
                    // Warn if un-expected number of matches.
                    if (matches != 3 && matches != 0) {
                        LOG(WARNING) << "Face matches, expected 3, read: " << matches;
                        break;
                    }
                } else {
                    // Warn if un-expected number of matches.
                    matches = fscanf(file, " %d//%d", &vertexId[2], &normalId[2]);
                    if (matches != 2 && matches != 0) {
                        LOG(WARNING) << "Face matches, expected 2, read: " << matches;
                        break;
                    }
                }

                if (matches == 0) {
                    break;
                }

                CopyFaceToCarVertex(currentVertices, currentTextures, currentNormals, vertexId[2],
                                    textureId[2], normalId[2], &carVertices[2]);

                carPartsMap->at(currentGroupName).vertices.push_back(carVertices[0]);
                carPartsMap->at(currentGroupName).vertices.push_back(carVertices[1]);
                carPartsMap->at(currentGroupName).vertices.push_back(carVertices[2]);

                carVertices[1] = carVertices[2];
            } while (true);

        } else {
            // LOG(WARNING) << "Unknown tag " << lineHeader << ". Skipped";
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
