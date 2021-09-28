package com.google.android.tv.setup.customizationsample;

import android.os.Bundle;

/**
 * A HOOK_END Activity that does nothing but allow finish().
 */
public class HookEndActivity extends BaseActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        showDrawable(R.drawable.hook);

        registerKeyHandlerResultCancelledBack();
        registerKeyHandlerResultCancelledCrashDpadLeft();
        registerKeyHandlerResultOkDpadCenterBoring();
    }
}
