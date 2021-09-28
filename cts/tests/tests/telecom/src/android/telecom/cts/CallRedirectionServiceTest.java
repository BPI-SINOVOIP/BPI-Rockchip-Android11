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

package android.telecom.cts;

import static android.telecom.cts.TestUtils.shouldTestTelecom;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.app.role.RoleManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;

import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Process;
import android.os.UserHandle;

import android.telecom.Call;
import android.telecom.PhoneAccount;
import android.telecom.cts.redirectiontestapp.CtsCallRedirectionService;
import android.telecom.cts.redirectiontestapp.CtsCallRedirectionServiceController;
import android.telecom.cts.redirectiontestapp.ICtsCallRedirectionServiceController;
import android.text.TextUtils;

import com.android.compatibility.common.util.CddTest;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class CallRedirectionServiceTest extends BaseTelecomTestWithMockServices {
    private static final String TAG = CallRedirectionServiceTest.class.getSimpleName();
    private static final String TEST_APP_NAME = "CTSCRTest";
    private static final String TEST_APP_PACKAGE = "android.telecom.cts.redirectiontestapp";
    private static final String TEST_APP_COMPONENT = TEST_APP_PACKAGE
                    + "/android.telecom.cts.redirectiontestapp.CtsCallRedirectionService";
    private static final String ROLE_CALL_REDIRECTION = "android.app.role.CALL_REDIRECTION";

    private static final Uri SAMPLE_HANDLE = Uri.fromParts(PhoneAccount.SCHEME_TEL, "0001112222",
            null);
    private static final Uri SAMPLE_HANDLE_WITH_POST_DIAL = new Uri.Builder()
            .scheme(PhoneAccount.SCHEME_TEL)
            .encodedOpaquePart("6505551212,1234567890")
            .build();
    private static final Uri SAMPLE_REDIRECT_HANDLE = new Uri.Builder()
            .scheme(PhoneAccount.SCHEME_TEL)
            .encodedOpaquePart("6505551213")
            .build();
    private static final Uri SAMPLE_REDIRECT_HANDLE_WITH_POST_DIAL = new Uri.Builder()
            .scheme(PhoneAccount.SCHEME_TEL)
            .encodedOpaquePart("6505551213,1234567890")
            .build();
    private static final int ASYNC_TIMEOUT = 10000;
    private RoleManager mRoleManager;
    private Handler mHandler;
    private Call mCall;
    private String mPreviousCallRedirectionPackage;
    private ICtsCallRedirectionServiceController mCallRedirectionServiceController;

    private MockInCallService mInCallService;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (!mShouldTestTelecom) {
            return;
        }
        NewOutgoingCallBroadcastReceiver.reset();
        mHandler = new Handler(Looper.getMainLooper());
        mRoleManager = (RoleManager) mContext.getSystemService(Context.ROLE_SERVICE);
        setupControlBinder();
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        rememberPreviousCallRedirectionApp();
        // Ensure CTS app holds the call redirection role.
        addRoleHolder(ROLE_CALL_REDIRECTION,
                CtsCallRedirectionService.class.getPackage().getName());
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        if (!mShouldTestTelecom) {
            return;
        }
        if (mCallRedirectionServiceController != null) {
            mCallRedirectionServiceController.reset();
        }
        // Remove the test app from the redirection role.
        removeRoleHolder(ROLE_CALL_REDIRECTION,
                CtsCallRedirectionService.class.getPackage().getName());

        if (!TextUtils.isEmpty(mPreviousCallRedirectionPackage)) {
            addRoleHolder(ROLE_CALL_REDIRECTION, mPreviousCallRedirectionPackage);
        }
    }

    public void testRedirectedCallWithRedirectedGateway()
            throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }
        mCallRedirectionServiceController.setRedirectCall(
                SAMPLE_HANDLE, null, false);
        placeAndVerifyCallByRedirection(false /* cancelledByCallRedirection */);
        mInCallService = mInCallCallbacks.getService();
        assertCallGatewayConstructed(mInCallService.getLastCall(), true);
        mCall = mInCallService.getLastCall();
        assertEquals(SAMPLE_HANDLE, mCall.getDetails().getGatewayInfo().getGatewayAddress());
        assertEquals(getTestNumber(), mCall.getDetails().getGatewayInfo().getOriginalAddress());
        assertEquals(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, mCall.getDetails().getAccountHandle());
        assertTrue(Call.STATE_DISCONNECTED != mCall.getState());
    }

    /**
     * Verifies that post-dial digits will be re-added to a number after redirection.
     * @throws Exception
     */
    public void testRedirectedCallWithPostDialDigits() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }
        mCallRedirectionServiceController.setRedirectCall(SAMPLE_REDIRECT_HANDLE, null, false);
        Bundle extras = new Bundle();
        extras.putParcelable(TestUtils.EXTRA_PHONE_NUMBER, SAMPLE_HANDLE_WITH_POST_DIAL);
        placeAndVerifyCallByRedirection(extras, false /* cancelledByCallRedirection */);
        mInCallService = mInCallCallbacks.getService();
        assertCallGatewayConstructed(mInCallService.getLastCall(), true);
        mCall = mInCallService.getLastCall();
        assertEquals(SAMPLE_REDIRECT_HANDLE_WITH_POST_DIAL,
                mCall.getDetails().getGatewayInfo().getGatewayAddress());
        // The , (pause) separators get URI encoded in the call intent; compare decoded scheme to
        // ensure proper equality for what it essentially the same thing.
        assertEquals(Uri.decode(SAMPLE_HANDLE_WITH_POST_DIAL.getSchemeSpecificPart()),
                Uri.decode(mCall.getDetails().getGatewayInfo().getOriginalAddress()
                        .getSchemeSpecificPart()));
        assertEquals(SAMPLE_REDIRECT_HANDLE_WITH_POST_DIAL,
                mCall.getDetails().getHandle());
        assertEquals(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, mCall.getDetails().getAccountHandle());
        assertTrue(Call.STATE_DISCONNECTED != mCall.getState());
    }

    public void testRedirectedCallWithRedirectedPhoneAccount()
            throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }
        mCallRedirectionServiceController.setRedirectCall(
                SAMPLE_HANDLE, TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2, false);
        placeAndVerifyCallByRedirection(false /* cancelledByCallRedirection */);
        mInCallService = mInCallCallbacks.getService();
        assertCallGatewayConstructed(mInCallService.getLastCall(), true);
        mCall = mInCallService.getLastCall();
        assertEquals(SAMPLE_HANDLE, mCall.getDetails().getGatewayInfo().getGatewayAddress());
        assertEquals(getTestNumber(), mCall.getDetails().getGatewayInfo().getOriginalAddress());
        assertEquals(TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2, mCall.getDetails().getAccountHandle());
        assertTrue(Call.STATE_DISCONNECTED != mCall.getState());
    }

    public void testCancelCall() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }
        mCallRedirectionServiceController.setCancelCall();
        placeAndVerifyCallByRedirection(true /* cancelledByCallRedirection */);
        mInCallService = mInCallCallbacks.getService();
        mCall = mInCallService.getLastCall();
        assertCallNotNull(mCall, false);
    }

    public void testPlaceCallUnmodified() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }
        mCallRedirectionServiceController.setPlaceCallUnmodified();
        placeAndVerifyCallByRedirection(false /* cancelledByCallRedirection */);
        mInCallService = mInCallCallbacks.getService();
        assertCallDetailsConstructed(mInCallService.getLastCall(), true);
        mCall = mInCallService.getLastCall();
        assertEquals(getTestNumber(), mCall.getDetails().getHandle());
        assertEquals(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, mCall.getDetails().getAccountHandle());
        assertTrue(Call.STATE_DISCONNECTED != mCall.getState());
    }

    /**
     * Sets up a binder used to control the CallRedirectionServiceCtsTestApp.
     * This app is a standalone APK so that it can reside in a package name outside of the one the
     * CTS test itself runs in.
     * @throws InterruptedException
     */
    private void setupControlBinder() throws InterruptedException {
        Intent bindIntent = new Intent(
                CtsCallRedirectionServiceController.CONTROL_INTERFACE_ACTION);
        bindIntent.setComponent(CtsCallRedirectionServiceController.CONTROL_INTERFACE_COMPONENT);
        final CountDownLatch bindLatch = new CountDownLatch(1);

        boolean success = mContext.bindService(bindIntent, new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mCallRedirectionServiceController =
                        ICtsCallRedirectionServiceController.Stub.asInterface(service);
                bindLatch.countDown();
            }
            @Override
            public void onServiceDisconnected(ComponentName name) {
                mCallRedirectionServiceController = null;
            }
        }, Context.BIND_AUTO_CREATE);
        if (!success) {
            fail("Failed to get control interface -- bind error");
        }
        bindLatch.await(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
    }

    /**
     * Use RoleManager to query the previous call redirection app so we can restore it later.
     */
    @CddTest(requirement ="3.2.3.5/C-2-5")
    private void rememberPreviousCallRedirectionApp() {
        runWithShellPermissionIdentity(() -> {
            List<String> callRedirectionApps = mRoleManager.getRoleHolders(ROLE_CALL_REDIRECTION);
            if (!callRedirectionApps.isEmpty()) {
                mPreviousCallRedirectionPackage = callRedirectionApps.get(0);
            } else {
                mPreviousCallRedirectionPackage = null;
            }
        });
    }

    @CddTest(requirement="3.2.3.5/C-2-4")
    private void addRoleHolder(String roleName, String packageName) throws Exception {
        UserHandle user = Process.myUserHandle();
        Executor executor = mContext.getMainExecutor();
        LinkedBlockingQueue<Boolean> queue = new LinkedBlockingQueue(1);

        runWithShellPermissionIdentity(() -> mRoleManager.addRoleHolderAsUser(roleName,
                packageName, RoleManager.MANAGE_HOLDERS_FLAG_DONT_KILL_APP, user, executor,
                successful -> {
                        try {
                            queue.put(successful);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }));
        boolean result = queue.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
        assertTrue(result);
    }

    private void removeRoleHolder(String roleName, String packageName)
            throws Exception {
        UserHandle user = Process.myUserHandle();
        Executor executor = mContext.getMainExecutor();
        LinkedBlockingQueue<Boolean> queue = new LinkedBlockingQueue(1);

        runWithShellPermissionIdentity(() -> mRoleManager.removeRoleHolderAsUser(roleName,
                packageName, RoleManager.MANAGE_HOLDERS_FLAG_DONT_KILL_APP, user, executor,
                successful -> {
                    try {
                        queue.put(successful);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }));
        boolean result = queue.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
        assertTrue(result);
    }
}
