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

package android.telecom.cts;

import static android.telecom.cts.TestUtils.TEST_PHONE_ACCOUNT_HANDLE;
import static android.telecom.cts.TestUtils.WAIT_FOR_STATE_CHANGE_TIMEOUT_MS;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.app.UiModeManager;
import android.app.role.RoleManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.telecom.TelecomManager;
import android.telecom.cts.thirdptyincallservice.CtsThirdPartyInCallService;
import android.telecom.cts.thirdptyincallservice.CtsThirdPartyInCallServiceControl;
import android.telecom.cts.thirdptyincallservice.ICtsThirdPartyInCallServiceControl;
import android.text.TextUtils;
import android.util.Log;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class ThirdPartyInCallServiceTest extends BaseTelecomTestWithMockServices {

    private static final String TAG = ThirdPartyInCallServiceTest.class.getSimpleName();
    private static final String ROLE_COMPANION_APP = "android.app.role.CALL_COMPANION";
    private static final Uri sTestUri = Uri.parse("tel:555-TEST");
    private Context mContext;
    private UiModeManager mUiModeManager;
    private TelecomManager mTelecomManager;
    private CtsRoleManagerAdapter mCtsRoleManagerAdapter;
    ICtsThirdPartyInCallServiceControl mICtsThirdPartyInCallServiceControl;
    private boolean mSkipNullUnboundLatch;
    private String mPreviousRoleHolder;
    private String mThirdPartyPackageName;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getContext();
        mCtsRoleManagerAdapter = new CtsRoleManagerAdapter(getInstrumentation());
        mTelecomManager = (TelecomManager) mContext.getSystemService(Context.TELECOM_SERVICE);
        mUiModeManager = (UiModeManager) mContext.getSystemService(Context.UI_MODE_SERVICE);
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        setUpControl();
        mICtsThirdPartyInCallServiceControl.resetLatchForServiceBound(false);
        mSkipNullUnboundLatch = false;

        mThirdPartyPackageName = CtsThirdPartyInCallService.class.getPackage().getName();

        // Ensure no ThirdPartyInCallService serves as a companion app.
        // (Probably from previous test failures, if any.)
        mCtsRoleManagerAdapter.removeCompanionAppRoleHolder(mThirdPartyPackageName);
    }

    @Override
    public void tearDown() throws Exception {
        mICtsThirdPartyInCallServiceControl.resetCalls();
        // Remove the third party companion app before tear down.
        mCtsRoleManagerAdapter.removeCompanionAppRoleHolder(mThirdPartyPackageName);

        super.tearDown();
        if (!mSkipNullUnboundLatch) {
            assertBindStatus(/* true: bind, false: unbind */false, /* expected result */true);
        }
    }

    public void doNotTestCallWithoutThirdPartyApp() throws Exception {
        // No companion apps
        List<String> previousHolders = mCtsRoleManagerAdapter.getRoleHolders(ROLE_COMPANION_APP);
        assertEquals(0, previousHolders.size());

        // No car mode dialer
        assertFalse(mThirdPartyPackageName.equals(mPreviousRoleHolder));

        int previousCallCount = mICtsThirdPartyInCallServiceControl.getLocalCallCount();
        addAndVerifyNewIncomingCall(sTestUri, null);
        assertBindStatus(/* true: bind, false: unbind */true, /* expected result */false);
        assertCallCount(previousCallCount);
        // Third Party InCallService hasn't been bound yet, unbound latch can be null when tearDown.
        mSkipNullUnboundLatch = true;
    }

    public void doNotTestCallWithCompanionApps() throws Exception {
        // Set companion app default.
        mCtsRoleManagerAdapter.addCompanionAppRoleHolder(mThirdPartyPackageName);
        List<String> previousHolders = mCtsRoleManagerAdapter.getRoleHolders(ROLE_COMPANION_APP);
        assertEquals(1, previousHolders.size());
        assertEquals(mThirdPartyPackageName, previousHolders.get(0));

        int previousCallCount = mICtsThirdPartyInCallServiceControl.getLocalCallCount();
        addAndVerifyNewIncomingCall(sTestUri, null);
        assertBindStatus(/* true: bind, false: unbind */true, /* expected result */true);
        assertCallCount(previousCallCount + 1);
        mICtsThirdPartyInCallServiceControl.resetLatchForServiceBound(true);
    }

    private void addAndVerifyNewIncomingCallInCarMode(Uri incomingHandle, Bundle extras)
            throws RemoteException {
        int currentCallCount = mICtsThirdPartyInCallServiceControl.getLocalCallCount();
        if (extras == null) {
            extras = new Bundle();
        }
        extras.putParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS, incomingHandle);
        mTelecomManager.addNewIncomingCall(TEST_PHONE_ACCOUNT_HANDLE, extras);
        assertBindStatus(/* true: bind, false: unbind */true, /* expected result */true);
        assertCallCount(currentCallCount + 1);
    }

    /**
     *
     * @param bind: check the status of InCallService bind latches.
     *             Values: true (bound latch), false (unbound latch).
     * @param success: whether the latch should have counted down.
     */
    private void assertBindStatus(boolean bind, boolean success) {
        waitUntilConditionIsTrueOrTimeout(new Condition() {
            @Override
            public Object expected() {
                return success;
            }

            @Override
            public Object actual() {
                try {
                    return mICtsThirdPartyInCallServiceControl.checkBindStatus(bind);
                } catch (RemoteException re) {
                    Log.e(TAG, "Remote exception when checking bind status: " + re);
                    return false;
                }
            }
        }, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS, "Unable to " + (bind ? "Bind" : "Unbind")
                + " third party in call service");
    }

    private void assertCallCount(int expected) {
        waitUntilConditionIsTrueOrTimeout(new Condition() {
            @Override
            public Object expected() {
                return expected;
            }

            @Override
            public Object actual() {
                try {
                    return mICtsThirdPartyInCallServiceControl.getLocalCallCount();
                } catch (RemoteException re) {
                    Log.e(TAG, "Remote exception when getting local call count: " + re);
                    return -1;
                }

            }
        }, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS,
                "Failed to match localCallCount and expected: " + expected);
    }

    private void setUpControl() throws InterruptedException {
        Intent bindIntent = new Intent(CtsThirdPartyInCallServiceControl.CONTROL_INTERFACE_ACTION);
        // mContext is android.telecom.cts, which doesn't include thirdptyincallservice.
        ComponentName controlComponentName =
                ComponentName.createRelative(
                        CtsThirdPartyInCallServiceControl.class.getPackage().getName(),
                        CtsThirdPartyInCallServiceControl.class.getName());

        bindIntent.setComponent(controlComponentName);
        final CountDownLatch bindLatch = new CountDownLatch(1);
        boolean success = mContext.bindService(bindIntent, new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                Log.i(TAG, "Service Connected: " + name);
                mICtsThirdPartyInCallServiceControl =
                        ICtsThirdPartyInCallServiceControl.Stub.asInterface(service);
                bindLatch.countDown();
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                mICtsThirdPartyInCallServiceControl = null;
            }
        }, Context.BIND_AUTO_CREATE);
        if (!success) {
            fail("Failed to get control interface -- bind error");
        }
        bindLatch.await(WAIT_FOR_STATE_CHANGE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private void cacheCurrentRoleHolder(String roleName) {
        runWithShellPermissionIdentity(() -> {
            List<String> previousHolders = mCtsRoleManagerAdapter.getRoleHolders(roleName);
            if (previousHolders == null || previousHolders.isEmpty()) {
                mPreviousRoleHolder = null;
            } else {
                mPreviousRoleHolder = previousHolders.get(0);
            }
        });
    }

}
