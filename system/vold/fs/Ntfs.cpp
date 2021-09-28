/*
* Copyright (C) 2010 The Android Open Source Project
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <linux/kdev_t.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <selinux/selinux.h>

#include <logwrap/logwrap.h>

#include "Utils.h"
#include "Ntfs.h"
#include "VoldUtil.h"

using android::base::StringPrintf;

namespace android {
namespace vold {
namespace ntfs {

static const char* kMkfsPath = "/system/bin/mkntfs";
static const char* kFsckPath = "/system/bin/ntfsfix";
static const char* kMountPath = "/system/bin/ntfs-3g";

bool IsSupported() {
    return access(kMkfsPath, X_OK) == 0 && access(kFsckPath, X_OK) == 0 &&
           access(kMountPath, X_OK) == 0;
}

status_t Check(const std::string& source) {
    int pass = 1;
    int rc = 0;
    do {
        std::vector<std::string> cmd;
        cmd.push_back(kFsckPath);
        cmd.push_back(source);

        // Ntfs devices are currently always untrusted
        rc = ForkExecvp(cmd, nullptr, sFsckUntrustedContext);

        if (rc < 0) {
            LOG(ERROR) << "Filesystem check failed due to logwrap error";
            errno = EIO;
            return -1;
        }

        switch (rc) {
            case 0:
                LOG(INFO) << "Filesystem check completed OK";
                return 0;

            default:
                LOG(ERROR) << "Filesystem check failed (unknown exit code " << rc << ")";
                errno = EIO;
                return -1;
        }
    } while (0);

    return 0;
}

status_t Mount(const std::string& source, const std::string& target, bool ro, bool remount,
               bool executable, int ownerUid, int ownerGid, int permMask, bool createLost) {
    int rc;

    auto mountData =
        android::base::StringPrintf("utf8,uid=%d,gid=%d,fmask=%o,dmask=%o,shortname=mixed,nodev,"
            "nosuid,dirsync,big_writes,noatime,delay_mtime=120%s",
            ownerUid, ownerGid, permMask, permMask, (ro ? ",ro" : " "));

    std::vector<std::string> cmd;
    cmd.push_back(kMountPath);
    cmd.push_back("-o");
    cmd.push_back(mountData);
    cmd.push_back(source);
    cmd.push_back(target);

    rc = ForkExecvp(cmd);    

    if (rc == 0 && createLost) {
        auto lost_path = android::base::StringPrintf("%s/LOST.DIR", target.c_str());
        if (access(lost_path.c_str(), F_OK)) {
            /*
             * Create a LOST.DIR in the root so we have somewhere to put
             * lost cluster chains (fsck_msdos doesn't currently do this)
             */
            if (mkdir(lost_path.c_str(), 0755)) {
                PLOG(ERROR) << "Unable to create LOST.DIR";
            }
        }
    }

    return rc;
}

status_t Format(const std::string& source, unsigned long numSectors) {
    std::vector<std::string> cmd;
    cmd.push_back(kMkfsPath);
    cmd.push_back("-f");
    cmd.push_back("-L");
    cmd.push_back("android");

    if (numSectors) {
        cmd.push_back(StringPrintf("%lu", numSectors));
    }

    cmd.push_back(source);

    int rc = ForkExecvp(cmd);
    if (rc < 0) {
        LOG(ERROR) << "Filesystem format failed due to logwrap error";
        errno = EIO;
        return -1;
    }

    if (rc == 0) {
        LOG(INFO) << "Filesystem formatted OK";
        return 0;
    } else {
        LOG(ERROR) << "Format failed (unknown exit code " << rc << ")";
        errno = EIO;
        return -1;
    }
    return 0;
}

}  // namespace ntfs
}  // namespace vold
}  // namespace android
