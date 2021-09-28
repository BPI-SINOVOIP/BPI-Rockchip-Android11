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

package android.car.apitest;

import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.Display.INVALID_DISPLAY;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;
import static org.testng.Assert.assertThrows;

import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Build/Install/Run:
 *  atest AndroidCarApiTest:CarActivityViewDisplayIdTest
 */
@RunWith(JUnit4.class)
@MediumTest
public class CarActivityViewDisplayIdTest extends CarActivityViewDisplayIdTestBase {
    private static final String CAR_LAUNCHER_PKG_NAME = "com.android.car.carlauncher";
    private static final int NONEXISTENT_DISPLAY_ID = Integer.MAX_VALUE;

    @Test
    public void testSingleActivityView() throws Exception {
        ActivityViewTestActivity activity = startActivityViewTestActivity(DEFAULT_DISPLAY);
        activity.waitForActivityViewReady();
        int virtualDisplayId = activity.getActivityView().getVirtualDisplayId();

        startTestActivity(ActivityInActivityView.class, virtualDisplayId);

        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(virtualDisplayId))
                .isEqualTo(DEFAULT_DISPLAY);
        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(DEFAULT_DISPLAY))
                .isEqualTo(INVALID_DISPLAY);

        activity.finish();
        activity.waitForActivityViewDestroyed();

        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(virtualDisplayId))
                .isEqualTo(INVALID_DISPLAY);
        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(DEFAULT_DISPLAY))
                .isEqualTo(INVALID_DISPLAY);
    }

    @Test
    public void testDoubleActivityView() throws Exception {
        ActivityViewTestActivity activity1 = startActivityViewTestActivity(DEFAULT_DISPLAY);
        activity1.waitForActivityViewReady();
        int virtualDisplayId1 = activity1.getActivityView().getVirtualDisplayId();

        ActivityViewTestActivity activity2 = startActivityViewTestActivity(virtualDisplayId1);
        activity2.waitForActivityViewReady();
        int virtualDisplayId2 = activity2.getActivityView().getVirtualDisplayId();

        startTestActivity(ActivityInActivityView.class, virtualDisplayId2);

        assertThat(virtualDisplayId1).isNotEqualTo(virtualDisplayId2);
        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(virtualDisplayId1))
                .isEqualTo(DEFAULT_DISPLAY);
        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(virtualDisplayId2))
                .isEqualTo(DEFAULT_DISPLAY);
        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(DEFAULT_DISPLAY))
                .isEqualTo(INVALID_DISPLAY);

        activity2.finish();
        activity1.finish();

        activity2.waitForActivityViewDestroyed();
        activity1.waitForActivityViewDestroyed();

        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(virtualDisplayId1))
                .isEqualTo(INVALID_DISPLAY);
        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(virtualDisplayId2))
                .isEqualTo(INVALID_DISPLAY);
        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(DEFAULT_DISPLAY))
                .isEqualTo(INVALID_DISPLAY);
    }

    @Test
    public void testThrowsExceptionOnReportingNonExistingDisplay() throws Exception {
        ActivityViewTestActivity activity = startActivityViewTestActivity(DEFAULT_DISPLAY);
        activity.waitForActivityViewReady();
        int virtualDisplayId = activity.getActivityView().getVirtualDisplayId();

        // This will pass since the test owns the display.
        mCarUxRestrictionsManager.reportVirtualDisplayToPhysicalDisplay(virtualDisplayId,
                NONEXISTENT_DISPLAY_ID);

        assertThat(getMappedPhysicalDisplayOfVirtualDisplay(virtualDisplayId))
                .isEqualTo(NONEXISTENT_DISPLAY_ID);

        activity.finish();
        activity.waitForActivityViewDestroyed();

        // Now the display was released, so expect to throw an Exception.
        assertThrows(
                java.lang.IllegalArgumentException.class,
                () -> mCarUxRestrictionsManager.reportVirtualDisplayToPhysicalDisplay(
                        virtualDisplayId, NONEXISTENT_DISPLAY_ID));
    }

    // TODO(b/143353546): Make the following tests not to rely on CarLauncher.
    @Test
    public void testThrowsExceptionOnReportingNonOwningDisplay() throws Exception {
        int displayIdOfCarLauncher = waitForActivityViewDisplayReady(CAR_LAUNCHER_PKG_NAME);
        assumeTrue(INVALID_DISPLAY != displayIdOfCarLauncher);

        // CarLauncher owns the display, so expect to throw an Exception.
        assertThrows(
                java.lang.SecurityException.class,
                () -> mCarUxRestrictionsManager.reportVirtualDisplayToPhysicalDisplay(
                        displayIdOfCarLauncher, DEFAULT_DISPLAY + 1));

    }
}
