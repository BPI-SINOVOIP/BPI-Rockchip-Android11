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

import com.android.textclassifier.common.logging.TextClassificationSessionId;
import javax.annotation.Nullable;

/**
 * Converts between {@link TextClassificationSessionId} and {@link
 * android.view.textclassifier.TextClassificationSessionId}.
 */
public final class TextClassificationSessionIdConverter {

  private TextClassificationSessionIdConverter() {}

  @Nullable
  public static TextClassificationSessionId fromPlatform(
      @Nullable android.view.textclassifier.TextClassificationSessionId sessionId) {
    if (sessionId == null) {
      return null;
    }
    return TextClassificationSessionId.unflattenFromString(sessionId.getValue());
  }
}
