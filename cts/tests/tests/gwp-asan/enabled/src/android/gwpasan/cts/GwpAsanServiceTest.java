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

package android.gwpasan.cts;

import android.content.Intent;
import android.os.IBinder;
import android.os.Parcel;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.rule.ServiceTestRule;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeoutException;

import static androidx.test.core.app.ApplicationProvider.getApplicationContext;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import android.gwpasan.GwpAsanEnabledService;
import android.gwpasan.GwpAsanDisabledService;
import android.gwpasan.GwpAsanDefaultService;

@RunWith(AndroidJUnit4.class)
public class GwpAsanServiceTest {
    @Rule
    public final ServiceTestRule mServiceRule = new ServiceTestRule();

    private boolean isGwpAsanEnabledInService(Class<?> cls) throws Exception {
        Intent serviceIntent = new Intent(getApplicationContext(), cls);
        IBinder binder = mServiceRule.bindService(serviceIntent);
        final Parcel request = Parcel.obtain();
        final Parcel reply = Parcel.obtain();
        if (!binder.transact(42, request, reply, 0)) {
          throw new Exception();
        }
        int res = reply.readInt();
        if (res < 0) {
          throw new Exception();
        }
        return res != 0;
    }

    @Test
    public void testEnabledService() throws Exception {
        assertTrue(isGwpAsanEnabledInService(GwpAsanEnabledService.class));
    }

    @Test
    public void testDisabledService() throws Exception {
        assertFalse(isGwpAsanEnabledInService(GwpAsanDisabledService.class));
    }

    @Test
    public void testDefaultService() throws Exception {
        // GwpAsanDefaultService inherits the application attribute.
        assertTrue(isGwpAsanEnabledInService(GwpAsanDefaultService.class));
    }
}
