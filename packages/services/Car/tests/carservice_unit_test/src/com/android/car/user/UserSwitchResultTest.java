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
 * limitations under the License.
 */

package com.android.car.user;

import static com.google.common.truth.Truth.assertThat;

import android.car.user.UserSwitchResult;

import org.junit.Test;

public final class UserSwitchResultTest {

    @Test
    public void testIUserSwitchResult_checkStatusAndMessage() {
        String msg = "Test Message";
        UserSwitchResult result =
                new UserSwitchResult(UserSwitchResult.STATUS_SUCCESSFUL, msg);
        assertThat(result.getStatus()).isEqualTo(UserSwitchResult.STATUS_SUCCESSFUL);
        assertThat(result.getErrorMessage()).isEqualTo(msg);
    }

    @Test
    public void testIUserSwitchResult_isSuccess_failure() {
        UserSwitchResult result =
                new UserSwitchResult(UserSwitchResult.STATUS_ANDROID_FAILURE, null);
        assertThat(result.isSuccess()).isFalse();
    }

    @Test
    public void testIUserSwitchResult_isSuccess_success() {
        UserSwitchResult result =
                new UserSwitchResult(UserSwitchResult.STATUS_SUCCESSFUL, null);
        assertThat(result.isSuccess()).isTrue();
    }

    @Test
    public void testIUserSwitchResult_isSuccess_requestedState() {
        UserSwitchResult result =
                new UserSwitchResult(UserSwitchResult.STATUS_OK_USER_ALREADY_IN_FOREGROUND, null);
        assertThat(result.isSuccess()).isTrue();
    }
}
