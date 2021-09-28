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

#include "AnimationModule.h"
#include "MathHelp.h"

#include <android-base/logging.h>
#include <algorithm>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {
namespace {
std::array<float, 3> operator*(std::array<float, 3> lvector, float scalar) {
    return std::array<float, 3>{
            lvector[0] * scalar,
            lvector[1] * scalar,
            lvector[2] * scalar,
    };
}

inline float getRationalNumber(const Range& mappedRange, float percentage) {
    return mappedRange.start + (mappedRange.end - mappedRange.start) * percentage;
}

inline float getRationalNumber(const Range& mappedRange, const Range& rawRange, float rawValue) {
    if (0 == rawRange.end - rawRange.start) {
        return mappedRange.start;
    }
    const float percentage = (rawValue - rawRange.start) / (rawRange.end - rawRange.start);
    return mappedRange.start + (mappedRange.end - mappedRange.start) * percentage > 1
            ? 1
            : percentage < 0 ? 0 : percentage;
}

inline uint64_t getCombinedId(const VehiclePropValue& vhalValueFloat) {
    return static_cast<uint64_t>(vhalValueFloat.prop) << 32 | vhalValueFloat.areaId;
}

float getVhalValueFloat(const VehiclePropValue& vhalValue) {
    int32_t type = vhalValue.prop & 0x00FF0000;
    switch (type) {
        case (int32_t)VehiclePropertyType::BOOLEAN:
            return 0 == vhalValue.value.int32Values[0] ? 0.0f : 1.0f;
        case (int32_t)VehiclePropertyType::FLOAT:
            return vhalValue.value.floatValues[0];
        case (int32_t)VehiclePropertyType::INT32:
            return (float)vhalValue.value.int32Values[0];
        case (int32_t)VehiclePropertyType::INT64:
            return (float)vhalValue.value.int64Values[0];
        default:
            return 0;
    }
}
}  // namespace

AnimationModule::AnimationModule(const std::map<std::string, CarPart>& partsMap,
                                 const std::map<std::string, CarTexture>& texturesMap,
                                 const std::vector<AnimationInfo>& animations) :
      mIsCalled(false), mPartsMap(partsMap), mTexturesMap(texturesMap), mAnimations(animations) {
    mapVhalToParts();
    initCarPartStatus();
}

void AnimationModule::mapVhalToParts() {
    for (const auto& animationInfo : mAnimations) {
        auto partId = animationInfo.partId;
        for (const auto& gammaOp : animationInfo.gammaOpsMap) {
            if (mVhalToPartsMap.find(gammaOp.first) != mVhalToPartsMap.end()) {
                mVhalToPartsMap.at(gammaOp.first).insert(partId);
            } else {
                mVhalToPartsMap.emplace(
                        std::make_pair(gammaOp.first, std::set<std::string>{partId}));
            }
        }
        for (const auto& textureOp : animationInfo.textureOpsMap) {
            if (mVhalToPartsMap.find(textureOp.first) != mVhalToPartsMap.end()) {
                mVhalToPartsMap.at(textureOp.first).insert(partId);
            } else {
                mVhalToPartsMap.emplace(
                        std::make_pair(textureOp.first, std::set<std::string>{partId}));
            }
        }
        for (const auto& rotationOp : animationInfo.rotationOpsMap) {
            if (mVhalToPartsMap.find(rotationOp.first) != mVhalToPartsMap.end()) {
                mVhalToPartsMap.at(rotationOp.first).insert(partId);
            } else {
                mVhalToPartsMap.emplace(
                        std::make_pair(rotationOp.first, std::set<std::string>{partId}));
            }
        }
        for (const auto& translationOp : animationInfo.translationOpsMap) {
            if (mVhalToPartsMap.find(translationOp.first) != mVhalToPartsMap.end()) {
                mVhalToPartsMap.at(translationOp.first).insert(partId);
            } else {
                mVhalToPartsMap.emplace(
                        std::make_pair(translationOp.first, std::set<std::string>{partId}));
            }
        }
        mPartsToAnimationMap.emplace(std::make_pair(partId, animationInfo));
    }
}

