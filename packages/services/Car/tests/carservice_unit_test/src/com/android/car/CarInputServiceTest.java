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

package com.android.car;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.ignoreStubs;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.annotation.UserIdInt;
import android.app.IActivityManager;
import android.car.CarProjectionManager;
import android.car.input.CarInputHandlingService.InputFilter;
import android.car.input.ICarInputListener;
import android.car.testapi.BlockingUserLifecycleListener;
import android.car.user.CarUserManager;
import android.car.userlib.CarUserManagerHelper;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;
import android.service.voice.VoiceInteractionSession;
import android.telecom.TelecomManager;
import android.test.mock.MockContentResolver;
import android.view.KeyEvent;

import androidx.test.core.app.ApplicationProvider;

import com.android.car.hal.InputHalService;
import com.android.car.hal.UserHalService;
import com.android.car.user.CarUserService;
import com.android.internal.app.AssistUtils;
import com.android.internal.util.test.BroadcastInterceptingContext;
import com.android.internal.util.test.FakeSettingsProvider;

import com.google.common.collect.Range;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.BitSet;
import java.util.function.BooleanSupplier;
import java.util.function.IntSupplier;
import java.util.function.Supplier;

@RunWith(MockitoJUnitRunner.class)
public class CarInputServiceTest {
    // TODO(b/152069895): decrease value once refactored. In fact, it should not even use
    // runWithScissors(), but only rely on CountdownLatches
    private static final long DEFAULT_TIMEOUT_MS = 5_000;

    @Mock InputHalService mInputHalService;
    @Mock TelecomManager mTelecomManager;
    @Mock AssistUtils mAssistUtils;
    @Mock CarInputService.KeyEventListener mDefaultMainListener;
    @Mock Supplier<String> mLastCallSupplier;
    @Mock IntSupplier mLongPressDelaySupplier;
    @Mock BooleanSupplier mShouldCallButtonEndOngoingCallSupplier;

    @Spy Context mContext = ApplicationProvider.getApplicationContext();
    @Spy Handler mHandler = new Handler(Looper.getMainLooper());

    private MockContext mMockContext;
    private CarUserService mCarUserService;
    private CarInputService mCarInputService;

    /**
     * A mock {@link Context}.
     * This class uses a mock {@link ContentResolver} and {@link android.content.ContentProvider} to
     * avoid changing real system settings. Besides, to emulate the case where the OEM changes
     * {@link R.string.rotaryService} to empty in the resource file (e.g., the OEM doesn't want to
     * start RotaryService), this class allows to return a given String when retrieving {@link
     * R.string.rotaryService}.
     */
    private static class MockContext extends BroadcastInterceptingContext {
        private final MockContentResolver mContentResolver;
        private final FakeSettingsProvider mContentProvider;
        private final Resources mResources;

        MockContext(Context base, String rotaryService) {
            super(base);
            FakeSettingsProvider.clearSettingsProvider();
            mContentResolver = new MockContentResolver(this);
            mContentProvider = new FakeSettingsProvider();
            mContentResolver.addProvider(Settings.AUTHORITY, mContentProvider);

            mResources = spy(base.getResources());
            doReturn(rotaryService).when(mResources).getString(R.string.rotaryService);
        }

        void release() {
            FakeSettingsProvider.clearSettingsProvider();
        }

        @Override
        public ContentResolver getContentResolver() {
            return mContentResolver;
        }

        @Override
        public Resources getResources() {
            return mResources;
        }
    }

    @Before
    public void setUp() {
        mCarUserService = mock(CarUserService.class);
        mCarInputService = new CarInputService(mContext, mInputHalService, mCarUserService,
                mHandler, mTelecomManager, mAssistUtils, mDefaultMainListener, mLastCallSupplier,
                /* customInputServiceComponent= */ null, mLongPressDelaySupplier,
                mShouldCallButtonEndOngoingCallSupplier);

        when(mInputHalService.isKeyInputSupported()).thenReturn(true);
        mCarInputService.init();

        // Delay Handler callbacks until flushHandler() is called.
        doReturn(true).when(mHandler).sendMessageAtTime(any(), anyLong());

        when(mShouldCallButtonEndOngoingCallSupplier.getAsBoolean()).thenReturn(false);
    }

