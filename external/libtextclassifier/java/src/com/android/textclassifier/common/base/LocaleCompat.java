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

package com.android.textclassifier.common.base;

import android.content.Context;
import android.os.Build;
import java.util.Locale;

/** Helper for accessing locale related stuff that works across different platform versions. */
public final class LocaleCompat {

  private LocaleCompat() {}

  /**
   * Returns a well-formed IETF BCP 47 language tag representing this locale. In older platforms,
   * only the ISO 639 language code will be returned.
   *
   * @see Locale#toLanguageTag()
   */
  public static String toLanguageTag(Locale locale) {
    if (Build.VERSION.SDK_INT >= 24) {
      return Api24Impl.toLanguageTag(locale);
    }
    return ApiBaseImpl.toLanguageTag(locale);
  }

  /** Returns the language tags in string for the current resources configuration. */
  public static String getResourceLanguageTags(Context context) {
    if (Build.VERSION.SDK_INT >= 24) {
      return Api24Impl.getResourceLanguageTags(context);
    } else if (Build.VERSION.SDK_INT >= 21) {
      return Api21Impl.getResourceLanguageTags(context);
    }
    return ApiBaseImpl.getResourceLanguageTags(context);
  }

  private static class Api24Impl {
    private Api24Impl() {}

    static String toLanguageTag(Locale locale) {
      return locale.toLanguageTag();
    }

    static String getResourceLanguageTags(Context context) {
      return context.getResources().getConfiguration().getLocales().toLanguageTags();
    }
  }

  private static class Api21Impl {
    private Api21Impl() {}

    static String getResourceLanguageTags(Context context) {
      return context.getResources().getConfiguration().locale.toLanguageTag();
    }
  }

  private static class ApiBaseImpl {
    private ApiBaseImpl() {}

    static String toLanguageTag(Locale locale) {
      return locale.getLanguage();
    }

    static String getResourceLanguageTags(Context context) {
      return context.getResources().getConfiguration().locale.getLanguage();
    }
  }
}
