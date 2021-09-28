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

package android.device.collectors;

import android.device.collectors.annotations.OptionClass;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.helpers.ProcessShowmapHelper;

import org.junit.runner.Description;

/**
 * A {@link ProcessShowmapListener} that measures process PSS, RSS, and VSS at the beginning and
 * end of a test method and adds the final values and their deltas to the metrics.
 *
 * Options:
 * -e processshowmap-process-name [processName] : the process from the test case that we want to
 * measure memory for
 */
@OptionClass(alias = "process-showmap-collector")
public class ProcessShowmapListener extends BaseCollectionListener<Long> {
    private static final String TAG = ProcessShowmapListener.class.getSimpleName();
    @VisibleForTesting static final String PROCESS_SEPARATOR = ",";
    @VisibleForTesting static final String PROCESS_NAMES_KEY = "showmap-process-names";
    private ProcessShowmapHelper mShowmapHelper = new ProcessShowmapHelper();

    public ProcessShowmapListener() {
        createHelperInstance(mShowmapHelper);
    }

    /**
     * Constructor to simulate receiving the instrumentation arguments. Should not be used except
     * for testing.
     */
    @VisibleForTesting
    public ProcessShowmapListener(Bundle args, ProcessShowmapHelper helper) {
        super(args, helper);
        mShowmapHelper = helper;
        createHelperInstance(mShowmapHelper);
    }

    @Override
    public void onTestRunStart(DataRecord runData, Description description) {
        Bundle args = getArgsBundle();
        String procsString = args.getString(PROCESS_NAMES_KEY);
        if (procsString == null) {
            Log.e(TAG, "No processes provided to sample");
            return;
        }
        String[] procs = procsString.split(PROCESS_SEPARATOR);
        mShowmapHelper.setUp(procs);
    }
}
