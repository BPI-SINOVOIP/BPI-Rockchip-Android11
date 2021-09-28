/*
 * Copyright 2019 The Android Open Source Project
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

package android.media.cts;

import android.media.TimedMetaData;
import android.test.AndroidTestCase;

import java.nio.charset.StandardCharsets;

/**
 * Tests for TimedMetaData.
 */
@NonMediaMainlineTest
public class TimedMetaDataTest extends AndroidTestCase {
    private static final String RAW_METADATA = "RAW_METADATA";

    public void testTimedMetaData() {
        TimedMetaData metadata = new TimedMetaData(1000, RAW_METADATA.getBytes());
        assertEquals(1000, metadata.getTimestamp());
        assertEquals(RAW_METADATA, new String(metadata.getMetaData(), StandardCharsets.UTF_8));
    }

    public void testTimedMetaDataNullMetaData() {
        try {
            TimedMetaData metadata = new TimedMetaData(0, null);
        } catch (IllegalArgumentException e) {
            // Expected
            return;
        }
        fail();
    }
}
