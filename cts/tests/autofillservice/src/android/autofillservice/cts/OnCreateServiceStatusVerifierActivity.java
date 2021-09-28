/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.autofillservice.cts;

import static android.autofillservice.cts.Helper.getAutofillServiceName;

import static com.google.common.truth.Truth.assertWithMessage;

import android.os.Bundle;
import android.util.Log;

/**
 * Activity used to verify whether the service is enable or not when it's launched.
 */
public class OnCreateServiceStatusVerifierActivity extends AbstractAutoFillActivity {

    private static final String TAG = "OnCreateServiceStatusVerifierActivity";

    static final String SERVICE_NAME = android.autofillservice.cts.NoOpAutofillService.SERVICE_NAME;

    private String mSettingsOnCreate;
    private boolean mEnabledOnCreate;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.simple_save_activity);

        mSettingsOnCreate = getAutofillServiceName();
        mEnabledOnCreate = getAutofillManager().isEnabled();
        Log.i(TAG, "On create: settings=" + mSettingsOnCreate + ", enabled=" + mEnabledOnCreate);
    }

    void assertServiceStatusOnCreate(boolean enabled) {
        if (enabled) {
            assertWithMessage("Wrong settings").that(mSettingsOnCreate)
                .isEqualTo(SERVICE_NAME);
            assertWithMessage("AutofillManager.isEnabled() is wrong").that(mEnabledOnCreate)
                .isTrue();

        } else {
            assertWithMessage("Wrong settings").that(mSettingsOnCreate).isNull();
            assertWithMessage("AutofillManager.isEnabled() is wrong").that(mEnabledOnCreate)
                .isFalse();
        }
    }
}
