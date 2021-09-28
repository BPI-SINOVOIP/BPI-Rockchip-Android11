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

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <android/util/ProtoOutputStream.h>
#include <utils/Log.h>
#include <utils/RefBase.h>

#include "packages/UidMap.h"

namespace android {
namespace os {
namespace statsd {

using android::util::ProtoOutputStream;

class StorageManager : public virtual RefBase {
public:
    struct FileInfo {
        FileInfo(std::string name, bool isHistory, int fileSize, long fileAge)
            : mFileName(name),
              mIsHistory(isHistory),
              mFileSizeBytes(fileSize),
              mFileAgeSec(fileAge) {
        }
        std::string mFileName;
        bool mIsHistory;
        int mFileSizeBytes;
        long mFileAgeSec;
    };

    /**
     * Writes a given byte array as a file to the specified file path.
     */
    static void writeFile(const char* file, const void* buffer, int numBytes);

    /**
     * Writes train info.
     */
    static bool writeTrainInfo(const InstallTrainInfo& trainInfo);

    /**
     * Reads train info.
     */
    static bool readTrainInfo(const std::string& trainName, InstallTrainInfo& trainInfo);

    /**
     * Reads train info assuming lock is obtained.
     */
    static bool readTrainInfoLocked(const std::string& trainName, InstallTrainInfo& trainInfo);

    /**
     * Reads all train info and returns a vector of train info.
     */
    static vector<InstallTrainInfo> readAllTrainInfo();

    /**
     * Reads the file content to the buffer.
     */
    static bool readFileToString(const char* file, string* content);

    /**
     * Deletes a single file given a file name.
     */
    static void deleteFile(const char* file);

    /**
     * Deletes all files in a given directory.
     */
    static void deleteAllFiles(const char* path);

    /**
     * Deletes all files whose name matches with a provided suffix.
     */
    static void deleteSuffixedFiles(const char* path, const char* suffix);

    /**
     * Send broadcasts to relevant receiver for each data stored on disk.
     */
    static void sendBroadcast(const char* path,
                              const std::function<void(const ConfigKey&)>& sendBroadcast);

    /**
     * Returns true if there's at least one report on disk.
     */
    static bool hasConfigMetricsReport(const ConfigKey& key);

    /**
     * Appends the ConfigMetricsReport found on disk to the specifid proto
     * and, if erase_data, deletes it from disk.
     *
     * [isAdb]: if the caller is adb dump. This includes local adb dump or dumpsys by
     * bugreport or incidentd. When true, we will append any local history data too.
     *
     * When
     * erase_data=true, isAdb=true:   append history data to output, remove all data after read
     * erase_data=false, isAdb=true:  append history data to output, keep data after read
     * erase_data=true, isAdb=false:  do not append history data, and remove data after read
     * erase_data=false, isAdb=false: do not append history data and *rename* all data files to
     *                                history files.
     */
    static void appendConfigMetricsReport(const ConfigKey& key, ProtoOutputStream* proto,
                                          bool erase_data, bool isAdb);

    /**
     * Call to load the saved configs from disk.
     */
    static void readConfigFromDisk(std::map<ConfigKey, StatsdConfig>& configsMap);

    /**
     * Call to load the specified config from disk. Returns false if the config file does not
     * exist or error occurs when reading the file.
     */
    static bool readConfigFromDisk(const ConfigKey& key, StatsdConfig* config);
    static bool readConfigFromDisk(const ConfigKey& key, string* config);

    /**
     * Trims files in the provided directory to limit the total size, number of
     * files, accumulation of outdated files.
     */
    static void trimToFit(const char* dir, bool parseTimestampOnly = false);

    /**
     * Returns true if there already exists identical configuration on device.
     */
    static bool hasIdenticalConfig(const ConfigKey& key,
                                   const vector<uint8_t>& config);

    /**
     * Prints disk usage statistics related to statsd.
     */
    static void printStats(int out);

    static string getDataFileName(long wallClockSec, int uid, int64_t id);

    static string getDataHistoryFileName(long wallClockSec, int uid, int64_t id);

    static void sortFiles(vector<FileInfo>* fileNames);

private:
    /**
     * Prints disk usage statistics about a directory related to statsd.
     */
    static void printDirStats(int out, const char* path);

    static std::mutex sTrainInfoMutex;
};

}  // namespace statsd
}  // namespace os
}  // namespace android

#endif  // STORAGE_MANAGER_H
