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

import android.car.CarAppFocusManager;
import android.car.IAppFocus;
import android.content.Context;
import android.os.Looper;

import com.android.car.AppFocusService;

/**
 * Fake service that is used by {@link FakeCar} to provide an implementation of {@link IAppFocus}
 * to allow the use of {@link CarAppFocusManager} in unit tests.
 */
public class FakeAppFocusService extends AppFocusService implements CarAppFocusController {
    FakeSystemActivityMonitoringService mSystemActivityMonitoringService;

    private FakeAppFocusService(
            Context context,
            FakeSystemActivityMonitoringService fakeSystemActivityMonitoringService) {
        super(context, fakeSystemActivityMonitoringService);
        mSystemActivityMonitoringService = fakeSystemActivityMonitoringService;
    }

    public FakeAppFocusService(Context context) {
        this(context, new FakeSystemActivityMonitoringService(context));
        super.init();
    }

    //************************* CarAppFocusController implementation ******************************/

    @Override
    public synchronized void setForegroundUid(int uid) {
        mSystemActivityMonitoringService.setForegroundUid(uid);
    }

    @Override
    public synchronized void setForegroundPid(int pid) {
        mSystemActivityMonitoringService.setForegroundPid(pid);
    }

    @Override
    public synchronized void resetForegroundUid() {
        mSystemActivityMonitoringService.resetForegroundUid();
    }

    @Override
    public synchronized void resetForegroundPid() {
        mSystemActivityMonitoringService.resetForegroundPid();
    }

    @Override
    public Looper getLooper() {
        return super.getLooper();
    }
}