void AnimationModule::initCarPartStatus() {
    for (const auto& part : mPartsMap) {
        // Get child parts list from mPartsToAnimationMap.
        std::vector<std::string> childIds;
        if (mPartsToAnimationMap.find(part.first) != mPartsToAnimationMap.end()) {
            childIds = mPartsToAnimationMap.at(part.first).childIds;
        }

        mCarPartsStatusMap.emplace(std::make_pair(part.first,
                                                  CarPartStatus{
                                                          .partId = part.first,
                                                          .childIds = childIds,
                                                          .parentModel = gMat4Identity,
                                                          .localModel = gMat4Identity,
                                                          .currentModel = gMat4Identity,
                                                          .gamma = 1,
                                                  }));
    }

    for (const auto& eachVhalToParts : mVhalToPartsMap) {
        for (const auto& part : eachVhalToParts.second) {
            if (mCarPartsStatusMap.at(part).vhalProgressMap.find(eachVhalToParts.first) !=
                mCarPartsStatusMap.at(part).vhalProgressMap.end()) {
                mCarPartsStatusMap.at(part).vhalProgressMap.at(eachVhalToParts.first) = 0.0f;
            } else {
                mCarPartsStatusMap.at(part).vhalProgressMap.emplace(
                        std::make_pair(eachVhalToParts.first, 0.0f));
            }
            if (mCarPartsStatusMap.at(part).vhalOffMap.find(eachVhalToParts.first) !=
                mCarPartsStatusMap.at(part).vhalOffMap.end()) {
                mCarPartsStatusMap.at(part).vhalOffMap.at(eachVhalToParts.first) = true;
            } else {
                mCarPartsStatusMap.at(part).vhalOffMap.emplace(
                        std::make_pair(eachVhalToParts.first, true));
            }
        }
    }
}

// This implementation assumes the tree level is small. If tree level is large,
// we may need to traverse the tree once and process each node(part) during
// the reaversal.
void AnimationModule::updateChildrenParts(const std::string& partId, const Mat4x4& parentModel) {
    for (auto& childPart : mCarPartsStatusMap.at(partId).childIds) {
        mCarPartsStatusMap.at(childPart).parentModel = parentModel;
        mCarPartsStatusMap.at(childPart).currentModel =
                appendMat(mCarPartsStatusMap.at(childPart).localModel,
                          mCarPartsStatusMap.at(childPart).parentModel);
        if (mUpdatedPartsMap.find(childPart) == mUpdatedPartsMap.end()) {
            AnimationParam animationParam(childPart);
            animationParam.SetModelMatrix(mCarPartsStatusMap.at(childPart).currentModel);
            mUpdatedPartsMap.emplace(std::make_pair(childPart, animationParam));
        } else {  // existing part in the map
            mUpdatedPartsMap.at(childPart).SetModelMatrix(
                    mCarPartsStatusMap.at(childPart).currentModel);
        }
        updateChildrenParts(childPart, mCarPartsStatusMap.at(childPart).currentModel);
    }
}

