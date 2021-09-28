/*
 * Copyright (C) 2013 The Android Open Source Project
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
 * limitations under the License
 */

package android.hardware.cts;

import static android.content.res.Configuration.SCREENLAYOUT_SIZE_LARGE;
import static android.content.res.Configuration.SCREENLAYOUT_SIZE_NORMAL;
import static android.content.res.Configuration.SCREENLAYOUT_SIZE_SMALL;
import static android.content.res.Configuration.SCREENLAYOUT_SIZE_XLARGE;
import static android.util.DisplayMetrics.DENSITY_400;
import static android.util.DisplayMetrics.DENSITY_560;
import static android.util.DisplayMetrics.DENSITY_HIGH;
import static android.util.DisplayMetrics.DENSITY_LOW;
import static android.util.DisplayMetrics.DENSITY_MEDIUM;
import static android.util.DisplayMetrics.DENSITY_TV;
import static android.util.DisplayMetrics.DENSITY_XHIGH;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.os.StatFs;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.WindowManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CddTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

/**
 * Tests that devices with low RAM specify themselves as Low RAM devices
 */
@RunWith(AndroidJUnit4.class)
public class LowRamDeviceTest {

    private static final long ONE_MEGABYTE = 1048576L;
    private static final String TAG = "LowRamDeviceTest";
    private static final long LOW_RAM_MAX = 1024;
    private static final float MIN_APP_DATA_PARTITION_SIZE_GB = 4f;
    private static final float MIN_APP_DATA_PARTITION_SIZE_LOW_RAM_GB = 1.1f;
    private static final float MIN_SHARED_DATA_PARTITION_SIZE_GB = 1f;
    private static final long GB_TO_BYTES_MULTIPLIER = 1024 * 1024 * 1024;

    private Context mContext;
    private PackageManager mPackageManager;
    private ActivityManager mActivityManager;
    private DisplayMetrics mDisplayMetrics;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mPackageManager = mContext.getPackageManager();
        mActivityManager =
                (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);

        mDisplayMetrics = new DisplayMetrics();
        WindowManager windowManager =
                (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        windowManager.getDefaultDisplay().getMetrics(mDisplayMetrics);
    }

    /**
     * Test the devices reported memory to ensure it meets the minimum values described
     * in CDD 7.6.1.
     */
    @Test
    @CddTest(requirement="7.6.1")
    public void testMinimumMemory() {
        int density = mDisplayMetrics.densityDpi;
        Boolean supports64Bit = supportsSixtyFourBit();
        int screenSize = getScreenSize();
        Boolean lowRamDevice = mActivityManager.isLowRamDevice();
        Boolean watch = mPackageManager.hasSystemFeature(PackageManager.FEATURE_WATCH);

        Log.i(TAG, String.format("density=%d, supports64Bit=%s, screenSize=%d, watch=%s",
                density, supports64Bit, screenSize, watch));

        if (watch) {
            assertFalse("Device is not expected to be 64-bit", supports64Bit);
            assertMinMemoryMb(416);
        } else if (lessThanDpi(density, DENSITY_HIGH, screenSize,
                SCREENLAYOUT_SIZE_NORMAL, SCREENLAYOUT_SIZE_SMALL) ||
                lessThanDpi(density, DENSITY_MEDIUM, screenSize, SCREENLAYOUT_SIZE_LARGE) ||
                lessThanDpi(density, DENSITY_LOW, screenSize, SCREENLAYOUT_SIZE_XLARGE)) {

            if (supports64Bit) {
                assertMinMemoryMb(816);
            } else {
                assertMinMemoryMb(416);
            }
        } else if (greaterThanDpi(density, DENSITY_560, screenSize,
                SCREENLAYOUT_SIZE_NORMAL, SCREENLAYOUT_SIZE_SMALL) ||
                greaterThanDpi(density, DENSITY_400, screenSize, SCREENLAYOUT_SIZE_LARGE) ||
                greaterThanDpi(density, DENSITY_XHIGH, screenSize, SCREENLAYOUT_SIZE_XLARGE)) {

            if (supports64Bit) {
                assertMinMemoryMb(1824);
            } else {
                assertMinMemoryMb(1344);
            }
        } else if (greaterThanDpi(density, DENSITY_400, screenSize,
                SCREENLAYOUT_SIZE_NORMAL, SCREENLAYOUT_SIZE_SMALL) ||
                greaterThanDpi(density, DENSITY_XHIGH, screenSize, SCREENLAYOUT_SIZE_LARGE) ||
                greaterThanDpi(density, DENSITY_TV, screenSize, SCREENLAYOUT_SIZE_XLARGE)) {

            if (supports64Bit) {
                assertMinMemoryMb(1280);
            } else {
                assertMinMemoryMb(896);
            }
        } else if (greaterThanDpi(density, DENSITY_XHIGH, screenSize,
                SCREENLAYOUT_SIZE_NORMAL, SCREENLAYOUT_SIZE_SMALL) ||
                greaterThanDpi(density, DENSITY_TV, screenSize, SCREENLAYOUT_SIZE_LARGE) ||
                greaterThanDpi(density, DENSITY_MEDIUM, screenSize, SCREENLAYOUT_SIZE_XLARGE)) {

            if (supports64Bit) {
                assertMinMemoryMb(944);
            } else {
                assertMinMemoryMb(592);
            }
        }
    }

