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

import android.os.Bundle;
import android.widget.TextView;

import com.android.cts.verifier.R;

/**
 * Test for manual verification of Instant Apps Recents function.
 *
 * The test verifies that Instant App can be accessed in Recent Apps screen.
 */
public class RecentAppsTestActivity extends BaseTestActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setInfoResources(R.string.ia_recents, R.string.ia_recents_info, -1);
        TextView extraText = (TextView) findViewById(R.id.instruction_extra_text);
        extraText.setText(R.string.ia_recents_instruction_label);
    }
}