void AnimationModule::performGammaOp(const std::string& partId, uint64_t vhalProperty,
                                     const GammaOp& gammaOp) {
    CarPartStatus& currentCarPartStatus = mCarPartsStatusMap.at(partId);
    float& currentProgress = currentCarPartStatus.vhalProgressMap.at(vhalProperty);
    if (currentCarPartStatus.vhalOffMap.at(vhalProperty)) {  // process off signal
        if (currentProgress > 0) {                           // part not rest
            if (0 == gammaOp.animationTime) {
                currentCarPartStatus.gamma = gammaOp.gammaRange.start;
                currentProgress = 0.0f;
            } else {
                const float progressDelta =
                        (mCurrentCallTime - mLastCallTime) / gammaOp.animationTime;
                if (progressDelta > currentProgress) {
                    currentCarPartStatus.gamma = gammaOp.gammaRange.start;
                    currentProgress = 0.0f;
                } else {
                    currentCarPartStatus.gamma =
                            getRationalNumber(gammaOp.gammaRange, currentProgress - progressDelta);
                    currentProgress -= progressDelta;
                }
            }
        } else {
            return;
        }
    } else {                               // regular signal process
        if (0 == gammaOp.animationTime) {  // continuous value
            currentCarPartStatus.gamma =
                    getRationalNumber(gammaOp.gammaRange, gammaOp.vhalRange,
                                      mVhalStatusMap.at(vhalProperty).vhalValueFloat);
            currentProgress = mVhalStatusMap.at(vhalProperty).vhalValueFloat;
        } else {  // non-continuous value
            const float progressDelta = (mCurrentCallTime - mLastCallTime) / gammaOp.animationTime;
            if (gammaOp.type == ADJUST_GAMMA_ONCE) {
                if (progressDelta + currentCarPartStatus.vhalProgressMap.at(vhalProperty) > 1) {
                    currentCarPartStatus.gamma = gammaOp.gammaRange.end;
                    currentProgress = 1.0f;
                } else {
                    currentCarPartStatus.gamma =
                            getRationalNumber(gammaOp.gammaRange, currentProgress + progressDelta);
                    currentProgress += progressDelta;
                }
            } else if (gammaOp.type == ADJUST_GAMMA_REPEAT) {
                if (progressDelta + currentCarPartStatus.vhalProgressMap.at(vhalProperty) > 1) {
                    if (progressDelta + currentCarPartStatus.vhalProgressMap.at(vhalProperty) - 1 >
                        1) {
                        currentCarPartStatus.gamma =
                                currentCarPartStatus.vhalProgressMap.at(vhalProperty) > 0.5
                                ? gammaOp.gammaRange.start
                                : gammaOp.gammaRange.end;
                        currentProgress =
                                currentCarPartStatus.vhalProgressMap.at(vhalProperty) > 0.5 ? 0.0f
                                                                                            : 1.0f;
                    } else {
                        currentCarPartStatus.gamma =
                                getRationalNumber(gammaOp.gammaRange,
                                                  progressDelta +
                                                          currentCarPartStatus.vhalProgressMap.at(
                                                                  vhalProperty) -
                                                          1);
                        currentProgress += progressDelta - 1;
                    }
                } else {
                    currentCarPartStatus.gamma =
                            getRationalNumber(gammaOp.gammaRange, currentProgress + progressDelta);
                    currentProgress += progressDelta;
                }
            } else {
                LOG(ERROR) << "Error type of gamma op: " << gammaOp.type;
            }
        }
    }

    if (mUpdatedPartsMap.find(partId) == mUpdatedPartsMap.end()) {
        AnimationParam animationParam(partId);
        animationParam.SetGamma(currentCarPartStatus.gamma);
        mUpdatedPartsMap.emplace(std::make_pair(partId, animationParam));
    } else {  // existing part in the map
        mUpdatedPartsMap.at(partId).SetGamma(currentCarPartStatus.gamma);
    }
}

