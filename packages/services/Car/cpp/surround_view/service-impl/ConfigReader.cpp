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

#include "ConfigReader.h"
#include "ConfigReaderUtil.h"
#include "core_lib.h"

#include <android-base/logging.h>
#include <tinyxml2.h>
#include <sstream>
#include <string>
#include <utility>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

using tinyxml2::XML_SUCCESS;
using tinyxml2::XMLDocument;
using tinyxml2::XMLElement;

using android_auto::surround_view::SurroundView2dParams;
using android_auto::surround_view::SurroundView3dParams;

namespace {

// Macro returning IoStatus::ERROR_CONFIG_FILE_FORMAT if condition evaluates to false.
#define RETURN_ERROR_STATUS_IF_FALSE(cond)             \
    do {                                               \
        if (!(cond)) {                                 \
            return IOStatus::ERROR_CONFIG_FILE_FORMAT; \
        }                                              \
    } while (0)

// ReadValue for SurroundView2dParams::BlendingType.
bool ReadValue2dBlendType(const XMLElement* parent, const char* elementName,
                          SurroundView2dParams::BlendingType* value) {
    const XMLElement* element = nullptr;
    RETURN_IF_FALSE(GetElement(parent, elementName, &element));
    RETURN_IF_FALSE(ElementHasText(element));
    const std::string blendingTypeStr(element->GetText());

    if (blendingTypeStr == "multiband") {
        *value = SurroundView2dParams::BlendingType::MULTIBAND;
    } else if (blendingTypeStr == "alpha") {
        *value = SurroundView2dParams::BlendingType::ALPHA;
    } else {
        LOG(ERROR) << "Unknown BlendingType specified: " << blendingTypeStr;
        return false;
    }
    return true;
}

bool ReadSvConfig2d(const XMLElement* parent, SvConfig2d* sv2dConfig) {
    RETURN_IF_FALSE(ReadValue(parent, "Sv2dEnabled", &sv2dConfig->sv2dEnabled));
    if (!sv2dConfig->sv2dEnabled) {
        return true;
    }

    SurroundView2dParams* sv2dParams = &sv2dConfig->sv2dParams;
    const XMLElement* param2dElem = nullptr;
    RETURN_IF_FALSE(GetElement(parent, "Sv2dParams", &param2dElem));
    {
        // OutputResolution
        const XMLElement* outputResolutionElem = nullptr;
        RETURN_IF_FALSE(GetElement(param2dElem, "OutputResolution", &outputResolutionElem));
        {
            RETURN_IF_FALSE(
                    ReadValue(outputResolutionElem, "Width", &sv2dParams->resolution.width));
            RETURN_IF_FALSE(
                    ReadValue(outputResolutionElem, "Height", &sv2dParams->resolution.height));
        }

        // GroundMapping
        const XMLElement* groundMappingElem = nullptr;
        RETURN_IF_FALSE(GetElement(param2dElem, "GroundMapping", &groundMappingElem));
        {
            RETURN_IF_FALSE(
                    ReadValue(groundMappingElem, "Width", &sv2dParams->physical_size.width));
            RETURN_IF_FALSE(
                    ReadValue(groundMappingElem, "Height", &sv2dParams->physical_size.height));

            // Center
            const XMLElement* centerElem = nullptr;
            RETURN_IF_FALSE(GetElement(groundMappingElem, "Center", &centerElem));
            {
                RETURN_IF_FALSE(ReadValue(centerElem, "X", &sv2dParams->physical_center.x));
                RETURN_IF_FALSE(ReadValue(centerElem, "Y", &sv2dParams->physical_center.y));
            }
        }

        // Car Bounding Box
        const XMLElement* carBoundingBoxElem = nullptr;
        RETURN_IF_FALSE(GetElement(param2dElem, "CarBoundingBox", &carBoundingBoxElem));
        {
            RETURN_IF_FALSE(
                    ReadValue(carBoundingBoxElem, "Width", &sv2dConfig->carBoundingBox.width));
            RETURN_IF_FALSE(
                    ReadValue(carBoundingBoxElem, "Height", &sv2dConfig->carBoundingBox.height));

            // Center
            const XMLElement* leftTopCornerElem = nullptr;
            RETURN_IF_FALSE(GetElement(carBoundingBoxElem, "LeftTopCorner", &leftTopCornerElem));
            {
                RETURN_IF_FALSE(ReadValue(leftTopCornerElem, "X", &sv2dConfig->carBoundingBox.x));
                RETURN_IF_FALSE(ReadValue(leftTopCornerElem, "Y", &sv2dConfig->carBoundingBox.y));
            }
        }

        // Blending type
        const XMLElement* blendingTypeElem = nullptr;
        RETURN_IF_FALSE(GetElement(param2dElem, "BlendingType", &blendingTypeElem));
        {
            RETURN_IF_FALSE(ReadValue2dBlendType(blendingTypeElem, "HighQuality",
                                                 &sv2dParams->high_quality_blending));
            RETURN_IF_FALSE(ReadValue2dBlendType(blendingTypeElem, "LowQuality",
                                                 &sv2dParams->low_quality_blending));
        }

        // GPU Acceleration enabled or not
        RETURN_IF_FALSE(ReadValue(param2dElem, "GpuAccelerationEnabled",
                                  &sv2dParams->gpu_acceleration_enabled));
    }
    return true;
}

bool ReadSvConfig3d(const XMLElement* parent, SvConfig3d* sv3dConfig) {
    RETURN_IF_FALSE(ReadValue(parent, "Sv3dEnabled", &sv3dConfig->sv3dEnabled));
    if (!sv3dConfig->sv3dEnabled) {
        return true;
    }
    RETURN_IF_FALSE(ReadValue(parent, "Sv3dAnimationsEnabled", &sv3dConfig->sv3dAnimationsEnabled));

    if (sv3dConfig->sv3dAnimationsEnabled) {
        RETURN_IF_FALSE(ReadValue(parent, "CarModelConfigFile", &sv3dConfig->carModelConfigFile));
    }

    RETURN_IF_FALSE(ReadValue(parent, "CarModelObjFile", &sv3dConfig->carModelObjFile));

    SurroundView3dParams* sv3dParams = &sv3dConfig->sv3dParams;
    const XMLElement* param3dElem = nullptr;
    RETURN_IF_FALSE(GetElement(parent, "Sv3dParams", &param3dElem));
    {
        // OutputResolution
        const XMLElement* outputResolutionElem = nullptr;
        RETURN_IF_FALSE(GetElement(param3dElem, "OutputResolution", &outputResolutionElem));
        {
            RETURN_IF_FALSE(
                    ReadValue(outputResolutionElem, "Width", &sv3dParams->resolution.width));
            RETURN_IF_FALSE(
                    ReadValue(outputResolutionElem, "Height", &sv3dParams->resolution.height));
        }

        // Bowl Params
        const XMLElement* bowlParamsElem = nullptr;
        RETURN_IF_FALSE(GetElement(param3dElem, "BowlParams", &bowlParamsElem));
        {
            RETURN_IF_FALSE(ReadValue(bowlParamsElem, "PlaneRadius", &sv3dParams->plane_radius));
            RETURN_IF_FALSE(
                    ReadValue(bowlParamsElem, "PlaneDivisions", &sv3dParams->plane_divisions));
            RETURN_IF_FALSE(ReadValue(bowlParamsElem, "CurveHeight", &sv3dParams->curve_height));
            RETURN_IF_FALSE(
                    ReadValue(bowlParamsElem, "CurveDivisions", &sv3dParams->curve_divisions));
            RETURN_IF_FALSE(
                    ReadValue(bowlParamsElem, "AngularDivisions", &sv3dParams->angular_divisions));
            RETURN_IF_FALSE(
                    ReadValue(bowlParamsElem, "CurveCoefficient", &sv3dParams->curve_coefficient));
        }

        // High Quality details
        const XMLElement* highQualityDetailsElem = nullptr;
        GetElement(param3dElem, "HighQualityDetails", &highQualityDetailsElem);
        {
            RETURN_IF_FALSE(ReadValue(highQualityDetailsElem, "Shadows",
                                      &sv3dParams->high_details_shadows));
            RETURN_IF_FALSE(ReadValue(highQualityDetailsElem, "Reflections",
                                      &sv3dParams->high_details_reflections));
        }
    }
    return true;
}

bool ReadCameraConfig(const XMLElement* parent, CameraConfig* cameraConfig) {
    const XMLElement* cameraConfigElem = nullptr;
    RETURN_IF_FALSE(GetElement(parent, "CameraConfig", &cameraConfigElem));
    {
        // Evs Group Id
        RETURN_IF_FALSE(ReadValue(cameraConfigElem, "EvsGroupId", &cameraConfig->evsGroupId));

        // Evs Cameras Ids
        const XMLElement* cameraIdsElem = nullptr;
        RETURN_IF_FALSE(GetElement(cameraConfigElem, "EvsCameraIds", &cameraIdsElem));
        {
            cameraConfig->evsCameraIds.resize(4);
            RETURN_IF_FALSE(ReadValue(cameraIdsElem, "Front", &cameraConfig->evsCameraIds[0]));
            RETURN_IF_FALSE(ReadValue(cameraIdsElem, "Right", &cameraConfig->evsCameraIds[1]));
            RETURN_IF_FALSE(ReadValue(cameraIdsElem, "Rear", &cameraConfig->evsCameraIds[2]));
            RETURN_IF_FALSE(ReadValue(cameraIdsElem, "Left", &cameraConfig->evsCameraIds[3]));
        }

        // Masks (Optional).
        const XMLElement* masksElem = nullptr;
        GetElement(cameraConfigElem, "Masks", &masksElem);
        if (masksElem != nullptr) {
            cameraConfig->maskFilenames.resize(4);
            RETURN_IF_FALSE(ReadValue(masksElem, "Front", &cameraConfig->maskFilenames[0]));
            RETURN_IF_FALSE(ReadValue(masksElem, "Right", &cameraConfig->maskFilenames[1]));
            RETURN_IF_FALSE(ReadValue(masksElem, "Rear", &cameraConfig->maskFilenames[2]));
            RETURN_IF_FALSE(ReadValue(masksElem, "Left", &cameraConfig->maskFilenames[3]));
        }
    }
    return true;
}

}  // namespace

