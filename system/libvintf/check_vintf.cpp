/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <getopt.h>
#include <sysexits.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <optional>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/result.h>
#include <android-base/strings.h>
#include <hidl/metadata.h>
#include <utils/Errors.h>
#include <vintf/KernelConfigParser.h>
#include <vintf/VintfObject.h>
#include <vintf/parse_string.h>
#include <vintf/parse_xml.h>
#include "utils.h"

namespace android {
namespace vintf {
namespace details {

// fake sysprops
using Properties = std::map<std::string, std::string>;

using Dirmap = std::map<std::string, std::string>;

enum Option : int {
    // Modes
    HELP,
    DUMP_FILE_LIST = 1,
    CHECK_COMPAT,
    CHECK_ONE,

    // Options
    ROOTDIR,
    PROPERTY,
    DIR_MAP,
    KERNEL,
};
// command line arguments
using Args = std::multimap<Option, std::string>;

class HostFileSystem : public details::FileSystemImpl {
   public:
    HostFileSystem(const Dirmap& dirmap, status_t missingError)
        : mDirMap(dirmap), mMissingError(missingError) {}
    status_t fetch(const std::string& path, std::string* fetched,
                   std::string* error) const override {
        auto resolved = resolve(path, error);
        if (resolved.empty()) {
            return mMissingError;
        }
        status_t status = details::FileSystemImpl::fetch(resolved, fetched, error);
        LOG(INFO) << "Fetch '" << resolved << "': " << toString(status);
        return status;
    }
    status_t listFiles(const std::string& path, std::vector<std::string>* out,
                       std::string* error) const override {
        auto resolved = resolve(path, error);
        if (resolved.empty()) {
            return mMissingError;
        }
        status_t status = details::FileSystemImpl::listFiles(resolved, out, error);
        LOG(INFO) << "List '" << resolved << "': " << toString(status);
        return status;
    }

   private:
    static std::string toString(status_t status) {
        return status == OK ? "SUCCESS" : strerror(-status);
    }
    std::string resolve(const std::string& path, std::string* error) const {
        for (auto [prefix, mappedPath] : mDirMap) {
            if (path == prefix) {
                return mappedPath;
            }
            if (android::base::StartsWith(path, prefix + "/")) {
                return mappedPath + "/" + path.substr(prefix.size() + 1);
            }
        }
        if (error) {
            *error = "Cannot resolve path " + path;
        } else {
            LOG(mMissingError == NAME_NOT_FOUND ? INFO : ERROR) << "Cannot resolve path " << path;
        }
        return "";
    }

    Dirmap mDirMap;
    status_t mMissingError;
};

class PresetPropertyFetcher : public PropertyFetcher {
   public:
    std::string getProperty(const std::string& key,
                            const std::string& defaultValue) const override {
        auto it = mProps.find(key);
        if (it == mProps.end()) {
            LOG(INFO) << "Sysprop " << key << " is missing, default to '" << defaultValue << "'";
            return defaultValue;
        }
        LOG(INFO) << "Sysprop " << key << "=" << it->second;
        return it->second;
    }
    uint64_t getUintProperty(const std::string& key, uint64_t defaultValue,
                             uint64_t max) const override {
        uint64_t result;
        std::string value = getProperty(key, "");
        if (!value.empty() && android::base::ParseUint(value, &result, max)) return result;
        return defaultValue;
    }
    bool getBoolProperty(const std::string& key, bool defaultValue) const override {
        std::string value = getProperty(key, "");
        if (value == "1" || value == "true") {
            return true;
        } else if (value == "0" || value == "false") {
            return false;
        }
        return defaultValue;
    }
    void setProperties(const Properties& props) { mProps.insert(props.begin(), props.end()); }

   private:
    std::map<std::string, std::string> mProps;
};

struct StaticRuntimeInfo : public RuntimeInfo {
    KernelVersion kernelVersion;
    std::string kernelConfigFile;

