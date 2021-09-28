package com.google.android.tv.setup.customizationsample;

import android.os.Bundle;

public class MockKatnissActivity extends BaseActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        showDrawable(R.drawable.mock_katniss);

        registerKeyHandlerResultOkDpadCenterBoring();
    }
}
