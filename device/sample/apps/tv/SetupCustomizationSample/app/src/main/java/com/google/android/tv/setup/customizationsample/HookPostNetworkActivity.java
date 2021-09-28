package com.google.android.tv.setup.customizationsample;

import android.content.Intent;
import android.os.Bundle;
import android.view.KeyEvent;

/**
 * A HOOK_POST_NETWORK Activity which simulates checking for a system upgrade.
 */
public class HookPostNetworkActivity extends BaseActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Here we pretend that we are checking if an upgrade is available.
        // We do not make visible any UI elements; instead we allow Android TV Setup's "Please wait"
        // step to show under us while we figure out if an upgrade is available. That way there are
        // fewer transitions which is especially useful in case we have nothing important to do.

        registerKeyHandlerResultCancelledBack();
        registerKeyHandlerResultCancelledCrashDpadLeft();

        registerKeyHandlerResultOk(KeyEvent.KEYCODE_DPAD_CENTER, "with no follow-up Activity");
        registerKeyHandlerResultOk(KeyEvent.KEYCODE_DPAD_RIGHT, "with a follow-up Activity", new Intent(getApplicationContext(), HookPostNetworkFollowupActivity.class));
    }
}


