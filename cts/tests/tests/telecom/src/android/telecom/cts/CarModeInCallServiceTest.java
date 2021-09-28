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

import android.app.UiModeManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.telecom.TelecomManager;
import android.telecom.cts.carmodetestapp.ICtsCarModeInCallServiceControl;

import androidx.test.InstrumentationRegistry;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class CarModeInCallServiceTest extends BaseTelecomTestWithMockServices {
    private static final int ASYNC_TIMEOUT = 10000;
    private static final String CARMODE_APP1_PACKAGE = "android.telecom.cts.carmodetestapp";
    private static final String CARMODE_APP2_PACKAGE = "android.telecom.cts.carmodetestapptwo";
    private ICtsCarModeInCallServiceControl mCarModeIncallServiceControlOne;
    private ICtsCarModeInCallServiceControl mCarModeIncallServiceControlTwo;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (!mShouldTestTelecom) {
            return;
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.ENTER_CAR_MODE_PRIORITIZED",
                        "android.permission.CONTROL_INCALL_EXPERIENCE");

        mCarModeIncallServiceControlOne = getControlBinder(CARMODE_APP1_PACKAGE);
        mCarModeIncallServiceControlTwo = getControlBinder(CARMODE_APP2_PACKAGE);
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);

        final UiModeManager uiModeManager = mContext.getSystemService(UiModeManager.class);
        assertEquals("Device must not be in car mode at start of test.",
                Configuration.UI_MODE_TYPE_NORMAL, uiModeManager.getCurrentModeType());
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        if (!mShouldTestTelecom) {
            return;
        }

        if (mCarModeIncallServiceControlOne != null) {
            mCarModeIncallServiceControlOne.reset();
        }

        if (mCarModeIncallServiceControlTwo != null) {
            mCarModeIncallServiceControlTwo.reset();
        }

        assertUiMode(Configuration.UI_MODE_TYPE_NORMAL);

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    /**
     * Verifies that a car mode InCallService can enter and exit car mode using a priority.
     */
    public void testSetCarMode() {
        if (!mShouldTestTelecom) {
            return;
        }

        enableAndVerifyCarMode(mCarModeIncallServiceControlOne, 1000);
        disableAndVerifyCarMode(mCarModeIncallServiceControlOne, Configuration.UI_MODE_TYPE_NORMAL);
    }

    /**
     * Verifies we bind to a car mode InCallService when a call is started when the device is
     * already in car mode.
     */
    public void testStartCallInCarMode() {
        if (!mShouldTestTelecom) {
            return;
        }

        enableAndVerifyCarMode(mCarModeIncallServiceControlOne, 1000);

        // Place a call and verify we bound to the Car Mode InCallService
        placeCarModeCall();
        verifyCarModeBound(mCarModeIncallServiceControlOne);
        assertCarModeCallCount(mCarModeIncallServiceControlOne, 1);
        disconnectAllCallsAndVerify(mCarModeIncallServiceControlOne);

        disableAndVerifyCarMode(mCarModeIncallServiceControlOne, Configuration.UI_MODE_TYPE_NORMAL);
    }

    /**
     * Tests a scenario where we have two apps enter car mode.
     * Ensures that the higher priority app is bound and receives information about the call.
     * When the higher priority app leaves car mode, verifies that the lower priority app is bound
     * and receives information about the call.
     */
    public void testStartCallInCarModeTwoServices() {
        if (!mShouldTestTelecom) {
            return;
        }

        enableAndVerifyCarMode(mCarModeIncallServiceControlOne, 1000);
        enableAndVerifyCarMode(mCarModeIncallServiceControlTwo, 999);

        // Place a call and verify we bound to the Car Mode InCallService
        placeCarModeCall();
        verifyCarModeBound(mCarModeIncallServiceControlOne);
        assertCarModeCallCount(mCarModeIncallServiceControlOne, 1);

        // Now disable car mode in the higher priority service
        disableAndVerifyCarMode(mCarModeIncallServiceControlOne, Configuration.UI_MODE_TYPE_CAR);
        verifyCarModeBound(mCarModeIncallServiceControlTwo);
        assertCarModeCallCount(mCarModeIncallServiceControlTwo, 1);

        // Drop the call from the second service.
        disconnectAllCallsAndVerify(mCarModeIncallServiceControlTwo);
    }

    /**
     * Verifies we can switch from the default dialer to the car-mode InCallService when car mode is
     * enabled.
     */
    public void testSwitchToCarMode() {
        if (!mShouldTestTelecom) {
            return;
        }

        // Place a call and verify it went to the default dialer
        placeAndVerifyCall();
        verifyConnectionForOutgoingCall();

        // Now, enable car mode; should have swapped to the InCallService.
        enableAndVerifyCarMode(mCarModeIncallServiceControlOne, 1000);
        verifyCarModeBound(mCarModeIncallServiceControlOne);
        assertCarModeCallCount(mCarModeIncallServiceControlOne, 1);
        disconnectAllCallsAndVerify(mCarModeIncallServiceControlOne);

        disableAndVerifyCarMode(mCarModeIncallServiceControlOne, Configuration.UI_MODE_TYPE_NORMAL);
    }

    /**
     * Similar to {@link #testSwitchToCarMode}, except exits car mode before the call terminates.
     */
    public void testSwitchToCarModeAndBack() {
        if (!mShouldTestTelecom) {
            return;
        }

        // Place a call and verify it went to the default dialer
        placeAndVerifyCall();
        verifyConnectionForOutgoingCall();

        // Now, enable car mode and confirm we're using the car mode ICS.
        enableAndVerifyCarMode(mCarModeIncallServiceControlOne, 1000);
        verifyCarModeBound(mCarModeIncallServiceControlOne);
        assertCarModeCallCount(mCarModeIncallServiceControlOne, 1);

        // Now, disable car mode and confirm we're no longer using the car mode ICS.
        disableAndVerifyCarMode(mCarModeIncallServiceControlOne, Configuration.UI_MODE_TYPE_NORMAL);
        verifyCarModeUnbound(mCarModeIncallServiceControlOne);

        // Verify that we did bind back to the default dialer.
        try {
            if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                    TimeUnit.SECONDS)) {
                fail("No call added to InCallService.");
            }
        } catch (InterruptedException e) {
            fail("Interupted!");
        }

        assertEquals(1, mInCallCallbacks.getService().getCallCount());
        mInCallCallbacks.getService().disconnectAllCalls();
    }

    /**
     * Similar to {@link #testSwitchToCarMode}, except enters car mode after the call starts.  Also
     * uses multiple car mode InCallServices.
     */
    public void testSwitchToCarModeMultiple() {
        if (!mShouldTestTelecom) {
            return;
        }

        // Place a call and verify it went to the default dialer
        placeAndVerifyCall();
        verifyConnectionForOutgoingCall();

        // Now, enable car mode and confirm we're using the car mode ICS.
        enableAndVerifyCarMode(mCarModeIncallServiceControlOne, 999);
        verifyCarModeBound(mCarModeIncallServiceControlOne);
        assertCarModeCallCount(mCarModeIncallServiceControlOne, 1);

        // Now, enable a higher priority car mode all and confirm we're using it.
        enableAndVerifyCarMode(mCarModeIncallServiceControlTwo, 1000);
        verifyCarModeUnbound(mCarModeIncallServiceControlOne);
        verifyCarModeBound(mCarModeIncallServiceControlTwo);
        assertCarModeCallCount(mCarModeIncallServiceControlTwo, 1);

        // Disable higher priority, verify we drop back to lower priority.
        disableAndVerifyCarMode(mCarModeIncallServiceControlTwo, Configuration.UI_MODE_TYPE_CAR);
        verifyCarModeUnbound(mCarModeIncallServiceControlTwo);
        verifyCarModeBound(mCarModeIncallServiceControlOne);
        assertCarModeCallCount(mCarModeIncallServiceControlOne, 1);

        // Finally, disable car mode at the lower priority and confirm we're using the default
        // dialer once more.
        disableAndVerifyCarMode(mCarModeIncallServiceControlOne, Configuration.UI_MODE_TYPE_NORMAL);
        verifyCarModeUnbound(mCarModeIncallServiceControlOne);

        // Verify that we did bind back to the default dialer.
        try {
            if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                    TimeUnit.SECONDS)) {
                fail("No call added to InCallService.");
            }
        } catch (InterruptedException e) {
            fail("Interupted!");
        }

        assertEquals(1, mInCallCallbacks.getService().getCallCount());
        mInCallCallbacks.getService().disconnectAllCalls();
    }


    /**
     * Places a call without verifying it is handled by the default dialer InCallService.
     */
    private void placeCarModeCall() {
        Bundle extras = new Bundle();
        extras.putParcelable(TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE,
                TestUtils.TEST_PHONE_ACCOUNT_HANDLE);

        Uri number = createTestNumber();
        mTelecomManager.placeCall(number, extras);

        verifyConnectionForOutgoingCall(number);
    }

    /**
     * Verify the car mode ICS has an expected call count.
     * @param expected
     */
    private void assertCarModeCallCount(ICtsCarModeInCallServiceControl control,  int expected) {
        waitUntilConditionIsTrueOrTimeout(
                new Condition() {
                    @Override
                    public Object expected() {
                        return expected;
                    }

                    @Override
                    public Object actual() {
                        int callCount = 0;
                        try {
                            callCount = control.getCallCount();
                        } catch (RemoteException re) {
                            fail("Bee-boop; can't control the incall service");
                        }
                        return callCount;
                    }
                },
                TestUtils.WAIT_FOR_STATE_CHANGE_TIMEOUT_MS,
                "Expected " + expected + " calls."
        );
    }

    /**
     * Verifies that we bound to the car-mode ICS.
     */
    private void verifyCarModeBound(ICtsCarModeInCallServiceControl control) {
        waitUntilConditionIsTrueOrTimeout(
                new Condition() {
                    @Override
                    public Object expected() {
                        return true;
                    }

                    @Override
                    public Object actual() {
                        boolean isBound = false;
                        try {
                            isBound = control.isBound();
                        } catch (RemoteException re) {
                            fail("Bee-boop; can't control the incall service");
                        }
                        return isBound;
                    }
                },
                TestUtils.WAIT_FOR_STATE_CHANGE_TIMEOUT_MS,
                "Expected to be bound"
        );
    }

    /**
     * Verifies that we bound to the car-mode ICS.
     */
    private void verifyCarModeUnbound(ICtsCarModeInCallServiceControl control) {
        waitUntilConditionIsTrueOrTimeout(
                new Condition() {
                    @Override
                    public Object expected() {
                        return true;
                    }

                    @Override
                    public Object actual() {
                        boolean isUnbound = false;
                        try {
                            isUnbound = control.isUnbound();
                        } catch (RemoteException re) {
                            fail("Bee-boop; can't control the incall service");
                        }
                        return isUnbound;
                    }
                },
                TestUtils.WAIT_FOR_STATE_CHANGE_TIMEOUT_MS,
                "Expected to be unbound"
        );
    }

    /**
     * Use the control interface to enable car mode at a specified priority.
     * @param priority the requested priority.
     */
    private void enableAndVerifyCarMode(ICtsCarModeInCallServiceControl control, int priority) {
        try {
            control.enableCarMode(priority);
        } catch (RemoteException re) {
            fail("Bee-boop; can't control the incall service");
        }
        assertUiMode(Configuration.UI_MODE_TYPE_CAR);
    }

    /**
     * Uses the control interface to disable car mode.
     * @param expectedUiMode
     */
    private void disableAndVerifyCarMode(ICtsCarModeInCallServiceControl control,
            int expectedUiMode) {
        try {
            control.disableCarMode();
        } catch (RemoteException re) {
            fail("Bee-boop; can't control the incall service");
        }
        assertUiMode(expectedUiMode);
    }

    private void disconnectAllCallsAndVerify(ICtsCarModeInCallServiceControl controlBinder) {
        try {
            controlBinder.disconnectCalls();
        } catch (RemoteException re) {
            fail("Bee-boop; can't control the incall service");
        }
        assertCarModeCallCount(controlBinder, 0);
    }

    /**
     * Setup a binder which can be used to control the CarModeTestApp.
     * @throws InterruptedException
     */
    private ICtsCarModeInCallServiceControl getControlBinder(String packageName)
            throws InterruptedException {
        Intent bindIntent = new Intent(
                android.telecom.cts.carmodetestapp.CtsCarModeInCallServiceControl
                        .CONTROL_INTERFACE_ACTION);
        bindIntent.setPackage(packageName);
        final LinkedBlockingQueue<ICtsCarModeInCallServiceControl> queue =
                new LinkedBlockingQueue(1);
        boolean success = mContext.bindService(bindIntent, new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                queue.offer(android.telecom.cts.carmodetestapp
                        .ICtsCarModeInCallServiceControl.Stub.asInterface(service));
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                queue.offer(null);
            }
        }, Context.BIND_AUTO_CREATE);
        if (!success) {
            fail("Failed to get control interface -- bind error");
        }
        return queue.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
    }
}
