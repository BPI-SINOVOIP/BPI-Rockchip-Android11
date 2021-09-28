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

import com.android.helpers.ThermalHelper;

/**
 * A {@link ThermalListener} captures thermal events during the test method.
 *
 * <p>Note: this should only be used per-run until an initialized value is set in the underlying
 * {@link ThermalHelper} class. That will be addressed in b/137793331, with an associated TODO.
 */
@OptionClass(alias = "thermal-collector")
public class ThermalListener extends BaseCollectionListener<StringBuilder> {
    public ThermalListener() {
        createHelperInstance(new ThermalHelper());
    }
}
