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

import android.content.Context;
import android.text.TextUtils;
import com.android.textclassifier.common.base.LocaleCompat;
import com.google.common.base.Joiner;
import com.google.common.base.Objects;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableList;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import javax.annotation.Nullable;

/** Provide utils to generate and parse the result id. */
public final class ResultIdUtils {
  private static final String CLASSIFIER_ID = "androidtc";
  private static final String SEPARATOR_MODEL_NAME = ";";
  private static final String SEPARATOR_LOCALES = ",";
  private static final Pattern EXTRACT_MODEL_NAME_FROM_RESULT_ID =
      Pattern.compile("^[^|]*\\|([^|]*)\\|[^|]*$");

  /** Creates a string id that may be used to identify a TextClassifier result. */
  public static String createId(
      Context context, String text, int start, int end, List<Optional<ModelInfo>> modelInfos) {
    Preconditions.checkNotNull(text);
    Preconditions.checkNotNull(context);
    Preconditions.checkNotNull(modelInfos);
    final int hash = Objects.hashCode(text, start, end, context.getPackageName());
    return createId(hash, modelInfos);
  }

  /** Creates a string id that may be used to identify a TextClassifier result. */
  public static String createId(int hash, List<Optional<ModelInfo>> modelInfos) {
    Preconditions.checkNotNull(modelInfos);
    final List<String> modelNames = new ArrayList<>();
    for (Optional<ModelInfo> modelInfo : modelInfos) {
      modelNames.add(modelInfo.transform(ModelInfo::toModelName).or(""));
    }
    return String.format(
        Locale.US,
        "%s|%s|%d",
        CLASSIFIER_ID,
        Joiner.on(SEPARATOR_MODEL_NAME).join(modelNames),
        hash);
  }

  /** Returns if the result id was generated from the default text classifier. */
  public static boolean isFromDefaultTextClassifier(String resultId) {
    return resultId.startsWith(CLASSIFIER_ID + '|');
  }

  /** Returns all the model names encoded in the signature. */
  public static ImmutableList<String> getModelNames(@Nullable String signature) {
    if (TextUtils.isEmpty(signature)) {
      return ImmutableList.of();
    }
    Matcher matcher = EXTRACT_MODEL_NAME_FROM_RESULT_ID.matcher(signature);
    if (!matcher.find()) {
      return ImmutableList.of();
    }
    return ImmutableList.copyOf(Splitter.on(SEPARATOR_MODEL_NAME).splitToList(matcher.group(1)));
  }

  private ResultIdUtils() {}

  /** Model information of a model file. */
  public static class ModelInfo {
    private final String modelName;

    public ModelInfo(int version, List<Locale> locales) {
      this(version, createSupportedLanguageTagsString(locales));
    }

    /**
     * Creates a {@link ModelInfo} object.
     *
     * @param version model version
     * @param supportedLanguageTags a comma-separated string of bcp47 language tags of supported
     *     languages
     */
    public ModelInfo(int version, String supportedLanguageTags) {
      this.modelName = createModelName(version, supportedLanguageTags);
    }

    private static String createSupportedLanguageTagsString(List<Locale> locales) {
      List<String> languageTags = new ArrayList<>();
      for (Locale locale : locales) {
        languageTags.add(LocaleCompat.toLanguageTag(locale));
      }
      return Joiner.on(SEPARATOR_LOCALES).join(languageTags);
    }

    private static String createModelName(int version, String supportedLanguageTags) {
      return String.format(Locale.US, "%s_v%d", supportedLanguageTags, version);
    }

    /** Returns a string representation of the model info. */
    public String toModelName() {
      return modelName;
    }
  }
}
