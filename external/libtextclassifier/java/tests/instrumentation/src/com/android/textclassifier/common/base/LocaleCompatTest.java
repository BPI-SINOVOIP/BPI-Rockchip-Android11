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

import static com.google.common.truth.Truth.assertThat;

import android.os.LocaleList;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SdkSuppress;
import androidx.test.filters.SmallTest;
import java.util.Locale;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class LocaleCompatTest {
  @SdkSuppress(minSdkVersion = 24)
  @Test
  public void toLanguageTag_minApi24() {
    Locale locale = Locale.TRADITIONAL_CHINESE;

    String languageTags = LocaleCompat.toLanguageTag(locale);

    assertThat(languageTags).isEqualTo("zh-TW");
  }

  @SdkSuppress(maxSdkVersion = 23)
  @Test
  public void toLanguageTag_base() {
    Locale locale = Locale.TRADITIONAL_CHINESE;

    String languageTags = LocaleCompat.toLanguageTag(locale);

    assertThat(languageTags).isEqualTo("zh");
  }

  @SdkSuppress(minSdkVersion = 24)
  @Test
  public void getResourceLanguageTags_minApi24() {
    ApplicationProvider.getApplicationContext()
        .getResources()
        .getConfiguration()
        .setLocales(LocaleList.forLanguageTags("zh-TW"));

    String resourceLanguageTags =
        LocaleCompat.getResourceLanguageTags(ApplicationProvider.getApplicationContext());

    assertThat(resourceLanguageTags).isEqualTo("zh-TW");
  }

  @SdkSuppress(minSdkVersion = 21, maxSdkVersion = 23)
  @Test
  public void getResourceLanguageTags_minApi21() {
    ApplicationProvider.getApplicationContext()
        .getResources()
        .getConfiguration()
        .setLocale(Locale.TAIWAN);

    String resourceLanguageTags =
        LocaleCompat.getResourceLanguageTags(ApplicationProvider.getApplicationContext());

    assertThat(resourceLanguageTags).isEqualTo("zh-TW");
  }

  @SdkSuppress(maxSdkVersion = 20)
  @Test
  public void getResourceLanguageTags_base() {
    ApplicationProvider.getApplicationContext().getResources().getConfiguration().locale =
        Locale.TAIWAN;

    String resourceLanguageTags =
        LocaleCompat.getResourceLanguageTags(ApplicationProvider.getApplicationContext());

    assertThat(resourceLanguageTags).isEqualTo("zh");
  }
}
