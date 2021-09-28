/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.permission.cts.telephony;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;

/**
 * Test the non-location-related functionality of TelephonyManager.
 */
@RunWith(AndroidJUnit4.class)
public class TelephonyManagerPermissionTest {

    private boolean mHasTelephony;
    TelephonyManager mTelephonyManager = null;
    private AudioManager mAudioManager;

    @Before
    public void setUp() throws Exception {
        mHasTelephony = getContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_TELEPHONY);
        mTelephonyManager =
                (TelephonyManager) getContext().getSystemService(Context.TELEPHONY_SERVICE);
        assertNotNull(mTelephonyManager);
        mAudioManager = (AudioManager) getContext().getSystemService(Context.AUDIO_SERVICE);
        assertNotNull(mAudioManager);
    }

    /**
     * Verify that TelephonyManager.getDeviceId requires Permission.
     * <p>
     * Requires Permission:
     * {@link android.Manifest.permission#READ_PHONE_STATE}.
     */
    @Test
    public void testGetDeviceId() {
        if (!mHasTelephony) {
            return;
        }

        try {
            String id = mTelephonyManager.getDeviceId();
            fail("Got device ID: " + id);
        } catch (SecurityException e) {
            // expected
        }
        try {
            String id = mTelephonyManager.getDeviceId(0);
            fail("Got device ID: " + id);
        } catch (SecurityException e) {
            // expected
        }
    }

    /**
     * Verify that TelephonyManager.getLine1Number requires Permission.
     * <p>
     * Requires Permission:
     * {@link android.Manifest.permission#READ_PHONE_STATE}.
     */
    @Test
    public void testGetLine1Number() {
        if (!mHasTelephony) {
            return;
        }

        try {
            String nmbr = mTelephonyManager.getLine1Number();
            fail("Got line 1 number: " + nmbr);
        } catch (SecurityException e) {
            // expected
        }
    }

    /**
     * Verify that TelephonyManager.getSimSerialNumber requires Permission.
     * <p>
     * Requires Permission:
     * {@link android.Manifest.permission#READ_PHONE_STATE}.
     */
    @Test
    public void testGetSimSerialNumber() {
        if (!mHasTelephony) {
            return;
        }

        try {
            String nmbr = mTelephonyManager.getSimSerialNumber();
            fail("Got SIM serial number: " + nmbr);
        } catch (SecurityException e) {
            // expected
        }
    }

    /**
     * Verify that TelephonyManager.getSubscriberId requires Permission.
     * <p>
     * Requires Permission:
     * {@link android.Manifest.permission#READ_PHONE_STATE}.
     */
    @Test
    public void testGetSubscriberId() {
        if (!mHasTelephony) {
            return;
        }

        try {
            String sid = mTelephonyManager.getSubscriberId();
            fail("Got subscriber id: " + sid);
        } catch (SecurityException e) {
            // expected
        }
    }

    /**
     * Verify that TelephonyManager.getVoiceMailNumber requires Permission.
     * <p>
     * Requires Permission:
     * {@link android.Manifest.permission#READ_PHONE_STATE}.
     */
    @Test
    public void testVoiceMailNumber() {
        if (!mHasTelephony) {
            return;
        }

        try {
            String vmnum = mTelephonyManager.getVoiceMailNumber();
            fail("Got voicemail number: " + vmnum);
        } catch (SecurityException e) {
            // expected
        }
    }
    /**
     * Verify that AudioManager.setMode requires Permission.
     * <p>
     * Requires Permissions:
     * {@link android.Manifest.permission#MODIFY_AUDIO_SETTINGS} and
     * {@link android.Manifest.permission#MODIFY_PHONE_STATE} for
     * {@link AudioManager#MODE_IN_CALL}.
     */
    @Test
    public void testSetMode() {
        if (!mHasTelephony) {
            return;
        }
        int audioMode = mAudioManager.getMode();
        mAudioManager.setMode(AudioManager.MODE_IN_CALL);
        assertEquals(audioMode, mAudioManager.getMode());
    }

    /**
     * Verify that TelephonyManager.setDataEnabled requires Permission.
     * <p>
     * Requires Permission:
     * {@link android.Manifest.permission#MODIFY_PHONE_STATE}.
     */
    @Test
    public void testSetDataEnabled() {
        if (!mHasTelephony) {
            return;
        }
        try {
            mTelephonyManager.setDataEnabled(false);
            fail("Able to set data enabled");
        } catch (SecurityException e) {
            // expected
        }
    }

     /**
     * Tests that isManualNetworkSelectionAllowed requires permission
     * Expects a security exception since the caller does not have carrier privileges.
     */
    @Test
    public void testIsManualNetworkSelectionAllowedWithoutPermission() {
        if (!mHasTelephony) {
            return;
        }
        try {
            mTelephonyManager.isManualNetworkSelectionAllowed();
            fail("Expected SecurityException. App does not have carrier privileges.");
        } catch (SecurityException expected) {
        }
    }

    /**
     * Tests that getManualNetworkSelectionPlmn requires permission
     * Expects a security exception since the caller does not have carrier privileges.
     */
    @Test
    public void testGetManualNetworkSelectionPlmnWithoutPermission() {
        if (!mHasTelephony) {
            return;
        }
        try {
            mTelephonyManager.getManualNetworkSelectionPlmn();
            fail("Expected SecurityException. App does not have carrier privileges.");
        } catch (SecurityException expected) {
        }
    }

    /**
     * Verify that Telephony related broadcasts are protected.
     */
    @Test
    public void testProtectedBroadcasts() {
        if (!mHasTelephony) {
            return;
        }
        try {
            Intent intent = new Intent("android.intent.action.SIM_STATE_CHANGED");
            getContext().sendBroadcast(intent);
            fail("SecurityException expected!");
        } catch (SecurityException e) {}
        try {
            Intent intent = new Intent("android.intent.action.SERVICE_STATE");
            getContext().sendBroadcast(intent);
            fail("SecurityException expected!");
        } catch (SecurityException e) {}
        try {
            Intent intent = new Intent("android.telephony.action.DEFAULT_SUBSCRIPTION_CHANGED");
            getContext().sendBroadcast(intent);
            fail("SecurityException expected!");
        } catch (SecurityException e) {}
        try {
            Intent intent = new Intent(
                    "android.intent.action.ACTION_DEFAULT_DATA_SUBSCRIPTION_CHANGED");
            getContext().sendBroadcast(intent);
            fail("SecurityException expected!");
        } catch (SecurityException e) {}
        try {
            Intent intent = new Intent(
                    "android.telephony.action.DEFAULT_SMS_SUBSCRIPTION_CHANGED");
            getContext().sendBroadcast(intent);
            fail("SecurityException expected!");
        } catch (SecurityException e) {}
        try {
            Intent intent = new Intent(
                    "android.intent.action.ACTION_DEFAULT_VOICE_SUBSCRIPTION_CHANGED");
            getContext().sendBroadcast(intent);
            fail("SecurityException expected!");
        } catch (SecurityException e) {}
        try {
            Intent intent = new Intent("android.intent.action.SIG_STR");
            getContext().sendBroadcast(intent);
            fail("SecurityException expected!");
        } catch (SecurityException e) {}
        try {
            Intent intent = new Intent("android.provider.Telephony.SECRET_CODE");
            getContext().sendBroadcast(intent);
            fail("SecurityException expected!");
        } catch (SecurityException e) {}
    }

    /**
     * Verify that TelephonyManager.getImei requires Permission.
     * <p>
     * Requires Permission:
     * {@link android.Manifest.permission#READ_PHONE_STATE}.
     */
    @Test
    public void testGetImei() {
        if (!mHasTelephony) {
            return;
        }

        try {
            String imei = mTelephonyManager.getImei();
            fail("Got IMEI: " + imei);
        } catch (SecurityException e) {
            // expected
        }
        try {
            String imei = mTelephonyManager.getImei(0);
            fail("Got IMEI: " + imei);
        } catch (SecurityException e) {
            // expected
        }
    }

    /**
     * Verify that getNetworkType and getDataNetworkType requires Permission.
     * <p>
     * Requires Permission:
     * {@link android.Manifest.permission#READ_PHONE_STATE}.
     */
    @Test
    public void testGetNetworkType() {
        if (!mHasTelephony) {
            return;
        }

        try {
            mTelephonyManager.getNetworkType();
            fail("getNetworkType did not throw a SecurityException");
        } catch (SecurityException e) {
            // expected
        }

        try {
            mTelephonyManager.getDataNetworkType();
            fail("getDataNetworkType did not throw a SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    /**
     * Tests that getNetworkSelectionMode requires permission
     * Expects a security exception since the caller does not have carrier privileges.
     */
    @Test
    public void testGetNetworkSelectionModeWithoutPermission() {
        if (!mHasTelephony) {
            return;
        }
        assertThrowsSecurityException(() -> mTelephonyManager.getNetworkSelectionMode(),
                "Expected SecurityException. App does not have carrier privileges.");
    }

    /**
     * Tests that setNetworkSelectionModeAutomatic requires permission
     * Expects a security exception since the caller does not have carrier privileges.
     */
    @Test
    public void testSetNetworkSelectionModeAutomaticWithoutPermission() {
        if (!mHasTelephony) {
            return;
        }
        assertThrowsSecurityException(() -> mTelephonyManager.setNetworkSelectionModeAutomatic(),
                "Expected SecurityException. App does not have carrier privileges.");
    }

    /**
     * Verify that setForbiddenPlmns requires Permission.
     * <p>
     * Requires Permission:
     * {@link android.Manifest.permission#READ_PHONE_STATE READ_PHONE_STATE}
     * or that the calling app has carrier privileges (see {@link #hasCarrierPrivileges}).
     */
    @Test
    public void testSetForbiddenPlmns() {
        if (!mHasTelephony) {
            return;
        }

        try {
            mTelephonyManager.setForbiddenPlmns(new ArrayList<String>());
            fail("SetForbiddenPlmns did not throw a SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    static final int PHONE_STATE_PERMISSION_MASK =
                PhoneStateListener.LISTEN_CALL_FORWARDING_INDICATOR
                        | PhoneStateListener.LISTEN_MESSAGE_WAITING_INDICATOR
                        | PhoneStateListener.LISTEN_EMERGENCY_NUMBER_LIST;

    static final int PRECISE_PHONE_STATE_PERMISSION_MASK =
                PhoneStateListener.LISTEN_PRECISE_DATA_CONNECTION_STATE
                        | PhoneStateListener.LISTEN_CALL_DISCONNECT_CAUSES
                        | PhoneStateListener.LISTEN_IMS_CALL_DISCONNECT_CAUSES
                        | PhoneStateListener.LISTEN_REGISTRATION_FAILURE
                        | PhoneStateListener.LISTEN_BARRING_INFO;

    static final int PHONE_PERMISSIONS_MASK =
            PHONE_STATE_PERMISSION_MASK | PRECISE_PHONE_STATE_PERMISSION_MASK;

    /**
     * Verify the documented permissions for PhoneStateListener.
     */
    @Test
    public void testListen() {
        PhoneStateListener psl = new PhoneStateListener((Runnable r) -> { });

        try {
            for (int i = 1; i != 0; i = i << 1) {
                if ((i & PHONE_PERMISSIONS_MASK) == 0) continue;
                final int listenBit = i;
                assertThrowsSecurityException(() -> mTelephonyManager.listen(psl, listenBit),
                        "Expected a security exception for " + Integer.toHexString(i));
            }
        } finally {
            mTelephonyManager.listen(psl, PhoneStateListener.LISTEN_NONE);
        }
    }

    private static Context getContext() {
        return InstrumentationRegistry.getContext();
    }

    // An actual version of assertThrows() was added in JUnit5
    private static <T extends Throwable> void assertThrows(Class<T> clazz, Runnable r,
            String message) {
        try {
            r.run();
        } catch (Exception expected) {
            assertTrue(clazz.isAssignableFrom(expected.getClass()));
            return;
        }
        fail(message);
    }

    private static void assertThrowsSecurityException(Runnable r, String message) {
        assertThrows(SecurityException.class, r, message);
    }
}
