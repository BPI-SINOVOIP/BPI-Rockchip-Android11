package com.google.android.tv.setup.customizationsample;

import android.os.Bundle;
import android.view.KeyEvent;

/**
 * A network delegation Activity, which simulates presenting a get-me-connected user interface.
 */
public class NetworkDelegationActivity extends BaseActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        showDrawable(R.drawable.home_internet);

        registerKeyHandlerResultCancelledBack();
        registerKeyHandlerResultCancelledCrashDpadLeft();

        registerKeyHandlerResultOk(KeyEvent.KEYCODE_DPAD_CENTER, "as if the user set up a network");
        registerKeyHandlerResultOk(KeyEvent.KEYCODE_DPAD_RIGHT, "as if the user chose to not set up a network", new String[] {Constants.PARTNER_HANDLED_NETWORK_USER_SKIPPED});
    }
}


