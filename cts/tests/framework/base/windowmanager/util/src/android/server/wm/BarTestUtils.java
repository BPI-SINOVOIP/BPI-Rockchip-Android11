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

package android.server.wm;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.util.Log;
import android.view.WindowInsets;

import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import org.junit.AssumptionViolatedException;

/**
 * Common assumptions for system bar tests.
 *
 * TODO: Unify with copy in systemui tests.
 */
public final class BarTestUtils {

    private BarTestUtils() {
    }

    public static void assumeHasColoredStatusBar(ActivityTestRule<?> rule) {
        assumeHasColoredBars();
        assumeHasStatusBar(rule);
    }

    public static void assumeHasStatusBar(ActivityTestRule<?> rule) {
        assumeFalse("No status bar when running in VR", isRunningInVr());

        assumeTrue("Top stable inset is non-positive, no status bar.",
                getInsets(rule).getStableInsetTop() > 0);
    }

    public static void assumeHasColoredNavigationBar(ActivityTestRule<?> rule) {
        assumeHasColoredBars();

        assumeTrue("Bottom stable inset is non-positive, no navigation bar",
                getInsets(rule).getStableInsetBottom() > 0);
    }

    public static void assumeHasColoredBars() {
        final PackageManager pm = getInstrumentation().getContext().getPackageManager();

        assumeHasBars();

        assumeFalse("Automotive navigation bar is opaque",
                pm.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE));

        assumeTrue("Only highEndGfx devices have colored system bars",
                ActivityManager.isHighEndGfx());
    }

    public static void assumeHasBars() {
        final PackageManager pm = getInstrumentation().getContext().getPackageManager();

        assumeFalse("Embedded devices don't have system bars",
                getInstrumentation().getContext().getPackageManager().hasSystemFeature(
                        PackageManager.FEATURE_EMBEDDED));

        assumeFalse("No bars on watches and TVs", pm.hasSystemFeature(PackageManager.FEATURE_WATCH)
                || pm.hasSystemFeature(PackageManager.FEATURE_TELEVISION)
                || pm.hasSystemFeature(PackageManager.FEATURE_LEANBACK));
    }

    private static boolean isRunningInVr() {
        final Context context = InstrumentationRegistry.getContext();
        final Configuration config = context.getResources().getConfiguration();
        return (config.uiMode & Configuration.UI_MODE_TYPE_MASK)
                == Configuration.UI_MODE_TYPE_VR_HEADSET;
    }

    private static WindowInsets getInsets(ActivityTestRule<?> rule) {
        final WindowInsets[] insets = new WindowInsets[1];
        try {
            rule.runOnUiThread(() -> {
                insets[0] = rule.getActivity().getWindow().getDecorView().getRootWindowInsets();
            });
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
        return insets[0];
    }

    public static boolean isAssumptionViolated(Runnable assumption) {
        try {
            assumption.run();
            return false;
        } catch (AssumptionViolatedException e) {
            Log.i("BarTestUtils", "Assumption violated", e);
            return true;
        }
    }
}
