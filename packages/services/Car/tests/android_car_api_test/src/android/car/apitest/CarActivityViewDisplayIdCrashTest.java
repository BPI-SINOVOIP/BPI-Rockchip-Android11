/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.car.apitest;

import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.Display.INVALID_DISPLAY;

import static com.google.common.truth.Truth.assertThat;

import android.app.ActivityManager;
import android.content.Intent;
import android.os.Process;
import android.os.SystemClock;

import androidx.test.filters.MediumTest;

import org.junit.Test;

import java.util.List;

/**
 * The test contained in this class kills a test activity, making the system unstable for a while.
 * That said, this class must have only one test method.
 */
@MediumTest
public final class CarActivityViewDisplayIdCrashTest extends CarActivityViewDisplayIdTestBase {

    private static final int INVALID_PID = -1;

    @Test
    public void testCleanUpAfterClientIsCrashed() throws Exception {
        Intent intent = new Intent(getContext(),
                CarActivityViewDisplayIdTest.MultiProcessActivityViewTestActivity.class);
        getContext().startActivity(intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        int pidOfTestActivity = waitForTestActivityReady();
        int displayIdOfTestActivity = waitForActivityViewDisplayReady(ACTIVITY_VIEW_TEST_PKG_NAME);

        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(displayIdOfTestActivity))
            .isEqualTo(DEFAULT_DISPLAY);

        Process.killProcess(pidOfTestActivity);

        assertThat(waitForMappedPhysicalDisplayOfVirtualDisplayCleared(displayIdOfTestActivity))
            .isEqualTo(INVALID_DISPLAY);
    }

    private int waitForMappedPhysicalDisplayOfVirtualDisplayCleared(int displayId) {
        // Initialized with a random number which is not DEFAULT_DISPLAY nor INVALID_DISPLAY.
        int physicalDisplayId = 999;
        for (int i = 0; i < TEST_TIMEOUT_MS / TEST_POLL_MS; ++i) {
            physicalDisplayId = getMappedPhysicalDisplayOfVirtualDisplay(displayId);
            if (physicalDisplayId == INVALID_DISPLAY) {
                return physicalDisplayId;
            }
            SystemClock.sleep(TEST_POLL_MS);
        }
        return physicalDisplayId;
    }

    private int waitForTestActivityReady() {
        for (int i = 0; i < TEST_TIMEOUT_MS / TEST_POLL_MS; ++i) {
            List<ActivityManager.RunningAppProcessInfo> appProcesses =
                    mActivityManager.getRunningAppProcesses();
            for (ActivityManager.RunningAppProcessInfo info : appProcesses) {
                if (info.processName.equals(ACTIVITY_VIEW_TEST_PROCESS_NAME) && info.importance
                        == ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND) {
                    return info.pid;
                }
            }
            SystemClock.sleep(TEST_POLL_MS);
        }
        return INVALID_PID;
    }
}
