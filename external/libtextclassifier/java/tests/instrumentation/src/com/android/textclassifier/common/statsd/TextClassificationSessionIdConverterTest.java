/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.textclassifier.common.statsd;

import static com.google.common.truth.Truth.assertThat;

import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassificationContext;
import android.view.textclassifier.TextClassificationManager;
import android.view.textclassifier.TextClassifier;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.android.textclassifier.common.logging.TextClassificationSessionId;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class TextClassificationSessionIdConverterTest {

  @Test
  public void testTextSelectionEvent_minimal() {
    TextClassificationManager textClassificationManager =
        ApplicationProvider.getApplicationContext()
            .getSystemService(TextClassificationManager.class);
    textClassificationManager.setTextClassifier(TextClassifier.NO_OP);
    TextClassifier textClassifier =
        textClassificationManager.createTextClassificationSession(
            new TextClassificationContext.Builder("com.pkg", TextClassifier.WIDGET_TYPE_TEXTVIEW)
                .build());
    SelectionEvent startedEvent =
        SelectionEvent.createSelectionStartedEvent(SelectionEvent.INVOCATION_LINK, /* start= */ 10);

    textClassifier.onSelectionEvent(startedEvent);
    android.view.textclassifier.TextClassificationSessionId platformSessionId =
        startedEvent.getSessionId();
    TextClassificationSessionId textClassificationSessionId =
        TextClassificationSessionIdConverter.fromPlatform(platformSessionId);

    assertThat(textClassificationSessionId).isNotNull();
    assertThat(textClassificationSessionId.getValue()).isEqualTo(platformSessionId.getValue());
  }
}
