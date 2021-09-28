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

package android.provider.cts.preconditions;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.ITargetCleaner;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;

/**
 * Creates secondary external storage for use during a test suite.
 */
public class ExternalStoragePreparer implements ITargetPreparer, ITargetCleaner {
    private static final boolean ENABLED = false;

    @Override
    public void setUp(ITestDevice device, IBuildInfo buildInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        if (!ENABLED) return;
        if (!hasIsolatedStorage(device)) return;

        device.executeShellCommand("sm set-virtual-disk false");
        device.executeShellCommand("sm set-virtual-disk true");

        // Partition disks to make sure they're usable by tests
        final String diskId = getVirtualDisk(device);
        device.executeShellCommand("sm partition " + diskId + " public");
    }

    @Override
    public void tearDown(ITestDevice device, IBuildInfo buildInfo, Throwable throwable)
            throws DeviceNotAvailableException {
        if (!ENABLED) return;
        if (!hasIsolatedStorage(device)) return;

        device.executeShellCommand("sm set-virtual-disk false");
    }

    private boolean hasIsolatedStorage(ITestDevice device) throws DeviceNotAvailableException {
        return device.executeShellCommand("getprop sys.isolated_storage_snapshot")
                .contains("true");
    }

    private String getVirtualDisk(ITestDevice device) throws DeviceNotAvailableException {
        int attempt = 0;
        String disks = device.executeShellCommand("sm list-disks");
        while ((disks == null || disks.isEmpty()) && attempt++ < 15) {
            try {
                Thread.sleep(1000);
            } catch (InterruptedException ignored) {
            }
            disks = device.executeShellCommand("sm list-disks");
        }

        if (disks == null || disks.isEmpty()) {
            throw new AssertionError("Device must support virtual disk to verify behavior");
        }
        return disks.split("\n")[0].trim();
    }
}