void AnimationModule::performTranslationOp(const std::string& partId, uint64_t vhalProperty,
                                           const TranslationOp& translationOp) {
    CarPartStatus& currentCarPartStatus = mCarPartsStatusMap.at(partId);
    float& currentProgress = currentCarPartStatus.vhalProgressMap.at(vhalProperty);
    if (currentCarPartStatus.vhalOffMap.at(vhalProperty)) {  // process off signal
        if (currentProgress > 0) {
            // part not rest
            if (0 == translationOp.animationTime) {
                currentCarPartStatus.localModel = gMat4Identity;
                currentCarPartStatus.currentModel = currentCarPartStatus.parentModel;
                currentProgress = 0.0f;
            } else {
                const float progressDelta =
                        (mCurrentCallTime - mLastCallTime) / translationOp.animationTime;
                float translationUnit =
                        getRationalNumber(translationOp.translationRange,
                                          std::max(currentProgress - progressDelta, 0.0f));
                currentCarPartStatus.localModel =
                        translationMatrixToMat4x4(translationOp.direction * translationUnit);
                currentCarPartStatus.currentModel = appendMatrix(currentCarPartStatus.localModel,
                                                                 currentCarPartStatus.parentModel);
                currentProgress = std::max(currentProgress - progressDelta, 0.0f);
            }
        } else {
            return;
        }
    } else {  // regular signal process
        if (translationOp.type == TRANSLATION) {
            if (0 == translationOp.animationTime) {
                float translationUnit =
                        getRationalNumber(translationOp.translationRange, translationOp.vhalRange,
                                          mVhalStatusMap.at(vhalProperty).vhalValueFloat);
                currentCarPartStatus.localModel =
                        translationMatrixToMat4x4(translationOp.direction * translationUnit);
                currentCarPartStatus.currentModel = appendMatrix(currentCarPartStatus.localModel,
                                                                 currentCarPartStatus.parentModel);
                currentProgress = mVhalStatusMap.at(vhalProperty).vhalValueFloat;
            } else {
                float progressDelta =
                        (mCurrentCallTime - mLastCallTime) / translationOp.animationTime;
                if (progressDelta + currentCarPartStatus.vhalProgressMap.at(vhalProperty) > 1) {
                    float translationUnit = translationOp.translationRange.end;

                    currentCarPartStatus.localModel =
                            translationMatrixToMat4x4(translationOp.direction * translationUnit);
                    currentCarPartStatus.currentModel =
                            appendMatrix(currentCarPartStatus.localModel,
                                         currentCarPartStatus.parentModel);
                    currentProgress = 1.0f;
                } else {
                    float translationUnit = getRationalNumber(translationOp.translationRange,
                                                              progressDelta + currentProgress);
                    currentCarPartStatus.localModel =
                            translationMatrixToMat4x4(translationOp.direction * translationUnit);
                    currentCarPartStatus.currentModel =
                            appendMatrix(currentCarPartStatus.localModel,
                                         currentCarPartStatus.parentModel);
                    currentProgress += progressDelta;
                }
            }
        } else {
            LOG(ERROR) << "Error type of translation op: " << translationOp.type;
        }
    }
    if (mUpdatedPartsMap.find(partId) == mUpdatedPartsMap.end()) {
        AnimationParam animationParam(partId);
        animationParam.SetModelMatrix(currentCarPartStatus.currentModel);
        mUpdatedPartsMap.emplace(std::make_pair(partId, animationParam));
    } else {  // existing part in the map
        mUpdatedPartsMap.at(partId).SetModelMatrix(currentCarPartStatus.currentModel);
    }
    updateChildrenParts(partId, currentCarPartStatus.currentModel);
}

