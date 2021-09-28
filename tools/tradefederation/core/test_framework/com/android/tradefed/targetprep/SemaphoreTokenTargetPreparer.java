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
package com.android.tradefed.targetprep;

import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;

import java.util.concurrent.Semaphore;

/**
 * This is a preparer used to use token to serialize test excution in tradefed host. only the device
 * acquire token will be allow to start the test. Others will wait until it released This can't be
 * only used when you have one test in tradefed and use shared resources. Please make sure only a
 * single test running on the host with different DUTs User need to add --semaphore-token:no-disable
 * in the command file.
 */
@OptionClass(alias = "semaphore-token")
public class SemaphoreTokenTargetPreparer extends BaseTargetPreparer {
    private boolean mTokenAcquired = true;
    static final Semaphore mRunToken = new Semaphore(1);

    public SemaphoreTokenTargetPreparer() {
        // This preparer is disabled by default.
        setDisable(true);
    }

    /** {@inheritDoc} */
    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        try {
            CLog.v("Waiting to acquire run token");
            mRunToken.acquire();
            mTokenAcquired = true;
            CLog.v("Token acquired");
        } catch (InterruptedException e) {
            mTokenAcquired = false;
            CLog.e(e);
            CLog.e("Interrupted error during token acquire");
        }
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        if (mTokenAcquired) {
            CLog.v("Releasing run token");
            mRunToken.drainPermits();
            mRunToken.release();
        } else {
            CLog.v("Did not acquire token, skip releasing run token");
        }
    }
}
