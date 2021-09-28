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

package android.platform.test.scenario.generic;

import android.os.SystemClock;
import android.platform.helpers.HelperAccessor;
import android.platform.test.option.LongOption;
import android.platform.test.option.StringOption;
import android.platform.test.rule.NaturalOrientationRule;
import android.platform.test.scenario.annotation.AppSetup;
import android.platform.test.scenario.annotation.Scenario;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Opens multiple applications generically and exits after, based on the provided options. */
@AppSetup
@Scenario
@RunWith(JUnit4.class)
public class OpenApps {
    // Class-level rules
    @ClassRule public static NaturalOrientationRule orientationRule = new NaturalOrientationRule();

    @Rule public final StringOption mNamesOption = new StringOption("names").setRequired(true);
    @Rule public final StringOption mPkgsOption = new StringOption("pkgs").setRequired(true);
    @Rule public final LongOption mIdleMs = new LongOption("idleMs").setDefault(1000L);

    private static HelperAccessor<OpenApp.IGenericAppHelper> sHelper =
            new HelperAccessor<>(OpenApp.IGenericAppHelper.class);

    @Test
    public void testOpen() {
        String[] names = mNamesOption.get().split(",");
        String[] pkgs = mPkgsOption.get().split(",");

        if (names.length != pkgs.length) {
            throw new IllegalArgumentException("Names and packages length must match, but don't.");
        }

        for (int i = 0; i < names.length; i++) {
            String name = names[i];
            String pkg = pkgs[i];

            OpenApp.IGenericAppHelper helper = sHelper.get();
            helper.setLauncherName(name);
            helper.setPackage(pkg);
            // Launch the application.
            helper.open();
            // Sleep for a little bit.
            SystemClock.sleep(mIdleMs.get());
            // Exit the application.
            helper.exit();
        }
    }
}
