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

#ifndef _MISCMANAGER_H
#define _MISCMANAGER_H

#include <pthread.h>

#include <utils/List.h>
#include <sysutils/SocketListener.h>
#include "Misc.h"

class MiscManager {
private:
    static MiscManager *sInstance;

private:
    SocketListener        *mBroadcaster;
    MiscCollection        *mMiscs;
    bool                   mDebug;

public:
    virtual ~MiscManager();

    int start();
    int stop();

    void handleEvent(NetlinkEvent *evt);

    int addMisc(Misc *v);
    
    void setDebug(bool enable);

    void setBroadcaster(SocketListener *sl) { mBroadcaster = sl; }
    SocketListener *getBroadcaster() { return mBroadcaster; }

    static MiscManager *Instance();

private:
    MiscManager();
    Misc *lookupMisc(const char *label);
};
#endif

