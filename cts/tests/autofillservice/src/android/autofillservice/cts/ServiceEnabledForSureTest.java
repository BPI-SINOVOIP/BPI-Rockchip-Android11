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

import static android.autofillservice.cts.Helper.disableAutofillService;
import static android.autofillservice.cts.Helper.enableAutofillService;
import static android.autofillservice.cts.OnCreateServiceStatusVerifierActivity.SERVICE_NAME;

import static com.google.common.truth.Truth.assertThat;

import android.util.Log;
import android.view.autofill.AutofillManager;

import org.junit.BeforeClass;
import org.junit.Test;

/**
 * Test case that guarantee the service is enabled before the activity launches.
 */
public class ServiceEnabledForSureTest
        extends AutoFillServiceTestCase.AutoActivityLaunch<OnCreateServiceStatusVerifierActivity> {

    private static final String TAG = "ServiceEnabledForSureTest";

    private OnCreateServiceStatusVerifierActivity mActivity;

    @BeforeClass
    public static void resetService() {
        enableAutofillService(sContext, SERVICE_NAME);
    }

    @Override
    protected AutofillActivityTestRule<OnCreateServiceStatusVerifierActivity> getActivityRule() {
        return new AutofillActivityTestRule<OnCreateServiceStatusVerifierActivity>(
                OnCreateServiceStatusVerifierActivity.class) {
            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
            }
        };
    }

    @Override
    protected void prepareServicePreTest() {
        // Doesn't need to prepare the service - that was already taken care of in a @BeforeClass -
        // but to guarantee the test finishes in the proper state
        Log.v(TAG, "prepareServicePreTest(): not doing anything");
        mSafeCleanerRule.run(() ->assertThat(mActivity.getAutofillManager().isEnabled()).isTrue());
    }

    @Test
    public void testIsAutofillEnabled() throws Exception {
        mActivity.assertServiceStatusOnCreate(true);

        final AutofillManager afm = mActivity.getAutofillManager();
        Helper.assertAutofillEnabled(afm, true);

        disableAutofillService(mContext);
        Helper.assertAutofillEnabled(afm, false);

        enableAutofillService(mContext, SERVICE_NAME);
        Helper.assertAutofillEnabled(afm, true);
    }
}
