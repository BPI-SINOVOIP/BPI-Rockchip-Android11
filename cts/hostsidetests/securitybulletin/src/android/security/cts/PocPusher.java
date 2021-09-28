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

package android.security.cts;


import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import java.io.File;
import java.io.FileNotFoundException;

import org.junit.runner.Description;
import org.junit.rules.TestWatcher;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.IAbi;

import static org.junit.Assume.*;
import static org.junit.Assert.*;

public class PocPusher extends TestWatcher {
    private ITestDevice device = null;
    private CompatibilityBuildHelper buildHelper = null;
    private IAbi abi = null;

    private Set<String> filesToCleanup = new HashSet();
    public boolean bitness32 = true;
    public boolean bitness64 = true;
    public boolean appendBitness = true;
    public boolean cleanup = true;

    @Override
    protected void starting(Description d) {
        bothBitness();
        appendBitness = true;
        cleanup = true;
    }

    @Override
    protected void finished(Description d) {
        for (Iterator<String> it = filesToCleanup.iterator(); it.hasNext();) {
            String file = it.next();
            try {
                CLog.i("Cleaning up %s", file);
                device.deleteFile(file);
            } catch (DeviceNotAvailableException e) {
                CLog.e("Device unavailable when cleaning up %s", file);
                continue; // try to remove next time
            }
            it.remove();
        }
    }

    public PocPusher setDevice(ITestDevice device) {
        this.device = device;
        return this;
    }

    public PocPusher setAbi(IAbi abi) {
        this.abi = abi;
        return this;
    }

    public PocPusher setBuild(IBuildInfo buildInfo) {
        buildHelper = new CompatibilityBuildHelper(buildInfo);
        return this;
    }

    public PocPusher appendBitness(boolean append) {
        this.appendBitness = append;
        return this;
    }

    public PocPusher cleanup(boolean cleanup) {
        this.cleanup = cleanup;
        return this;
    }

    public PocPusher only32() {
        bitness32 = true;
        bitness64 = false;
        return this;
    }

    public PocPusher only64() {
        bitness32 = false;
        bitness64 = true;
        return this;
    }

    public PocPusher bothBitness() {
        bitness32 = true;
        bitness64 = true;
        return this;
    }

    public void pushFile(String testFile, String remoteFile)
            throws FileNotFoundException, DeviceNotAvailableException {
        if (appendBitness) {
            // if neither 32 or 64, nothing would ever be pushed.
            assertTrue("bitness must be 32, 64, or both.", bitness32 || bitness64);

            String bitness = SecurityTestCase.getAbi(device).getBitness().trim();

            // 32-bit doesn't have a 64-bit compatibility layer; skipping.
            assumeFalse(bitness.equals("32") && !bitness32);

            // push the 32-bit file on 64-bit device if a 64-bit file doesn't exist.
            if (bitness.equals("64") && !bitness64) {
                bitness = "32";
                CLog.i("Pushing a 32-bit file onto a 64-bit device.");
            }
            testFile += bitness;
        }
        CLog.i("Pushing local: %s to remote: %s", testFile.toString(), remoteFile);
        File localFile = buildHelper.getTestFile(testFile);
        device.pushFile(localFile, remoteFile);
        if (cleanup) {
            filesToCleanup.add(remoteFile);
        }
    }
}