    @Test
    @CddTest(requirement="7.6.2")
    public void testMinSharedDataPartitionSize() {
        assertDataPartitionMinimumSize(
                "Shared data",
                mContext.getExternalFilesDir(null),
                MIN_SHARED_DATA_PARTITION_SIZE_GB);
    }

    @Test
    @CddTest(requirement="7.6.1/H-9-2,7.6.1/H-10-1")
    public void testMinDataPartitionSize() {
        long totalMemoryMb = getTotalMemory() / ONE_MEGABYTE;
        boolean lowRam = totalMemoryMb <= LOW_RAM_MAX;

        if (lowRam) {
            assertDataPartitionMinimumSize(
                    "Application data",
                    mContext.getFilesDir(),
                    MIN_APP_DATA_PARTITION_SIZE_LOW_RAM_GB);
        } else {
            assertDataPartitionMinimumSize(
                    "Application data", mContext.getFilesDir(), MIN_APP_DATA_PARTITION_SIZE_GB);
        }
    }

    /**
     * @return the total memory accessible by the kernel as defined by
     * {@code ActivityManager.MemoryInfo}.
     */
    private long getTotalMemory() {
        ActivityManager.MemoryInfo memoryInfo = new ActivityManager.MemoryInfo();
        mActivityManager.getMemoryInfo(memoryInfo);
        return memoryInfo.totalMem;
    }

    /** @return the screen size as defined in {@Configuration}. */
    private int getScreenSize() {
        Configuration config = mContext.getResources().getConfiguration();
        return config.screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK;
    }

    /** @return true iff this device supports 64 bit ABIs */
    private static boolean supportsSixtyFourBit() {
        return Build.SUPPORTED_64_BIT_ABIS.length > 0;
    }

    /** Asserts that the given values conform to the specs in CDD 7.6.1 */
    private void assertMinMemoryMb(long minMb) {

        long totalMemoryMb = getTotalMemory() / ONE_MEGABYTE;
        boolean lowRam = totalMemoryMb <= LOW_RAM_MAX;
        boolean lowRamDevice = mActivityManager.isLowRamDevice();

        Log.i(TAG, String.format("minMb=%,d", minMb));
        Log.i(TAG, String.format("totalMemoryMb=%,d", totalMemoryMb));
        Log.i(TAG, "lowRam=" + lowRam);
        Log.i(TAG, "lowRamDevice=" + lowRamDevice);

        assertTrue(String.format("Does not meet minimum memory requirements (CDD 7.6.1)."
                + "Found = %d, Minimum = %d", totalMemoryMb, minMb), totalMemoryMb >= minMb);

        assertTrue("Device must specify low RAM property: ro.config.low_ram=true",
                !lowRam || (lowRam && lowRamDevice));
    }

    private static boolean lessThanDpi(int actualDensityDpi, int expectedDensityDpi,
            int actualScreenSize, int... expectedScreenSizes) {
        return actualDensityDpi <= expectedDensityDpi &&
                contains(expectedScreenSizes, actualScreenSize);
    }

    private static boolean greaterThanDpi(int actualDensityDpi, int expectedDensityDpi,
            int actualScreenSize, int... expectedScreenSizes) {
        return actualDensityDpi >= expectedDensityDpi &&
                contains(expectedScreenSizes, actualScreenSize);
    }

    /** @return true iff the {@code array} contains the {@code target} */
    private static boolean contains(int [] array, int target) {
        for(int a : array) {
            if (a == target) {
                return true;
            }
        }
        return false;
    }

    private void assertDataPartitionMinimumSize(
            String partitionName, File fileInPartition, float minPartitionSizeGb) {
        StatFs statFs = new StatFs(fileInPartition.getAbsolutePath());
        long size = statFs.getTotalBytes();
        long minSizeBytes = (long) minPartitionSizeGb * GB_TO_BYTES_MULTIPLIER;

        assertTrue(
                String.format("%s partition size does not meet requirement. "
                        + "Found = %d, Minimum = %d", partitionName, size, minSizeBytes),
                size > minSizeBytes);
    }
}
