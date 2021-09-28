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

package android.server.wm;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.Intent;
import android.platform.test.annotations.Presubmit;
import android.view.WindowInsets;

import org.junit.Test;

import java.util.concurrent.TimeUnit;

/**
 * Build/Install/Run:
 *  atest CtsWindowManagerDeviceTestCases:DecorInsetTests
 */
@Presubmit
public class DecorInsetTests extends DecorInsetTestsBase {

    @Test
    public void testDecorView_consumesAllInsets_byDefault() throws Throwable {
        TestActivity activity = mDecorActivity.launchActivity(new Intent()
                .putExtra(ARG_LAYOUT_STABLE, false)
                .putExtra(ARG_LAYOUT_FULLSCREEN, false)
                .putExtra(ARG_LAYOUT_HIDE_NAV, false)
                .putExtra(ARG_DECOR_FITS_SYSTEM_WINDOWS, true));
        activity.mLaidOut.await(4, TimeUnit.SECONDS);

        assertNotNull("test setup failed", activity.mLastDecorInsets);
        assertNull("unexpected content insets", activity.mLastContentInsets);

        assertContentViewLocationMatchesInsets();
    }

    @Test
    public void testDecorView_consumesNavBar_ifLayoutHideNavIsNotSet() throws Throwable {
        TestActivity activity = mDecorActivity.launchActivity(new Intent()
                .putExtra(ARG_LAYOUT_STABLE, true)
                .putExtra(ARG_LAYOUT_FULLSCREEN, true)
                .putExtra(ARG_LAYOUT_HIDE_NAV, false)
                .putExtra(ARG_DECOR_FITS_SYSTEM_WINDOWS, true));
        activity.mLaidOut.await(4, TimeUnit.SECONDS);

        assertNotNull("test setup failed", activity.mLastDecorInsets);
        assertEquals("unexpected bottom inset: ", 0, activity.mLastContentInsets.getInsets(
                WindowInsets.Type.systemBars()).bottom);

        assertContentViewLocationMatchesInsets();
    }

    @Test
    public void testDecorView_doesntConsumeNavBar_ifLayoutHideNavIsSet() throws Throwable {
        TestActivity activity = mDecorActivity.launchActivity(new Intent()
                .putExtra(ARG_LAYOUT_STABLE, true)
                .putExtra(ARG_LAYOUT_FULLSCREEN, false)
                .putExtra(ARG_LAYOUT_HIDE_NAV, true)
                .putExtra(ARG_DECOR_FITS_SYSTEM_WINDOWS, true));
        activity.mLaidOut.await(4, TimeUnit.SECONDS);

        assertNotNull("test setup failed", activity.mLastDecorInsets);
        assertEquals("insets were unexpectedly consumed: ",
                activity.mLastDecorInsets.getSystemWindowInsets(),
                activity.mLastContentInsets.getSystemWindowInsets());

        assertContentViewLocationMatchesInsets();
    }

    @Test
    public void testDecorView_doesntConsumeNavBar_ifDecorDoesntFitSystemWindows() throws Throwable {
        TestActivity activity = mDecorActivity.launchActivity(new Intent()
                .putExtra(ARG_LAYOUT_STABLE, false)
                .putExtra(ARG_LAYOUT_FULLSCREEN, false)
                .putExtra(ARG_LAYOUT_HIDE_NAV, false)
                .putExtra(ARG_DECOR_FITS_SYSTEM_WINDOWS, false));
        activity.mLaidOut.await(4, TimeUnit.SECONDS);

        assertEquals(0, activity.getWindow().getDecorView().getWindowSystemUiVisibility());

        assertNotNull("test setup failed", activity.mLastDecorInsets);
        assertEquals("insets were unexpectedly consumed: ",
                activity.mLastDecorInsets.getSystemWindowInsets(),
                activity.mLastContentInsets.getSystemWindowInsets());

        assertContentViewLocationMatchesInsets();
    }
}
