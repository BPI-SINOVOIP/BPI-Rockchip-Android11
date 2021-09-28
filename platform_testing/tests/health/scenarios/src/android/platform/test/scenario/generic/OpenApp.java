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

import android.app.Instrumentation;
import android.os.SystemClock;
import android.platform.helpers.AbstractStandardAppHelper;
import android.platform.helpers.HelperAccessor;
import android.platform.helpers.IAppHelper;
import android.platform.test.option.LongOption;
import android.platform.test.option.StringOption;
import android.platform.test.rule.NaturalOrientationRule;
import android.platform.test.scenario.annotation.Scenario;

import org.junit.AfterClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Opens any application generically and exits after, based on the provided options. */
@Scenario
@RunWith(JUnit4.class)
public class OpenApp {
    // Class-level rules
    @ClassRule public static NaturalOrientationRule orientationRule = new NaturalOrientationRule();

    @ClassRule public static StringOption sNameOption = new StringOption("name").setRequired(true);
    @ClassRule public static StringOption sPkgOption = new StringOption("pkg").setRequired(true);

    @ClassRule
    public static LongOption sIdleMsOption =
            new LongOption("idleMs").setRequired(false).setDefault(0L);

    private static HelperAccessor<IGenericAppHelper> sHelper =
            new HelperAccessor<>(IGenericAppHelper.class);

    @Test
    public void testOpen() {
        IGenericAppHelper helper = sHelper.get();
        helper.setLauncherName(sNameOption.get());
        helper.setPackage(sPkgOption.get());
        helper.open();
        // Idle for specified time
        SystemClock.sleep(sIdleMsOption.get());
    }

    @AfterClass
    public static void closeApp() {
        // Doesn't need a real implementation.
        sHelper.get().exit();
    }

    /**
     * Interfaces are a needed workaround for the HelperAccessor/HelperManager. Similarly, these
     * extra methods are necessary to access options in a static class, which cannot access member
     * level rule variables. The class is static for HelperManager access reasons.
     */
    public interface IGenericAppHelper extends IAppHelper {
        public void setPackage(String pkg);

        public void setLauncherName(String name);
    }

    /** Create a generic app helper that can be used to open any package by option. */
    public static class GenericAppHelperImpl extends AbstractStandardAppHelper
            implements IGenericAppHelper {
        private String mPackage = "";
        private String mLauncherName = "";

        public GenericAppHelperImpl(Instrumentation instrumentation) {
            super(instrumentation);
        }

        @Override
        public void dismissInitialDialogs() {
            // Do nothing; dismiss dialogs isn't supported here.
        }

        @Override
        public String getPackage() {
            return mPackage;
        }

        @Override
        public String getLauncherName() {
            return mLauncherName;
        }

        public void setPackage(String pkg) {
            mPackage = pkg;
        }

        public void setLauncherName(String name) {
            mLauncherName = name;
        }
    }
}