void AnimationModule::performRotationOp(const std::string& partId, uint64_t vhalProperty,
                                        const RotationOp& rotationOp) {
    CarPartStatus& currentCarPartStatus = mCarPartsStatusMap.at(partId);
    float& currentProgress = currentCarPartStatus.vhalProgressMap.at(vhalProperty);
    if (currentCarPartStatus.vhalOffMap.at(vhalProperty)) {
        // process off signal
        if (currentProgress > 0) {  // part not rest
            if (0 == rotationOp.animationTime) {
                currentCarPartStatus.localModel = gMat4Identity;
                currentCarPartStatus.currentModel = currentCarPartStatus.parentModel;
                currentProgress = 0.0f;
            } else {
                const float progressDelta =
                        (mCurrentCallTime - mLastCallTime) / rotationOp.animationTime;
                if (progressDelta > currentProgress) {
                    currentCarPartStatus.localModel = gMat4Identity;
                    currentCarPartStatus.currentModel = currentCarPartStatus.parentModel;
                    currentProgress = 0.0f;
                } else {
                    float anlgeInDegree = getRationalNumber(rotationOp.rotationRange,
                                                            currentProgress - progressDelta);
                    currentCarPartStatus.localModel =
                            rotationAboutPoint(anlgeInDegree, rotationOp.axis.rotationPoint,
                                               rotationOp.axis.axisVector);
                    currentCarPartStatus.currentModel =
                            appendMatrix(currentCarPartStatus.localModel,
                                         currentCarPartStatus.parentModel);
                    currentProgress -= progressDelta;
                }
            }
        } else {
            return;
        }
    } else {  // regular signal process
        if (rotationOp.type == ROTATION_ANGLE) {
            if (0 == rotationOp.animationTime) {
                float angleInDegree =
                        getRationalNumber(rotationOp.rotationRange, rotationOp.vhalRange,
                                          mVhalStatusMap.at(vhalProperty).vhalValueFloat);
                currentCarPartStatus.localModel =
                        rotationAboutPoint(angleInDegree, rotationOp.axis.rotationPoint,
                                           rotationOp.axis.axisVector);
                currentCarPartStatus.currentModel = appendMatrix(currentCarPartStatus.localModel,
                                                                 currentCarPartStatus.parentModel);
                currentProgress = mVhalStatusMap.at(vhalProperty).vhalValueFloat;
            } else {
                float progressDelta = (mCurrentCallTime - mLastCallTime) / rotationOp.animationTime;
                if (progressDelta + currentProgress > 1) {
                    float angleInDegree = rotationOp.rotationRange.end;
                    currentCarPartStatus.localModel =
                            rotationAboutPoint(angleInDegree, rotationOp.axis.rotationPoint,
                                               rotationOp.axis.axisVector);
                    currentCarPartStatus.currentModel =
                            appendMatrix(currentCarPartStatus.localModel,
                                         currentCarPartStatus.parentModel);
                    currentProgress = 1.0f;
                } else {
                    float anlgeInDegree = getRationalNumber(rotationOp.rotationRange,
                                                            currentProgress + progressDelta);
                    currentCarPartStatus.localModel =
                            rotationAboutPoint(anlgeInDegree, rotationOp.axis.rotationPoint,
                                               rotationOp.axis.axisVector);
                    currentCarPartStatus.currentModel =
                            appendMatrix(currentCarPartStatus.localModel,
                                         currentCarPartStatus.parentModel);
                    currentProgress += progressDelta;
                }
            }
        } else if (rotationOp.type == ROTATION_SPEED) {
            float angleDelta = (mCurrentCallTime - mLastCallTime) *
                    getRationalNumber(rotationOp.rotationRange, rotationOp.vhalRange,
                                      mVhalStatusMap.at(vhalProperty)
                                              .vhalValueFloat);  // here vhalValueFloat unit is
                                                                 // radian/ms.
            currentCarPartStatus.localModel =
                    appendMat(rotationAboutPoint(angleDelta, rotationOp.axis.rotationPoint,
                                                 rotationOp.axis.axisVector),
                              currentCarPartStatus.localModel);
            currentCarPartStatus.currentModel =
                    appendMatrix(currentCarPartStatus.localModel, currentCarPartStatus.parentModel);
            currentProgress = 1.0f;
        } else {
            LOG(ERROR) << "Error type of rotation op: " << rotationOp.type;
        }
    }
    if (mUpdatedPartsMap.find(partId) == mUpdatedPartsMap.end()) {
        AnimationParam animationParam(partId);
        animationParam.SetModelMatrix(currentCarPartStatus.currentModel);
        mUpdatedPartsMap.emplace(std::make_pair(partId, animationParam));
    } else {  // existing part in the map
        mUpdatedPartsMap.at(partId).SetModelMatrix(currentCarPartStatus.currentModel);
    }
    updateChildrenParts(partId, currentCarPartStatus.currentModel);
}

