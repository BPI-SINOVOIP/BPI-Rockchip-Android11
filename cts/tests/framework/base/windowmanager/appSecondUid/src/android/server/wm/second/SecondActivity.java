/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * limitations under the License
 */

package android.server.wm.second;

import static android.server.wm.second.Components.SecondActivity.EXTRA_DISPLAY_ACCESS_CHECK;

import android.os.Bundle;
import android.server.wm.CommandSession;
import android.view.View;
import android.view.WindowManager;

public class SecondActivity extends CommandSession.BasicTestActivity {

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);

        if (getIntent().hasExtra(EXTRA_DISPLAY_ACCESS_CHECK)) {
            // Verify that this activity can add views to the display where it is placed.
            final View view = new View(this);
            WindowManager.LayoutParams params = new WindowManager.LayoutParams();
            params.height = WindowManager.LayoutParams.WRAP_CONTENT;
            params.width = WindowManager.LayoutParams.WRAP_CONTENT;
            params.type = WindowManager.LayoutParams.TYPE_BASE_APPLICATION;
            params.setTitle("CustomView");
            getWindowManager().addView(view, params);
        }
    }
}
