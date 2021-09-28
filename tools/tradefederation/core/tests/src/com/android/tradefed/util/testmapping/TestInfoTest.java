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
package com.android.tradefed.util.testmapping;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link TestOption}. */
@RunWith(JUnit4.class)
public class TestInfoTest {

    /** Test for {@link TestInfo#addOption()} implementation. */
    @Test
    public void testAddOption() throws Exception {
        Set<String> keywords = new HashSet<>();
        keywords.add("key1");
        keywords.add("key2");
        TestInfo info = new TestInfo("test1", "folder1", false, keywords);
        info.addOption(new TestOption("option2", "value2"));
        info.addOption(new TestOption("option1", "value1"));
        // Confirm the options are sorted.
        assertEquals("option1", info.getOptions().get(0).getName());
        assertEquals("value1", info.getOptions().get(0).getValue());
        assertEquals("option2", info.getOptions().get(1).getName());
        assertEquals("value2", info.getOptions().get(1).getValue());
        assertEquals(
                "test1\n\t" +
                "Options: option1:value1,option2:value2\n\t" +
                "Keywords: key1,key2\n\t" +
                "Sources: folder1\n\t" +
                "Host: false",
                info.toString());
        assertEquals("test1 - false", info.getNameAndHostOnly());
    }
}