    @Test
    public void rotaryServiceSettingsUpdated_whenRotaryServiceIsNotEmpty() throws Exception {
        final String rotaryService = "com.android.car.rotary/com.android.car.rotary.RotaryService";
        init(rotaryService);
        assertThat(mMockContext.getString(R.string.rotaryService)).isEqualTo(rotaryService);

        final int userId = 11;

        // By default RotaryService is not enabled.
        String enabledServices = Settings.Secure.getStringForUser(
                mMockContext.getContentResolver(),
                Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES,
                userId);
        assertThat(enabledServices == null ? "" : enabledServices).doesNotContain(rotaryService);

        String enabled = Settings.Secure.getStringForUser(
                mMockContext.getContentResolver(),
                Settings.Secure.ACCESSIBILITY_ENABLED,
                userId);
        assertThat(enabled).isNull();

        // Enable RotaryService by sending user switch event.
        sendUserLifecycleEvent(CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING, userId);

        enabledServices = Settings.Secure.getStringForUser(
                mMockContext.getContentResolver(),
                Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES,
                userId);
        assertThat(enabledServices).contains(rotaryService);

        enabled = Settings.Secure.getStringForUser(
                mMockContext.getContentResolver(),
                Settings.Secure.ACCESSIBILITY_ENABLED,
                userId);
        assertThat(enabled).isEqualTo("1");
    }

    @Test
    public void rotaryServiceSettingsNotUpdated_whenRotaryServiceIsEmpty() throws Exception {
        final String rotaryService = "";
        init(rotaryService);
        assertThat(mMockContext.getString(R.string.rotaryService)).isEqualTo(rotaryService);

        final int userId = 11;

        // By default the Accessibility is disabled.
        String enabled = Settings.Secure.getStringForUser(
                mMockContext.getContentResolver(),
                Settings.Secure.ACCESSIBILITY_ENABLED,
                userId);
        assertThat(enabled).isNull();

        sendUserLifecycleEvent(CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING, userId);

        // Sending user switch event shouldn't enable the Accessibility because RotaryService is
        // empty.
        enabled = Settings.Secure.getStringForUser(
                mMockContext.getContentResolver(),
                Settings.Secure.ACCESSIBILITY_ENABLED,
                userId);
        assertThat(enabled).isNull();
    }

    @Test
    public void ordinaryEvents_onMainDisplay_routedToInputManager() {
        KeyEvent event = send(Key.DOWN, KeyEvent.KEYCODE_ENTER, Display.MAIN);

        verify(mDefaultMainListener).onKeyEvent(event);
    }

    @Test
    public void ordinaryEvents_onInstrumentClusterDisplay_notRoutedToInputManager() {
        send(Key.DOWN, KeyEvent.KEYCODE_ENTER, Display.INSTRUMENT_CLUSTER);

        verify(mDefaultMainListener, never()).onKeyEvent(any());
    }

    @Test
    public void ordinaryEvents_onInstrumentClusterDisplay_routedToListener() {
        CarInputService.KeyEventListener listener = mock(CarInputService.KeyEventListener.class);
        mCarInputService.setInstrumentClusterKeyListener(listener);

        KeyEvent event = send(Key.DOWN, KeyEvent.KEYCODE_ENTER, Display.INSTRUMENT_CLUSTER);
        verify(listener).onKeyEvent(event);
    }

