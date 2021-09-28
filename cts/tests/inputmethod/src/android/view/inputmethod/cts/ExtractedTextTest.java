/*
 * Copyright (C) 2008 The Android Open Source Project
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
import static org.junit.Assert.assertTrue;

import android.os.Parcel;
import android.text.Annotation;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.view.inputmethod.ExtractedText;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ExtractedTextTest {

    private static final int EXPECTED_FLAGS = ExtractedText.FLAG_SINGLE_LINE;
    private static final int EXPECTED_SELECTION_START = 2;
    private static final int EXPECTED_SELECTION_END = 11;
    private static final int EXPECTED_START_OFFSET = 1;

    private static final Annotation EXPECTED_TEXT_ANNOTATION =
            new Annotation("testKey", "testValue");
    private static final Annotation EXPECTED_HINT_ANNOTATION =
            new Annotation("testHintKey", "testHintValue");

    private static SpannableStringBuilder getExpectedText() {
        final SpannableStringBuilder text = new SpannableStringBuilder(
                "01234567890abcdef".subSequence(EXPECTED_SELECTION_START, EXPECTED_SELECTION_END));
        text.setSpan(EXPECTED_TEXT_ANNOTATION, 0, text.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        return text;
    }

    private static SpannableStringBuilder getExpectedHint() {
        final SpannableStringBuilder text = new SpannableStringBuilder("hint");
        final Annotation span = new Annotation("testHintKey", "testHintValue");
        text.setSpan(EXPECTED_HINT_ANNOTATION, 1, 2, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        return text;
    }

    /**
     * @return An instance of {@link ExtractedText} for test.
     */
    @AnyThread
    @NonNull
    static ExtractedText createForTest() {
        final ExtractedText extractedText = new ExtractedText();
        extractedText.flags = EXPECTED_FLAGS;
        extractedText.selectionStart = EXPECTED_SELECTION_START;
        extractedText.selectionEnd = EXPECTED_SELECTION_END;
        extractedText.startOffset = EXPECTED_START_OFFSET;
        extractedText.text = getExpectedText();
        extractedText.hint = getExpectedHint();
        return extractedText;
    }

    /**
     * Ensures that the specified object is equivalent to the one returned from
     * {@link #createForTest()}.
     *
     * @param extractedText {@link ExtractedText} to be tested.
     */
    static void assertTestInstance(@Nullable ExtractedText extractedText) {
        assertNotNull(extractedText);

        assertEquals(EXPECTED_FLAGS, extractedText.flags);
        assertEquals(EXPECTED_SELECTION_START, extractedText.selectionStart);
        assertEquals(EXPECTED_SELECTION_END, extractedText.selectionEnd);
        assertEquals(EXPECTED_START_OFFSET, extractedText.startOffset);

        assertEquals(getExpectedText().toString(), extractedText.text.toString());
        {
            assertTrue(extractedText.text instanceof Spanned);
            final Spanned spannedText = (Spanned) extractedText.text;
            final Annotation[] textAnnotations =
                    spannedText.getSpans(0, spannedText.length(), Annotation.class);
            assertEquals(1, textAnnotations.length);
            final Annotation textAnnotation = textAnnotations[0];
            assertEquals(EXPECTED_TEXT_ANNOTATION.getKey(), textAnnotation.getKey());
            assertEquals(EXPECTED_TEXT_ANNOTATION.getValue(), textAnnotation.getValue());
        }

        assertEquals(getExpectedHint().toString(), extractedText.hint.toString());
        {
            assertTrue(extractedText.hint instanceof Spanned);
            final Spanned spannedHint = (Spanned) extractedText.hint;
            final Annotation[] hintAnnotations =
                    spannedHint.getSpans(0, spannedHint.length(), Annotation.class);
            assertEquals(1, hintAnnotations.length);
            final Annotation hintAnnotation = hintAnnotations[0];
            assertEquals(EXPECTED_HINT_ANNOTATION.getKey(), hintAnnotation.getKey());
            assertEquals(EXPECTED_HINT_ANNOTATION.getValue(), hintAnnotation.getValue());
        }

        assertEquals(0, extractedText.describeContents());
    }


    /**
     * Ensures that {@link ExtractedText} can be serialized and deserialized via {@link Parcel}.
     */
    @Test
    public void testWriteToParcel() {
        final ExtractedText original = createForTest();
        assertTestInstance(original);
        final ExtractedText copied = cloneViaParcel(original);
        assertTestInstance(copied);
    }

    @NonNull
    private static ExtractedText cloneViaParcel(@NonNull ExtractedText src) {
        final Parcel parcel = Parcel.obtain();
        try {
            src.writeToParcel(parcel, 0);
            parcel.setDataPosition(0);
            return ExtractedText.CREATOR.createFromParcel(parcel);
        } finally {
            parcel.recycle();
        }
    }
}
