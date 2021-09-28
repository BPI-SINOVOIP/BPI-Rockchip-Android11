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
package com.android.tradefed.result;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link MultiFailureDescription}. */
@RunWith(JUnit4.class)
public class MultiFailureDescriptionTest {

    private MultiFailureDescription mMultiFailureDesc;

    @Test
    public void testCreation() {
        FailureDescription failure1 = FailureDescription.create("error message 1");
        FailureDescription failure2 = FailureDescription.create("error message 2");
        mMultiFailureDesc = new MultiFailureDescription(failure1, failure2);
        assertEquals(2, mMultiFailureDesc.getFailures().size());
        assertEquals(
                "There were 2 failures:\n  error message 1\n  error message 2",
                mMultiFailureDesc.toString());
    }

    @Test
    public void testCreation_nested() {
        FailureDescription failure1 = FailureDescription.create("error message 1");
        FailureDescription failure2 = FailureDescription.create("error message 2");
        MultiFailureDescription nested = new MultiFailureDescription(failure2);
        mMultiFailureDesc = new MultiFailureDescription(failure1, nested);
        assertEquals(2, mMultiFailureDesc.getFailures().size());
        assertEquals(
                "There were 2 failures:\n  error message 1\n  error message 2",
                mMultiFailureDesc.toString());
    }
}
