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

package com.android.tradefed.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.Map;

/** Unit tests for {@link ResourceUtil}. */
@RunWith(JUnit4.class)
public class ResourceUtilTest {

    /**
     * Test {@link ResourceUtil#readConfigurationFromResource(String)} when the resource doesn't
     * exists.
     */
    @Test
    public void testLoadNonExisting() {
        Map<String, String> res = ResourceUtil.readConfigurationFromResource("this/doesnt/exists");
        assertTrue(res.isEmpty());
    }

    /** Test {@link ResourceUtil#readConfigurationFromResource(String)}. */
    @Test
    public void testLoad() {
        Map<String, String> res =
                ResourceUtil.readConfigurationFromResource("/testdata/test_property.cfg");
        assertEquals("value", res.get("key"));
        assertEquals("value2", res.get("key2"));
    }

    /**
     * Test {@link ResourceUtil#readConfigurationFromResource(String)} when the resource doesn't
     * have an expected format.
     */
    @Test
    public void testLoad_invalid() {
        Map<String, String> res =
                ResourceUtil.readConfigurationFromResource("/testdata/invalid_property.cfg");
        assertTrue(res.isEmpty());
    }
}
