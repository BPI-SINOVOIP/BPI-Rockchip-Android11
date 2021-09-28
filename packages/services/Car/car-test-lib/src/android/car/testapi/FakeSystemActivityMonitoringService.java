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

package android.car.testapi;

import android.content.Context;

import com.android.car.SystemActivityMonitoringService;

/**
 * This is fake implementation of the SystemActivityMonitoringService which is used by the
 * {@link com.android.car.AppFocusService}. A faked version is needed for offline unit tests so that
 * the foreground app ID can be changed manually.
 *
 * @hide
 */
public class FakeSystemActivityMonitoringService extends SystemActivityMonitoringService {
    private static final int DEFAULT_FOREGROUND_ID = -1;

    private int mForegroundPid = DEFAULT_FOREGROUND_ID;
    private int mForegroundUid = DEFAULT_FOREGROUND_ID;

    FakeSystemActivityMonitoringService(Context context) {
         super(context);
    }

    @Override
    public boolean isInForeground(int pid, int uid) {
        return (mForegroundPid == DEFAULT_FOREGROUND_ID || mForegroundPid == pid)
                && (mForegroundUid == DEFAULT_FOREGROUND_ID || mForegroundUid == uid);
    }

    void setForegroundPid(int pid) {
        mForegroundPid = pid;
    }

    void setForegroundUid(int uid) {
        mForegroundUid = uid;
    }

    void resetForegroundPid() {
        mForegroundPid = DEFAULT_FOREGROUND_ID;
    }

    void resetForegroundUid() {
        mForegroundUid = DEFAULT_FOREGROUND_ID;
    }
}
