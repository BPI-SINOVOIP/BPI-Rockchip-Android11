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
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <tinyxml2.h>

#include <system/camera_metadata.h>
#include <android/hardware/automotive/evs/1.1/types.h>
#include <android-base/logging.h>

#include "ConfigManagerUtil.h"

using namespace std;
using namespace tinyxml2;

using ::android::hardware::hidl_vec;
using ::android::hardware::camera::device::V3_2::Stream;
using ::android::hardware::automotive::evs::V1_1::CameraParam;

/*
 * Plese note that this is different from what is defined in
 * libhardware/modules/camera/3_4/metadata/types.h; this has one additional
 * field to store a framerate.
 */
const size_t kStreamCfgSz = 6;
typedef std::array<int32_t, kStreamCfgSz> RawStreamConfiguration;

class ConfigManager {
public:
    static std::unique_ptr<ConfigManager> Create();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    virtual ~ConfigManager();

    /* Camera device's capabilities and metadata */
    class CameraInfo {
    public:
        CameraInfo() :
            characteristics(nullptr) {
            /* Nothing to do */
        }

        virtual ~CameraInfo();

        /* Allocate memory for camera_metadata_t */
        bool allocate(size_t entry_cap, size_t data_cap) {
            if (characteristics != nullptr) {
                LOG(ERROR) << "Camera metadata is already allocated";
                return false;
            }

            characteristics = allocate_camera_metadata(entry_cap, data_cap);
            return characteristics != nullptr;
        }

        /*
         * List of supported controls that the master client can program.
         * Paraemters are stored with its valid range
         */
        unordered_map<CameraParam,
                      tuple<int32_t, int32_t, int32_t>> controls;

        /*
         * List of supported output stream configurations; each array stores
         * format, width, height, and direction values in the order.
         */
        unordered_map<int32_t, RawStreamConfiguration> streamConfigurations;

        /*
         * Internal storage for camera metadata.  Each entry holds a pointer to
         * data and number of elements
         */
        unordered_map<camera_metadata_tag_t,
                      pair<void *, size_t>> cameraMetadata;

        /* Camera module characteristics */
        camera_metadata_t *characteristics;
    };

    class CameraGroupInfo : public CameraInfo {
    public:
        CameraGroupInfo() {}

        /* ID of member camera devices */
        unordered_set<string> devices;

        /* The capture operation of member camera devices are synchronized */
        int32_t synchronized = 0;
    };

    class SystemInfo {
    public:
        /* number of available cameras */
        int32_t numCameras = 0;
    };

    class DisplayInfo {
    public:
        /*
         * List of supported input stream configurations; each array stores
         * format, width, height, and direction values in the order.
         */
        unordered_map<int32_t, RawStreamConfiguration> streamConfigurations;
    };

    /*
     * Return system information
     *
     * @return SystemInfo
     *         Constant reference of SystemInfo.
     */
    const SystemInfo &getSystemInfo() {
        unique_lock<mutex> lock(mConfigLock);
        mConfigCond.wait(lock, [this] { return mIsReady; });
        return mSystemInfo;
    }

    /*
     * Return a list of camera identifiers
     *
     * This function assumes that it is not being called frequently.
     *
     * @return vector<string>
     *         A vector that contains unique camera device identifiers.
     */
    vector<string> getCameraIdList() {
        unique_lock<mutex> lock(mConfigLock);
        mConfigCond.wait(lock, [this] { return mIsReady; });

        vector<string> aList;
        for (auto&& v : mCameraInfo) {
            aList.emplace_back(v.first);
        }

        return aList;
    }

    /*
     * Return a list of camera group identifiers
     *
     * This function assumes that it is not being called frequently.
     *
     * @return vector<string>
     *         A vector that contains unique camera device identifiers.
     */
    vector<string> getCameraGroupIdList() {
        unique_lock<mutex> lock(mConfigLock);
        mConfigCond.wait(lock, [this] { return mIsReady; });

        vector<string> aList;
        for (auto&& v : mCameraGroups) {
            aList.emplace_back(v.first);
        }

        return aList;
    }

    /*
     * Return a pointer to the camera group
     *
     * @return CameraGroup
     *         A pointer to a camera group identified by a given id.
     */
    unique_ptr<CameraGroupInfo>& getCameraGroupInfo(const string& gid) {
        unique_lock<mutex> lock(mConfigLock);
        mConfigCond.wait(lock, [this] { return mIsReady; });

        return mCameraGroups[gid];
    }

    /*
     * Return a camera metadata
     *
     * @param  cameraId
     *         Unique camera node identifier in string
     *
     * @return unique_ptr<CameraInfo>
     *         A pointer to CameraInfo that is associated with a given camera
     *         ID.  This returns a null pointer if this does not recognize a
     *         given camera identifier.
     */
    unique_ptr<CameraInfo>& getCameraInfo(const string cameraId) noexcept {
        unique_lock<mutex> lock(mConfigLock);
        mConfigCond.wait(lock, [this] { return mIsReady; });

        return mCameraInfo[cameraId];
    }

