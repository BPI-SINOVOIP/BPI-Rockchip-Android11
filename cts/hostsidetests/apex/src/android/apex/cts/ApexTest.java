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

package android.apex.cts;

import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.util.CommandResult;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

@RunWith(DeviceJUnit4ClassRunner.class)
public class ApexTest extends BaseHostJUnit4Test {
  private boolean isApexUpdatable() throws Exception {
    return Boolean.parseBoolean(getDevice().getProperty("ro.apex.updatable"));
  }

  private boolean isGSI() throws Exception {
    String systemProduct = getDevice().getProperty("ro.product.system_ext.name");
    return systemProduct.equals("aosp_arm")
      || systemProduct.equals("aosp_arm64")
      || systemProduct.equals("aosp_x86")
      || systemProduct.equals("aosp_x86_64")
      || systemProduct.equals("aosp_arm_ab") // _ab for Legacy GSI
      || systemProduct.equals("aosp_arm64_ab")
      || systemProduct.equals("aosp_x86_ab")
      || systemProduct.equals("aosp_x86_64_ab")
      || systemProduct.equals("aosp_tv_arm")
      || systemProduct.equals("aosp_tv_arm64");
  }

  /**
   * Ensures that the built-in APEXes are all with flattened APEXes
   * or non-flattend APEXes. Mixture of them is not supported and thus
   * not allowed.
   *
   * GSI is exempt from this test since it exceptionally includes both types os APEXes.
   */
  @Test
  public void testApexType() throws Exception {
    if (isGSI()) {
      return;
    }

    String[] builtinDirs = {
      "/system/apex",
      "/system_ext/apex",
      "/product/apex",
      "/vendor/apex"
    };

    int numFlattenedApexes = 0;
    int numNonFlattenedApexes = 0;
    for (String dir : builtinDirs) {
      numFlattenedApexes += countFlattenedApexes(dir);
      numNonFlattenedApexes += countNonFlattenedApexes(dir);
    }

    Assert.assertTrue(
        "No APEX found",
        (numFlattenedApexes + numNonFlattenedApexes) != 0);

    if (isApexUpdatable()) {
      Assert.assertTrue(numFlattenedApexes +
          " flattened APEX(es) found on a device supporting updatable APEX",
          numFlattenedApexes == 0);
    } else {
      Assert.assertTrue(numNonFlattenedApexes +
          " non-flattened APEX(es) found on a device not supporting updatable APEX",
          numNonFlattenedApexes == 0);
    }
  }

  // CTS shim APEX can be non-flattened - even when ro.apex.updatable=false.
  // Don't count it.
  private final static String CTS_SHIM_APEX_NAME = "com.android.apex.cts.shim";

  private int countFlattenedApexes(String dir) throws Exception {
    CommandResult result = getDevice().executeShellV2Command(
        "find " + dir + " -type f -name \"apex_manifest.pb\" ! -path \"*" +
        CTS_SHIM_APEX_NAME + "*\" | wc -l");
    return result.getExitCode() == 0 ? Integer.parseInt(result.getStdout().trim()) : 0;
  }

  private int countNonFlattenedApexes(String dir) throws Exception {
    CommandResult result = getDevice().executeShellV2Command(
        "find " + dir + " -type f -name \"*.apex\" ! -name \"" +
        CTS_SHIM_APEX_NAME + ".apex\" | wc -l");
    return result.getExitCode() == 0 ? Integer.parseInt(result.getStdout().trim()) : 0;
  }

  /**
   * Ensures that pre-apexd processes (e.g. vold) and post-apexd processes (e.g. init) are using
   * different mount namespaces (in case of ro.apexd.updatable is true), or not.
   */
  @Test
  public void testMountNamespaces() throws Exception {
    final int rootMountIdOfInit = getMountEntry("1", "/").mountId;
    final int rootMountIdOfVold = getMountEntry("$(pidof vold)", "/").mountId;
    if (isApexUpdatable()) {
      Assert.assertNotEquals("device supports updatable APEX, but is not using multiple mount namespaces",
          rootMountIdOfInit, rootMountIdOfVold);
    } else {
      Assert.assertEquals("device supports updatable APEX, but is using multiple mount namespaces",
          rootMountIdOfInit, rootMountIdOfVold);
    }
  }

  private static class MountEntry {
    public final int mountId;
    public final String mountPoint;

    public MountEntry(String mountInfoLine) {
      String[] tokens = mountInfoLine.split(" ");
      if (tokens.length < 5) {
        throw new RuntimeException(mountInfoLine + " doesn't seem to be from mountinfo");
      }
      mountId = Integer.parseInt(tokens[0]);
      mountPoint = tokens[4];
    }
  }

  private String[] readMountInfo(String pidExpression) throws Exception {
    CommandResult result = getDevice().executeShellV2Command(
        "cat /proc/" + pidExpression + "/mountinfo");
    if (result.getExitCode() != 0) {
      throw new RuntimeException("failed to read mountinfo for " + pidExpression);
    }
    return result.getStdout().trim().split("\n");
  }

  private MountEntry getMountEntry(String pidExpression, String mountPoint) throws Exception {
    return Arrays.asList(readMountInfo(pidExpression)).stream()
        .map(MountEntry::new)
        .filter(entry -> mountPoint.equals(entry.mountPoint)).findAny().get();
  }
}
