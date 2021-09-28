package com.google.android.tv.setup.customizationsample;

import android.os.Bundle;

public class TransparentActivity extends BaseActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        showDrawable(R.drawable.opaque_tile);

        registerKeyHandlerResultOkDpadCenterBoring();
    }
}
