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
package com.android.tradefed.lite;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * This class is intended to purely test the processing pipeline of JUnit tests in the isolation
 * environment. Each test should have deterministic, dependency-free behavior so that we can assert
 * that they pass/fail etc. and that these are assumed to do so reliably (to test the test runner).
 */
@RunWith(JUnit4.class)
public final class SampleTests {

    @Test
    public void testAddition() {
        int expected = 4;
        int res = 2 + 2;
        assertEquals(expected, res);
    }

    @Test
    public void testMultiplication() {
        int expected = 4;
        int res = 2 * 2;
        assertEquals(expected, res);
    }

    @Test
    public void testSubtraction() {
        int expected = 10;
        int res = 12 - 2;
        assertEquals(expected, res);
    }
}