    @Test
    public void customEventHandler_capturesRegisteredEvents_ignoresUnregisteredEvents()
            throws RemoteException {
        KeyEvent event;
        ICarInputListener listener = registerInputListener(
                new InputFilter(KeyEvent.KEYCODE_ENTER, InputHalService.DISPLAY_MAIN),
                new InputFilter(KeyEvent.KEYCODE_ENTER, InputHalService.DISPLAY_INSTRUMENT_CLUSTER),
                new InputFilter(KeyEvent.KEYCODE_MENU, InputHalService.DISPLAY_MAIN));

        CarInputService.KeyEventListener instrumentClusterListener =
                mock(CarInputService.KeyEventListener.class);
        mCarInputService.setInstrumentClusterKeyListener(instrumentClusterListener);

        event = send(Key.DOWN, KeyEvent.KEYCODE_ENTER, Display.MAIN);
        verify(listener).onKeyEvent(event, InputHalService.DISPLAY_MAIN);
        verify(mDefaultMainListener, never()).onKeyEvent(any());

        event = send(Key.DOWN, KeyEvent.KEYCODE_ENTER, Display.INSTRUMENT_CLUSTER);
        verify(listener).onKeyEvent(event, InputHalService.DISPLAY_INSTRUMENT_CLUSTER);
        verify(instrumentClusterListener, never()).onKeyEvent(any());

        event = send(Key.DOWN, KeyEvent.KEYCODE_MENU, Display.MAIN);
        verify(listener).onKeyEvent(event, InputHalService.DISPLAY_MAIN);
        verify(mDefaultMainListener, never()).onKeyEvent(any());

        event = send(Key.DOWN, KeyEvent.KEYCODE_MENU, Display.INSTRUMENT_CLUSTER);
        verify(listener, never()).onKeyEvent(event, InputHalService.DISPLAY_INSTRUMENT_CLUSTER);
        verify(instrumentClusterListener).onKeyEvent(event);
    }

    @Test
    public void voiceKey_shortPress_withRegisteredEventHandler_triggersEventHandler() {
        CarProjectionManager.ProjectionKeyEventHandler eventHandler =
                registerProjectionKeyEventHandler(
                        CarProjectionManager.KEY_EVENT_VOICE_SEARCH_SHORT_PRESS_KEY_UP);

        send(Key.DOWN, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);
        send(Key.UP, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);

        verify(eventHandler)
                .onKeyEvent(CarProjectionManager.KEY_EVENT_VOICE_SEARCH_SHORT_PRESS_KEY_UP);
    }

    @Test
    public void voiceKey_longPress_withRegisteredEventHandler_triggersEventHandler() {
        CarProjectionManager.ProjectionKeyEventHandler eventHandler =
                registerProjectionKeyEventHandler(
                        CarProjectionManager.KEY_EVENT_VOICE_SEARCH_SHORT_PRESS_KEY_UP,
                        CarProjectionManager.KEY_EVENT_VOICE_SEARCH_LONG_PRESS_KEY_DOWN);

        send(Key.DOWN, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);
        verify(eventHandler, never()).onKeyEvent(anyInt());

        // Simulate the long-press timer expiring.
        flushHandler();
        verify(eventHandler)
                .onKeyEvent(CarProjectionManager.KEY_EVENT_VOICE_SEARCH_LONG_PRESS_KEY_DOWN);

        // Ensure that the short-press handler is *not* called.
        send(Key.UP, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);
        flushHandler();
        verify(eventHandler, never())
                .onKeyEvent(CarProjectionManager.KEY_EVENT_VOICE_SEARCH_SHORT_PRESS_KEY_UP);
    }

