/*
 * Copyright (C) 2019 The Android Open Source Project
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
#include "KernelInfo.h"

#include "parse_string.h"
#include "parse_xml.h"
#include "utils.h"

namespace android {
namespace vintf {

extern XmlConverter<KernelInfo>& gKernelInfoConverter;

using details::mergeField;

const KernelVersion& KernelInfo::version() const {
    return mVersion;
}

const std::map<std::string, std::string>& KernelInfo::configs() const {
    return mConfigs;
}

Level KernelInfo::level() const {
    return mLevel;
}

bool KernelInfo::matchKernelConfigs(const std::vector<KernelConfig>& matrixConfigs,
                                    std::string* error) const {
    for (const KernelConfig& matrixConfig : matrixConfigs) {
        const std::string& key = matrixConfig.first;
        auto it = this->mConfigs.find(key);
        if (it == this->mConfigs.end()) {
            // special case: <value type="tristate">n</value> matches if the config doesn't exist.
            if (matrixConfig.second == KernelConfigTypedValue::gMissingConfig) {
                continue;
            }
            if (error != nullptr) {
                *error = "Missing config " + key;
            }
            return false;
        }
        const std::string& kernelValue = it->second;
        if (!matrixConfig.second.matchValue(kernelValue)) {
            if (error != nullptr) {
                *error = "For config " + key + ", value = " + kernelValue + " but required " +
                         to_string(matrixConfig.second);
            }
            return false;
        }
    }
    return true;
}

bool KernelInfo::matchKernelVersion(const KernelVersion& minLts) const {
    return mVersion.dropMinor() == minLts.dropMinor() && minLts.minorRev <= mVersion.minorRev;
}

std::vector<const MatrixKernel*> KernelInfo::getMatchedKernelRequirements(
    const std::vector<MatrixKernel>& kernels, Level kernelLevel, std::string* error) const {
    std::map<Level, std::vector<const MatrixKernel*>> kernelsForLevel;
    for (const MatrixKernel& matrixKernel : kernels) {
        const auto& minLts = matrixKernel.minLts();
        auto matrixKernelLevel = matrixKernel.getSourceMatrixLevel();

        // Filter out kernels with different x.y.
        if (mVersion.dropMinor() != minLts.dropMinor()) {
            continue;
        }

        // Check matrix kernel level

        // Use legacy behavior when kernel FCM version is not specified. Blindly add all of them
        // here. The correct one (with smallest matrixKernelLevel) will be picked later.
        if (kernelLevel == Level::UNSPECIFIED) {
            kernelsForLevel[matrixKernelLevel].push_back(&matrixKernel);
            continue;
        }

        if (matrixKernelLevel == Level::UNSPECIFIED) {
            if (error) {
                *error = "Seen unspecified source matrix level; this should not happen.";
            }
            return {};
        }

        if (matrixKernelLevel < kernelLevel) {
            continue;
        }

        // matrix level >= kernel level
        kernelsForLevel[matrixKernelLevel].push_back(&matrixKernel);
    }

    if (kernelsForLevel.empty()) {
        if (error) {
            std::stringstream ss;
            ss << "No kernel entry found for kernel version " << mVersion.dropMinor()
               << " at kernel FCM version "
               << (kernelLevel == Level::UNSPECIFIED ? "unspecified" : to_string(kernelLevel))
               << ". The following kernel requirements are checked:";
            for (const MatrixKernel& matrixKernel : kernels) {
                ss << "\n  Minimum LTS: " << matrixKernel.minLts()
                   << ", kernel FCM version: " << matrixKernel.getSourceMatrixLevel()
                   << (matrixKernel.conditions().empty() ? "" : ", with conditionals");
            };
            *error = ss.str();
        }
        return {};
    }

    // At this point, kernelsForLevel contains kernel requirements for each level.
    // For example, if the running kernel version is 4.14.y then kernelsForLevel contains
    // 4.14-p, 4.14-q, 4.14-r.
    // (This excludes kernels < kernel FCM version, or device FCM version if kernel FCM version is
    // empty. For example, if device level = Q and kernel level is unspecified, this list only
    // contains 4.14-q and 4.14-r).

    // Use legacy behavior when kernel FCM version is not specified. e.g. target FCM version 3 (P)
    // matches kernel 4.4-p, 4.9-p, 4.14-p, 4.19-q, etc., but not 4.9-q or 4.14-q.
    // Since we already filtered |kernels| based on kernel version, we only need to check the first
    // item in kernelsForLevel.
    // Note that this excludes *-r and above kernels. Devices with target FCM version >= 5 (R) must
    // state kernel FCM version explicitly in the device manifest. The value is automatically
    // inserted for devices with target FCM version >= 5 when manifest is built with assemble_vintf.
    if (kernelLevel == Level::UNSPECIFIED) {
        auto [matrixKernelLevel, matrixKernels] = *kernelsForLevel.begin();

        // Do not allow *-r and above kernels.
        if (matrixKernelLevel != Level::UNSPECIFIED && matrixKernelLevel >= Level::R) {
            if (error) {
                KernelInfo msg;
                msg.mLevel = Level::R;
                *error = "Kernel FCM version is not specified, but kernel version " +
                         to_string(mVersion) +
                         " is found. Fix by specifying kernel FCM version in device manifest. "
                         "For example, for a *-r kernel:\n" +
                         gKernelInfoConverter(msg);
            }
            return {};
        }

        auto matchedMatrixKernels = getMatchedKernelVersionAndConfigs(matrixKernels, error);
        if (matchedMatrixKernels.empty()) {
            return {};
        }
        return matchedMatrixKernels;
    }

    // Use new behavior when kernel FCM version is specified. e.g. kernel FCM version 3 (P)
    // matches kernel 4.4-p, 4.9-p, 4.14-p, 4.9-q, 4.14-q, 4.14-r etc., but not 5.4-r.
    // Note we already filtered |kernels| based on kernel version.
    auto [firstMatrixKernelLevel, firstMatrixKernels] = *kernelsForLevel.begin();
    if (firstMatrixKernelLevel == Level::UNSPECIFIED || firstMatrixKernelLevel > kernelLevel) {
        if (error) {
            *error = "Kernel FCM Version is " + to_string(kernelLevel) + " and kernel version is " +
                     to_string(mVersion) +
                     ", but the first kernel FCM version allowed for kernel version " +
                     to_string(mVersion.dropMinor()) + ".y is " + to_string(firstMatrixKernelLevel);
        }
        return {};
    }
    for (auto [matrixKernelLevel, matrixKernels] : kernelsForLevel) {
        if (matrixKernelLevel == Level::UNSPECIFIED || matrixKernelLevel < kernelLevel) {
            continue;
        }
        std::string errorForLevel;
        auto matchedMatrixKernels =
            getMatchedKernelVersionAndConfigs(matrixKernels, &errorForLevel);
        if (matchedMatrixKernels.empty()) {
            if (error) {
                *error += "For kernel requirements at matrix level " +
                          to_string(matrixKernelLevel) + ", " + errorForLevel + "\n";
            }
            continue;
        }
        return matchedMatrixKernels;
    }

    if (error) {
        error->insert(0, "No compatible kernel requirement found (kernel FCM version = " +
                             to_string(kernelLevel) + ").\n");
    }
    return {};
}

std::vector<const MatrixKernel*> KernelInfo::getMatchedKernelVersionAndConfigs(
    const std::vector<const MatrixKernel*>& kernels, std::string* error) const {
    std::vector<const MatrixKernel*> result;
    bool foundMatchedKernelVersion = false;
    for (const MatrixKernel* matrixKernel : kernels) {
        if (!matchKernelVersion(matrixKernel->minLts())) {
            continue;
        }
        foundMatchedKernelVersion = true;
        // ignore this fragment if not all conditions are met.
        if (!matchKernelConfigs(matrixKernel->conditions(), error)) {
            continue;
        }
        if (!matchKernelConfigs(matrixKernel->configs(), error)) {
            return {};
        }
        result.push_back(matrixKernel);
    }
    if (!foundMatchedKernelVersion) {
        if (error != nullptr) {
            std::stringstream ss;
            ss << "Framework is incompatible with kernel version " << version()
               << ", compatible kernel versions are:";
            for (const MatrixKernel* matrixKernel : kernels) {
                ss << "\n  Minimum LTS: " << matrixKernel->minLts()
                   << ", kernel FCM version: " << matrixKernel->getSourceMatrixLevel()
                   << (matrixKernel->conditions().empty() ? "" : ", with conditionals");
            };
            *error = ss.str();
        }
        return {};
    }
    if (result.empty()) {
        // This means matchKernelVersion passes but all matchKernelConfigs(conditions) fails.
        // This should not happen because first <conditions> for each <kernel> must be
        // empty. Reject here for inconsistency.
        if (error != nullptr) {
            error->insert(0, "Framework matches kernel version with unmet conditions.");
        }
        return {};
    }
    if (error != nullptr) {
        error->clear();
    }
    return result;
}

bool KernelInfo::operator==(const KernelInfo& other) const {
    return mVersion == other.mVersion && mConfigs == other.mConfigs;
}

bool KernelInfo::merge(KernelInfo* other, std::string* error) {
    if (!mergeField(&mVersion, &other->mVersion)) {
        if (error) {
            *error = "Conflicting kernel version: " + to_string(version()) + " vs. " +
                     to_string(other->version());
        }
        return false;
    }

    // Do not allow merging configs. One of them must be empty.
    if (!mergeField(&mConfigs, &other->mConfigs)) {
        if (error) {
            *error = "Found <kernel><config> items in two manifests.";
        }
        return false;
    }

    if (!mergeField(&mLevel, &other->mLevel, Level::UNSPECIFIED)) {
        if (error) {
            *error = "Conflicting kernel level: " + to_string(level()) + " vs. " +
                     to_string(other->level());
        }
        return false;
    }
    return true;
}

}  // namespace vintf
}  // namespace android
