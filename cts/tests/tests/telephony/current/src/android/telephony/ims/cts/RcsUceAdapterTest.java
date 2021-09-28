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

package android.telephony.ims.cts;

import static junit.framework.Assert.assertTrue;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.provider.Telephony;
import android.telecom.PhoneAccount;
import android.telephony.SubscriptionManager;
import android.telephony.ims.ImsException;
import android.telephony.ims.ImsManager;
import android.telephony.ims.ImsRcsManager;
import android.telephony.ims.RcsUceAdapter;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.ShellIdentityUtils;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

@RunWith(AndroidJUnit4.class)
public class RcsUceAdapterTest {

    private static int sTestSub = SubscriptionManager.INVALID_SUBSCRIPTION_ID;
    private static final Uri TEST_NUMBER_URI =
            Uri.fromParts(PhoneAccount.SCHEME_TEL, "6505551212", null);
    private static final Uri LISTENER_URI = Uri.withAppendedPath(Telephony.SimInfo.CONTENT_URI,
            Telephony.SimInfo.COLUMN_IMS_RCS_UCE_ENABLED);
    private static HandlerThread sHandlerThread;

    private ContentObserver mUceObserver;

    @BeforeClass
    public static void beforeAllTests() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        sTestSub = ImsUtils.getPreferredActiveSubId();

