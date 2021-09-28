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
package com.android.compatibility.tradefed;

import static org.junit.Assert.assertEquals;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.tradefed.build.CompatibilityBuildProvider;


import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

@RunWith(JUnit4.class)
public class CSuiteTradefedTest {

    @Rule public final TemporaryFolder tempFolder = new TemporaryFolder();

    private static final String ROOT_DIR_PROPERTY_NAME = "CSUITE_ROOT";
    private static final String SUITE_FULL_NAME = "App Compatibility Test Suite";
    private static final String SUITE_NAME = "C-Suite";

    private CompatibilityBuildProvider mProvider;

    @Before
    public void setUp() throws Exception {
        System.setProperty(ROOT_DIR_PROPERTY_NAME, tempFolder.getRoot().getAbsolutePath());
        File base = tempFolder.newFolder("android-csuite");
        File tests = tempFolder.newFolder("testcases");

        mProvider =
                new CompatibilityBuildProvider() {
                    @Override
                    protected String getSuiteInfoName() {
                        return SUITE_NAME;
                    }

                    @Override
                    protected String getSuiteInfoFullname() {
                        return SUITE_FULL_NAME;
                    }
                };
    }

    @Test
    public void testSuiteInfoLoad() throws Exception {
        CompatibilityBuildHelper helper = new CompatibilityBuildHelper(mProvider.getBuild());
        assertEquals("Incorrect suite full name", SUITE_FULL_NAME, helper.getSuiteFullName());
        assertEquals("Incorrect suite name", SUITE_NAME, helper.getSuiteName());
    }

    @After
    public void cleanUp() throws Exception {
        System.clearProperty(ROOT_DIR_PROPERTY_NAME);
    }
}
