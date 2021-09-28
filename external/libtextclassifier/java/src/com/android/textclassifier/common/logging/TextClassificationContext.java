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

import androidx.annotation.NonNull;
import com.google.common.base.Preconditions;
import java.util.Locale;
import javax.annotation.Nullable;

/** A representation of the context in which text classification would be performed. */
public final class TextClassificationContext {

  private final String packageName;
  private final String widgetType;
  @Nullable private final String widgetVersion;

  private TextClassificationContext(
      String packageName, String widgetType, @Nullable String widgetVersion) {
    this.packageName = Preconditions.checkNotNull(packageName);
    this.widgetType = Preconditions.checkNotNull(widgetType);
    this.widgetVersion = widgetVersion;
  }

  /** Returns the package name for the calling package. */
  public String getPackageName() {
    return packageName;
  }

  /** Returns the widget type for this classification context. */
  public String getWidgetType() {
    return widgetType;
  }

  /**
   * Returns a custom version string for the widget type.
   *
   * @see #getWidgetType()
   */
  @Nullable
  public String getWidgetVersion() {
    return widgetVersion;
  }

  @Override
  public String toString() {
    return String.format(
        Locale.US,
        "TextClassificationContext{" + "packageName=%s, widgetType=%s, widgetVersion=%s}",
        packageName,
        widgetType,
        widgetVersion);
  }

  /** A builder for building a TextClassification context. */
  public static final class Builder {

    private final String packageName;
    private final String widgetType;

    @Nullable private String widgetVersion;

    /**
     * Initializes a new builder for text classification context objects.
     *
     * @param packageName the name of the calling package
     * @param widgetType the type of widget e.g. {@link
     *     android.view.textclassifier.TextClassifier#WIDGET_TYPE_TEXTVIEW}
     * @return this builder
     */
    public Builder(String packageName, String widgetType) {
      this.packageName = Preconditions.checkNotNull(packageName);
      this.widgetType = Preconditions.checkNotNull(widgetType);
    }

    /**
     * Sets an optional custom version string for the widget type.
     *
     * @return this builder
     */
    public Builder setWidgetVersion(@Nullable String widgetVersion) {
      this.widgetVersion = widgetVersion;
      return this;
    }

    /**
     * Builds the text classification context object.
     *
     * @return the built TextClassificationContext object
     */
    @NonNull
    public TextClassificationContext build() {
      return new TextClassificationContext(packageName, this.widgetType, widgetVersion);
    }
  }
}
