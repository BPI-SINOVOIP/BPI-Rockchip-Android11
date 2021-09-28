/*
 * Copyright (C) 2016 The Android Open Source Project
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

import android.app.AlertDialog;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.view.View;

import com.android.cts.verifier.ArrayTestListAdapter;
import com.android.cts.verifier.DialogTestListActivity;
import com.android.cts.verifier.R;
import com.android.cts.verifier.TestResult;

/**
 * This test verifies that if a work profile is locked with a separate password, Recents views for
 * for applications in the work profile are redacted appropriately.
 */
public class RecentsRedactionActivity extends DialogTestListActivity {
    private static final String TAG = RecentsRedactionActivity.class.getSimpleName();

    public static final String ACTION_RECENTS =
            "com.android.cts.verifier.managedprovisioning.RECENTS";

    private ComponentName mAdmin;
    private DevicePolicyManager mDevicePolicyManager;

    private DialogTestListItem mVerifyRedacted;
    private DialogTestListItem mVerifyNotRedacted;

    public RecentsRedactionActivity() {
        super(R.layout.provisioning_byod,
                /* title */ R.string.provisioning_byod_recents,
                /* description */ R.string.provisioning_byod_recents_info,
                /* instructions */ R.string.provisioning_byod_recents_instructions);
    }

    // Default listener will use setResult(), which won't work due to activity being in a new task.
    private View.OnClickListener clickListener = target -> {
        final int resultCode;
        switch (target.getId()) {
            case R.id.pass_button:
                resultCode = TestResult.TEST_RESULT_PASSED;
                break;
            case R.id.fail_button:
                resultCode = TestResult.TEST_RESULT_FAILED;
                break;
            default:
                throw new IllegalArgumentException("Unknown id: " + target.getId());
        }
        Intent resultIntent = TestResult.createResult(RecentsRedactionActivity.this, resultCode,
                getTestId(), getTestDetails(), getReportLog(), getHistoryCollection());

        new ByodFlowTestHelper(this).sendResultToPrimary(resultIntent);
        finish();
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        findViewById(R.id.pass_button).setOnClickListener(clickListener);
        findViewById(R.id.fail_button).setOnClickListener(clickListener);

        mPrepareTestButton.setText(R.string.provisioning_byod_recents_lock_now);
        mPrepareTestButton.setOnClickListener((View view) -> {
            mDevicePolicyManager.lockNow();
        });

        mAdmin = new ComponentName(this, DeviceAdminTestReceiver.class.getName());
        mDevicePolicyManager = (DevicePolicyManager)
                getSystemService(Context.DEVICE_POLICY_SERVICE);
    }

    @Override
    protected void setupTests(ArrayTestListAdapter adapter) {
        mVerifyRedacted = new DialogTestListItem(this,
                /* name */ R.string.provisioning_byod_recents_verify_redacted,
                /* testId */ "BYOD_recents_verify_redacted",
                /* instruction */ R.string.provisioning_byod_recents_verify_redacted_instruction,
                /* action */ new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD));

        mVerifyNotRedacted = new DialogTestListItem(this,
                /* name */ R.string.provisioning_byod_recents_verify_not_redacted,
                /* testId */ "BYOD_recents_verify_not_redacted",
                /* intruction */ R.string.provisioning_byod_recents_verify_not_redacted_instruction,
                /* action */ new Intent(Settings.ACTION_SECURITY_SETTINGS));

        adapter.add(mVerifyRedacted);
        adapter.add(mVerifyNotRedacted);
    }

    @Override
    public void onBackPressed() {
        if (hasPassword()) {
            requestRemovePassword();
            return;
        }
        super.onBackPressed();
    }

    private boolean hasPassword() {
        try {
            mDevicePolicyManager.setPasswordQuality(mAdmin,
                    DevicePolicyManager.PASSWORD_QUALITY_SOMETHING);
            return mDevicePolicyManager.isActivePasswordSufficient();
        } finally {
            mDevicePolicyManager.setPasswordQuality(mAdmin,
                    DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED);
        }
    }

    private void requestRemovePassword() {
        new AlertDialog.Builder(this)
                .setIcon(android.R.drawable.ic_dialog_info)
                .setTitle(R.string.provisioning_byod_recents)
                .setMessage(R.string.provisioning_byod_recents_remove_password)
                .setPositiveButton(android.R.string.ok, (DialogInterface dialog, int which) -> {
                    startActivity(new Intent(Settings.ACTION_SECURITY_SETTINGS));
                })
                .show();
    }
}
