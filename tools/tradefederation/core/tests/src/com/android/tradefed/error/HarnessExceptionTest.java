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
package com.android.tradefed.error;

import static org.junit.Assert.assertEquals;

import com.android.tradefed.result.error.ErrorIdentifier;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.util.SerializationUtil;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit tests for {@link HarnessException}. */
@RunWith(JUnit4.class)
public class HarnessExceptionTest {

    /** Ensure that the harness exception is serializable like a proper exception. */
    @Test
    public void testSerializable() throws Exception {
        ErrorIdentifier id = InfraErrorIdentifier.UNDETERMINED;
        HarnessException e = new HarnessException(id);
        assertEquals("com.android.tradefed.error.HarnessExceptionTest", e.getOrigin());
        assertEquals(InfraErrorIdentifier.UNDETERMINED, e.getErrorId());

        File serial = SerializationUtil.serialize(e);
        HarnessException deserialized =
                (HarnessException) SerializationUtil.deserialize(serial, true);

        assertEquals("com.android.tradefed.error.HarnessExceptionTest", deserialized.getOrigin());
        assertEquals(InfraErrorIdentifier.UNDETERMINED, deserialized.getErrorId());
    }
}
