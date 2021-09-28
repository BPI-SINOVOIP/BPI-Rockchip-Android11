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

package com.android.car.settings.accounts;

import static com.android.car.settings.common.PreferenceController.AVAILABLE;
import static com.android.car.settings.common.PreferenceController.DISABLED_FOR_USER;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.testutils.ShadowUserHelper;
import com.android.car.settings.users.UserHelper;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/** Unit test for {@link AccountsEntryPreferenceController}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUserHelper.class})
public class AccountsEntryPreferenceControllerTest {

    @Mock
    private UserHelper mMockUserHelper;
    private AccountsEntryPreferenceController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowUserHelper.setInstance(mMockUserHelper);

        mController = new PreferenceControllerTestHelper<>(RuntimeEnvironment.application,
                AccountsEntryPreferenceController.class).getController();
    }

    @After
    public void tearDown() {
        ShadowUserHelper.reset();
    }

    @Test
    public void getAvailabilityStatus_cannotModifyAccounts_disabledForUser() {
        when(mMockUserHelper.canCurrentProcessModifyAccounts()).thenReturn(false);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(DISABLED_FOR_USER);
    }

    @Test
    public void getAvailabilityStatus_canModifyAccounts_available() {
        when(mMockUserHelper.canCurrentProcessModifyAccounts()).thenReturn(true);

        assertThat(mController.getAvailabilityStatus()).isEqualTo(AVAILABLE);
    }
}
