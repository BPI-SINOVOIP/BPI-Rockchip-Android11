/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.cts.verifier.instantapps;

import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.net.Uri;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.ImageButton;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

/**
 * Base {@link Activity} class for testing Instant Apps System UIs.
 */
public abstract class BaseTestActivity extends PassFailButtons.Activity
        implements OnClickListener {

    private static final String TAG = "InstantApps";
    private static final String APP_URL = "https://source.android.com";
    private static final String APP_PACKAGE = "com.android.cts.instantapp";

    private ImageButton mPassButton;
    private ImageButton mFailButton;
    private Button mStartTestButton;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.instant_apps);
        setPassFailButtonClickListeners();

        mPassButton = (ImageButton) findViewById(R.id.pass_button);
        mFailButton = (ImageButton) findViewById(R.id.fail_button);
        mStartTestButton = (Button) findViewById(R.id.start_test_button);
        mStartTestButton.setOnClickListener(this);

        resetButtons();

        mStartTestButton.setEnabled(true);
    }

    @Override
    public void onClick(View view) {
        if (view != mStartTestButton) return;

        Log.v(TAG, "Start Test button clicked");
        try {
          Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(APP_URL));
          intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
          intent.addCategory(Intent.CATEGORY_BROWSABLE);
          intent.setPackage(APP_PACKAGE);
          startActivity(intent);

          mStartTestButton.setEnabled(false);
          enablePassFailButtons(true);
        } catch (ActivityNotFoundException e) {
          // Use ActivityNotFoundException as an indicator that Instant App is
          // not installed.
          Log.v(TAG, "Instant App not installed.");

          // Display alert dialog with instruction for installing the Instant App.
          new AlertDialog.Builder(
              BaseTestActivity.this)
                  .setIcon(android.R.drawable.ic_dialog_info)
                  .setTitle(R.string.ia_install_dialog_title)
                  .setMessage(R.string.ia_install_dialog_text)
                  .setPositiveButton(android.R.string.ok, null)
                  .show();
        }
    }

    private void resetButtons() {
        enablePassFailButtons(false);
    }

    private void enablePassFailButtons(boolean enable) {
        mPassButton.setEnabled(enable);
        mFailButton.setEnabled(enable);
    }
}
