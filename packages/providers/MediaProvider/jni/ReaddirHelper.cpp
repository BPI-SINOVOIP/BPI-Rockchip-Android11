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
#define LOG_TAG "ReaddirHelper"

#include "libfuse_jni/ReaddirHelper.h"
#include <android-base/logging.h>
#include <sys/types.h>

namespace mediaprovider {
namespace fuse {
namespace {

inline bool is_dot_or_dotdot(const char* name) {
    return name && name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

}  // namespace

bool isDirectory(const dirent& entry) {
    if (entry.d_type == DT_DIR) return true;
    return false;
}

void addDirectoryEntriesFromLowerFs(DIR* dirp, bool (*const filter)(const dirent&),
        std::vector<std::shared_ptr<DirectoryEntry>>* directory_entries) {
    while (1) {
        errno = 0;
        const struct dirent* entry = readdir(dirp);
        if (entry == nullptr) {
            if (errno) {
                PLOG(ERROR) << "DEBUG: readdir(): readdir failed with %d" << errno;
                directory_entries->resize(0);
                directory_entries->push_back(std::make_shared<DirectoryEntry>("", errno));
            }
            break;
        }
        // Ignore '.' & '..' to maintain consistency with directory entries
        // returned by MediaProvider.
        if (is_dot_or_dotdot(entry->d_name)) continue;
        if (filter == nullptr || filter(*entry)) {
            directory_entries->push_back(
                    std::make_shared<DirectoryEntry>(entry->d_name, entry->d_type));
        }
    }
}

}  // namespace fuse
}  // namespace mediaprovider
