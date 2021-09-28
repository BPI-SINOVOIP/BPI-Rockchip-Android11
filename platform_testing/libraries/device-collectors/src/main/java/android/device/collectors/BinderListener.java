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
import com.android.helpers.BinderCollectionHelper;

import java.util.Arrays;

/**
 * A {@link BaseCollectionListener} that captures and records Binder metrics for a specific package
 * or for all packages if none are specified.
 */
@OptionClass(alias = "binder-listener")
public class BinderListener extends BaseCollectionListener<Double> {

    private static final String LOG_TAG = BinderListener.class.getSimpleName();

    @VisibleForTesting static final String PROCESS_SEPARATOR = ",";
    @VisibleForTesting static final String PROCESS_NAMES_KEY = "binder-process-names";

    public BinderListener() {
        createHelperInstance(new BinderCollectionHelper());
    }

    @VisibleForTesting
    public BinderListener(Bundle args, BinderCollectionHelper helper) {
        super(args, helper);
    }

    /** Tracks the provided processes if specified, or all processes if not specified. */
    @Override
    public void setupAdditionalArgs() {
        Bundle args = getArgsBundle();
        String processes = args.getString(PROCESS_NAMES_KEY);
        if (processes != null) {
            Log.v(LOG_TAG, String.format("Adding processes: %s", processes));
            // Basic malformed input check: trim processes and remove empty ones.
            String[] splitProcesses =
                    Arrays.stream(processes.split(PROCESS_SEPARATOR))
                            .map(String::trim)
                            .filter(item -> !item.isEmpty())
                            .toArray(String[]::new);
            ((BinderCollectionHelper) mHelper).addTrackedProcesses(splitProcesses);
        } else {
            Log.v(LOG_TAG, "Tracking all processes for binder.");
        }
        Log.i(LOG_TAG, "Binder setup additonal args.");
    }
}