    status_t fetchAllInformation(FetchFlags flags) override {
        if (flags & RuntimeInfo::FetchFlag::CPU_VERSION) {
            mKernel.mVersion = kernelVersion;
            LOG(INFO) << "fetched kernel version " << kernelVersion;
        }
        if (flags & RuntimeInfo::FetchFlag::CONFIG_GZ) {
            std::string content;
            if (!android::base::ReadFileToString(kernelConfigFile, &content)) {
                LOG(ERROR) << "Cannot read " << kernelConfigFile;
                return UNKNOWN_ERROR;
            }
            KernelConfigParser parser;
            auto status = parser.processAndFinish(content);
            if (status != OK) {
                return status;
            }
            mKernel.mConfigs = std::move(parser.configs());
            LOG(INFO) << "read kernel configs from " << kernelConfigFile;
        }
        if (flags & RuntimeInfo::FetchFlag::POLICYVERS) {
            mKernelSepolicyVersion = SIZE_MAX;
        }
        return OK;
    }
};

struct StubRuntimeInfo : public RuntimeInfo {
    status_t fetchAllInformation(FetchFlags) override { return UNKNOWN_ERROR; }
};

struct StaticRuntimeInfoFactory : public ObjectFactory<RuntimeInfo> {
    std::shared_ptr<RuntimeInfo> info;
    StaticRuntimeInfoFactory(std::shared_ptr<RuntimeInfo> i) : info(i) {}
    std::shared_ptr<RuntimeInfo> make_shared() const override {
        if (info) return info;
        return std::make_shared<StubRuntimeInfo>();
    }
};

// helper functions
template <typename T>
std::unique_ptr<T> readObject(FileSystem* fileSystem, const std::string& path,
                              const XmlConverter<T>& converter) {
    std::string xml;
    std::string error;
    status_t err = fileSystem->fetch(path, &xml, &error);
    if (err != OK) {
        LOG(ERROR) << "Cannot read '" << path << "' (" << strerror(-err) << "): " << error;
        return nullptr;
    }
    auto ret = std::make_unique<T>();
    if (!converter(ret.get(), xml, &error)) {
        LOG(ERROR) << "Cannot parse '" << path << "': " << error;
        return nullptr;
    }
    return ret;
}

int checkCompatibilityForFiles(const std::string& manifestPath, const std::string& matrixPath) {
    auto fileSystem = std::make_unique<FileSystemImpl>();
    auto manifest = readObject(fileSystem.get(), manifestPath, gHalManifestConverter);
    auto matrix = readObject(fileSystem.get(), matrixPath, gCompatibilityMatrixConverter);
    if (manifest == nullptr || matrix == nullptr) {
        return -1;
    }

    std::string error;
    if (!manifest->checkCompatibility(*matrix, &error)) {
        LOG(ERROR) << "Incompatible: " << error;
        std::cout << "false" << std::endl;
        return 1;
    }

    std::cout << "true" << std::endl;
    return 0;
}

Args parseArgs(int argc, char** argv) {
    int longOptFlag;
    int optionIndex;
    Args ret;
    std::vector<struct option> longopts{
        // Modes
        {"help", no_argument, &longOptFlag, HELP},
        {"dump-file-list", no_argument, &longOptFlag, DUMP_FILE_LIST},
        {"check-compat", no_argument, &longOptFlag, CHECK_COMPAT},
        {"check-one", no_argument, &longOptFlag, CHECK_ONE},
        // Options
        {"rootdir", required_argument, &longOptFlag, ROOTDIR},
        {"property", required_argument, &longOptFlag, PROPERTY},
        {"dirmap", required_argument, &longOptFlag, DIR_MAP},
        {"kernel", required_argument, &longOptFlag, KERNEL},
        {0, 0, 0, 0}};
    std::map<int, Option> shortopts{
        {'h', HELP}, {'D', PROPERTY}, {'c', CHECK_COMPAT},
    };
    for (;;) {
        int c = getopt_long(argc, argv, "hcD:", longopts.data(), &optionIndex);
        if (c == -1) {
            break;
        }
        std::string argValue = optarg ? optarg : std::string{};
        if (c == 0) {
            ret.emplace(static_cast<Option>(longOptFlag), std::move(argValue));
        } else {
            ret.emplace(shortopts[c], std::move(argValue));
        }
    }
    if (optind < argc) {
        // see non option
        LOG(ERROR) << "unrecognized option `" << argv[optind] << "'";
        return {{HELP, ""}};
    }
    return ret;
}

template <typename T>
std::map<std::string, std::string> splitArgs(const T& args, char split) {
    std::map<std::string, std::string> ret;
    for (const auto& arg : args) {
        auto pos = arg.find(split);
        auto key = arg.substr(0, pos);
        auto value = pos == std::string::npos ? std::string{} : arg.substr(pos + 1);
        ret[key] = value;
    }
    return ret;
}
template <typename T>
Properties getProperties(const T& args) {
    return splitArgs(args, '=');
}

template <typename T>
Dirmap getDirmap(const T& args) {
    return splitArgs(args, ':');
}

template <typename T>
std::shared_ptr<StaticRuntimeInfo> getRuntimeInfo(const T& args) {
    auto ret = std::make_shared<StaticRuntimeInfo>();
    if (std::distance(args.begin(), args.end()) > 1) {
        LOG(ERROR) << "Can't have multiple --kernel options";
        return nullptr;
    }
    auto pair = android::base::Split(*args.begin(), ":");
    if (pair.size() != 2) {
        LOG(ERROR) << "Invalid --kernel";
        return nullptr;
    }
    if (!parse(pair[0], &ret->kernelVersion)) {
        LOG(ERROR) << "Cannot parse " << pair[0] << " as kernel version";
        return nullptr;
    }
    ret->kernelConfigFile = std::move(pair[1]);
    return ret;
}

int usage(const char* me) {
    LOG(ERROR)
        << me << ": check VINTF metadata." << std::endl
        << "    Modes:" << std::endl
        << "        --dump-file-list: Dump a list of directories / files on device" << std::endl
        << "                that is required to be used by --check-compat." << std::endl
        << "        -c, --check-compat: check compatibility for files under the root" << std::endl
        << "                directory specified by --root-dir." << std::endl
        << "        --check-one: check consistency of VINTF metadata for a single partition."
        << std::endl
        << std::endl
        << "    Options:" << std::endl
        << "        --rootdir=<dir>: specify root directory for all metadata. Same as " << std::endl
        << "                --dirmap /:<dir>" << std::endl
        << "        -D, --property <key>=<value>: specify sysprops." << std::endl
        << "        --dirmap </system:/dir/to/system> [--dirmap </vendor:/dir/to/vendor>[...]]"
        << std::endl
        << "                Map partitions to directories. Cannot be specified with --rootdir."
        << "        --kernel <x.y.z:path/to/config>" << std::endl
        << "                Use the given kernel version and config to check. If" << std::endl
        << "                unspecified, kernel requirements are skipped." << std::endl
        << std::endl
        << "        --help: show this message." << std::endl
        << std::endl
        << "    Example:" << std::endl
        << "        # Get the list of required files." << std::endl
        << "        " << me << " --dump-file-list > /tmp/files.txt" << std::endl
        << "        # Pull from ADB, or use your own command to extract files from images"
        << std::endl
        << "        ROOTDIR=/tmp/device/" << std::endl
        << "        cat /tmp/files.txt | xargs -I{} bash -c \"mkdir -p $ROOTDIR`dirname {}` && adb "
           "pull {} $ROOTDIR{}\""
        << std::endl
        << "        # Check compatibility." << std::endl
        << "        " << me << " --check-compat --rootdir=$ROOTDIR \\" << std::endl
        << "            --property ro.product.first_api_level=`adb shell getprop "
           "ro.product.first_api_level` \\"
        << std::endl
        << "            --property ro.boot.product.hardware.sku=`adb shell getprop "
           "ro.boot.product.hardware.sku`";
    return EX_USAGE;
}

// If |result| is already an error, don't do anything. Otherwise, set it to
// an error with |errorCode|. Return reference to Error object for appending
// additional error messages.
android::base::Error& SetErrorCode(std::optional<android::base::Error>* retError,
                                   int errorCode = 0) {
    if (!retError->has_value()) {
        retError->emplace(errorCode);
    } else {
        // Use existing error code.
        // There should already been an error message appended. Add a new line char for
        // additional messages.
        (**retError) << "\n";
    }
    return **retError;
}

// If |other| is an error, add it to |retError|.
template <typename T>
void AddResult(std::optional<android::base::Error>* retError,
               const android::base::Result<T>& other) {
    if (other.ok()) return;
    SetErrorCode(retError, other.error().code()) << other.error();
}

android::base::Result<void> checkAllFiles(const Dirmap& dirmap, const Properties& props,
                                          std::shared_ptr<StaticRuntimeInfo> runtimeInfo) {
    auto hostPropertyFetcher = std::make_unique<PresetPropertyFetcher>();
    hostPropertyFetcher->setProperties(props);

    CheckFlags::Type flags = CheckFlags::DEFAULT;
    if (!runtimeInfo) flags = flags.disableRuntimeInfo();

    auto vintfObject =
        VintfObject::Builder()
            .setFileSystem(std::make_unique<HostFileSystem>(dirmap, UNKNOWN_ERROR))
            .setPropertyFetcher(std::move(hostPropertyFetcher))
            .setRuntimeInfoFactory(std::make_unique<StaticRuntimeInfoFactory>(runtimeInfo))
            .build();

    std::optional<android::base::Error> retError = std::nullopt;

    std::string compatibleError;
    int compatibleResult = vintfObject->checkCompatibility(&compatibleError, flags);
    if (compatibleResult == INCOMPATIBLE) {
        SetErrorCode(&retError) << compatibleError;
    } else if (compatibleResult != COMPATIBLE) {
        SetErrorCode(&retError, -compatibleResult) << compatibleError;
    }

    auto hidlMetadata = HidlInterfaceMetadata::all();

    std::string deprecateError;
    int deprecateResult = vintfObject->checkDeprecation(hidlMetadata, &deprecateError);
    if (deprecateResult == DEPRECATED) {
        SetErrorCode(&retError) << deprecateError;
    } else if (deprecateResult != NO_DEPRECATED_HALS) {
        SetErrorCode(&retError, -deprecateResult) << deprecateError;
    }

    auto hasFcmExt = vintfObject->hasFrameworkCompatibilityMatrixExtensions();
    AddResult(&retError, hasFcmExt);

    auto deviceManifest = vintfObject->getDeviceHalManifest();
    Level targetFcm = Level::UNSPECIFIED;
    if (deviceManifest == nullptr) {
        SetErrorCode(&retError, -NAME_NOT_FOUND) << "No device HAL manifest";
    } else {
        targetFcm = deviceManifest->level();
    }

    if (hasFcmExt.value_or(false) || (targetFcm != Level::UNSPECIFIED && targetFcm >= Level::R)) {
        AddResult(&retError, vintfObject->checkUnusedHals(hidlMetadata));
    } else {
        LOG(INFO) << "Skip checking unused HALs.";
    }

    if (retError.has_value()) {
        return *retError;
    } else {
        return {};
    }
}

int checkDirmaps(const Dirmap& dirmap, const Properties& props) {
    auto hostPropertyFetcher = std::make_unique<PresetPropertyFetcher>();
    hostPropertyFetcher->setProperties(props);
    auto exitCode = EX_OK;
    for (auto&& [prefix, mappedPath] : dirmap) {
        auto vintfObject =
            VintfObject::Builder()
                .setFileSystem(std::make_unique<HostFileSystem>(dirmap, NAME_NOT_FOUND))
                .setPropertyFetcher(std::move(hostPropertyFetcher))
                .setRuntimeInfoFactory(std::make_unique<StaticRuntimeInfoFactory>(nullptr))
                .build();

        if (android::base::StartsWith(prefix, "/system")) {
            LOG(INFO) << "Checking system manifest.";
            auto manifest = vintfObject->getFrameworkHalManifest();
            if (!manifest) {
                LOG(ERROR) << "Cannot fetch system manifest.";
                exitCode = EX_SOFTWARE;
            }
            LOG(INFO) << "Checking system matrix.";
            auto matrix = vintfObject->getFrameworkCompatibilityMatrix();
            if (!matrix) {
                LOG(ERROR) << "Cannot fetch system matrix.";
                exitCode = EX_SOFTWARE;
            }
            continue;
        }

        if (android::base::StartsWith(prefix, "/vendor")) {
            LOG(INFO) << "Checking vendor manifest.";
            auto manifest = vintfObject->getDeviceHalManifest();
            if (!manifest) {
                LOG(ERROR) << "Cannot fetch vendor manifest.";
                exitCode = EX_SOFTWARE;
            }
            LOG(INFO) << "Checking vendor matrix.";
            auto matrix = vintfObject->getDeviceCompatibilityMatrix();
            if (!matrix) {
                LOG(ERROR) << "Cannot fetch vendor matrix.";
                exitCode = EX_SOFTWARE;
            }
            continue;
        }

        LOG(ERROR) << "--check-one does not work with --dirmap " << prefix;
        exitCode = EX_SOFTWARE;
    }
    return exitCode;
}

}  // namespace details
}  // namespace vintf
}  // namespace android

