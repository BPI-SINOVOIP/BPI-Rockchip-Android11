/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.view.inputmethod.ctstestapp;

import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE;

import android.app.Activity;
import android.net.Uri;
import android.os.Bundle;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

/**
 * A test {@link Activity} that automatically shows the input method.
 */
public final class MainActivity extends Activity {

    private static final String EXTRA_KEY_PRIVATE_IME_OPTIONS =
            "android.view.inputmethod.ctstestapp.EXTRA_KEY_PRIVATE_IME_OPTIONS";

    @Nullable
    private String getPrivateImeOptions() {
        if (getPackageManager().isInstantApp()) {
            final Uri uri = getIntent().getData();
            if (uri == null || !uri.isHierarchical()) {
                return null;
            }
            return uri.getQueryParameter(EXTRA_KEY_PRIVATE_IME_OPTIONS);
        }
        return getIntent().getStringExtra(EXTRA_KEY_PRIVATE_IME_OPTIONS);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        final EditText editText = new EditText(this);
        editText.setHint("editText");
        final String privateImeOptions = getPrivateImeOptions();
        if (privateImeOptions != null) {
            editText.setPrivateImeOptions(privateImeOptions);
        }
        editText.requestFocus();
        layout.addView(editText);
        getWindow().setSoftInputMode(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
        setContentView(layout);
    }
}
