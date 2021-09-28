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
package android.platform.helpers;

import android.app.Instrumentation;

public class PerformanceLaunchHelperImpl extends AbstractStandardAppHelper
        implements IPerformanceLaunchHelper {
    private static final String UI_PACKAGE_NAME = "com.android.performanceLaunch";

    public PerformanceLaunchHelperImpl(Instrumentation instrumentation) {
        super(instrumentation);
    }

    /** {@inheritDoc} */
    @Override
    public String getPackage() {
        return UI_PACKAGE_NAME;
    }

    /** {@inheritDoc} */
    @Override
    public String getLauncherName() {
        return "PerformanceLaunch";
    }

    /** {@inheritDoc} */
    @Override
    public void dismissInitialDialogs() {
        // There are no initial dialogs to dismiss for Business Card.
    }
}
