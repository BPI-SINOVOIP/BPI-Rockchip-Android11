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

import com.android.helpers.JankCollectionHelper;

import java.util.Arrays;

/**
 * A {@link BaseCollectionListener} that captures and records jank metrics for a specific package or
 * for all packages if none are specified.
 */
@OptionClass(alias = "jank-listener")
public class JankListener extends BaseCollectionListener<Double> {
    private static final String LOG_TAG = JankListener.class.getSimpleName();

    @VisibleForTesting static final String PACKAGE_SEPARATOR = ",";
    @VisibleForTesting static final String PACKAGE_NAMES_KEY = "jank-package-names";

    public JankListener() {
        createHelperInstance(new JankCollectionHelper());
    }

    @VisibleForTesting
    public JankListener(Bundle args, JankCollectionHelper helper) {
        super(args, helper);
    }

    /** Tracks the provided packages if specified, or all packages if not specified. */
    @Override
    public void setupAdditionalArgs() {
        Bundle args = getArgsBundle();
        String pkgs = args.getString(PACKAGE_NAMES_KEY);
        if (pkgs != null) {
            Log.v(LOG_TAG, String.format("Adding packages: %s", pkgs));
            // Basic malformed input check: trim packages and remove empty ones.
            String[] splitPkgs =
                    Arrays.stream(pkgs.split(PACKAGE_SEPARATOR))
                            .map(String::trim)
                            .filter(item -> !item.isEmpty())
                            .toArray(String[]::new);
            ((JankCollectionHelper) mHelper).addTrackedPackages(splitPkgs);
        } else {
            Log.v(LOG_TAG, "Tracking all packages for jank.");
        }
    }
}
