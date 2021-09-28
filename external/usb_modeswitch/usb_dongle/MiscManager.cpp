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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define LOG_TAG "USB_DONGLE"

#include <cutils/log.h>

#include <sysutils/NetlinkEvent.h>

#include "MiscManager.h"


MiscManager *MiscManager::sInstance = NULL;

MiscManager *MiscManager::Instance() {
    if (!sInstance)
        sInstance = new MiscManager();
    return sInstance;
}

MiscManager::MiscManager() {
    mDebug = false;
    mMiscs = new MiscCollection();
    mBroadcaster = NULL;
}

MiscManager::~MiscManager() {
    delete mMiscs;
}

void MiscManager::setDebug(bool enable) {
    mDebug = enable;
    MiscCollection::iterator it;
    for (it = mMiscs->begin(); it != mMiscs->end(); ++it) {
        (*it)->setDebug(enable);
    }
}

int MiscManager::start() {
    return 0;
}

int MiscManager::stop() {
    return 0;
}

int MiscManager::addMisc(Misc *m) {
    mMiscs->push_back(m);
    return 0;
}

void MiscManager::handleEvent(NetlinkEvent *evt) {
	const char *subsys = evt->getSubsystem();
    MiscCollection::iterator it;
    bool hit = false;

	SLOGD("%s, %d", subsys, evt->getAction());
	
		
    for (it = mMiscs->begin(); it != mMiscs->end(); ++it) {
	    if (!strcmp(subsys, "usb")) {
			(*it)->handleUsbEvent(evt);
	    } else if (!strcmp(subsys, "scsi_device")) {
	    	(*it)->handleScsiEvent(evt);
	    }
    }
}

