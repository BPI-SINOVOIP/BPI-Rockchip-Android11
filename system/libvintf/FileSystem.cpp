
/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <vintf/FileSystem.h>

#include <dirent.h>

#include <android-base/file.h>

namespace android {
namespace vintf {
namespace details {

status_t FileSystemImpl::fetch(const std::string& path, std::string* fetched,
                               std::string* error) const {
    if (!android::base::ReadFileToString(path, fetched, true /* follow_symlinks */)) {
        int saved_errno = errno;
        if (error) {
            *error = "Cannot read " + path + ": " + strerror(saved_errno);
        }
        return saved_errno == 0 ? UNKNOWN_ERROR : -saved_errno;
    }
    return OK;
}

status_t FileSystemImpl::listFiles(const std::string& path, std::vector<std::string>* out,
                                   std::string* error) const {
    std::unique_ptr<DIR, decltype(&closedir)> dir(opendir(path.c_str()), closedir);
    if (!dir) {
        int saved_errno = errno;
        if (error) {
            *error = "Cannot open " + path + ": " + strerror(saved_errno);
        }
        return saved_errno == 0 ? UNKNOWN_ERROR : -saved_errno;
    }

    dirent* dp;
    while (errno = 0, dp = readdir(dir.get()), dp != nullptr) {
        if (dp->d_type != DT_DIR) {
            out->push_back(dp->d_name);
        }
    }
    int saved_errno = errno;
    if (saved_errno != 0) {
        if (error) {
            *error = "Failed while reading directory " + path + ": " + strerror(saved_errno);
        }
    }
    return -saved_errno;
}

status_t FileSystemNoOp::fetch(const std::string&, std::string*, std::string*) const {
    return NAME_NOT_FOUND;
}

status_t FileSystemNoOp::listFiles(const std::string&, std::vector<std::string>*,
                                   std::string*) const {
    return NAME_NOT_FOUND;
}

FileSystemUnderPath::FileSystemUnderPath(const std::string& rootdir) {
    mRootDir = rootdir;
    if (!mRootDir.empty() && mRootDir.back() != '/') {
        mRootDir.push_back('/');
    }
}

status_t FileSystemUnderPath::fetch(const std::string& path, std::string* fetched,
                                    std::string* error) const {
    return mImpl.fetch(mRootDir + path, fetched, error);
}

status_t FileSystemUnderPath::listFiles(const std::string& path, std::vector<std::string>* out,
                                        std::string* error) const {
    return mImpl.listFiles(mRootDir + path, out, error);
}

const std::string& FileSystemUnderPath::getRootDir() const {
    return mRootDir;
}

}  // namespace details
}  // namespace vintf
}  // namespace android
