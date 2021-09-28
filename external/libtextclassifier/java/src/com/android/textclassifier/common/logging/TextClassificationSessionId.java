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

package com.android.textclassifier.common.logging;

import com.google.common.base.Preconditions;
import java.util.Locale;
import java.util.Objects;
import java.util.UUID;

/** This class represents the id of a text classification session. */
public final class TextClassificationSessionId {
  private final String value;

  /** Creates a new instance. */
  public TextClassificationSessionId() {
    this(UUID.randomUUID().toString());
  }

  private TextClassificationSessionId(String value) {
    this.value = Preconditions.checkNotNull(value);
  }

  @Override
  public String toString() {
    return String.format(Locale.US, "TextClassificationSessionId {%s}", value);
  }

  @Override
  public int hashCode() {
    return value.hashCode();
  }

  @Override
  public boolean equals(Object o) {
    if (this == o) {
      return true;
    }
    if (o == null || getClass() != o.getClass()) {
      return false;
    }
    TextClassificationSessionId that = (TextClassificationSessionId) o;
    return Objects.equals(value, that.value);
  }

  /**
   * Flattens this id to a string.
   *
   * @return The flattened id.
   */
  public String getValue() {
    return value;
  }

  /**
   * Recovers a TextClassificationSessionId from a string of the form returned by {@link
   * #getValue()}.
   */
  public static TextClassificationSessionId unflattenFromString(String value) {
    return new TextClassificationSessionId(value);
  }
}
