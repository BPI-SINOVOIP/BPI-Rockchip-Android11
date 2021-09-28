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
package com.android.compatibility.common.tradefed.config;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationFactory;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit tests for {@link ConfigurationFactory} imported from Trade Federation to check vts
 * configuration loading.
 */
@RunWith(JUnit4.class)
public class ConfigurationFactoryTest {
    private ConfigurationFactory mConfigFactory;

    @Before
    public void setUp() throws Exception {
        mConfigFactory = (ConfigurationFactory) ConfigurationFactory.getInstance();
    }

    /**
     * Sanity test to ensure all config names on classpath are loadable.
     */
    @Test
    public void testLoadAllConfigs() throws ConfigurationException {
        // we dry-run the templates otherwise it will always fail.
        mConfigFactory.loadAllConfigs(false);
    }

    /**
     * Sanity test to ensure all configs on classpath can be fully loaded and parsed.
     */
    @Test
    public void testLoadAndPrintAllConfigs() throws ConfigurationException {
        // Printing the help involves more checks since it tries to resolve the config objects.
        mConfigFactory.loadAndPrintAllConfigs();
    }
}