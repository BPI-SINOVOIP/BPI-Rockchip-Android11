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

package android.device.collectors;

import android.device.collectors.annotations.OptionClass;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import com.android.helpers.ProcLoadHelper;

/**
 * A {@link ProcLoadListener} that waits until the proc/load threshold is met
 * or timeout expires.
 *
 * Options:
 * <p>-e proc-loadavg-threshold 1 : The threshold the system cpu load has
 * to be less than or equal in last minute.
 *
 * <p>-e proc-loadavg-timeout 1000 :
 * Timeout to wait before the threshold is met.
 *
 * <p>-e proc-loadavg-interval 100 :
 * Interval frequency to check if the threshold is met or not.
 *
 */
@OptionClass(alias = "procload-collector")
public class ProcLoadListener extends BaseCollectionListener<Double> {

    private static final String TAG = ProcLoadListener.class.getSimpleName();
    @VisibleForTesting
    static final String PROC_LOAD_THRESHOLD = "proc-loadavg-threshold";
    @VisibleForTesting
    static final String PROC_THRESHOLD_TIMEOUT = "proc-loadavg-timeout";
    @VisibleForTesting
    static final String PROC_LOAD_INTERVAL = "proc-loadavg-interval";

    private ProcLoadHelper mProcLoadHelper = new ProcLoadHelper();

    public ProcLoadListener() {
        createHelperInstance(mProcLoadHelper);
    }

    /**
     * Constructor to simulate receiving the instrumentation arguments. Should not be used except
     * for testing.
     */
    @VisibleForTesting
    public ProcLoadListener(Bundle args, ProcLoadHelper helper) {
        super(args, helper);
        mProcLoadHelper = helper;
        createHelperInstance(mProcLoadHelper);
    }

    /**
     * Adds the options for total pss collector.
     */
    @Override
    public void setupAdditionalArgs() {
        Bundle args = getArgsBundle();

        if (args.getString(PROC_LOAD_THRESHOLD) != null) {
            mProcLoadHelper.setProcLoadThreshold(Double.parseDouble(args
                    .getString(PROC_LOAD_THRESHOLD)));
        }

        if (args.getString(PROC_THRESHOLD_TIMEOUT) != null) {
            mProcLoadHelper.setProcLoadWaitTimeInMs(Long.parseLong(args
                    .getString(PROC_THRESHOLD_TIMEOUT)));
        }

        if (args.getString(PROC_LOAD_INTERVAL) != null) {
            mProcLoadHelper.setProcLoadIntervalInMs(Long.parseLong(args
                    .getString(PROC_LOAD_INTERVAL)));
        }

    }
}
