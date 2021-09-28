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

package android.car;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Application;
import android.car.CarAppFocusManager.OnAppFocusChangedListener;
import android.car.CarAppFocusManager.OnAppFocusOwnershipCallback;
import android.car.testapi.CarAppFocusController;
import android.car.testapi.FakeCar;
import android.os.Looper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.internal.DoNotInstrument;
import org.robolectric.shadows.ShadowBinder;

@RunWith(RobolectricTestRunner.class)
@DoNotInstrument
public class CarAppFocusManagerTest {
    @Rule
    public MockitoRule rule = MockitoJUnit.rule();

    private Application mContext;

    private FakeCar mFakeCar;
    private CarAppFocusManager mCarAppFocusManager;
    private CarAppFocusController mCarAppFocusController;
    private Looper mAppFocusServiceLooper;

    private static final int APP1_UID = 1041;
    private static final int APP1_PID = 1043;
    private static final int APP2_UID = 1072;
    private static final int APP2_PID = 1074;
    private static final int APP3_UID = 1111;
    private static final int APP3_PID = 2222;

    @Mock OnAppFocusOwnershipCallback mApp1Callback;
    @Mock OnAppFocusChangedListener mApp1Listener;
    @Mock OnAppFocusOwnershipCallback mApp2Callback;
    @Mock OnAppFocusChangedListener mApp2Listener;
    @Mock OnAppFocusOwnershipCallback mApp3Callback;
    @Mock OnAppFocusChangedListener mApp3Listener;

    @Before
    public void setUp() {
        ShadowBinder.reset();
        mContext = ApplicationProvider.getApplicationContext();
        mFakeCar = FakeCar.createFakeCar(mContext);
        mCarAppFocusManager =
                (CarAppFocusManager) mFakeCar.getCar().getCarManager(Car.APP_FOCUS_SERVICE);
        mCarAppFocusController = mFakeCar.getAppFocusController();
        mAppFocusServiceLooper = mCarAppFocusController.getLooper();
    }

    @Test
    public void defaultState_noFocusesHeld() {
        assertThat(mCarAppFocusManager.getActiveAppTypes()).isEmpty();
    }

    @Test
    public void requestNavFocus_noCurrentFocus_requestShouldSucceed() {
        int result = mCarAppFocusManager.requestAppFocus(
                CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION, mApp1Callback);
        assertThat(result).isEqualTo(CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED);
    }

    @Test
    public void requestNavFocus_noCurrentFocus_callbackIsRun() {
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp1Callback);
        shadowOf(mAppFocusServiceLooper).runToEndOfTasks();

        verify(mApp1Callback)
                .onAppFocusOwnershipGranted(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
    }

    @Test
    public void requestNavFocus_noCurrentFocus_holdsOwnership() {
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp1Callback);

