/*
 * Copyright 2020 The Android Open Source Project
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

package android.graphics.cts;

import static org.junit.Assert.assertTrue;

import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CddTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test that feature flag android.software.vulkan.deqp.level is present and that it has an
 * acceptable value.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class VulkanDeqpLevelTest {

    private static final String TAG = VulkanDeqpLevelTest.class.getSimpleName();
    private static final boolean DEBUG = false;

    private static final int MINIMUM_VULKAN_DEQP_LEVEL = 0x07E30301; // Corresponds to 2019-03-01

    // Require patch version 3 for Vulkan 1.0: It was the first publicly available version,
    // and there was an important bugfix relative to 1.0.2.
    private static final int VULKAN_1_0 = 0x00400003; // 1.0.3

    private PackageManager mPm;
    private FeatureInfo mVulkanHardwareVersion = null;

    @Before
    public void setup() throws Throwable {
        mPm = InstrumentationRegistry.getTargetContext().getPackageManager();
        FeatureInfo[] features = mPm.getSystemAvailableFeatures();
        if (features != null) {
            for (FeatureInfo feature : features) {
                if (PackageManager.FEATURE_VULKAN_HARDWARE_VERSION.equals(feature.name)) {
                    mVulkanHardwareVersion = feature;
                    if (DEBUG) {
                        Log.d(TAG, feature.name + "=0x" + Integer.toHexString(feature.version));
                    }
                }
            }
        }
    }

    @CddTest(requirement = "7.1.4.2/C-1-8,C-1-9")
    @Test
    public void testVulkanDeqpLevel() {
        if (mVulkanHardwareVersion != null
                && mVulkanHardwareVersion.version >= VULKAN_1_0) {
            if (DEBUG) {
                Log.d(TAG, "Checking whether " + PackageManager.FEATURE_VULKAN_DEQP_LEVEL
                        + " has an acceptable value");
            }
            assertTrue("Feature " + PackageManager.FEATURE_VULKAN_DEQP_LEVEL + " must be present "
                            + "and have at least version " + MINIMUM_VULKAN_DEQP_LEVEL,
                    mPm.hasSystemFeature(PackageManager.FEATURE_VULKAN_DEQP_LEVEL,
                            MINIMUM_VULKAN_DEQP_LEVEL));
        }
    }

}
