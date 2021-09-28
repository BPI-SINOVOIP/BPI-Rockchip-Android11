/*
 * Copyright (C) 2009 The Android Open Source Project
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

package android.dpi.cts;

import android.content.Context;
import android.content.pm.PackageManager;
import android.platform.test.annotations.Presubmit;
import android.test.AndroidTestCase;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.WindowManager;

import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.FeatureUtil;

import java.util.HashSet;
import java.util.Set;

/**
 * Test for verifying a device's screen configuration.
 */
public class ConfigurationTest extends AndroidTestCase {

    private DisplayMetrics mMetrics;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        WindowManager windowManager =
                (WindowManager) getContext().getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        mMetrics = new DisplayMetrics();
        display.getRealMetrics(mMetrics);
    }

    @Presubmit
    public void testScreenConfiguration() {
        double xInches = (double) mMetrics.widthPixels / mMetrics.xdpi;
        double yInches = (double) mMetrics.heightPixels / mMetrics.ydpi;
        double diagonalInches = Math.sqrt(Math.pow(xInches, 2) + Math.pow(yInches, 2));
        double minSize = 2.5d;
        if (FeatureUtil.isWatch()) {
            // Watches have a different minimum diagonal.
            minSize = 1.0d;
        } else if (FeatureUtil.isAutomotive()) {
            // Cars have a different minimum diagonal.
            minSize = 6.0d;
        }
        assertTrue("Screen diagonal must be at least " + minSize + " inches: " + diagonalInches,
                diagonalInches >= minSize);

        double density = 160.0d * mMetrics.density;
        assertTrue("Screen density must be at least 100 dpi: " + density, density >= 100.0d);

        Set<Integer> allowedDensities = new HashSet<Integer>();
        allowedDensities.add(DisplayMetrics.DENSITY_LOW);
        allowedDensities.add(DisplayMetrics.DENSITY_140);
        allowedDensities.add(DisplayMetrics.DENSITY_MEDIUM);
        allowedDensities.add(DisplayMetrics.DENSITY_180);
        allowedDensities.add(DisplayMetrics.DENSITY_200);
        allowedDensities.add(DisplayMetrics.DENSITY_TV);
        allowedDensities.add(DisplayMetrics.DENSITY_220);
        allowedDensities.add(DisplayMetrics.DENSITY_HIGH);
        allowedDensities.add(DisplayMetrics.DENSITY_260);
        allowedDensities.add(DisplayMetrics.DENSITY_280);
        allowedDensities.add(DisplayMetrics.DENSITY_300);
        allowedDensities.add(DisplayMetrics.DENSITY_XHIGH);
        allowedDensities.add(DisplayMetrics.DENSITY_340);
        allowedDensities.add(DisplayMetrics.DENSITY_360);
        allowedDensities.add(DisplayMetrics.DENSITY_400);
        allowedDensities.add(DisplayMetrics.DENSITY_420);
        allowedDensities.add(DisplayMetrics.DENSITY_440);
        allowedDensities.add(DisplayMetrics.DENSITY_450);
        allowedDensities.add(DisplayMetrics.DENSITY_XXHIGH);
        allowedDensities.add(DisplayMetrics.DENSITY_560);
        allowedDensities.add(DisplayMetrics.DENSITY_600);
        allowedDensities.add(DisplayMetrics.DENSITY_XXXHIGH);
        assertTrue("DisplayMetrics.DENSITY_DEVICE_STABLE must be one of the DisplayMetrics.DENSITY_* values: "
                + allowedDensities, allowedDensities.contains(DisplayMetrics.DENSITY_DEVICE_STABLE));

        assertEquals(mMetrics.density,
                (float) mMetrics.densityDpi / DisplayMetrics.DENSITY_DEFAULT,
                0.5f / DisplayMetrics.DENSITY_DEFAULT);
    }

    @CddTest(requirement="2.5.1")
    public void testAutomotiveMinimumScreenSize() {
        if (!FeatureUtil.isAutomotive()) {
            return;
        }
        float dpHeight = mMetrics.heightPixels / mMetrics.density;
        float dpWidth = mMetrics.widthPixels / mMetrics.density;

        assertTrue("Height must be >= 480dp, found: " + dpHeight, dpHeight >= 480);
        assertTrue("Width must be >= 750dp, found: " + dpWidth, dpWidth >= 750);
    }
}
