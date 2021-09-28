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
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.helpers.TotalPssHelper;

/**
 * A {@link TotalPssMetricListener} that measures process total pss tracked per
 * process in activity manaager.
 *
 * Options:
 * -e process-names [processName] : the process from the test case that we want to
 * measure memory for.
 */
@OptionClass(alias = "totalpss-collector")
public class TotalPssMetricListener extends BaseCollectionListener<Long> {

    private static final String TAG = TotalPssMetricListener.class.getSimpleName();
    @VisibleForTesting static final String PROCESS_SEPARATOR = ",";
    @VisibleForTesting static final String PROCESS_NAMES_KEY = "process-names";
    @VisibleForTesting static final String MIN_ITERATIONS_KEY = "min_iterations";
    @VisibleForTesting static final String MAX_ITERATIONS_KEY = "max_iterations";
    @VisibleForTesting static final String SLEEP_TIME_KEY = "sleep_time_ms";
    @VisibleForTesting static final String THRESHOLD_KEY = "threshold_kb";
    private TotalPssHelper mTotalPssHelper = new TotalPssHelper();

    public TotalPssMetricListener() {
        createHelperInstance(mTotalPssHelper);
    }

    /**
     * Constructor to simulate receiving the instrumentation arguments. Should not be used except
     * for testing.
     */
    @VisibleForTesting
    public TotalPssMetricListener(Bundle args, TotalPssHelper helper) {
        super(args, helper);
        mTotalPssHelper = helper;
        createHelperInstance(mTotalPssHelper);
    }

    /**
     * Adds the options for total pss collector.
     */
    @Override
    public void setupAdditionalArgs() {
        Bundle args = getArgsBundle();
        String procsString = args.getString(PROCESS_NAMES_KEY);
        if (procsString == null) {
            Log.e(TAG, "No processes provided to sample");
            return;
        }

        String[] procs = procsString.split(PROCESS_SEPARATOR);
        mTotalPssHelper.setUp(procs);

        if (args.getString(MIN_ITERATIONS_KEY) != null) {
            mTotalPssHelper.setMinIterations(Integer.parseInt(args.getString(MIN_ITERATIONS_KEY)));
        }

        if (args.getString(MAX_ITERATIONS_KEY) != null) {
            mTotalPssHelper.setMaxIterations(Integer.parseInt(args.getString(MAX_ITERATIONS_KEY)));
        }

        if (args.getString(SLEEP_TIME_KEY) != null) {
            mTotalPssHelper.setSleepTime(Integer.parseInt(args.getString(SLEEP_TIME_KEY)));
        }

        if (args.getString(THRESHOLD_KEY) != null) {
            mTotalPssHelper.setThreshold(Integer.parseInt(args.getString(THRESHOLD_KEY)));
        }
    }
}