    @Test
    public void voiceKey_shortPress_withoutRegisteredEventHandler_triggersAssistUtils() {
        when(mAssistUtils.getAssistComponentForUser(anyInt()))
                .thenReturn(new ComponentName("pkg", "cls"));

        send(Key.DOWN, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);
        send(Key.UP, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);

        ArgumentCaptor<Bundle> bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mAssistUtils).showSessionForActiveService(
                bundleCaptor.capture(),
                eq(VoiceInteractionSession.SHOW_SOURCE_PUSH_TO_TALK),
                any(),
                isNull());
        assertThat(bundleCaptor.getValue().getBoolean(CarInputService.EXTRA_CAR_PUSH_TO_TALK))
                .isTrue();
    }

    @Test
    public void voiceKey_longPress_withoutRegisteredEventHandler_triggersAssistUtils() {
        when(mAssistUtils.getAssistComponentForUser(anyInt()))
                .thenReturn(new ComponentName("pkg", "cls"));

        send(Key.DOWN, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);
        flushHandler();

        ArgumentCaptor<Bundle> bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mAssistUtils).showSessionForActiveService(
                bundleCaptor.capture(),
                eq(VoiceInteractionSession.SHOW_SOURCE_PUSH_TO_TALK),
                any(),
                isNull());
        assertThat(bundleCaptor.getValue().getBoolean(CarInputService.EXTRA_CAR_PUSH_TO_TALK))
                .isTrue();

        send(Key.UP, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);
        verifyNoMoreInteractions(ignoreStubs(mAssistUtils));
    }

    @Test
    public void voiceKey_keyDown_withEventHandler_triggersEventHandler() {
        CarProjectionManager.ProjectionKeyEventHandler eventHandler =
                registerProjectionKeyEventHandler(
                        CarProjectionManager.KEY_EVENT_VOICE_SEARCH_KEY_DOWN);

        send(Key.DOWN, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);

        verify(eventHandler).onKeyEvent(CarProjectionManager.KEY_EVENT_VOICE_SEARCH_KEY_DOWN);
    }

    @Test
    public void voiceKey_keyUp_afterLongPress_withEventHandler_triggersEventHandler() {
        CarProjectionManager.ProjectionKeyEventHandler eventHandler =
                registerProjectionKeyEventHandler(
                        CarProjectionManager.KEY_EVENT_VOICE_SEARCH_LONG_PRESS_KEY_UP);

        send(Key.DOWN, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);
        flushHandler();
        verify(eventHandler, never())
                .onKeyEvent(CarProjectionManager.KEY_EVENT_VOICE_SEARCH_LONG_PRESS_KEY_UP);

        send(Key.UP, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);
        verify(eventHandler)
                .onKeyEvent(CarProjectionManager.KEY_EVENT_VOICE_SEARCH_LONG_PRESS_KEY_UP);
    }

    @Test
    public void voiceKey_repeatedEvents_ignored() {
        // Pressing a key starts the long-press timer.
        send(Key.DOWN, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN);
        verify(mHandler).sendMessageAtTime(any(), anyLong());
        clearInvocations(mHandler);

        // Repeated KEY_DOWN events don't reset the timer.
        sendWithRepeat(Key.DOWN, KeyEvent.KEYCODE_VOICE_ASSIST, Display.MAIN, 1);
        verify(mHandler, never()).sendMessageAtTime(any(), anyLong());
    }

    @Test
    public void callKey_shortPress_withoutEventHandler_launchesDialer() {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        doNothing().when(mContext).startActivityAsUser(any(), any(), any());

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        send(Key.UP, KeyEvent.KEYCODE_CALL, Display.MAIN);

        verify(mContext).startActivityAsUser(
                intentCaptor.capture(), any(), eq(UserHandle.CURRENT_OR_SELF));
        assertThat(intentCaptor.getValue().getAction()).isEqualTo(Intent.ACTION_DIAL);
    }

    @Test
    public void callKey_shortPress_withoutEventHandler_whenCallRinging_answersCall() {
        when(mTelecomManager.isRinging()).thenReturn(true);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        send(Key.UP, KeyEvent.KEYCODE_CALL, Display.MAIN);

        verify(mTelecomManager).acceptRingingCall();
        // Ensure default handler does not run.
        verify(mContext, never()).startActivityAsUser(any(), any(), any());
    }

    @Test
    public void callKey_shortPress_withEventHandler_triggersEventHandler() {
        CarProjectionManager.ProjectionKeyEventHandler eventHandler =
                registerProjectionKeyEventHandler(
                        CarProjectionManager.KEY_EVENT_CALL_SHORT_PRESS_KEY_UP);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        send(Key.UP, KeyEvent.KEYCODE_CALL, Display.MAIN);

        verify(eventHandler).onKeyEvent(CarProjectionManager.KEY_EVENT_CALL_SHORT_PRESS_KEY_UP);
        // Ensure default handlers do not run.
        verify(mTelecomManager, never()).acceptRingingCall();
        verify(mContext, never()).startActivityAsUser(any(), any(), any());
    }

    @Test
    public void callKey_shortPress_withEventHandler_whenCallRinging_answersCall() {
        CarProjectionManager.ProjectionKeyEventHandler eventHandler =
                registerProjectionKeyEventHandler(
                        CarProjectionManager.KEY_EVENT_CALL_SHORT_PRESS_KEY_UP);
        when(mTelecomManager.isRinging()).thenReturn(true);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        send(Key.UP, KeyEvent.KEYCODE_CALL, Display.MAIN);

        verify(mTelecomManager).acceptRingingCall();
        verify(eventHandler, never()).onKeyEvent(anyInt());
    }

    @Test
    public void callKey_shortPress_duringCall_endCallViaCallButtonOn_endsCall() {
        when(mShouldCallButtonEndOngoingCallSupplier.getAsBoolean()).thenReturn(true);
        when(mTelecomManager.isInCall()).thenReturn(true);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        send(Key.UP, KeyEvent.KEYCODE_CALL, Display.MAIN);

        verify(mTelecomManager).endCall();
    }

    @Test
    public void callKey_shortPress_duringCall_endCallViaCallButtonOff_doesNotEndCall() {
        when(mTelecomManager.isInCall()).thenReturn(true);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        send(Key.UP, KeyEvent.KEYCODE_CALL, Display.MAIN);

        verify(mTelecomManager, never()).endCall();
    }

    @Test
    public void callKey_longPress_withoutEventHandler_redialsLastCall() {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        when(mLastCallSupplier.get()).thenReturn("1234567890");
        doNothing().when(mContext).startActivityAsUser(any(), any(), any());

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        flushHandler();

        verify(mContext).startActivityAsUser(
                intentCaptor.capture(), any(), eq(UserHandle.CURRENT_OR_SELF));

        Intent intent = intentCaptor.getValue();
        assertThat(intent.getAction()).isEqualTo(Intent.ACTION_CALL);
        assertThat(intent.getData()).isEqualTo(Uri.parse("tel:1234567890"));

        clearInvocations(mContext);
        send(Key.UP, KeyEvent.KEYCODE_CALL, Display.MAIN);
        verify(mContext, never()).startActivityAsUser(any(), any(), any());
    }

    @Test
    public void callKey_longPress_withoutEventHandler_withNoLastCall_doesNothing() {
        when(mLastCallSupplier.get()).thenReturn("");

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        flushHandler();

        verify(mContext, never()).startActivityAsUser(any(), any(), any());
    }

    @Test
    public void callKey_longPress_withoutEventHandler_whenCallRinging_answersCall() {
        when(mTelecomManager.isRinging()).thenReturn(true);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        flushHandler();

        verify(mTelecomManager).acceptRingingCall();

        send(Key.UP, KeyEvent.KEYCODE_CALL, Display.MAIN);
        // Ensure that default handler does not run, either after accepting ringing call,
        // or as a result of key-up.
        verify(mContext, never()).startActivityAsUser(any(), any(), any());
    }

    @Test
    public void callKey_longPress_withEventHandler_triggersEventHandler() {
        CarProjectionManager.ProjectionKeyEventHandler eventHandler =
                registerProjectionKeyEventHandler(
                        CarProjectionManager.KEY_EVENT_CALL_LONG_PRESS_KEY_DOWN);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        flushHandler();

        verify(eventHandler).onKeyEvent(CarProjectionManager.KEY_EVENT_CALL_LONG_PRESS_KEY_DOWN);
        verify(mContext, never()).startActivityAsUser(any(), any(), any());
    }

    @Test
    public void callKey_longPress_withEventHandler_whenCallRinging_answersCall() {
        CarProjectionManager.ProjectionKeyEventHandler eventHandler =
                registerProjectionKeyEventHandler(
                        CarProjectionManager.KEY_EVENT_CALL_LONG_PRESS_KEY_DOWN);
        when(mTelecomManager.isRinging()).thenReturn(true);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        flushHandler();

        verify(mTelecomManager).acceptRingingCall();

        send(Key.UP, KeyEvent.KEYCODE_CALL, Display.MAIN);
        // Ensure that event handler does not run, either after accepting ringing call,
        // or as a result of key-up.
        verify(eventHandler, never()).onKeyEvent(anyInt());
    }

    @Test
    public void callKey_longPress_duringCall_endCallViaCallButtonOn_endsCall() {
        when(mShouldCallButtonEndOngoingCallSupplier.getAsBoolean()).thenReturn(true);
        when(mTelecomManager.isInCall()).thenReturn(true);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        flushHandler();

        verify(mTelecomManager).endCall();
    }

    @Test
    public void callKey_longPress_duringCall_endCallViaCallButtonOff_doesNotEndCall() {
        when(mTelecomManager.isInCall()).thenReturn(true);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        flushHandler();

        verify(mTelecomManager, never()).endCall();
    }

    @Test
    public void callKey_keyDown_withEventHandler_triggersEventHandler() {
        CarProjectionManager.ProjectionKeyEventHandler eventHandler =
                registerProjectionKeyEventHandler(
                        CarProjectionManager.KEY_EVENT_CALL_KEY_DOWN);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);

        verify(eventHandler).onKeyEvent(CarProjectionManager.KEY_EVENT_CALL_KEY_DOWN);
    }

    @Test
    public void callKey_keyUp_afterLongPress_withEventHandler_triggersEventHandler() {
        CarProjectionManager.ProjectionKeyEventHandler eventHandler =
                registerProjectionKeyEventHandler(
                        CarProjectionManager.KEY_EVENT_CALL_LONG_PRESS_KEY_UP);

        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        flushHandler();
        verify(eventHandler, never())
                .onKeyEvent(CarProjectionManager.KEY_EVENT_CALL_LONG_PRESS_KEY_UP);

        send(Key.UP, KeyEvent.KEYCODE_CALL, Display.MAIN);
        verify(eventHandler).onKeyEvent(CarProjectionManager.KEY_EVENT_CALL_LONG_PRESS_KEY_UP);
    }

    @Test
    public void callKey_repeatedEvents_ignored() {
        // Pressing a key starts the long-press timer.
        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        verify(mHandler).sendMessageAtTime(any(), anyLong());
        clearInvocations(mHandler);

        // Repeated KEY_DOWN events don't reset the timer.
        sendWithRepeat(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN, 1);
        verify(mHandler, never()).sendMessageAtTime(any(), anyLong());
    }

    @Test
    public void longPressDelay_obeysValueFromSystem() {
        final int systemDelay = 4242;

        when(mLongPressDelaySupplier.getAsInt()).thenReturn(systemDelay);
        ArgumentCaptor<Long> timeCaptor = ArgumentCaptor.forClass(long.class);

        long then = SystemClock.uptimeMillis();
        send(Key.DOWN, KeyEvent.KEYCODE_CALL, Display.MAIN);
        long now = SystemClock.uptimeMillis();

        verify(mHandler).sendMessageAtTime(any(), timeCaptor.capture());

        // The message time must be the expected delay time (as provided by the supplier) after
        // the time the message was sent to the handler. We don't know that exact time, but we
        // can put a bound on it - it's no sooner than immediately before the call to send(), and no
        // later than immediately afterwards. Check to make sure the actual observed message time is
        // somewhere in the valid range.

        assertThat(timeCaptor.getValue()).isIn(Range.closed(then + systemDelay, now + systemDelay));
    }

    @After
    public void tearDown() {
        if (mMockContext != null) {
            mMockContext.release();
            mMockContext = null;
        }
    }

    /**
     * Initializes {@link #mMockContext}, {@link #mCarUserService}, and {@link #mCarInputService}.
     */
    private void init(String rotaryService) {
        mMockContext = new MockContext(mContext, rotaryService);

        UserManager userManager = mock(UserManager.class);
        UserInfo userInfo = mock(UserInfo.class);
        doReturn(userInfo).when(userManager).getUserInfo(anyInt());
        UserHalService userHal = mock(UserHalService.class);
        CarUserManagerHelper carUserManagerHelper = mock(CarUserManagerHelper.class);
        IActivityManager iActivityManager = mock(IActivityManager.class);
        mCarUserService = new CarUserService(mMockContext, userHal, carUserManagerHelper,
                userManager, iActivityManager, /* maxRunningUsers= */ 2);

        mCarInputService = new CarInputService(mMockContext, mInputHalService, mCarUserService,
                mHandler, mTelecomManager, mAssistUtils, mDefaultMainListener, mLastCallSupplier,
                /* customInputServiceComponent= */ null, mLongPressDelaySupplier,
                mShouldCallButtonEndOngoingCallSupplier);
        mCarInputService.init();
    }

    private void sendUserLifecycleEvent(@CarUserManager.UserLifecycleEventType int eventType,
            @UserIdInt int userId) throws InterruptedException {
        // Add a blocking listener to ensure CarUserService event notification is completed
        // before proceeding with test execution.
        BlockingUserLifecycleListener blockingListener =
                BlockingUserLifecycleListener.forAnyEvent().build();
        mCarUserService.addUserLifecycleListener(blockingListener);

        runOnMainThreadAndWaitForIdle(() -> mCarUserService.onUserLifecycleEvent(eventType,
                /* timestampMs= */ 0, /* fromUserId= */ UserHandle.USER_NULL, userId));
        blockingListener.waitForAnyEvent();
    }

    private static void runOnMainThreadAndWaitForIdle(Runnable r) {
        Handler.getMain().runWithScissors(r, DEFAULT_TIMEOUT_MS);
        // Run empty runnable to make sure that all posted handlers are done.
        Handler.getMain().runWithScissors(() -> { }, DEFAULT_TIMEOUT_MS);
    }

    private enum Key {DOWN, UP}

    private enum Display {MAIN, INSTRUMENT_CLUSTER}

    private KeyEvent send(Key action, int keyCode, Display display) {
        return sendWithRepeat(action, keyCode, display, 0);
    }

    private KeyEvent sendWithRepeat(Key action, int keyCode, Display display, int repeatCount) {
        KeyEvent event = new KeyEvent(
                /* downTime= */ 0L,
                /* eventTime= */ 0L,
                action == Key.DOWN ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP,
                keyCode,
                repeatCount);
        mCarInputService.onKeyEvent(
                event,
                display == Display.MAIN
                        ? InputHalService.DISPLAY_MAIN
                        : InputHalService.DISPLAY_INSTRUMENT_CLUSTER);
        return event;
    }

    private ICarInputListener registerInputListener(InputFilter... handledKeys) {
        ICarInputListener listener = mock(ICarInputListener.class);
        mCarInputService.mCarInputListener = listener;
        mCarInputService.setHandledKeys(handledKeys);
        return listener;
    }

    private CarProjectionManager.ProjectionKeyEventHandler registerProjectionKeyEventHandler(
            int... events) {
        BitSet eventSet = new BitSet();
        for (int event : events) {
            eventSet.set(event);
        }

        CarProjectionManager.ProjectionKeyEventHandler projectionKeyEventHandler =
                mock(CarProjectionManager.ProjectionKeyEventHandler.class);
        mCarInputService.setProjectionKeyEventHandler(projectionKeyEventHandler, eventSet);
        return projectionKeyEventHandler;
    }

    private void flushHandler() {
        ArgumentCaptor<Message> messageCaptor = ArgumentCaptor.forClass(Message.class);

        verify(mHandler, atLeast(0)).sendMessageAtTime(messageCaptor.capture(), anyLong());

        for (Message message : messageCaptor.getAllValues()) {
            mHandler.dispatchMessage(message);
        }

        clearInvocations(mHandler);
    }
}