        if (Looper.getMainLooper() == null) {
            Looper.prepareMainLooper();
        }
        sHandlerThread = new HandlerThread("CtsTelephonyTestCases");
        sHandlerThread.start();
    }

    @AfterClass
    public static void afterAllTests() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        if (sHandlerThread != null) {
            sHandlerThread.quit();
        }
    }

    @Before
    public void beforeTest() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        if (!SubscriptionManager.isValidSubscriptionId(sTestSub)) {
            fail("This test requires that there is a SIM in the device!");
        }
    }

    @Test
    public void testCapabilityDiscoveryIntentReceiverExists() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        PackageManager packageManager = getContext().getPackageManager();
        ResolveInfo info = packageManager.resolveActivity(
                new Intent(ImsRcsManager.ACTION_SHOW_CAPABILITY_DISCOVERY_OPT_IN),
                PackageManager.MATCH_DEFAULT_ONLY);
        assertNotNull(ImsRcsManager.ACTION_SHOW_CAPABILITY_DISCOVERY_OPT_IN
                + " Intent action must be handled by an appropriate settings application.", info);
        assertNotEquals(ImsRcsManager.ACTION_SHOW_CAPABILITY_DISCOVERY_OPT_IN
                + " activity intent filter must have a > 0 priority.", 0, info.priority);
    }

    @Test
    public void testGetAndSetUceSetting() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        RcsUceAdapter adapter = imsManager.getImsRcsManager(sTestSub).getUceAdapter();
        assertNotNull("RcsUceAdapter can not be null!", adapter);

        Boolean isEnabled = null;
        try {
            isEnabled = ShellIdentityUtils.invokeThrowableMethodWithShellPermissions(
                    adapter, RcsUceAdapter::isUceSettingEnabled, ImsException.class,
                    "android.permission.READ_PHONE_STATE");
            assertNotNull(isEnabled);

            // Ensure the ContentObserver gets the correct callback based on the change.
            LinkedBlockingQueue<Uri> queue = new LinkedBlockingQueue<>(1);
            registerUceObserver(queue::offer);
            boolean userSetIsEnabled = isEnabled;
            ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(
                    adapter, a -> a.setUceSettingEnabled(!userSetIsEnabled), ImsException.class,
                    "android.permission.MODIFY_PHONE_STATE");
            Uri result = queue.poll(ImsUtils.TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            assertNotNull(result);
            assertTrue("Unexpected URI, should only receive URIs with prefix " + LISTENER_URI,
                    result.isPathPrefixMatch(LISTENER_URI));
            // Verify the subId associated with the Observer is correct.
            List<String> pathSegments = result.getPathSegments();
            String subId = pathSegments.get(pathSegments.size() - 1);
            assertEquals("Subscription ID contained in ContentObserver URI doesn't match the "
                            + "subscription that has changed.",
                    String.valueOf(sTestSub), subId);

            Boolean setResult = ShellIdentityUtils.invokeThrowableMethodWithShellPermissions(
                    adapter, RcsUceAdapter::isUceSettingEnabled, ImsException.class,
                    "android.permission.READ_PHONE_STATE");
            assertNotNull(setResult);
            assertEquals("Incorrect setting!", !userSetIsEnabled, setResult);
        } catch (ImsException e) {
            if (e.getCode() != ImsException.CODE_ERROR_UNSUPPORTED_OPERATION) {
                fail("failed getting UCE setting with code: " + e.getCode());
            }
        } finally {
            if (isEnabled != null) {
                boolean userSetIsEnabled = isEnabled;
                // set back to user preference
                ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(
                        adapter, a -> a.setUceSettingEnabled(userSetIsEnabled), ImsException.class,
                        "android.permission.MODIFY_PHONE_STATE");
            }
            unregisterUceObserver();
        }
    }

    @Test
    public void testMethodPermissions() throws Exception {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsManager imsManager = getContext().getSystemService(ImsManager.class);
        RcsUceAdapter uceAdapter = imsManager.getImsRcsManager(sTestSub).getUceAdapter();
        assertNotNull("UCE adapter should not be null!", uceAdapter);
        ArrayList<Uri> numbers = new ArrayList<>(1);
        numbers.add(TEST_NUMBER_URI);

        // isUceSettingEnabled - read
        Boolean isUceSettingEnabledResult = null;
        try {
            isUceSettingEnabledResult =
                    ShellIdentityUtils.invokeThrowableMethodWithShellPermissions(
                    uceAdapter, RcsUceAdapter::isUceSettingEnabled, ImsException.class,
                    "android.permission.READ_PHONE_STATE");
            assertNotNull("result from isUceSettingEnabled should not be null",
                    isUceSettingEnabledResult);
        } catch (SecurityException e) {
            fail("isUceSettingEnabled should succeed with READ_PHONE_STATE.");
        } catch (ImsException e) {
            // unsupported is a valid fail cause.
            if (e.getCode() != ImsException.CODE_ERROR_UNSUPPORTED_OPERATION) {
                fail("isUceSettingEnabled failed with code " + e.getCode());
            }
        }

        // isUceSettingEnabled - read_privileged
        try {
            isUceSettingEnabledResult =
                    ShellIdentityUtils.invokeThrowableMethodWithShellPermissions(
                            uceAdapter, RcsUceAdapter::isUceSettingEnabled, ImsException.class,
                            "android.permission.READ_PRIVILEGED_PHONE_STATE");
            assertNotNull("result from isUceSettingEnabled should not be null",
                    isUceSettingEnabledResult);
        } catch (SecurityException e) {
            fail("isUceSettingEnabled should succeed with READ_PRIVILEGED_PHONE_STATE.");
        } catch (ImsException e) {
            // unsupported is a valid fail cause.
            if (e.getCode() != ImsException.CODE_ERROR_UNSUPPORTED_OPERATION) {
                fail("isUceSettingEnabled failed with code " + e.getCode());
            }
        }

        // setUceSettingEnabled
        boolean isUceSettingEnabled =
                (isUceSettingEnabledResult == null ? false : isUceSettingEnabledResult);
        try {
            ShellIdentityUtils.invokeThrowableMethodWithShellPermissionsNoReturn(uceAdapter,
                    (m) -> m.setUceSettingEnabled(isUceSettingEnabled), ImsException.class,
                    "android.permission.MODIFY_PHONE_STATE");
        } catch (SecurityException e) {
            fail("setUceSettingEnabled should succeed with MODIFY_PHONE_STATE.");
        } catch (ImsException e) {
            // unsupported is a valid fail cause.
            if (e.getCode() != ImsException.CODE_ERROR_UNSUPPORTED_OPERATION) {
                fail("setUceSettingEnabled failed with code " + e.getCode());
            }
        }

        // getUcePublishState
        try {
            uceAdapter.getUcePublishState();
            fail("getUcePublishState should require READ_PRIVILEGED_PHONE_STATE.");
        } catch (SecurityException e) {
            //expected
        }

        // requestCapabilities
        try {
            uceAdapter.requestCapabilities(Runnable::run, numbers,
                    new RcsUceAdapter.CapabilitiesCallback() {});
            fail("requestCapabilities should require READ_PRIVILEGED_PHONE_STATE.");
        } catch (SecurityException e) {
            //expected
        }
    }

    private void registerUceObserver(Consumer<Uri> resultConsumer) {
        mUceObserver = new ContentObserver(new Handler(sHandlerThread.getLooper())) {
            @Override
            public void onChange(boolean selfChange, Uri uri) {
                resultConsumer.accept(uri);
            }
        };
        getContext().getContentResolver().registerContentObserver(LISTENER_URI,
                true /*notifyForDecendents*/, mUceObserver);
    }

    private void unregisterUceObserver() {
        if (mUceObserver != null) {
            getContext().getContentResolver().unregisterContentObserver(mUceObserver);
        }
    }

    private static Context getContext() {
        return InstrumentationRegistry.getInstrumentation().getContext();
    }
}
