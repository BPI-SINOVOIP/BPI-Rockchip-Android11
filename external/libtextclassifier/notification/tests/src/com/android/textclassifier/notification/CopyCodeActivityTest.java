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

package com.android.textclassifier.notification;

import static com.google.common.truth.Truth.assertThat;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Intent;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public final class CopyCodeActivityTest {

  private static final String CODE_TO_COPY = "code";
  private static final Intent EMPTY_INTENT =
      new Intent(Intent.ACTION_VIEW).putExtra(Intent.EXTRA_TEXT, "");
  private static final Intent CODE_INTENT =
      new Intent(Intent.ACTION_VIEW).putExtra(Intent.EXTRA_TEXT, CODE_TO_COPY);

  @Rule
  public ActivityTestRule<CopyCodeActivity> activityRule =
      new ActivityTestRule<>(
          CopyCodeActivity.class, /* initialTouchMode= */ false, /* launchActivity= */ false);

  @Test
  public void onCreate_emptyCode() throws Exception {
    ClipboardManager clipboardManager =
        ApplicationProvider.getApplicationContext().getSystemService(ClipboardManager.class);
    // Use shell's permissions to ensure we can access the clipboard
    InstrumentationRegistry.getInstrumentation().getUiAutomation().adoptShellPermissionIdentity();
    clipboardManager.clearPrimaryClip();

    activityRule.launchActivity(EMPTY_INTENT);

    try {
      assertThat(clipboardManager.hasPrimaryClip()).isFalse();
    } finally {
      InstrumentationRegistry.getInstrumentation().getUiAutomation().dropShellPermissionIdentity();
    }
  }

  @Test
  public void onCreate_codeCopied() throws Exception {
    ClipboardManager clipboardManager =
        ApplicationProvider.getApplicationContext().getSystemService(ClipboardManager.class);
    // Use shell's permissions to ensure we can access the clipboard
    InstrumentationRegistry.getInstrumentation().getUiAutomation().adoptShellPermissionIdentity();
    clipboardManager.clearPrimaryClip();

    activityRule.launchActivity(CODE_INTENT);

    ClipData clipFromClipboard;
    try {
      assertThat(clipboardManager.hasPrimaryClip()).isTrue();
      clipFromClipboard = clipboardManager.getPrimaryClip();
    } finally {
      clipboardManager.clearPrimaryClip();
      InstrumentationRegistry.getInstrumentation().getUiAutomation().dropShellPermissionIdentity();
    }

    assertThat(clipFromClipboard).isNotNull();
    assertThat(clipFromClipboard.getItemCount()).isEqualTo(1);
    assertThat(clipFromClipboard.getItemAt(0).getText().toString()).isEqualTo(CODE_TO_COPY);
  }
}
