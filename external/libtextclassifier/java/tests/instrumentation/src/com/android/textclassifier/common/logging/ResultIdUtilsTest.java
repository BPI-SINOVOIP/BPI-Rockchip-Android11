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

import static com.google.common.truth.Truth.assertThat;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.android.textclassifier.common.logging.ResultIdUtils.ModelInfo;
import com.google.common.base.Optional;
import com.google.common.collect.ImmutableList;
import java.util.Locale;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ResultIdUtilsTest {
  private static final int MODEL_VERSION = 703;
  private static final int HASH = 12345;

  @Test
  public void createId_customHash() {
    ImmutableList<Optional<ModelInfo>> modelInfos =
        ImmutableList.of(
            Optional.absent(),
            Optional.of(
                new ModelInfo(/* version= */ 1, ImmutableList.of(Locale.ENGLISH, Locale.FRENCH))),
            Optional.absent(),
            Optional.of(new ModelInfo(/* version= */ 2, ImmutableList.of(Locale.CHINESE))),
            Optional.absent());

    String resultId = ResultIdUtils.createId(HASH, modelInfos);

    assertThat(resultId).isEqualTo("androidtc|;en,fr_v1;;zh_v2;|12345");
  }

  @Test
  public void createId_selection() {
    String resultId =
        ResultIdUtils.createId(
            ApplicationProvider.getApplicationContext(),
            "text",
            1,
            2,
            ImmutableList.of(
                Optional.of(new ModelInfo(MODEL_VERSION, ImmutableList.of(Locale.ENGLISH)))));

    assertThat(resultId).matches("androidtc\\|en_v703\\|-?\\d+");
  }

  @Test
  public void getModelName_invalid() {
    assertThat(ResultIdUtils.getModelNames("a|b")).isEmpty();
  }

  @Test
  public void getModelNames() {
    assertThat(ResultIdUtils.getModelNames("androidtc|;en_v703;;zh_v101;|12344"))
        .containsExactly("", "en_v703", "", "zh_v101", "")
        .inOrder();
  }

  @Test
  public void getModelNames_invalid() {
    assertThat(ResultIdUtils.getModelNames("a|b")).isEmpty();
    assertThat(ResultIdUtils.getModelNames("a|b|c|d")).isEmpty();
  }

  @Test
  public void modelInfo_toModelName() {
    ModelInfo modelInfo = new ModelInfo(700, ImmutableList.of(Locale.ENGLISH));

    assertThat(modelInfo.toModelName()).isEqualTo("en_v700");
  }

  @Test
  public void modelInfo_toModelName_supportedLanguageTags() {
    ModelInfo modelInfo = new ModelInfo(700, "en,fr");

    assertThat(modelInfo.toModelName()).isEqualTo("en,fr_v700");
  }

  @Test
  public void isFromDefaultTextClassifier_true() {
    assertThat(ResultIdUtils.isFromDefaultTextClassifier("androidtc|en_v703|12344")).isTrue();
  }

  @Test
  public void isFromDefaultTextClassifier_false() {
    assertThat(ResultIdUtils.isFromDefaultTextClassifier("aiai|en_v703|12344")).isFalse();
  }
}
