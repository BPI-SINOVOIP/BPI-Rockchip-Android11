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

package com.android.cts.verifier.managedprovisioning;

import static com.android.cts.verifier.managedprovisioning.CommandReceiverActivity.COMMAND_SET_LOCK_SCREEN_INFO;
import static com.android.cts.verifier.managedprovisioning.CommandReceiverActivity.EXTRA_COMMAND;
import static com.android.cts.verifier.managedprovisioning.CommandReceiverActivity.EXTRA_VALUE;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.TextView;
import android.widget.Toast;
import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

/**
 * Test class to verify setting a customized Lockscreen message.
 */
public class LockscreenMessageTestActivity extends PassFailButtons.Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.lockscreen_message);
        setPassFailButtonClickListeners();
        setButtonClickListeners();
    }

    private void setButtonClickListeners() {
        findViewById(R.id.lockscreen_message_set_button)
                .setOnClickListener(v -> setLockscreenMessage());
        findViewById(R.id.go_button).setOnClickListener(v -> goToSettings());
    }

    private void setLockscreenMessage() {
        TextView lockscreenMessageView = findViewById(R.id.lockscreen_message_edit_text);
        String lockscreenMessage = lockscreenMessageView.getText().toString();
        if (lockscreenMessage.isEmpty()) {
            Toast.makeText(this, R.string.device_owner_lockscreen_message_cannot_be_empty,
                    Toast.LENGTH_SHORT).show();
            return;
        }

        Intent intent = new Intent(this, CommandReceiverActivity.class)
                .putExtra(EXTRA_COMMAND, COMMAND_SET_LOCK_SCREEN_INFO)
                .putExtra(EXTRA_VALUE, lockscreenMessage);
        startActivity(intent);
    }

    private void goToSettings() {
        Intent intent = new Intent(Settings.ACTION_SETTINGS);
        startActivity(intent);
    }
}
