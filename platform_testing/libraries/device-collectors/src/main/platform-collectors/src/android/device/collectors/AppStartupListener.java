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

import com.android.helpers.AppStartupHelper;

/**
 * A {@link AppStartupListener} that captures app startup during the test method.
 *
 * Do NOT throw exception anywhere in this class. We don't want to halt the test when metrics
 * collection fails.
 */
@OptionClass(alias = "appstartup-collector")
public class AppStartupListener extends BaseCollectionListener<StringBuilder> {

    private static final String DISABLE_PROC_START_DETAILS = "disable_process_start_details";

    public AppStartupListener() {
        createHelperInstance(new AppStartupHelper());
    }

    /**
     * Adds the options to filter the app startup metrics
     */
    @Override
    public void setupAdditionalArgs() {
        Bundle args = getArgsBundle();
        AppStartupHelper appstartupHelper = (AppStartupHelper) mHelper;
        if ("true".equals(args.getString(DISABLE_PROC_START_DETAILS))) {
            appstartupHelper.setDisableProcStartDetails();
        }
    }
}
