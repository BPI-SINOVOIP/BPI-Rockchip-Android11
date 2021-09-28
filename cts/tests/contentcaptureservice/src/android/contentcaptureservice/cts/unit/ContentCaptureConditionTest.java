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
package android.contentcaptureservice.cts.unit;

import static android.view.contentcapture.ContentCaptureCondition.FLAG_IS_REGEX;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.content.LocusId;
import android.platform.test.annotations.AppModeFull;
import android.view.contentcapture.ContentCaptureCondition;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnitRunner;

@AppModeFull(reason = "unit test")
@RunWith(MockitoJUnitRunner.class)
public class ContentCaptureConditionTest {

    private static final int NO_FLAGS = 0;

    private final LocusId mLocusId = new LocusId("http://com.example/");

    @Test
    public void testInvalid_nullLocusId() {
        assertThrows(NullPointerException.class, () -> new ContentCaptureCondition(null, NO_FLAGS));
    }

    @Test
    public void testValid() {
        final ContentCaptureCondition condition =
                new ContentCaptureCondition(mLocusId, FLAG_IS_REGEX);
        assertThat(condition).isNotNull();
        assertThat(condition.getLocusId()).isSameAs(mLocusId);
        assertThat(condition.getFlags()).isSameAs(FLAG_IS_REGEX);
    }
}
