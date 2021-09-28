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
package com.android.tradefed.testtype.junit4;

import com.android.tradefed.device.DeviceNotAvailableException;

import org.junit.runner.Description;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunNotifier;

/** Wrapper of {@link RunNotifier} so we can carry the {@link DeviceNotAvailableException}. */
public class RunNotifierWrapper extends RunNotifier {

    private DeviceNotAvailableException mDnae;
    private RunNotifier notifier;

    public RunNotifierWrapper(RunNotifier notifier) {
        this.notifier = notifier;
    }

    @Override
    public void fireTestFailure(Failure failure) {
        notifier.fireTestFailure(failure);
        if (failure.getException() instanceof DeviceNotAvailableException) {
            mDnae = (DeviceNotAvailableException) failure.getException();
        }
    }

    @Override
    public void fireTestAssumptionFailed(Failure failure) {
        notifier.fireTestAssumptionFailed(failure);
    }

    @Override
    public void fireTestFinished(Description description) {
        notifier.fireTestFinished(description);
    }

    @Override
    public void fireTestStarted(Description description) {
        notifier.fireTestStarted(description);
    }

    @Override
    public void fireTestIgnored(Description description) {
        notifier.fireTestIgnored(description);
    }

    /** Returns the {@link DeviceNotAvailableException} if any was thrown. */
    public DeviceNotAvailableException getDeviceNotAvailableException() {
        return mDnae;
    }
}
