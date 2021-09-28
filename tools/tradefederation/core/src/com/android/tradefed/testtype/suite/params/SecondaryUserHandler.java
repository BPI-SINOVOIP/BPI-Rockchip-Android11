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
package com.android.tradefed.testtype.suite.params;

import android.platform.test.annotations.SystemUserOnly;

import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.targetprep.CreateUserPreparer;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.RunCommandTargetPreparer;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestAnnotationFilterReceiver;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Handler for {@link ModuleParameters#SECONDARY_USER}. */
public class SecondaryUserHandler implements IModuleParameter {
    @Override
    public String getParameterIdentifier() {
        return "secondary_user";
    }

    /** {@inheritDoc} */
    @Override
    public void addParameterSpecificConfig(IConfiguration moduleConfiguration) {
        for (IDeviceConfiguration deviceConfig : moduleConfiguration.getDeviceConfig()) {
            List<ITargetPreparer> preparers = deviceConfig.getTargetPreparers();
            // The first things module will do is switch to a secondary user
            preparers.add(0, new CreateUserPreparer());
            // Add a preparer to setup the location settings on the new user
            preparers.add(1, createLocationPreparer());
        }
    }

    @Override
    public void applySetup(IConfiguration moduleConfiguration) {
        // Add filter to exclude @SystemUserOnly
        for (IRemoteTest test : moduleConfiguration.getTests()) {
            if (test instanceof ITestAnnotationFilterReceiver) {
                ITestAnnotationFilterReceiver filterTest = (ITestAnnotationFilterReceiver) test;
                // Retrieve the current set of excludeAnnotations to maintain for after the
                // clearing/reset of the annotations.
                Set<String> excludeAnnotations = new HashSet<>(filterTest.getExcludeAnnotations());
                // Prevent system user only tests from running
                excludeAnnotations.add(SystemUserOnly.class.getCanonicalName());
                // Reset the annotations of the tests
                filterTest.clearExcludeAnnotations();
                filterTest.addAllExcludeAnnotation(excludeAnnotations);
            }
        }
    }

    private RunCommandTargetPreparer createLocationPreparer() {
        RunCommandTargetPreparer location = new RunCommandTargetPreparer();
        location.addRunCommand("settings put secure location_providers_allowed +gps");
        location.addRunCommand("settings put secure location_providers_allowed +network");
        return location;
    }
}
