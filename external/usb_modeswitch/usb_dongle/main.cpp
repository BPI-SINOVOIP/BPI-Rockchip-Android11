/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "NetlinkManager.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <cutils/klog.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>
#include <fcntl.h>
#include <dirent.h>

#ifdef USE_USB_MODE_SWITCH
#include "G3Dev.h"
#include  "MiscManager.h"
#endif

#define LOG_TAG "USB_DONGLE"

static void coldboot(const char *path);

struct fstab *fstab;

struct selabel_handle *sehandle;

using android::base::StringPrintf;

int main(int argc, char** argv) {
    setenv("ANDROID_LOG_TAGS", "*:v", 1);
    android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));

    LOG(INFO) << "USB_MODE_SWITCH";

    NetlinkManager *nm;


    klog_set_level(6);

    if (!(nm = NetlinkManager::Instance())) {
        LOG(ERROR) << "Unable to create NetlinkManager";
        exit(1);
    }

    if (nm->start()) {
        PLOG(ERROR) << "Unable to start NetlinkManager";
        exit(1);
    }

#ifdef USE_USB_MODE_SWITCH
	MiscManager *mm;
	if (!(mm = MiscManager::Instance())) {
		PLOG(ERROR) << "Unable to create MiscManager";
		exit(1);
	};
	//mm->setBroadcaster((SocketListener *) cl);
	if (mm->start()) {
		LOG(ERROR) << "Unable to start MiscManager";
		exit(1);
	}
	G3Dev* g3 = new G3Dev(mm);
	g3->handleUsb();
	mm->addMisc(g3);
#endif
    // Do coldboot here so it won't block booting,
    // also the cold boot is needed in case we have flash drive
    coldboot("/sys/block");
    // Eventually we'll become the monitoring thread
    while(1) {
        pause();
    }

    LOG(ERROR) << "USB_MODE_SWITCH exiting";
    exit(0);
}

static void do_coldboot(DIR *d, int lvl) {
    struct dirent *de;
    int dfd, fd;

    dfd = dirfd(d);

    fd = openat(dfd, "uevent", O_WRONLY | O_CLOEXEC);
    if(fd >= 0) {
        write(fd, "add\n", 4);
        close(fd);
    }

    while((de = readdir(d))) {
        DIR *d2;

        if (de->d_name[0] == '.')
            continue;

        if (de->d_type != DT_DIR && lvl > 0)
            continue;

        fd = openat(dfd, de->d_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if(fd < 0)
            continue;

        d2 = fdopendir(fd);
        if(d2 == 0)
            close(fd);
        else {
            do_coldboot(d2, lvl + 1);
            closedir(d2);
        }
    }
}

static void coldboot(const char *path) {
    DIR *d = opendir(path);
    if(d) {
        do_coldboot(d, 0);
        closedir(d);
    }
}
