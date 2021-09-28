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

#include "CarModelConfigReader.h"
#include "ConfigReaderUtil.h"
#include "MathHelp.h"
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

// Macro returning IoStatus::ERROR_READ_ANIMATION if condition evaluates to false.
#define RETURN_ERROR_STATUS_IF_FALSE(cond)         \
    do {                                           \
        if (!(cond)) {                             \
            return IOStatus::ERROR_READ_ANIMATION; \
        }                                          \
    } while (0)

using tinyxml2::XML_SUCCESS;
using tinyxml2::XMLDocument;
using tinyxml2::XMLElement;

namespace {

bool ReadValueHex(const XMLElement* parent, const char* elementName, uint32_t* hex) {
    const XMLElement* element = nullptr;
    RETURN_IF_FALSE(GetElement(parent, elementName, &element));
    RETURN_IF_FALSE(ElementHasText(element));
    *hex = std::stoul(element->GetText(), nullptr, 16);
    return true;
}

bool ReadValueList(const XMLElement* parent, const char* elementName,
                   std::vector<std::string>* valueList) {
    valueList->clear();
    for (const XMLElement* elem = parent->FirstChildElement(elementName); elem != nullptr;
         elem = elem->NextSiblingElement(elementName)) {
        RETURN_IF_FALSE(ElementHasText(elem));
        valueList->push_back(std::string(elem->GetText()));
    }
    return true;
}

// ReadValue for SurroundView2dParams::BlendingType.
bool ReadAnimationType(const XMLElement* parent, const char* elementName, AnimationType* type) {
    const XMLElement* element = nullptr;
    RETURN_IF_FALSE(GetElement(parent, elementName, &element));
    RETURN_IF_FALSE(ElementHasText(element));
    const std::string animationTypeStr(element->GetText());

    if (animationTypeStr == "RotationAngle") {
        *type = AnimationType::ROTATION_ANGLE;
    } else if (animationTypeStr == "RotationSpeed") {
        *type = AnimationType::ROTATION_SPEED;
    } else if (animationTypeStr == "Translation") {
        *type = AnimationType::TRANSLATION;
    } else if (animationTypeStr == "SwitchTextureOnce") {
        *type = AnimationType::SWITCH_TEXTURE_ONCE;
    } else if (animationTypeStr == "AdjustGammaOnce") {
        *type = AnimationType::ADJUST_GAMMA_ONCE;
    } else if (animationTypeStr == "SwitchTextureRepeat") {
        *type = AnimationType::SWITCH_TEXTURE_REPEAT;
    } else if (animationTypeStr == "AdjustGammaRepeat") {
        *type = AnimationType::ADJUST_GAMMA_REPEAT;
    } else {
        LOG(ERROR) << "Unknown AnimationType specified: " << animationTypeStr;
        return false;
    }
    return true;
}

bool ReadRange(const XMLElement* parent, const char* elementName, Range* range) {
    const XMLElement* rangeElem = nullptr;
    RETURN_IF_FALSE(GetElement(parent, elementName, &rangeElem));
    {
        RETURN_IF_FALSE(ReadValue(rangeElem, "Start", &range->start));
        RETURN_IF_FALSE(ReadValue(rangeElem, "End", &range->end));
    }
    return true;
}

bool ReadFloat3(const XMLElement* parent, const char* elementName, std::array<float, 3>* float3) {
    const XMLElement* arrayElem = nullptr;
    RETURN_IF_FALSE(GetElement(parent, elementName, &arrayElem));
    {
        RETURN_IF_FALSE(ReadValue(arrayElem, "X", &float3->at(0)));
        RETURN_IF_FALSE(ReadValue(arrayElem, "Y", &float3->at(1)));
        RETURN_IF_FALSE(ReadValue(arrayElem, "Z", &float3->at(2)));
    }
    return true;
}

// Generic template for reading a animation op, each op type must be specialized.
template <typename OpType>
bool ReadOp(const XMLElement* opElem, OpType* op) {
    (void)opElem;
    (void)op;
    LOG(ERROR) << "Unexpected internal error: Op type in not supported.";
    return false;
}

// Reads vhal property.
bool ReadVhalProperty(const XMLElement* parent, const char* elementName, uint64_t* vhalProperty) {
    const XMLElement* vhalPropElem = nullptr;
    RETURN_IF_FALSE(GetElement(parent, elementName, &vhalPropElem));
    {
        uint32_t propertyId;
        uint32_t areaId;
        RETURN_IF_FALSE(ReadValueHex(vhalPropElem, "PropertyId", &propertyId));
        RETURN_IF_FALSE(ReadValueHex(vhalPropElem, "AreaId", &areaId));
        *vhalProperty = (static_cast<uint64_t>(propertyId) << 32) | areaId;
    }
    return true;
}

template <>
bool ReadOp<RotationOp>(const XMLElement* rotationOpElem, RotationOp* rotationOp) {
    RETURN_IF_FALSE(ReadVhalProperty(rotationOpElem, "VhalProperty", &rotationOp->vhalProperty));

    RETURN_IF_FALSE(ReadAnimationType(rotationOpElem, "AnimationType", &rotationOp->type));

    RETURN_IF_FALSE(ReadFloat3(rotationOpElem, "RotationAxis", &rotationOp->axis.axisVector));

    RETURN_IF_FALSE(ReadFloat3(rotationOpElem, "RotationPoint", &rotationOp->axis.rotationPoint));

    RETURN_IF_FALSE(
            ReadValue(rotationOpElem, "DefaultRotationValue", &rotationOp->defaultRotationValue));

    RETURN_IF_FALSE(ReadValue(rotationOpElem, "AnimationTimeMs", &rotationOp->animationTime));

    RETURN_IF_FALSE(ReadRange(rotationOpElem, "RotationRange", &rotationOp->rotationRange));

    RETURN_IF_FALSE(ReadRange(rotationOpElem, "VhalRange", &rotationOp->vhalRange));

    return true;
}

template <>
bool ReadOp<TranslationOp>(const XMLElement* translationOpElem, TranslationOp* translationOp) {
    RETURN_IF_FALSE(
            ReadVhalProperty(translationOpElem, "VhalProperty", &translationOp->vhalProperty));

    RETURN_IF_FALSE(ReadAnimationType(translationOpElem, "AnimationType", &translationOp->type));

    RETURN_IF_FALSE(ReadFloat3(translationOpElem, "Direction", &translationOp->direction));

    RETURN_IF_FALSE(ReadValue(translationOpElem, "DefaultTranslationValue",
                              &translationOp->defaultTranslationValue));

    RETURN_IF_FALSE(ReadValue(translationOpElem, "AnimationTimeMs", &translationOp->animationTime));

    RETURN_IF_FALSE(
            ReadRange(translationOpElem, "TranslationRange", &translationOp->translationRange));

    RETURN_IF_FALSE(ReadRange(translationOpElem, "VhalRange", &translationOp->vhalRange));

    return true;
}

template <>
bool ReadOp<TextureOp>(const XMLElement* textureOpElem, TextureOp* textureOp) {
    RETURN_IF_FALSE(ReadVhalProperty(textureOpElem, "VhalProperty", &textureOp->vhalProperty));

    RETURN_IF_FALSE(ReadAnimationType(textureOpElem, "AnimationType", &textureOp->type));

    RETURN_IF_FALSE(ReadValue(textureOpElem, "DefaultTexture", &textureOp->defaultTexture));

    RETURN_IF_FALSE(ReadValue(textureOpElem, "AnimationTimeMs", &textureOp->animationTime));

    RETURN_IF_FALSE(ReadRange(textureOpElem, "TextureRange", &textureOp->textureRange));

    RETURN_IF_FALSE(ReadRange(textureOpElem, "VhalRange", &textureOp->vhalRange));

    return true;
}

template <>
bool ReadOp<GammaOp>(const XMLElement* gammaOpElem, GammaOp* gammaOp) {
    RETURN_IF_FALSE(ReadVhalProperty(gammaOpElem, "VhalProperty", &gammaOp->vhalProperty));

    RETURN_IF_FALSE(ReadAnimationType(gammaOpElem, "AnimationType", &gammaOp->type));

    RETURN_IF_FALSE(ReadValue(gammaOpElem, "AnimationTimeMs", &gammaOp->animationTime));

    RETURN_IF_FALSE(ReadRange(gammaOpElem, "GammaRange", &gammaOp->gammaRange));

    RETURN_IF_FALSE(ReadRange(gammaOpElem, "VhalRange", &gammaOp->vhalRange));

    return true;
}

template <typename OpType>
bool ReadAllOps(const XMLElement* animationElem, const char* elemName,
                std::map<uint64_t, std::vector<OpType>>* mapOps) {
    for (const XMLElement* elem = animationElem->FirstChildElement(elemName); elem != nullptr;
         elem = elem->NextSiblingElement(elemName)) {
        OpType op;
        RETURN_IF_FALSE(ReadOp(elem, &op));
        if (mapOps->find(op.vhalProperty) == mapOps->end()) {
            mapOps->emplace(op.vhalProperty, std::vector<OpType>());
        }
        mapOps->at(op.vhalProperty).push_back(op);
    }
    return true;
}

bool ReadAnimation(const XMLElement* animationElem, AnimationInfo* animationInfo) {
    RETURN_IF_FALSE(ReadValue(animationElem, "PartId", &animationInfo->partId));
    RETURN_IF_FALSE(ReadValue(animationElem, "ParentPartId", &animationInfo->parentId));

    // Child Part Ids (Optional)
    const XMLElement* childPartsElem = nullptr;
    GetElement(animationElem, "ChildParts", &childPartsElem);
    if (childPartsElem != nullptr) {
        RETURN_IF_FALSE(ReadValueList(childPartsElem, "PartId", &animationInfo->childIds));
    }

    // Set to the default Identity.
    animationInfo->pose = gMat4Identity;

    // All animation operations.
    RETURN_IF_FALSE(ReadAllOps(animationElem, "RotationOp", &animationInfo->rotationOpsMap));
    RETURN_IF_FALSE(ReadAllOps(animationElem, "TranslationOp", &animationInfo->translationOpsMap));
    RETURN_IF_FALSE(ReadAllOps(animationElem, "TextureOp", &animationInfo->textureOpsMap));
    RETURN_IF_FALSE(ReadAllOps(animationElem, "GammaOp", &animationInfo->gammaOpsMap));
    return true;
}

bool ReadAllAnimations(const XMLElement* rootElem, std::vector<AnimationInfo>* animations) {
    animations->clear();
    // Loop over animation elements.
    for (const XMLElement* elem = rootElem->FirstChildElement("Animation"); elem != nullptr;
         elem = elem->NextSiblingElement("Animation")) {
        AnimationInfo animationInfo;
        RETURN_IF_FALSE(ReadAnimation(elem, &animationInfo));
        animations->push_back(animationInfo);
    }
    return true;
}

}  // namespace

IOStatus ReadCarModelConfig(const std::string& carModelConfigFile,
                            AnimationConfig* animationConfig) {
    XMLDocument xmlDoc;

    /* load and parse a configuration file */
    xmlDoc.LoadFile(carModelConfigFile.c_str());
    if (xmlDoc.ErrorID() != XML_SUCCESS) {
        LOG(ERROR) << "Failed to load and/or parse a configuration file, " << xmlDoc.ErrorStr();
        return IOStatus::ERROR_READ_ANIMATION;
    }

    const XMLElement* rootElem = xmlDoc.RootElement();
    if (strcmp(rootElem->Name(), "SurroundViewCarModelConfig")) {
        LOG(ERROR) << "Config file is not in the required format: " << carModelConfigFile;
        return IOStatus::ERROR_READ_ANIMATION;
    }

    // version
    RETURN_ERROR_STATUS_IF_FALSE(ReadValue(rootElem, "Version", &animationConfig->version));

    // animations
    RETURN_ERROR_STATUS_IF_FALSE(ReadAllAnimations(rootElem, &animationConfig->animations));

    return IOStatus::OK;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
