/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.tradefed.command;

import static org.junit.Assert.assertEquals;

import com.android.tradefed.config.ArgsOptionParser;
import com.android.tradefed.config.ConfigurationException;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link CommandOptions} */
@RunWith(JUnit4.class)
public class CommandOptionsTest {

    /**
     * Test that {@link CommandOptions#getLoopTime()} returns min-loop-time when
     * max-random-loop-time is not set.
     */
    @Test
    public void testGetLoopTime_minset() throws ConfigurationException {
        CommandOptions co = new CommandOptions();
        ArgsOptionParser p = new ArgsOptionParser(co);
        p.parse("--min-loop-time", "10");
        assertEquals(10, co.getLoopTime());
    }

    /** Test that setting multiple --min-loop-time values results in the latest value provided */
    @Test
    public void testGetLoopTime_least() throws ConfigurationException {
        CommandOptions co = new CommandOptions();
        ArgsOptionParser p = new ArgsOptionParser(co);
        p.parse("--min-loop-time", "5", "--min-loop-time", "0");
        assertEquals(0, co.getLoopTime());
    }
}