int main(int argc, char** argv) {
    android::base::SetLogger(android::base::StderrLogger);

    using namespace android::vintf;
    using namespace android::vintf::details;
    // legacy usage: check_vintf <manifest.xml> <matrix.xml>
    if (argc == 3 && *argv[1] != '-' && *argv[2] != '-') {
        int ret = checkCompatibilityForFiles(argv[1], argv[2]);
        if (ret >= 0) return ret;
    }

    Args args = parseArgs(argc, argv);

    if (!iterateValues(args, HELP).empty()) {
        return usage(argv[0]);
    }

    if (!iterateValues(args, DUMP_FILE_LIST).empty()) {
        for (const auto& file : dumpFileList()) {
            std::cout << file << std::endl;
        }
        return 0;
    }

    auto dirmap = getDirmap(iterateValues(args, DIR_MAP));
    auto properties = getProperties(iterateValues(args, PROPERTY));

    if (!iterateValues(args, CHECK_ONE).empty()) {
        return checkDirmaps(dirmap, properties);
    }

    auto checkCompat = iterateValues(args, CHECK_COMPAT);
    if (checkCompat.empty()) {
        return usage(argv[0]);
    }

    auto rootdirs = iterateValues(args, ROOTDIR);
    if (!rootdirs.empty()) {
        if (std::distance(rootdirs.begin(), rootdirs.end()) > 1) {
            LOG(ERROR) << "Can't have multiple --rootdir options";
            return usage(argv[0]);
        }
        args.emplace(DIR_MAP, "/:" + *rootdirs.begin());
    }

    std::shared_ptr<StaticRuntimeInfo> runtimeInfo;
    auto kernelArgs = iterateValues(args, KERNEL);
    if (!kernelArgs.empty()) {
        runtimeInfo = getRuntimeInfo(kernelArgs);
        if (runtimeInfo == nullptr) {
            return usage(argv[0]);
        }
    }

    if (dirmap.empty()) {
        LOG(ERROR) << "Missing --rootdir or --dirmap option.";
        return usage(argv[0]);
    }

    auto compat = checkAllFiles(dirmap, properties, runtimeInfo);

    if (compat.ok()) {
        std::cout << "COMPATIBLE" << std::endl;
        return EX_OK;
    }
    if (compat.error().code() == 0) {
        LOG(ERROR) << "files are incompatible: " << compat.error();
        std::cout << "INCOMPATIBLE" << std::endl;
        return EX_DATAERR;
    }
    LOG(ERROR) << strerror(compat.error().code()) << ": " << compat.error();
    return EX_SOFTWARE;
}
