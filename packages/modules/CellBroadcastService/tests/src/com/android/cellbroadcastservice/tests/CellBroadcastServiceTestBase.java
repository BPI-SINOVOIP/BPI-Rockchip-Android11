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

package com.android.cellbroadcastservice.tests;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.location.LocationManager;
import android.os.Handler;
import android.os.IPowerManager;
import android.os.IThermalService;
import android.os.PowerManager;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.test.mock.MockContentResolver;
import android.testing.TestableLooper;

import com.google.common.collect.ArrayListMultimap;
import com.google.common.collect.Multimap;

import junit.framework.TestCase;

import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;

/**
 * This is the test base class can be extended by all cell broadcast service unit test classes.
 */
public class CellBroadcastServiceTestBase extends TestCase {

    @Mock
    protected Context mMockedContext;

    @Mock
    protected Resources mMockedResources;

    @Mock
    protected SubscriptionManager mMockedSubscriptionManager;

    @Mock
    protected TelephonyManager mMockedTelephonyManager;

    @Mock
    protected LocationManager mMockedLocationManager;

    @Mock
    protected PackageManager mMockedPackageManager;

    private final MockContentResolver mMockedContentResolver = new MockContentResolver();

    private final Multimap<String, BroadcastReceiver> mBroadcastReceiversByAction =
            ArrayListMultimap.create();

    private final HashMap<InstanceKey, Object> mOldInstances = new HashMap<>();

    private final LinkedList<InstanceKey> mInstanceKeys = new LinkedList<>();

    protected static final int FAKE_SUBID = 1;

    private static class InstanceKey {
        final Class mClass;
        final String mInstName;
        final Object mObj;

        InstanceKey(final Class c, final String instName, final Object obj) {
            mClass = c;
            mInstName = instName;
            mObj = obj;
        }

        @Override
        public int hashCode() {
            return (mClass.getName().hashCode() * 31 + mInstName.hashCode()) * 31;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj == null || obj.getClass() != getClass()) {
                return false;
            }

            InstanceKey other = (InstanceKey) obj;
            return (other.mClass == mClass && other.mInstName.equals(mInstName)
                    && other.mObj == mObj);
        }
    }

    protected void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        doReturn(mMockedContentResolver).when(mMockedContext).getContentResolver();
        doReturn(mMockedResources).when(mMockedContext).getResources();

        // Can't directly mock power manager because it's final.
        PowerManager powerManager = new PowerManager(mMockedContext, mock(IPowerManager.class),
                mock(IThermalService.class),
                new Handler(TestableLooper.get(CellBroadcastServiceTestBase.this).getLooper()));
        doReturn(powerManager).when(mMockedContext).getSystemService(Context.POWER_SERVICE);
        doReturn(mMockedTelephonyManager).when(mMockedContext)
                .getSystemService(Context.TELEPHONY_SERVICE);
        doReturn(Context.TELEPHONY_SERVICE).when(mMockedContext)
                .getSystemServiceName(TelephonyManager.class);
        doReturn(mMockedSubscriptionManager).when(mMockedContext)
                .getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        doReturn(mMockedLocationManager).when(mMockedContext)
                .getSystemService(Context.LOCATION_SERVICE);
        doReturn(mMockedPackageManager).when(mMockedContext)
                .getPackageManager();
        doReturn(mMockedContext).when(mMockedContext).createContextAsUser(any(), anyInt());
        doReturn(new int[]{FAKE_SUBID}).when(mMockedSubscriptionManager)
                .getSubscriptionIds(anyInt());
        doReturn(mMockedTelephonyManager).when(mMockedTelephonyManager)
                .createForSubscriptionId(anyInt());
        doAnswer(invocation -> {
            BroadcastReceiver receiver = invocation.getArgument(0);
            IntentFilter intentFilter = invocation.getArgument(1);
            for (int i = 0; i < intentFilter.countActions(); i++) {
                mBroadcastReceiversByAction.put(intentFilter.getAction(i), receiver);
            }
            return null;
        }).when(mMockedContext).registerReceiver(
                any(BroadcastReceiver.class), any(IntentFilter.class));
    }

    protected void tearDown() throws Exception {
        restoreInstances();
    }

    void sendBroadcast(Intent intent) {
        if (mBroadcastReceiversByAction.containsKey(intent.getAction())) {
            for (BroadcastReceiver receiver : mBroadcastReceiversByAction.get(intent.getAction())) {
                receiver.onReceive(mMockedContext, intent);
            }
        }
    }

    void putResources(int id, Object value) {
        if (value instanceof String[]) {
            doReturn(value).when(mMockedResources).getStringArray(eq(id));
        } else if (value instanceof Boolean) {
            doReturn(value).when(mMockedResources).getBoolean(eq(id));
        } else if (value instanceof Integer) {
            doReturn(value).when(mMockedResources).getInteger(eq(id));
        } else if (value instanceof Integer[]) {
            doReturn(value).when(mMockedResources).getIntArray(eq(id));
        } else if (value instanceof int[]) {
            doReturn(value).when(mMockedResources).getIntArray(eq(id));
        } else if (value instanceof String) {
            doReturn(value).when(mMockedResources).getString(eq(id));
        }
    }

    synchronized void replaceInstance(final Class c, final String instanceName,
                                              final Object obj, final Object newValue)
            throws Exception {
        Field field = c.getDeclaredField(instanceName);
        field.setAccessible(true);

        InstanceKey key = new InstanceKey(c, instanceName, obj);
        if (!mOldInstances.containsKey(key)) {
            mOldInstances.put(key, field.get(obj));
            mInstanceKeys.add(key);
        }
        field.set(obj, newValue);
    }

    private synchronized void restoreInstances() throws Exception {
        Iterator<InstanceKey> it = mInstanceKeys.descendingIterator();

        while (it.hasNext()) {
            InstanceKey key = it.next();
            Field field = key.mClass.getDeclaredField(key.mInstName);
            field.setAccessible(true);
            field.set(key.mObj, mOldInstances.get(key));
        }

        mInstanceKeys.clear();
        mOldInstances.clear();
    }

    /**
     * Converts a hex String to a byte array.
     *
     * @param s A string of hexadecimal characters, must be an even number of
     *          chars long
     *
     * @return byte array representation
     *
     * @throws RuntimeException on invalid format
     */
    public static byte[] hexStringToBytes(String s) {
        byte[] ret;

        if (s == null) return null;

        int sz = s.length();

        ret = new byte[sz / 2];

        for (int i = 0; i < sz; i += 2) {
            ret[i / 2] = (byte) ((hexCharToInt(s.charAt(i)) << 4) | hexCharToInt(s.charAt(i + 1)));
        }

        return ret;
    }

    /**
     * Converts a hex char to its integer value
     *
     * @param c A single hexadecimal character. Must be in one of these ranges:
     *          - '0' to '9', or
     *          - 'a' to 'f', or
     *          - 'A' to 'F'
     *
     * @return the integer representation of {@code c}
     *
     * @throws RuntimeException on invalid character
     */
    private static int hexCharToInt(char c) {
        if (c >= '0' && c <= '9') return (c - '0');
        if (c >= 'A' && c <= 'F') return (c - 'A' + 10);
        if (c >= 'a' && c <= 'f') return (c - 'a' + 10);

        throw new RuntimeException("invalid hex char '" + c + "'");
    }
}
