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

package android.device.preparers;

import android.device.collectors.BaseMetricListener;
import android.device.collectors.DataRecord;
import android.device.collectors.ProcessShowmapListener;
import android.device.collectors.annotations.OptionClass;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.helpers.GarbageCollectionHelper;

import org.junit.runner.Description;

/**
 * A {@link GarbageCollectionPreparer} that garbage collects the provided processes before and after
 * each test. This should be used before memory metric listeners (e.g.
 * {@link ProcessShowmapListener}) to reduce noise.
 *
 * Options:
 * -e garbagecollection-process-names [process1,process2,process3...] : comma delimited list of
 * processes to garbage collect
 * -e garbagecollection-wait-time [ms to wait] : custom time to wait after gc in order for memory
 * stabilize. Default is specified in {@link GarbageCollectionHelper}. This should be tuned based
 * off noise in metric results (i.e. increase sleep if results are noisy, decrease if it's taking
 * too long).
 * -e garbagecollection-run-end [boolean] : whether to run GC on end of test. Default is true.
 */
@OptionClass(alias = "garbage-collection-preparer")
public final class GarbageCollectionPreparer extends BaseMetricListener {
    private static final String TAG = GarbageCollectionPreparer.class.getSimpleName();
    @VisibleForTesting
    static final String PROCESS_SEPARATOR = ",";
    @VisibleForTesting
    static final String PROCESS_NAMES_KEY = "garbagecollection-process-names";
    @VisibleForTesting
    static final String GC_WAIT_TIME_KEY = "garbagecollection-wait-time";
    @VisibleForTesting static final String GC_RUN_END = "garbagecollection-run-end";

    private final GarbageCollectionHelper mGcHelper;
    // Whether the preparer successfully set up and initialized.
    private boolean mSetUp;
    private Long mWaitTime;
    private boolean mRunEnd;

    public GarbageCollectionPreparer() {
        super();
        mGcHelper = new GarbageCollectionHelper();
    }

    /**
     * Constructor to simulate receiving the instrumentation arguments. Should not be used except
     * for testing.
     */
    @VisibleForTesting
    public GarbageCollectionPreparer(Bundle args, GarbageCollectionHelper helper) {
        super(args);
        mGcHelper = helper;
    }

    @Override
    public void onTestRunStart(DataRecord runData, Description description) {
        Bundle args = getArgsBundle();
        String procsString = args.getString(PROCESS_NAMES_KEY);
        if (procsString == null) {
            Log.e(TAG, "No processes provided to GC");
            return;
        }
        String[] procs = procsString.split(PROCESS_SEPARATOR);
        mGcHelper.setUp(procs);
        String gcWaitString = args.getString(GC_WAIT_TIME_KEY);
        if (gcWaitString != null) {
            try {
                mWaitTime = Long.parseLong(gcWaitString);
            } catch (NumberFormatException e) {
                Log.e(TAG, "Unexpected wait time format. Using default time", e);
            }
        }
        String gcRunEnd = args.getString(GC_RUN_END);
        mRunEnd = gcRunEnd == null || Boolean.parseBoolean(gcRunEnd);
        mSetUp = true;
    }

    @Override
    public void onTestStart(DataRecord testData, Description description) {
        garbageCollect();
    }

    @Override
    public void onTestEnd(DataRecord testData, Description description) {
        if (mRunEnd) {
            garbageCollect();
        }
    }

    private void garbageCollect() {
        if (!mSetUp) {
            return;
        }
        if (mWaitTime == null || mWaitTime < 0l) {
            // If no wait time specified or invalid, use default garbage collect call.
            mGcHelper.garbageCollect();
        } else {
            mGcHelper.garbageCollect(mWaitTime);
        }
    }
}
