package com.google.android.tv.setup.customizationsample;

import android.os.Bundle;
import android.view.KeyEvent;

/**
 * A HOOK_POST_NETWORK follow-up Activity, which simulates performing a system upgrade.
 */
public class HookPostNetworkFollowupActivity extends BaseActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        showDrawable(R.drawable.download);

        registerKeyHandlerResultCancelledBack();
        registerKeyHandlerResultCancelledCrashDpadLeft();

        registerKeyHandlerResultOkDpadCenterBoring();
        registerKeyHandlerResultOk(KeyEvent.KEYCODE_DPAD_RIGHT, "simulate an upgrade by doing a reboot (this is a very weak simulation of an upgrade)");
    }
}


