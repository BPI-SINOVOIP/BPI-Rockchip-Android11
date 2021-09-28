/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.app.cts;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.app.KeyguardManager;
import android.app.stubs.KeyguardManagerActivity;
import android.content.Context;
import android.test.ActivityInstrumentationTestCase2;

import java.lang.reflect.Method;
import java.util.concurrent.CountDownLatch;

public class KeyguardManagerTest
        extends ActivityInstrumentationTestCase2<KeyguardManagerActivity> {

    private static final String TAG = "KeyguardManagerTest";

    public KeyguardManagerTest() {
        super("android.app.stubs", KeyguardManagerActivity.class);
    }

    public void testNewKeyguardLock() {
        final Context c = getInstrumentation().getContext();
        final KeyguardManager keyguardManager = (KeyguardManager) c.getSystemService(
                Context.KEYGUARD_SERVICE);
        final KeyguardManager.KeyguardLock keyLock = keyguardManager.newKeyguardLock(TAG);
        assertNotNull(keyLock);
    }

    public void testPrivateNotificationsAllowed() throws Exception {
        Context c = getInstrumentation().getContext();
        KeyguardManager keyguardManager = (KeyguardManager) c.getSystemService(
                Context.KEYGUARD_SERVICE);
        Method set = KeyguardManager.class.getMethod(
                "setPrivateNotificationsAllowed", boolean.class);
        Method get = KeyguardManager.class.getMethod("getPrivateNotificationsAllowed");
        CountDownLatch signal = new CountDownLatch(1);
        runWithShellPermissionIdentity(() -> {
                set.invoke(keyguardManager, false);
                assertFalse((boolean) get.invoke(keyguardManager));
                set.invoke(keyguardManager, true);
                assertTrue((boolean) get.invoke(keyguardManager));
                signal.countDown();
        }, "android.permission.CONTROL_KEYGUARD_SECURE_NOTIFICATIONS");
        signal.await();
    }
}
