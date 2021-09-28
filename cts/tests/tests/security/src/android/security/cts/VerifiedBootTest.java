/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.security.cts;

import android.content.pm.PackageManager;
import android.os.Build;
import android.platform.test.annotations.SecurityTest;
import android.test.AndroidTestCase;
import com.android.compatibility.common.util.PropertyUtil;

@SecurityTest
public class VerifiedBootTest extends AndroidTestCase {
  private static final String TAG = "VerifiedBootTest";

  private static boolean isLowRamExempt(PackageManager pm) {
    if (pm.hasSystemFeature(PackageManager.FEATURE_RAM_NORMAL)) {
      // No exemption for normal RAM
      return false;
    }
    return (PropertyUtil.getFirstApiLevel() < Build.VERSION_CODES.P);
  }

  /**
   * Asserts that Verified Boot is supported.
   *
   * A device is exempt if it launched on a pre-O_MR1 level.
   *
   * A device without the feature flag android.hardware.ram.normal is exempt if
   * it launched on a pre-P level.
   */
  public void testVerifiedBootSupport() throws Exception {
    if (PropertyUtil.getFirstApiLevel() < Build.VERSION_CODES.O_MR1) {
      return;
    }
    PackageManager pm = getContext().getPackageManager();
    assertNotNull("PackageManager must not be null", pm);
    if (isLowRamExempt(pm)) {
      return;
    }
    assertTrue("Verified boot must be supported on the device",
        pm.hasSystemFeature(PackageManager.FEATURE_VERIFIED_BOOT));
  }
}
