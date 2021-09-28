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

import com.android.helpers.FreeMemHelper;

/**
 * A {@link FreeMemListener} that captures and records free memory available
 * in the device.
 */
@OptionClass(alias = "freemem-listener")
public class FreeMemListener extends BaseCollectionListener<Long> {
    private static final String DROP_CACHE_ARG = "drop-cache";

    private FreeMemHelper mFreeMemHelper = new FreeMemHelper();

    public FreeMemListener() {
        createHelperInstance(mFreeMemHelper);
    }

    @VisibleForTesting
    public FreeMemListener(Bundle args, FreeMemHelper helper) {
        super(args, helper);
    }

    /**
     * Adds the options for free memory collector.
     */
    @Override
    public void setupAdditionalArgs() {
        Bundle args = getArgsBundle();
        // By default set to false
        boolean isDropCache = Boolean.parseBoolean(args.getString(DROP_CACHE_ARG));

        // Set the flag to drop the cache before taking the memory measurement.
        if (isDropCache) {
            mFreeMemHelper.setDropCache();
        }
    }
}
