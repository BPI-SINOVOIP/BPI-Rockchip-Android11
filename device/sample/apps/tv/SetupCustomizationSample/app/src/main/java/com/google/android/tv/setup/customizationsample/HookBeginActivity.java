package com.google.android.tv.setup.customizationsample;

import android.os.Bundle;

/**
 * A HOOK_BEGIN Activity that does nothing but allow finish().
 */
public class HookBeginActivity extends BaseActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        showDrawable(R.drawable.hook);

        registerKeyHandlerResultCancelledBack();
        registerKeyHandlerResultCancelledCrashDpadLeft();
        registerKeyHandlerResultOkDpadCenterBoring();
    }
}