    /*
     * Tell whether the configuration data is ready to be used
     *
     * @return bool
     *         True if configuration data is ready to be consumed.
     */
    bool isReady() const {
        return mIsReady;
    }

private:
    /* Constructors */
    ConfigManager() :
        mBinaryFilePath("") {
    }

    static const char* CONFIG_DEFAULT_PATH;
    static const char* CONFIG_OVERRIDE_PATH;

    /* System configuration */
    SystemInfo mSystemInfo;

    /* Internal data structure for camera device information */
    unordered_map<string, unique_ptr<CameraInfo>> mCameraInfo;

    /* Internal data structure for camera device information */
    unordered_map<string, unique_ptr<DisplayInfo>> mDisplayInfo;

    /* Camera groups are stored in <groud id, CameraGroup> hash map */
    unordered_map<string, unique_ptr<CameraGroupInfo>> mCameraGroups;

    /*
     * Camera positions are stored in <position, camera id set> hash map.
     * The position must be one of front, rear, left, and right.
     */
    unordered_map<string, unordered_set<string>>  mCameraPosition;

    /* Configuration data lock */
    mutex mConfigLock;

    /*
     * This condition is signalled when it completes a configuration data
     * preparation.
     */
    condition_variable mConfigCond;

    /* A path to a binary configuration file */
    const char *mBinaryFilePath;

    /* Configuration data readiness */
    bool mIsReady = false;

    /*
     * Parse a given EVS configuration file and store the information
     * internally.
     *
     * @return bool
     *         True if it completes parsing a file successfully.
     */
    bool readConfigDataFromXML() noexcept;

    /*
     * read the information of the vehicle
     *
     * @param  aSysElem
     *         A pointer to "system" XML element.
     */
    void readSystemInfo(const XMLElement * const aSysElem);

    /*
     * read the information of camera devices
     *
     * @param  aCameraElem
     *         A pointer to "camera" XML element that may contain multiple
     *         "device" elements.
     */
    void readCameraInfo(const XMLElement * const aCameraElem);

    /*
     * read display device information
     *
     * @param  aDisplayElem
     *         A pointer to "display" XML element that may contain multiple
     *         "device" elements.
     */
    void readDisplayInfo(const XMLElement * const aDisplayElem);

    /*
     * read camera device information
     *
     * @param  aCamera
     *         A pointer to CameraInfo that will be completed by this
     *         method.
     *         aDeviceElem
     *         A pointer to "device" XML element that contains camera module
     *         capability info and its characteristics.
     *
     * @return bool
     *         Return false upon any failure in reading and processing camera
     *         device information.
     */
    bool readCameraDeviceInfo(CameraInfo *aCamera,
                              const XMLElement *aDeviceElem);

    /*
     * read camera metadata
     *
     * @param  aCapElem
     *         A pointer to "cap" XML element.
     * @param  aCamera
     *         A pointer to CameraInfo that is being filled by this method.
     * @param  dataSize
     *         Required size of memory to store camera metadata found in this
     *         method.  This is calculated in this method and returned to the
     *         caller for camera_metadata allocation.
     *
     * @return size_t
     *         Number of camera metadata entries
     */
    size_t readCameraCapabilities(const XMLElement * const aCapElem,
                                  CameraInfo *aCamera,
                                  size_t &dataSize);

    /*
     * read camera metadata
     *
     * @param  aParamElem
     *         A pointer to "characteristics" XML element.
     * @param  aCamera
     *         A pointer to CameraInfo that is being filled by this method.
     * @param  dataSize
     *         Required size of memory to store camera metadata found in this
     *         method.
     *
     * @return size_t
     *         Number of camera metadata entries
     */
    size_t readCameraMetadata(const XMLElement * const aParamElem,
                              CameraInfo *aCamera,
                              size_t &dataSize);

    /*
     * construct camera_metadata_t from camera capabilities and metadata
     *
     * @param  aCamera
     *         A pointer to CameraInfo that is being filled by this method.
     * @param  totalEntries
     *         Number of camera metadata entries to be added.
     * @param  totalDataSize
     *         Sum of sizes of camera metadata entries to be added.
     *
     * @return bool
     *         False if either it fails to allocate memory for camera metadata
     *         or its size is not large enough to add all found camera metadata
     *         entries.
     */
    bool constructCameraMetadata(CameraInfo *aCamera,
                                 const size_t totalEntries,
                                 const size_t totalDataSize);

    /*
     * Read configuration data from the binary file
     *
     * @return bool
     *         True if it succeeds to read configuration data from a binary
     *         file.
     */
    bool readConfigDataFromBinary();

    /*
     * Store configuration data to the file
     *
     * @return bool
     *         True if it succeeds to serialize mCameraInfo to the file.
     */
    bool writeConfigDataToBinary();

    /*
     * debugging method to print out all XML elements and their attributes in
     * logcat message.
     *
     * @param  aNode
     *         A pointer to the root XML element to navigate.
     * @param  prefix
     *         A prefix to XML string.
     */
    void printElementNames(const XMLElement *aNode, string prefix = "") const;
};
#endif // CONFIG_MANAGER_H

