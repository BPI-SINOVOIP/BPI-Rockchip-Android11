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

import com.android.helpers.SfStatsCollectionHelper;

@OptionClass(alias = "sfstats-listener")
public class SfStatsListener extends BaseCollectionListener<Double> {
    private static final String LOG_TAG = SfStatsListener.class.getSimpleName();

    public SfStatsListener() {
        createHelperInstance(new SfStatsCollectionHelper());
    }

    @VisibleForTesting
    public SfStatsListener(Bundle args, SfStatsCollectionHelper helper) {
        super(args, helper);
    }
}
