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

package android.view.inputmethod.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.testng.Assert.assertThrows;

import android.content.Context;
import android.os.AsyncTask;
import android.os.Parcel;
import android.util.Size;
import android.view.inputmethod.InlineSuggestion;
import android.view.inputmethod.InlineSuggestionInfo;
import android.widget.inline.InlineContentView;
import android.widget.inline.InlinePresentationSpec;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.function.Consumer;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class InlineSuggestionTest {
    private static final String TAG = "InlineSuggestionTest";

    private InlinePresentationSpec mInlinePresentationSpec = new InlinePresentationSpec.Builder(
            new Size(100, 100), new Size(400, 100)).build();

    @Test
    public void testNullInlineSuggestionInfoThrowsException() {
        assertThrows(NullPointerException.class,
                () -> InlineSuggestion.newInlineSuggestion(/* info */ null));
    }

    @Test
    public void testInlineSuggestionValues() {
        InlineSuggestion suggestion = createInlineSuggestion();

        assertThat(suggestion.getInfo()).isNotNull();
    }

    @Test
    public void testInlineSuggestionParcelizeDeparcelize() {
        InlineSuggestion suggestion = createInlineSuggestion();
        Parcel p = Parcel.obtain();
        suggestion.writeToParcel(p, 0);
        p.setDataPosition(0);

        InlineSuggestion targetSuggestion = InlineSuggestion.CREATOR.createFromParcel(p);
        p.recycle();

        assertThat(targetSuggestion).isEqualTo(suggestion);
    }

    @Test
    public void testInflateInvalidSizeThrowsException() {
        InlineSuggestion suggestion = createInlineSuggestion();
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Consumer<InlineContentView> mockConsumer = mock(Consumer.class);

        assertThrows(IllegalArgumentException.class,
                () -> suggestion.inflate(context, new Size(10, 10), AsyncTask.THREAD_POOL_EXECUTOR,
                        mockConsumer));
    }

    @Test
    public void testInflateNoContentProviderAcceptNullView() throws Exception {
        InlineSuggestion suggestion = createInlineSuggestion();
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        CountDownLatch latch = new CountDownLatch(1);
        Consumer<InlineContentView> consumer = view -> {
            assertThat(view).isNull();
            latch.countDown();
        };
        suggestion.inflate(context, new Size(100, 100), AsyncTask.THREAD_POOL_EXECUTOR,
                consumer);
        latch.await();
    }

    @Test
    public void testInflateTwiceThrowsException() {
        InlineSuggestion suggestion = createInlineSuggestion();
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Consumer<InlineContentView> mockConsumer = mock(Consumer.class);

        suggestion.inflate(context, new Size(100, 100), AsyncTask.THREAD_POOL_EXECUTOR,
                mockConsumer);
        assertThrows(IllegalStateException.class,
                () -> suggestion.inflate(context, new Size(100, 100),
                        AsyncTask.THREAD_POOL_EXECUTOR, mockConsumer));
    }

    private InlineSuggestion createInlineSuggestion() {
        InlineSuggestionInfo info = InlineSuggestionInfo.newInlineSuggestionInfo(
                mInlinePresentationSpec, InlineSuggestionInfo.SOURCE_AUTOFILL, new String[]{""},
                InlineSuggestionInfo.TYPE_SUGGESTION, /* isPinned */ false);
        return InlineSuggestion.newInlineSuggestion(info);
    }
}
