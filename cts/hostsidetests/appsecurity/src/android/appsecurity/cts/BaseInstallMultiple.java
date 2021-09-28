/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
package android.appsecurity.cts;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.util.AbiUtils;

import junit.framework.TestCase;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.List;

/**
 * Base class for invoking the install-multiple command via ADB. Subclass this for less typing:
 *
 * <code>
 *     private class InstallMultiple extends BaseInstallMultiple&lt;InstallMultiple&gt; {
 *         public InstallMultiple() {
 *             super(getDevice(), null, null);
 *         }
 *     }
 * </code>
 */
public class BaseInstallMultiple<T extends BaseInstallMultiple<?>> {
    private final ITestDevice mDevice;
    private final IBuildInfo mBuild;
    private final IAbi mAbi;

    private final List<String> mArgs = new ArrayList<>();
    private final List<File> mFiles = new ArrayList<>();
    private final List<String> mSplits = new ArrayList<>();
    private boolean mUseNaturalAbi;

    public BaseInstallMultiple(ITestDevice device, IBuildInfo buildInfo, IAbi abi) {
        this(device, buildInfo, abi, true);
    }

    public BaseInstallMultiple(ITestDevice device, IBuildInfo buildInfo, IAbi abi,
            boolean grantPermissions) {
        mDevice = device;
        mBuild = buildInfo;
        mAbi = abi;
        if (grantPermissions) {
            addArg("-g");
        }
    }

    T addArg(String arg) {
        mArgs.add(arg);
        return (T) this;
    }

    T addFile(String file) throws FileNotFoundException {
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mBuild);
        mFiles.add(buildHelper.getTestFile(file, mAbi));
        return (T) this;
    }

    T removeSplit(String split) {
        mSplits.add(split);
        return (T) this;
    }

    T inheritFrom(String packageName) {
        addArg("-r");
        addArg("-p " + packageName);
        return (T) this;
    }

    T useNaturalAbi() {
        mUseNaturalAbi = true;
        return (T) this;
    }

    T allowTest() {
        addArg("-t");
        return (T) this;
    }

    T locationAuto() {
        addArg("--install-location 0");
        return (T) this;
    }

    T locationInternalOnly() {
        addArg("--install-location 1");
        return (T) this;
    }

    T locationPreferExternal() {
        addArg("--install-location 2");
        return (T) this;
    }

    T forceUuid(String uuid) {
        addArg("--force-uuid " + uuid);
        return (T) this;
    }

    T forUser(int userId) {
        addArg("--user " + userId);
        return (T) this;
    }

    T restrictPermissions() {
        addArg("--restrict-permissions");
        return (T) this;
    }

    protected String deriveRemoteName(String originalName, int index) {
        return index + "_" + originalName;
    }

    void run() throws DeviceNotAvailableException {
        run(true, null);
    }

    void runExpectingFailure() throws DeviceNotAvailableException {
        run(false, null);
    }

    void runExpectingFailure(String failure) throws DeviceNotAvailableException {
        run(false, failure);
    }

    private void run(boolean expectingSuccess, String failure) throws DeviceNotAvailableException {
        final ITestDevice device = mDevice;

        // Create an install session
        final StringBuilder cmd = new StringBuilder();
        cmd.append("pm install-create");
        for (String arg : mArgs) {
            cmd.append(' ').append(arg);
        }
        if (!mUseNaturalAbi && mAbi != null) {
            cmd.append(' ').append(AbiUtils.createAbiFlag(mAbi.getName()));
        }

        String result = device.executeShellCommand(cmd.toString());
        TestCase.assertTrue(result, result.startsWith("Success"));

        final int start = result.lastIndexOf("[");
        final int end = result.lastIndexOf("]");
        int sessionId = -1;
        try {
            if (start != -1 && end != -1 && start < end) {
                sessionId = Integer.parseInt(result.substring(start + 1, end));
            }
        } catch (NumberFormatException e) {
        }
        if (sessionId == -1) {
            throw new IllegalStateException("Failed to create install session: " + result);
        }

        // Push our files into session. Ideally we'd use stdin streaming,
        // but ddmlib doesn't support it yet.
        for (int i = 0; i < mFiles.size(); i++) {
            final File file = mFiles.get(i);
            final String remoteName = deriveRemoteName(file.getName(), i);
            final String remotePath = "/data/local/tmp/" + remoteName;
            if (!device.pushFile(file, remotePath)) {
                throw new IllegalStateException("Failed to push " + file);
            }

            cmd.setLength(0);
            cmd.append("pm install-write");
            cmd.append(' ').append(sessionId);
            cmd.append(' ').append(remoteName);
            cmd.append(' ').append(remotePath);

            result = device.executeShellCommand(cmd.toString());
            TestCase.assertTrue(result, result.startsWith("Success"));
        }

        for (int i = 0; i < mSplits.size(); i++) {
            final String split = mSplits.get(i);

            cmd.setLength(0);
            cmd.append("pm install-remove");
            cmd.append(' ').append(sessionId);
            cmd.append(' ').append(split);

            result = device.executeShellCommand(cmd.toString());
            TestCase.assertTrue(result, result.startsWith("Success"));
        }

        // Everything staged; let's pull trigger
        cmd.setLength(0);
        cmd.append("pm install-commit");
        cmd.append(' ').append(sessionId);

        result = device.executeShellCommand(cmd.toString()).trim();
        if (failure == null) {
            if (expectingSuccess) {
                TestCase.assertTrue(result, result.startsWith("Success"));
            } else {
                TestCase.assertFalse(result, result.startsWith("Success"));
            }
        } else {
            TestCase.assertTrue(result, result.contains(failure));
        }
    }
}
