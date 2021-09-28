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

package android.server.wm.app;

import static android.server.wm.app.Components.InputMethodTestActivity.EXTRA_PRIVATE_IME_OPTIONS;
import static android.server.wm.app.Components.InputMethodTestActivity.EXTRA_TEST_CURSOR_ANCHOR_INFO;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputMethodManager;
import android.widget.LinearLayout;

import com.android.compatibility.common.util.ImeAwareEditText;

public final class InputMethodTestActivity extends Activity {

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        final Intent intent = getIntent();
        final CursorAnchorInfo info = intent.getParcelableExtra(EXTRA_TEST_CURSOR_ANCHOR_INFO);

        final LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);

        final ImeAwareEditText editText = new ImeAwareEditText(this) {
            @Override
            public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
                final InputConnection original = super.onCreateInputConnection(outAttrs);
                final View view = this;
                return new InputConnectionWrapper(original, false) {
                    @Override
                    public boolean requestCursorUpdates(int cursorUpdateMode) {
                        if (info != null) {
                            // If EXTRA_TEST_CURSOR_ANCHOR_INFO is provided, then call
                            // IMM#updateCursorAnchorInfo() with that object.
                            view.post(() -> view.getContext()
                                    .getSystemService(InputMethodManager.class)
                                    .updateCursorAnchorInfo(view, info));
                            return true;
                        }
                        return super.requestCursorUpdates(cursorUpdateMode);
                    }
                };
            }
        };

        final String privateImeOption = intent.getStringExtra(EXTRA_PRIVATE_IME_OPTIONS);
        if (privateImeOption != null) {
            editText.setPrivateImeOptions(privateImeOption);
        }
        layout.addView(editText);
        editText.scheduleShowSoftInput();
        editText.requestFocus();

        setContentView(layout);
    }
}
