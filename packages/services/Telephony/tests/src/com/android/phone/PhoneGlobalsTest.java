/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.phone;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.TelephonyTestBase;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Locale;


@RunWith(AndroidJUnit4.class)
public class PhoneGlobalsTest extends TelephonyTestBase {

    @Before
    public void setUp() throws Exception {
        super.setUp();
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @Test
    public void testGetImsResources() throws Exception {
        // Do not use test context here, we are testing that overlaying for different locales works
        // correctly
        Context realContext = InstrumentationRegistry.getTargetContext();
        String defaultImsMmtelPackage = getResourcesForLocale(realContext, Locale.US).getString(
                R.string.config_ims_mmtel_package);
        String defaultImsMmtelPackageUk = getResourcesForLocale(realContext, Locale.UK).getString(
                R.string.config_ims_mmtel_package);
        assertEquals("locales changed IMS package configuration!", defaultImsMmtelPackage,
                defaultImsMmtelPackageUk);
    }

    private Resources getResourcesForLocale(Context context, Locale locale) {
        Configuration config = new Configuration();
        config.setToDefaults();
        config.setLocale(locale);
        Context localeContext = context.createConfigurationContext(config);
        return localeContext.getResources();
    }
}
