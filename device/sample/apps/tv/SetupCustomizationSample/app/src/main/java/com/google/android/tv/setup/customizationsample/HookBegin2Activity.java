package com.google.android.tv.setup.customizationsample;

import android.os.Bundle;
import android.view.KeyEvent;

/**
 * A HOOK_BEGIN Activity that illustrates the partner_handled_network extra.
 */
public class HookBegin2Activity extends BaseActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        showDrawable(R.drawable.hook);

        registerKeyHandlerResultCancelledBack();
        registerKeyHandlerResultCancelledCrashDpadLeft();

        registerKeyHandlerResultOkDpadCenterBoring();
        registerKeyHandlerResultOk(KeyEvent.KEYCODE_DPAD_RIGHT, "as if the partner handled network setup", new String[] {Constants.PARTNER_HANDLED_NETWORK});
        registerKeyHandlerResultOk(KeyEvent.KEYCODE_DPAD_UP, "as if the partner handles network setup but the user chose not to set up a network", new String[] {Constants.PARTNER_HANDLED_NETWORK,Constants.PARTNER_HANDLED_NETWORK_USER_SKIPPED});
    }
}