IOStatus ReadSurroundViewConfig(const std::string& configFile, SurroundViewConfig* svConfig) {
    XMLDocument xmlDoc;

    // load and parse a configuration file
    xmlDoc.LoadFile(configFile.c_str());
    if (xmlDoc.ErrorID() != XML_SUCCESS) {
        LOG(ERROR) << "Failed to load and/or parse a configuration file, " << xmlDoc.ErrorStr();
        return IOStatus::ERROR_READ_CONFIG_FILE;
    }

    const XMLElement* rootElem = xmlDoc.RootElement();
    if (strcmp(rootElem->Name(), "SurroundViewConfig")) {
        LOG(ERROR) << "Config file is not in the required format: " << configFile;
        return IOStatus::ERROR_READ_CONFIG_FILE;
    }

    // version
    RETURN_ERROR_STATUS_IF_FALSE(ReadValue(rootElem, "Version", &svConfig->version));

    // CameraConfig
    RETURN_ERROR_STATUS_IF_FALSE(ReadCameraConfig(rootElem, &svConfig->cameraConfig));

    // Surround View 2D
    RETURN_ERROR_STATUS_IF_FALSE(ReadSvConfig2d(rootElem, &svConfig->sv2dConfig));

    // Surround View 3D
    RETURN_ERROR_STATUS_IF_FALSE(ReadSvConfig3d(rootElem, &svConfig->sv3dConfig));

    return IOStatus::OK;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
