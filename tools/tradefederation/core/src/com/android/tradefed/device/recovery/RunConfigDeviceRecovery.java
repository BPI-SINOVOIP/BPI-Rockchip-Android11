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
package com.android.tradefed.device.recovery;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.command.ICommandScheduler;
import com.android.tradefed.command.ICommandScheduler.IScheduledInvocationListener;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.DeviceManager.FastbootDevice;
import com.android.tradefed.device.FreeDeviceState;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.device.IManagedTestDevice;
import com.android.tradefed.device.IMultiDeviceRecovery;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.QuotationAwareTokenizer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * Generic base {@link IMultiDeviceRecovery} to run a tradefed configuration to do the recovery
 * step.
 */
public class RunConfigDeviceRecovery implements IMultiDeviceRecovery {

    @Option(name = "disable", description = "Completely disable the recoverer.")
    private boolean mDisable = false;

    @Option(
        name = "recovery-config-name",
        description = "The configuration to be used on the device to recover.",
        mandatory = true
    )
    private String mRecoveryConfigName = null;

    @Option(
            name = "extra-arg",
            description = "Extra arguments to be passed to the recovery invocation.")
    private List<String> mExtraArgs = new ArrayList<>();

    @Override
    public void recoverDevices(List<IManagedTestDevice> managedDevices) {
        if (mDisable) {
            return;
        }

        for (IManagedTestDevice device : managedDevices) {
            if (DeviceAllocationState.Allocated.equals(device.getAllocationState())) {
                continue;
            }
            if (device.getIDevice() instanceof StubDevice
                    && !(device.getIDevice() instanceof FastbootDevice)) {
                continue;
            }
            if (shouldSkip(device)) {
                continue;
            }

            List<String> argList = new ArrayList<>();
            argList.add(mRecoveryConfigName);

            List<String> deviceExtraArgs = getExtraArguments(device);
            if (deviceExtraArgs == null) {
                CLog.w("Something went wrong recovery cannot be attempted.");
                continue;
            }

            argList.addAll(deviceExtraArgs);
            for (String args : mExtraArgs) {
                String[] extraArgs = QuotationAwareTokenizer.tokenizeLine(args);
                if (extraArgs.length != 0) {
                    argList.addAll(Arrays.asList(extraArgs));
                }
            }

            String serial = device.getSerialNumber();
            ITestDevice deviceToRecover = getDeviceManager().forceAllocateDevice(serial);
            if (deviceToRecover == null) {
                CLog.e("Fail to force allocate '%s'", serial);
                continue;
            }
            try {
                getCommandScheduler()
                        .execCommand(
                                new FreeDeviceHandler(getDeviceManager()),
                                deviceToRecover,
                                argList.toArray(new String[0]));
            } catch (ConfigurationException e) {
                CLog.e("Device multi recovery is misconfigured");
                CLog.e(e);
                // In this case, the device doesn't go through regular de-allocation so we
                // explicitly deallocate.
                getDeviceManager().freeDevice(device, FreeDeviceState.UNAVAILABLE);
                return;
            }
        }
    }

    /**
     * Get the list of extra arguments to be passed to the configuration. If null is returned
     * something went wrong and recovery should be attempted.
     *
     * @param device The {@link ITestDevice} to run recovery against
     * @return The list of extra arguments to be used. Or null if something went wrong.
     */
    public List<String> getExtraArguments(ITestDevice device) {
        return new ArrayList<>();
    }

    /**
     * Extra chance to skip the recovery on a given device by doing extra checks.
     *
     * @param device The {@link IManagedTestDevice} considered for recovery.
     * @return True if recovery should be skipped.
     */
    public boolean shouldSkip(IManagedTestDevice device) {
        return false;
    }

    /** Returns a {@link IDeviceManager} instance. Exposed for testing. */
    @VisibleForTesting
    protected IDeviceManager getDeviceManager() {
        return GlobalConfiguration.getInstance().getDeviceManager();
    }

    /** Returns a {@link ICommandScheduler} instance. Exposed for testing. */
    @VisibleForTesting
    protected ICommandScheduler getCommandScheduler() {
        return GlobalConfiguration.getInstance().getCommandScheduler();
    }

    /** Handler to free up the device once the invocation completes */
    private class FreeDeviceHandler implements IScheduledInvocationListener {

        private final IDeviceManager mDeviceManager;

        FreeDeviceHandler(IDeviceManager deviceManager) {
            mDeviceManager = deviceManager;
        }

        @Override
        public void invocationComplete(
                IInvocationContext context, Map<ITestDevice, FreeDeviceState> devicesStates) {
            for (ITestDevice device : context.getDevices()) {
                mDeviceManager.freeDevice(device, devicesStates.get(device));
                if (device instanceof IManagedTestDevice) {
                    // This is quite an important setting so we do make sure it's reset.
                    ((IManagedTestDevice) device).setFastbootPath(mDeviceManager.getFastbootPath());
                }
            }
        }
    }
}
