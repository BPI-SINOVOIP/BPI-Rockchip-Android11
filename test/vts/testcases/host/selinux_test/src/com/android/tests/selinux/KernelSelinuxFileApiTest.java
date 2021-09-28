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

package com.android.tests.selinux;

import static org.junit.Assert.assertTrue;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.DeviceParameterizedRunner;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.TargetFileUtils;
import com.android.tradefed.util.TargetFileUtils.FilePermission;
import java.io.File;
import java.io.IOException;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

/* VTS test to verify Selinux file api. */
@RunWith(DeviceParameterizedRunner.class)
public class KernelSelinuxFileApiTest extends BaseHostJUnit4Test {
    private static File mTempDir;

    @BeforeClass
    public static void setUpClass() throws IOException {
        mTempDir = FileUtil.createNamedTempDir("vts_selinux");
    }

    @AfterClass
    public static void teardown() {
        mTempDir.delete();
    }

    /** Validate /sys/fs/selinux/checkreqprot content and permissions. */
    @Test
    public void testSelinuxCheckReqProt() throws IOException, DeviceNotAvailableException {
        String filePath = "/sys/fs/selinux/checkreqprot";
        CLog.i("Testing existence of %s", filePath);
        assertTrue(
                "%s: File does not exist.".format(filePath), getDevice().doesFileExist(filePath));
        CLog.i("Testing permissions of %s", filePath);
        String permissionBits = TargetFileUtils.getPermission(filePath, getDevice());
        assertTrue(TargetFileUtils.hasPermission(FilePermission.READ, permissionBits)
                && TargetFileUtils.hasPermission(FilePermission.WRITE, permissionBits));
        CLog.i("Testing format of %s", filePath);
        File selinuxTmpFile = FileUtil.createTempFile("SelinuxFile", "", mTempDir);
        getDevice().pullFile(filePath, selinuxTmpFile);
        String content = FileUtil.readStringFromFile(selinuxTmpFile);
        assertTrue("Results not valid!", "0".equals(content) || "1".equals(content));
    }

    /** Validate /sys/fs/selinux/policy. */
    @Test
    public void testSelinuxPolicy() throws DeviceNotAvailableException {
        String filePath = "/sys/fs/selinux/policy";
        CLog.i("Testing existence of %s", filePath);
        assertTrue(
                "%s: File does not exist.".format(filePath), getDevice().doesFileExist(filePath));
    }

    /** Validate /sys/fs/selinux/null permissions. */
    @Test
    public void testSelinuxNull() throws DeviceNotAvailableException {
        String filePath = "/sys/fs/selinux/null";
        CLog.i("Testing existence of %s", filePath);
        assertTrue(
                "%s: File does not exist.".format(filePath), getDevice().doesFileExist(filePath));
        CLog.i("Testing permissions of %s", filePath);
        String permissionBits = TargetFileUtils.getPermission(filePath, getDevice());
        assertTrue(TargetFileUtils.hasPermission(FilePermission.READ, permissionBits)
                && TargetFileUtils.hasPermission(FilePermission.WRITE, permissionBits));
    }
}