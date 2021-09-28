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

import android.car.user.CarUserManager.UserLifecycleEvent;
import android.os.UserHandle;

import org.junit.Test;

public final class UserLifecycleEventTest {

    @Test
    public void testFullConstructor() {
        int eventType = 42;
        int from = 10;
        int to = 20;
        UserLifecycleEvent event = new UserLifecycleEvent(eventType, from, to);

        assertThat(event.getEventType()).isEqualTo(eventType);
        assertThat(event.getUserId()).isEqualTo(to);
        assertThat(event.getUserHandle().getIdentifier()).isEqualTo(to);
        assertThat(event.getPreviousUserId()).isEqualTo(from);
        assertThat(event.getPreviousUserHandle().getIdentifier()).isEqualTo(from);
    }

    @Test
    public void testAlternativeConstructor() {
        int eventType = 42;
        int to = 20;
        UserLifecycleEvent event = new UserLifecycleEvent(eventType, to);

        assertThat(event.getEventType()).isEqualTo(eventType);
        assertThat(event.getUserId()).isEqualTo(to);
        assertThat(event.getUserHandle().getIdentifier()).isEqualTo(to);
        assertThat(event.getPreviousUserId()).isEqualTo(UserHandle.USER_NULL);
        assertThat(event.getPreviousUserHandle()).isNull();
    }
}
