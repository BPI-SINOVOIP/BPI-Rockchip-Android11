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

import android.media.SubtitleData;
import android.test.AndroidTestCase;

import java.nio.charset.StandardCharsets;

/**
 * Tests for SubtitleData.
 */
@NonMediaMainlineTest
public class SubtitleDataTest extends AndroidTestCase {
    private static final String SUBTITLE_RAW_DATA = "RAW_DATA";

    public void testSubtitleData() {
        SubtitleData subtitle = new SubtitleData(1, 1000, 100, SUBTITLE_RAW_DATA.getBytes());
        assertEquals(1, subtitle.getTrackIndex());
        assertEquals(1000, subtitle.getStartTimeUs());
        assertEquals(100, subtitle.getDurationUs());
        assertEquals(SUBTITLE_RAW_DATA, new String(subtitle.getData(), StandardCharsets.UTF_8));
    }

    public void testSubtitleDataNullData() {
        try {
            SubtitleData subtitle2 = new SubtitleData(1, 0, 0, null);
        } catch (IllegalArgumentException e) {
            // Expected
            return;
        }
        fail();
    }
}
