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
package com.android.car.dialer.ui.warning;

import static com.google.common.truth.Truth.assertThat;

import android.view.View;
import android.widget.TextView;

import com.android.car.dialer.CarDialerRobolectricTestRunner;
import com.android.car.dialer.FragmentTestActivity;
import com.android.car.dialer.R;
import com.android.car.dialer.TestDialerApplication;
import com.android.car.dialer.bluetooth.UiBluetoothMonitor;
import com.android.car.dialer.telecom.UiCallManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;

@RunWith(CarDialerRobolectricTestRunner.class)
public class NoHfpFragmentTest {

    private NoHfpFragment mNoHfpFragment;
    private FragmentTestActivity mFragmentTestActivity;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        ((TestDialerApplication) RuntimeEnvironment.application).initUiCallManager();
        UiBluetoothMonitor.init(RuntimeEnvironment.application);

        mNoHfpFragment = new NoHfpFragment();
        mFragmentTestActivity = Robolectric.buildActivity(
                FragmentTestActivity.class).create().start().resume().get();
        mFragmentTestActivity.setFragment(mNoHfpFragment);
    }

    @Test
    public void createView_notNull() {
        assertThat(mNoHfpFragment.getView()).isNotNull();
    }

    @Test
    public void createView_displayErrorMsg() {
        View rootView = mNoHfpFragment.getView();
        TextView errorMsgView = rootView.findViewById(R.id.error_string);
        assertThat(errorMsgView.getText()).isEqualTo(
                mNoHfpFragment.getString(R.string.bluetooth_disabled));
    }

    @After
    public void tearDown() {
        UiBluetoothMonitor.get().tearDown();
        UiCallManager.get().tearDown();
    }
}
