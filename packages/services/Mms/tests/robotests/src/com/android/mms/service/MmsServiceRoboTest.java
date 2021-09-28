/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.mms.service;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.content.Intent;
import android.net.Uri;
import android.os.Process;
import android.os.RemoteException;

import com.android.internal.telephony.IMms;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.shadows.ShadowBinder;

@RunWith(RobolectricTestRunner.class)
public final class MmsServiceRoboTest {
    private IMms.Stub binder;

    @Before
    public void setUp() {
        MmsService mmsService = Robolectric.setupService(MmsService.class);

        final Intent intent = new Intent();

        binder = (IMms.Stub) mmsService.onBind(intent);
    }

    @Test
    public void testOnBind_IsNotNull() {
        assertThat(binder).isNotNull();
    }

    @Test
    public void testSendMessage_DoesNotThrowIfSystemUid() throws RemoteException {
        ShadowBinder.setCallingUid(Process.SYSTEM_UID);
        binder.sendMessage(/* subId= */ 0, "callingPkg", Uri.parse("contentUri"),
                "locationUrl", /* configOverrides= */ null, /* sentIntent= */ null,
                /* messageId= */ 0L);
    }

    @Test
    public void testSendMessageThrows_IfNotSystemUid() {
        assertThrows(SecurityException.class,
                () -> binder.sendMessage(/* subId= */ 0, "callingPkg", Uri.parse("contentUri"),
                        "locationUrl", /* configOverrides= */ null, /* sentIntent= */ null,
                        /* messageId= */ 0L));
    }
}
