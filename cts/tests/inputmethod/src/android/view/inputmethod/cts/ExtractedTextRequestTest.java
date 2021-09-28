/*
 * Copyright (C) 2009 The Android Open Source Project
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

package android.view.inputmethod.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.os.Parcel;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ExtractedTextRequestTest {

    private static final int EXPECTED_FLAGS = InputConnection.GET_TEXT_WITH_STYLES;
    private static final int EXPECTED_HINT_MAX_CHARS = 100;
    private static final int EXPECTED_HINT_MAX_LINES = 10;
    private static final int EXPECTED_TOKEN = 2;

    /**
     * @return An instance of {@link ExtractedTextRequest} for test.
     */
    @AnyThread
    @NonNull
    static ExtractedTextRequest createForTest() {
        final ExtractedTextRequest request = new ExtractedTextRequest();
        request.flags = EXPECTED_FLAGS;
        request.hintMaxChars = EXPECTED_HINT_MAX_CHARS;
        request.hintMaxLines = EXPECTED_HINT_MAX_LINES;
        request.token = EXPECTED_TOKEN;
        return request;
    }

    /**
     * Ensures that the specified object is equivalent to the one returned from
     * {@link #createForTest()}.
     *
     * @param request {@link ExtractedTextRequest} to be tested.
     */
    static void assertTestInstance(@Nullable ExtractedTextRequest request) {
        assertNotNull(request);

        assertEquals(EXPECTED_FLAGS, request.flags);
        assertEquals(EXPECTED_HINT_MAX_CHARS, request.hintMaxChars);
        assertEquals(EXPECTED_HINT_MAX_LINES, request.hintMaxLines);
        assertEquals(EXPECTED_TOKEN, request.token);
        assertEquals(0, request.describeContents());
    }

    /**
     * Ensures that {@link ExtractedTextRequest} can be serialized and deserialized via
     * {@link Parcel}.
     */
    @Test
    public void testWriteToParcel() {
        final ExtractedTextRequest original = createForTest();
        assertTestInstance(original);
        final ExtractedTextRequest copied = cloneViaParcel(original);
        assertTestInstance(copied);
    }

    @NonNull
    private static ExtractedTextRequest cloneViaParcel(@NonNull ExtractedTextRequest src) {
        final Parcel parcel = Parcel.obtain();
        try {
            src.writeToParcel(parcel, 0);
            parcel.setDataPosition(0);
            return ExtractedTextRequest.CREATOR.createFromParcel(parcel);
        } finally {
            parcel.recycle();
        }
    }
}
