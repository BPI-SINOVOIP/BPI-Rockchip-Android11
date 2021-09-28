/*
 * Copyright (C) 2020 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.managedprovisioning.provisioning;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertTrue;

import static com.android.managedprovisioning.model.ProvisioningParams.EXTRA_PROVISIONING_PARAMS;

import android.content.ComponentName;
import android.content.Intent;

import androidx.test.espresso.intent.rule.IntentsTestRule;
import androidx.test.filters.SmallTest;

import com.android.managedprovisioning.R;
import com.android.managedprovisioning.model.ProvisioningParams;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit tests for {@link FinancedDeviceLandingActivity}
 */
@SmallTest
@RunWith(JUnit4.class)
public class FinancedDeviceLandingActivityTest {

    private static final ComponentName TEST_COMPONENT_NAME =
            new ComponentName("test", "test");

    @Rule
    public IntentsTestRule<FinancedDeviceLandingActivity> mActivityRule =
            new IntentsTestRule(FinancedDeviceLandingActivity.class, true, false);

    @Test
    public void onAcceptAndContinueButtonClicked() {
        // GIVEN the activity launched
        ProvisioningParams params = generateProvisioningParams();
        launchActivityWithParams(params);

        // WHEN the user clicks Accept & continue
        onView(withText(R.string.accept_and_continue)).perform(click());

        // THEN the activity should finish
        assertTrue(mActivityRule.getActivity().isFinishing());
    }

    private ProvisioningParams generateProvisioningParams() {
        return new ProvisioningParams.Builder()
                .setDeviceAdminComponentName(TEST_COMPONENT_NAME)
                .setProvisioningAction("")
                .build();
    }

    private void launchActivityWithParams(ProvisioningParams params) {
        Intent intent = new Intent();
        intent.putExtra(EXTRA_PROVISIONING_PARAMS, params);
        mActivityRule.launchActivity(intent);
    }
}
