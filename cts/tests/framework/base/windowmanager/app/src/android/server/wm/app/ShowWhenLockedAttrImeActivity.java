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

import static android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE;

import android.os.Bundle;
import android.widget.EditText;
import android.widget.LinearLayout;

public class ShowWhenLockedAttrImeActivity extends AbstractLifecycleLogActivity {

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        final EditText editText = new EditText(this);
        final LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.addView(editText);
        setContentView(layout);

        getWindow().setSoftInputMode(SOFT_INPUT_STATE_ALWAYS_VISIBLE);
        editText.requestFocus();
    }
}
