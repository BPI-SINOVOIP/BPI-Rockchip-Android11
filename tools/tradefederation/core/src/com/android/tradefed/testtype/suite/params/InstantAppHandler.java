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
package com.android.tradefed.testtype.suite.params;

import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;

import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TestAppInstallSetup;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestAnnotationFilterReceiver;

import java.util.HashSet;
import java.util.Set;

/** Handler for {@link ModuleParameters#INSTANT_APP}. */
public class InstantAppHandler implements IModuleParameter {

    /** {@inheritDoc} */
    @Override
    public String getParameterIdentifier() {
        return "instant";
    }

    /** {@inheritDoc} */
    @Override
    public void applySetup(IConfiguration moduleConfiguration) {
        // First, force target_preparers if they support it to install app in instant mode.
        for (IDeviceConfiguration deviceConfig : moduleConfiguration.getDeviceConfig()) {
            for (ITargetPreparer preparer : deviceConfig.getTargetPreparers()) {
                if (preparer instanceof TestAppInstallSetup) {
                    ((TestAppInstallSetup) preparer).setInstantMode(true);
                }
            }
        }
        // TODO: Second, notify HostTest that instant mode might be needed for apks.

        // Third, add filter to exclude @FullAppMode and allow @AppModeInstant
        for (IRemoteTest test : moduleConfiguration.getTests()) {
            if (test instanceof ITestAnnotationFilterReceiver) {
                ITestAnnotationFilterReceiver filterTest = (ITestAnnotationFilterReceiver) test;
                // Retrieve the current set of excludeAnnotations to maintain for after the
                // clearing/reset of the annotations.
                Set<String> excludeAnnotations = new HashSet<>(filterTest.getExcludeAnnotations());
                // Remove any global filter on AppModeInstant so instant mode tests can run.
                excludeAnnotations.remove(AppModeInstant.class.getCanonicalName());
                // Prevent full mode tests from running.
                excludeAnnotations.add(AppModeFull.class.getCanonicalName());
                // Reset the annotations of the tests
                filterTest.clearExcludeAnnotations();
                filterTest.addAllExcludeAnnotation(excludeAnnotations);
            }
        }
    }
}
