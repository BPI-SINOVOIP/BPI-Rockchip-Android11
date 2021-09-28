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
package android.icu.extratest;

import com.android.icu.util.Icu4cMetadata;
import org.junit.Before;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.Test;

import android.icu.dev.test.TestFmwk;
import android.icu.testsharding.MainTestShard;
import android.icu.util.VersionInfo;

import java.io.IOException;
import java.io.InputStream;
import java.util.Properties;

@MainTestShard
@RunWith(JUnit4.class)
public class AndroidIcuVersionTest extends TestFmwk {

    private static final String PROP_FILE = "android_icu_version.properties";
    private static final String VERSION_PROP_NAME = "version";

    private VersionInfo expectedIcuVersion;

    @Before
    public void setUp() throws IOException {
        try (InputStream in = AndroidIcuVersionTest.class.getResourceAsStream(PROP_FILE)) {
            Properties prop = new Properties();
            prop.load(in);
            String propValue = prop.getProperty(VERSION_PROP_NAME);
            assertNotNull("Expected ICU version can't be null", propValue);
            expectedIcuVersion = VersionInfo.getInstance(propValue);
        }
    }

    @Test
    public void testAndroidIcuVersion() {
        // Check ICU4J.
        VersionInfo actualIcu4jVersion = VersionInfo.ICU_VERSION;
        assertEquals("The ICU4J major version is not expected.",
                expectedIcuVersion.getMajor(), actualIcu4jVersion.getMajor());
        assertTrue("ICU4J minor version can't be smaller than the expected.",
                expectedIcuVersion.getMinor() <= actualIcu4jVersion.getMinor());

        // Check ICU4C.
        VersionInfo actualIcu4cVersion = VersionInfo.getInstance(Icu4cMetadata.getIcuVersion());
        assertEquals("The ICU4C major version is not expected.",
                expectedIcuVersion.getMajor(), actualIcu4cVersion.getMajor());
        assertTrue("ICU4C minor version can't be smaller than the expected.",
                expectedIcuVersion.getMinor() <= actualIcu4cVersion.getMinor());
    }

}
