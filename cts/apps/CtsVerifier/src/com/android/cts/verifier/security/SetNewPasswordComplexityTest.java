package com.android.cts.verifier.security;

import android.app.admin.DevicePolicyManager;
import android.content.Intent;
import android.os.Bundle;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

public class SetNewPasswordComplexityTest extends PassFailButtons.Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.pass_fail_set_password_complexity);
        setPassFailButtonClickListeners();
        setInfoResources(R.string.set_complexity_test_title, R.string.set_complexity_test_message,
                -1);

        findViewById(R.id.set_complexity_high_btn).setOnClickListener(v -> {
            Intent intent = new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD);
            intent.putExtra(DevicePolicyManager.EXTRA_PASSWORD_COMPLEXITY,
                    DevicePolicyManager.PASSWORD_COMPLEXITY_HIGH);
            startActivity(intent);
        });

        findViewById(R.id.set_complexity_medium_btn).setOnClickListener(v -> {
            Intent intent = new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD);
            intent.putExtra(DevicePolicyManager.EXTRA_PASSWORD_COMPLEXITY,
                    DevicePolicyManager.PASSWORD_COMPLEXITY_MEDIUM);
            startActivity(intent);
        });

        findViewById(R.id.set_complexity_low_btn).setOnClickListener(v -> {
            Intent intent = new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD);
            intent.putExtra(DevicePolicyManager.EXTRA_PASSWORD_COMPLEXITY,
                    DevicePolicyManager.PASSWORD_COMPLEXITY_LOW);
            startActivity(intent);
        });

        findViewById(R.id.set_complexity_none_btn).setOnClickListener(v -> {
            Intent intent = new Intent(DevicePolicyManager.ACTION_SET_NEW_PASSWORD);
            intent.putExtra(DevicePolicyManager.EXTRA_PASSWORD_COMPLEXITY,
                    DevicePolicyManager.PASSWORD_COMPLEXITY_NONE);
            startActivity(intent);
        });
    }
}
