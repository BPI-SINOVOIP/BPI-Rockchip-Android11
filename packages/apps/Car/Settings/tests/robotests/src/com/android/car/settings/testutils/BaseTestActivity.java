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

package com.android.car.settings.testutils;

import android.car.drivingstate.CarUxRestrictions;
import android.os.Bundle;

import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentHost;
import com.android.car.settings.common.UxRestrictionsProvider;

/**
 * Test activity used for testing {@code BaseFragment} instances.
 */
public class BaseTestActivity extends FragmentActivity implements FragmentHost,
        UxRestrictionsProvider {

    private boolean mOnBackPressedFlag;
    private CarUxRestrictions mRestrictionInfo = new CarUxRestrictions.Builder(/* reqOpt= */ true,
            CarUxRestrictions.UX_RESTRICTIONS_BASELINE, /* timestamp= */ 0).build();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.car_setting_activity);
    }

    /**
     * Places fragment in place of fragment container.
     *
     * @param fragment Fragment to add to activity.
     */
    @Override
    public void launchFragment(Fragment fragment) {
        if (fragment instanceof DialogFragment) {
            throw new IllegalArgumentException(
                    "cannot launch dialogs with launchFragment() - use showDialog() instead");
        }
        String tag = Integer.toString(getSupportFragmentManager().getBackStackEntryCount());
        getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.fragment_container, fragment, tag)
                .addToBackStack(null)
                .commit();
    }

    @Override
    public void showBlockingMessage() {
        // no-op
    }

    @Override
    public CarUxRestrictions getCarUxRestrictions() {
        return mRestrictionInfo;
    }

    public void setCarUxRestrictions(CarUxRestrictions restrictionInfo) {
        mRestrictionInfo = restrictionInfo;
    }

    /**
     * Override to catch onBackPressed invocations on the activity.
     */
    @Override
    public void onBackPressed() {
        mOnBackPressedFlag = true;
        getSupportFragmentManager().popBackStackImmediate();
    }

    /**
     * Gets a boolean flag indicating whether onBackPressed has been called.
     *
     * @return {@code true} if onBackPressed called, {@code false} otherwise.
     */
    public boolean getOnBackPressedFlag() {
        return mOnBackPressedFlag;
    }

    /**
     * Clear the boolean flag for onBackPressed by setting it to false.
     */
    public void clearOnBackPressedFlag() {
        mOnBackPressedFlag = false;
    }

    @Override
    public void goBack() {
        onBackPressed();
    }
}
