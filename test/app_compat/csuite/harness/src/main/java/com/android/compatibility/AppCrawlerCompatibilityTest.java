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

package com.android.compatibility;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.InstrumentationTest;

import java.util.ArrayList;
import java.util.Arrays;

@OptionClass(alias = "app-compatibility-crawler")
public final class AppCrawlerCompatibilityTest extends AppCompatibilityTest {
    private static final String WALKMAN_RUN_MS_LABEL = "maxDuration";
    private static final String WALKMAN_STEPS_LABEL = "maxSteps";

    @Option(
            name = "walkman-run-ms",
            description = "Time to run walkman in msecs (only used if test-strategy=walkman).")
    private int mWalkmanRunMs = 60 * 1000;

    @Option(
            name = "walkman-steps",
            description =
                    "Max number of steps to run walkman (only used if test-strategy=walkman)."
                            + " -1 for no limit")
    private int mWalkmanSteps = -1;

    public AppCrawlerCompatibilityTest() {
        super(
                "com.google.android.apps.common.walkman.apps",
                "com.google.android.apps.common.testing.testrunner"
                        + ".Google3InstrumentationTestRunner",
                /*
                 * We are using /google/data/ro/teams/walkman/walkman.apk which has parameter
                 * "packages" unlike the up-to-date version in source which uses "package"
                 * see: com.google.android.apps.common.walkman.apps.EngineFactory::getCrawlEngine.
                 * This currently works with the up-to-date version in source, as well.
                 *
                 * Neither of these should be confused with "package_to_launch", which is used by
                 * AppCompatibilityRunner
                 */
                "packages");
    }

    @Override
    public InstrumentationTest createInstrumentationTest(String packageBeingTested) {
        InstrumentationTest instrTest = createDefaultInstrumentationTest(packageBeingTested);

        instrTest.addInstrumentationArg(WALKMAN_RUN_MS_LABEL, Integer.toString(mWalkmanRunMs));
        instrTest.addInstrumentationArg(WALKMAN_STEPS_LABEL, Integer.toString(mWalkmanSteps));

        String launcherClass = mLauncherPackage + ".WalkmanInstrumentationEntry";
        instrTest.setClassName(launcherClass);
        /*
         * InstrumentationTest can't deduce the exact test to run, so we specify it
         * manually. Note that the TestDescription we use here is a different one from
         * the one returned by {@link TestStrategy#createTestDescription}.
         *
         * This list is required to be mutable, so we wrap in ArrayList.
         */
        instrTest.setTestsToRun(
                new ArrayList<>(Arrays.asList(new TestDescription(launcherClass, "testEntry"))));

        return instrTest;
    }
}