std::vector<AnimationParam> AnimationModule::getUpdatedAnimationParams(
        const std::vector<VehiclePropValue>& vehiclePropValue) {
    mLastCallTime = mCurrentCallTime;
    if (!mIsCalled) {
        mIsCalled = true;
        mLastCallTime = (float)elapsedRealtimeNano() / 1e6;
    }

    // get current time
    mCurrentCallTime = (float)elapsedRealtimeNano() / 1e6;

    // reset mUpdatedPartsMap
    mUpdatedPartsMap.clear();

    for (const auto& vhalSignal : vehiclePropValue) {
        // existing vhal signal
        const uint64_t combinedId = getCombinedId(vhalSignal);
        if (mVhalToPartsMap.find(combinedId) != mVhalToPartsMap.end()) {
            const float valueFloat = getVhalValueFloat(vhalSignal);
            if (mVhalStatusMap.find(combinedId) != mVhalStatusMap.end()) {
                mVhalStatusMap.at(combinedId).vhalValueFloat = valueFloat;
            } else {
                mVhalStatusMap.emplace(std::make_pair(combinedId,
                                                      VhalStatus{
                                                              .vhalValueFloat = valueFloat,
                                                      }));
            }
            bool offStatus = 0 == valueFloat;
            for (const auto& eachPart : mVhalToPartsMap.at(combinedId)) {
                mCarPartsStatusMap.at(eachPart).vhalOffMap.at(combinedId) = offStatus;
            }
        }
    }

    for (auto& vhalStatus : mVhalStatusMap) {
        // VHAL signal not found in animation
        uint64_t vhalProperty = vhalStatus.first;
        if (mVhalToPartsMap.find(vhalProperty) == mVhalToPartsMap.end()) {
            LOG(WARNING) << "VHAL " << vhalProperty << " not processed.";
        } else {  // VHAL signal found
            const auto& partsSet = mVhalToPartsMap.at(vhalProperty);
            for (const auto& partId : partsSet) {
                const auto& animationInfo = mPartsToAnimationMap.at(partId);
                if (animationInfo.gammaOpsMap.find(vhalProperty) !=
                    animationInfo.gammaOpsMap.end()) {
                    LOG(INFO) << "Processing VHAL " << vhalProperty << " for gamma op.";
                    // TODO(b/158244276): add priority check.
                    for (const auto& gammaOp : animationInfo.gammaOpsMap.at(vhalProperty)) {
                        performGammaOp(partId, vhalProperty, gammaOp);
                    }
                }
                if (animationInfo.textureOpsMap.find(vhalProperty) !=
                    animationInfo.textureOpsMap.end()) {
                    LOG(INFO) << "Processing VHAL " << vhalProperty << " for texture op.";
                    LOG(INFO) << "Texture op currently not supported. Skipped.";
                    // TODO(b158244721): do texture op.
                }
                if (animationInfo.rotationOpsMap.find(vhalProperty) !=
                    animationInfo.rotationOpsMap.end()) {
                    LOG(INFO) << "Processing VHAL " << vhalProperty << " for rotation op.";
                    for (const auto& rotationOp : animationInfo.rotationOpsMap.at(vhalProperty)) {
                        performRotationOp(partId, vhalProperty, rotationOp);
                    }
                }
                if (animationInfo.translationOpsMap.find(vhalProperty) !=
                    animationInfo.translationOpsMap.end()) {
                    LOG(INFO) << "Processing VHAL " << vhalProperty << " for translation op.";
                    for (const auto& translationOp :
                         animationInfo.translationOpsMap.at(vhalProperty)) {
                        performTranslationOp(partId, vhalProperty, translationOp);
                    }
                }
            }
        }
    }

    std::vector<AnimationParam> output;
    for (auto& updatedPart : mUpdatedPartsMap) {
        output.push_back(updatedPart.second);
    }
    return output;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
