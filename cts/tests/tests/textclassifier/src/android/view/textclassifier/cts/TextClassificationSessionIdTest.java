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

package android.view.textclassifier.cts;

import static com.google.common.truth.Truth.assertThat;

import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassificationSessionId;
import android.view.textclassifier.TextClassifier;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Test;

@SmallTest
public class TextClassificationSessionIdTest {
    private final TextClassificationManager mTcm =
            ApplicationProvider.getApplicationContext().getSystemService(
                    TextClassificationManager.class);

    @After
    public void tearDown() {
        // Clear the custom TC.
        mTcm.setTextClassifier(null);
    }

    @Test
    public void getValue() {
        mTcm.setTextClassifier(TextClassifier.NO_OP);
        TextClassifier textClassifier = mTcm.createTextClassificationSession(
                new TextClassificationContext.Builder(
                        "com.pkg",
                        TextClassifier.WIDGET_TYPE_TEXTVIEW)
                        .build());
        SelectionEvent startedEvent =
                SelectionEvent.createSelectionStartedEvent(
                        SelectionEvent.INVOCATION_LINK, /* start= */ 10);

        textClassifier.onSelectionEvent(startedEvent);
        TextClassificationSessionId sessionId = startedEvent.getSessionId();

        assertThat(sessionId).isNotNull();
        assertThat(sessionId.getValue()).isNotEmpty();
    }
}
