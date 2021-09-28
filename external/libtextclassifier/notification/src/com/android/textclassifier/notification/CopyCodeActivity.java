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

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;
import javax.annotation.Nullable;

/** Handles the copy code action. */
public class CopyCodeActivity extends Activity {
  private static final String TAG = "CopyCodeActivity";

  @Override
  protected void onCreate(@Nullable Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    handleIntent();
    finish();
  }

  private void handleIntent() {
    String code = getIntent().getStringExtra(Intent.EXTRA_TEXT);
    if (TextUtils.isEmpty(code)) {
      Log.w(TAG, "handleIntent: empty code");
      return;
    }
    ClipboardManager clipboardManager = getSystemService(ClipboardManager.class);
    ClipData clipData = ClipData.newPlainText(null, code);
    clipboardManager.setPrimaryClip(clipData);
    Toast.makeText(
            getApplicationContext(), R.string.tc_notif_code_copied_to_clipboard, Toast.LENGTH_SHORT)
        .show();
  }
}
