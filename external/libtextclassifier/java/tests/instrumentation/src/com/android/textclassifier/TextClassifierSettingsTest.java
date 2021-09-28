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

package com.android.textclassifier;

import static com.google.common.truth.Truth.assertThat;

import android.provider.DeviceConfig;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import java.util.function.Consumer;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class TextClassifierSettingsTest {
  private static final String WRITE_DEVICE_CONFIG_PERMISSION =
      "android.permission.WRITE_DEVICE_CONFIG";
  private static final float EPSILON = 0.0001f;

  @Before
  public void setup() {
    InstrumentationRegistry.getInstrumentation()
        .getUiAutomation()
        .adoptShellPermissionIdentity(WRITE_DEVICE_CONFIG_PERMISSION);
  }

  @After
  public void tearDown() {
    InstrumentationRegistry.getInstrumentation().getUiAutomation().dropShellPermissionIdentity();
  }

  @Test
  public void booleanSetting() {
    assertSettings(
        TextClassifierSettings.TEMPLATE_INTENT_FACTORY_ENABLED,
        "false",
        settings -> assertThat(settings.isTemplateIntentFactoryEnabled()).isFalse());
  }

  @Test
  public void intSetting() {
    assertSettings(
        TextClassifierSettings.SUGGEST_SELECTION_MAX_RANGE_LENGTH,
        "8",
        settings -> assertThat(settings.getSuggestSelectionMaxRangeLength()).isEqualTo(8));
  }

  @Test
  public void floatSetting() {
    assertSettings(
        TextClassifierSettings.LANG_ID_THRESHOLD_OVERRIDE,
        "3.14",
        settings -> assertThat(settings.getLangIdThresholdOverride()).isWithin(EPSILON).of(3.14f));
  }

  @Test
  public void stringListSetting() {
    assertSettings(
        TextClassifierSettings.ENTITY_LIST_DEFAULT,
        "email:url",
        settings ->
            assertThat(settings.getEntityListDefault()).containsExactly("email", "url").inOrder());
  }

  @Test
  public void floatListSetting() {
    assertSettings(
        TextClassifierSettings.LANG_ID_CONTEXT_SETTINGS,
        "30:0.5:0.3",
        settings ->
            assertThat(settings.getLangIdContextSettings())
                .usingTolerance(EPSILON)
                .containsExactly(30f, 0.5f, 0.3f)
                .inOrder());
  }

  private static void assertSettings(
      String key, String value, Consumer<TextClassifierSettings> settingsConsumer) {
    final String originalValue =
        DeviceConfig.getProperty(DeviceConfig.NAMESPACE_TEXTCLASSIFIER, key);
    TextClassifierSettings settings = new TextClassifierSettings();
    try {
      setDeviceConfig(key, value);
      settingsConsumer.accept(settings);
    } finally {
      setDeviceConfig(key, originalValue);
    }
  }

  private static void setDeviceConfig(String key, String value) {
    DeviceConfig.setProperty(
        DeviceConfig.NAMESPACE_TEXTCLASSIFIER, key, value, /* makeDefault */ false);
  }
}