        assertThat(
                mCarAppFocusManager
                        .isOwningFocus(mApp1Callback, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION))
                .isTrue();
    }

    @Test
    public void requestNavFocus_noCurrentFocus_onlyNavActive() {
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp1Callback);

        assertThat(mCarAppFocusManager.getActiveAppTypes())
                .isEqualTo(new int[] {CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION});
    }

    private void setCallingApp(int uid, int pid) {
        ShadowBinder.setCallingUid(uid);
        ShadowBinder.setCallingPid(pid);
    }

    private void app2GainsFocus_app1BroughtToForeground() {
        setCallingApp(APP2_UID, APP2_PID);
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp2Callback);
        mCarAppFocusController.setForegroundUid(APP1_UID);
        mCarAppFocusController.setForegroundPid(APP1_PID);
        setCallingApp(APP2_UID, APP1_PID);
    }

    @Test
    public void requestNavFocus_currentOwnerInBackground_requestShouldSucceed() {
        app2GainsFocus_app1BroughtToForeground();

        assertThat(
                mCarAppFocusManager
                        .requestAppFocus(
                                CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION, mApp1Callback))
                .isEqualTo(CarAppFocusManager.APP_FOCUS_REQUEST_SUCCEEDED);
    }

    @Test
    public void requestNavFocus_currentOwnerInBackground_callbackIsRun() {
        app2GainsFocus_app1BroughtToForeground();
        mCarAppFocusManager
                .requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION, mApp1Callback);
        shadowOf(mAppFocusServiceLooper).runToEndOfTasks();

        verify(mApp1Callback)
                .onAppFocusOwnershipGranted(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
    }

    @Test
    public void requestNavFocus_currentOwnerInBackground_holdsOwnership() {
        app2GainsFocus_app1BroughtToForeground();
        mCarAppFocusManager
                .requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION, mApp1Callback);

        assertThat(
                mCarAppFocusManager
                        .isOwningFocus(mApp1Callback, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION))
                .isTrue();
    }

    @Test
    public void requestNavFocus_currentOwnerInForeground_requestFails() {
        setCallingApp(APP2_UID, APP2_PID);
        mCarAppFocusController.setForegroundUid(APP2_UID);
        mCarAppFocusController.setForegroundPid(APP2_PID);
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp2Callback);
        setCallingApp(APP1_UID, APP1_PID);

        assertThat(
                mCarAppFocusManager
                        .requestAppFocus(
                                CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION, mApp1Callback))
                .isEqualTo(CarAppFocusManager.APP_FOCUS_REQUEST_FAILED);
    }

    @Test
    public void requestAppFocus_callingAppNotified() {
        setCallingApp(APP1_UID, APP1_PID);
        mCarAppFocusManager
                .addFocusListener(mApp1Listener, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp1Callback);
        shadowOf(mAppFocusServiceLooper).runToEndOfTasks();

        verify(mApp1Listener)
                .onAppFocusChanged(eq(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION), anyBoolean());
    }

    @Test
    public void requestAppFocus_otherAppNotified() {
        setCallingApp(APP2_UID, APP2_PID);
        mCarAppFocusManager
                .addFocusListener(mApp2Listener, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        setCallingApp(APP1_UID, APP1_PID);
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp1Callback);
        shadowOf(mAppFocusServiceLooper).runToEndOfTasks();

        verify(mApp2Listener)
                .onAppFocusChanged(eq(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION), eq(true));
    }

    @Test
    public void requestAppFocus_focusLost_otherAppRequest_callbackRun() {
        setCallingApp(APP2_UID, APP2_PID);
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp2Callback);
        setCallingApp(APP1_UID, APP1_PID);
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp1Callback);
        shadowOf(mAppFocusServiceLooper).runToEndOfTasks();

        verify(mApp2Callback)
                .onAppFocusOwnershipLost(eq(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION));
    }

    @Test
    public void abandonAppFocus_callingAppNotified() {
        setCallingApp(APP1_UID, APP1_PID);
        mCarAppFocusManager
                .addFocusListener(mApp1Listener, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp1Callback);
        mCarAppFocusManager
                .abandonAppFocus(mApp1Callback, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        shadowOf(mAppFocusServiceLooper).runToEndOfTasks();

        verify(mApp1Listener)
                .onAppFocusChanged(eq(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION), eq(false));
    }

    @Test
    public void abandonAppFocus_otherAppNotified() {
        setCallingApp(APP2_UID, APP2_PID);
        mCarAppFocusManager
                .addFocusListener(mApp2Listener, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        setCallingApp(APP1_UID, APP1_PID);
        mCarAppFocusManager.requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION,
                mApp1Callback);
        mCarAppFocusManager
                .abandonAppFocus(mApp1Callback, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        shadowOf(mAppFocusServiceLooper).runToEndOfTasks();

        verify(mApp2Listener)
                .onAppFocusChanged(eq(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION), eq(false));
    }

    @Test
    public void gainAppFocus_multipleListenersRegistered_bothUnownedTrigger() {
        setCallingApp(APP1_UID, APP1_PID);
        mCarAppFocusManager
                .addFocusListener(mApp1Listener, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        setCallingApp(APP2_UID, APP2_PID);
        mCarAppFocusManager
                .addFocusListener(mApp2Listener, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        setCallingApp(APP3_UID, APP3_PID);
        mCarAppFocusManager
                .addFocusListener(mApp3Listener, CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION);
        mCarAppFocusManager
                .requestAppFocus(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION, mApp3Callback);
        shadowOf(mAppFocusServiceLooper).runToEndOfTasks();

        verify(mApp1Listener)
                .onAppFocusChanged(eq(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION), eq(true));
        verify(mApp2Listener)
                .onAppFocusChanged(eq(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION), eq(true));
        verify(mApp3Listener)
                .onAppFocusChanged(eq(CarAppFocusManager.APP_FOCUS_TYPE_NAVIGATION), eq(true));
    }
}
