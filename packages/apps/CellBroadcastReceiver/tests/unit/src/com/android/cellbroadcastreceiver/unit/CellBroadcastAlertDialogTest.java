/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.cellbroadcastreceiver.unit;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.IPowerManager;
import android.os.IThermalService;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;
import android.preference.PreferenceManager;
import android.telephony.SmsCbMessage;
import android.view.KeyEvent;
import android.view.WindowManager;
import android.widget.TextView;

import com.android.cellbroadcastreceiver.CellBroadcastAlertDialog;
import com.android.cellbroadcastreceiver.CellBroadcastAlertService;
import com.android.cellbroadcastreceiver.CellBroadcastSettings;
import com.android.internal.telephony.gsm.SmsCbConstants;

import org.junit.After;
import org.junit.Before;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;

public class CellBroadcastAlertDialogTest extends
        CellBroadcastActivityTestCase<CellBroadcastAlertDialog> {

    @Mock
    private NotificationManager mMockedNotificationManager;

    @Mock
    private IPowerManager.Stub mMockedPowerManagerService;

    @Mock
    private IThermalService.Stub mMockedThermalService;

    @Captor
    private ArgumentCaptor<Integer> mInt;

    @Captor
    private ArgumentCaptor<Notification> mNotification;

    private PowerManager mPowerManager;
    private int mSubId = 0;

    public CellBroadcastAlertDialogTest() {
        super(CellBroadcastAlertDialog.class);
    }

    private int mServiceCategory = SmsCbConstants.MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL;
    private int mCmasMessageClass = 0;

    private ArrayList<SmsCbMessage> mMessageList;

    @Override
    protected Intent createActivityIntent() {
        mMessageList = new ArrayList<>(1);
        mMessageList.add(CellBroadcastAlertServiceTest.createMessageForCmasMessageClass(12412,
                mServiceCategory,
                mCmasMessageClass));

        Intent intent = new Intent(getInstrumentation().getTargetContext(),
                CellBroadcastAlertDialog.class);
        intent.putParcelableArrayListExtra(CellBroadcastAlertService.SMS_CB_MESSAGE_EXTRA,
                mMessageList);
        return intent;
    }

    @Before
    public void setUp() throws Exception {
        super.setUp();
        MockitoAnnotations.initMocks(this);
        injectSystemService(NotificationManager.class, mMockedNotificationManager);
        // PowerManager is a final class so we can't use Mockito to mock it, but we can mock
        // its underlying service.
        doReturn(true).when(mMockedPowerManagerService).isInteractive();
        mPowerManager = new PowerManager(mContext, mMockedPowerManagerService,
                mMockedThermalService, null);
        injectSystemService(PowerManager.class, mPowerManager);
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    public void testTitleAndMessageText() throws Throwable {
        startActivity();
        waitForMs(100);

        CharSequence alertString =
                getActivity().getResources().getText(com.android.cellbroadcastreceiver.R.string
                        .cmas_presidential_level_alert);
        assertTrue(getActivity().getTitle().toString().startsWith(alertString.toString()));
        assertTrue(((TextView) getActivity().findViewById(
                com.android.cellbroadcastreceiver.R.id.alertTitle)).getText().toString()
                .startsWith(alertString.toString()));

        waitUntilAssertPasses(()-> {
            String body = CellBroadcastAlertServiceTest.createMessage(34596).getMessageBody();
            assertEquals(body, ((TextView) getActivity().findViewById(
                            com.android.cellbroadcastreceiver.R.id.message)).getText().toString());
        }, 1000);

        stopActivity();
    }

    public void waitUntilAssertPasses(Runnable r, long maxWaitMs) {
        long waitTime = 0;
        while (waitTime < maxWaitMs) {
            try {
                r.run();
                // if the assert succeeds, return
                return;
            } catch (Exception e) {
                waitTime += 100;
                waitForMs(100);
            }
        }
        // if timed out, run one last time without catching exception
        r.run();
    }

    public void testAddToNotification() throws Throwable {
        startActivity();
        waitForMs(100);
        stopActivity();
        waitForMs(100);
        verify(mMockedNotificationManager, times(1)).notify(mInt.capture(),
                mNotification.capture());
        Bundle b = mNotification.getValue().extras;

        assertEquals(1, (int) mInt.getValue());

        assertTrue(getActivity().getTitle().toString().startsWith(
                b.getCharSequence(Notification.EXTRA_TITLE).toString()));
        assertEquals(CellBroadcastAlertServiceTest.createMessage(98235).getMessageBody(),
                b.getCharSequence(Notification.EXTRA_TEXT));
    }

    @InstrumentationTest
    // This test has a module dependency (it uses the CellBroadcastContentProvider), so it is
    // disabled for OEM testing because it is not a true unit test
    public void testDismiss() throws Throwable {
        CellBroadcastAlertDialog activity = startActivity();
        waitForMs(100);
        activity.dismiss();

        verify(mMockedNotificationManager, times(1)).cancel(
                eq(CellBroadcastAlertService.NOTIFICATION_ID));
    }

    @InstrumentationTest
    // This test has a module dependency (it uses the CellBroadcastContentProvider), so it is
    // disabled for OEM testing because it is not a true unit test
    public void testDismissWithDialog() throws Throwable {
        // in order to trigger mShowOptOutDialog=true, the message should not be a presidential
        // alert (the default message we send in this test)
        mServiceCategory = SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY;
        mCmasMessageClass = SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY;

        // prepare the looper so we can create opt out dialog
        Looper.prepare();

        // enable opt out dialog in shared prefs
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(mContext);
        prefs.edit().putBoolean(CellBroadcastSettings.KEY_SHOW_CMAS_OPT_OUT_DIALOG, true).apply();

        boolean triedToCreateDialog = false;
        try {
            CellBroadcastAlertDialog activity = startActivity();
            waitForMs(100);
            activity.dismiss();
        } catch (WindowManager.BadTokenException e) {
            triedToCreateDialog = true;
        }

        assertTrue(triedToCreateDialog);
    }

    public void testOnNewIntent() throws Throwable {
        Intent intent = createActivityIntent();
        intent.putExtra(CellBroadcastAlertDialog.FROM_NOTIFICATION_EXTRA, true);

        Looper.prepare();
        CellBroadcastAlertDialog activity = startActivity(intent, null, null);
        waitForMs(100);

        // add more messages to list
        mMessageList.add(CellBroadcastAlertServiceTest.createMessageForCmasMessageClass(12413,
                SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY,
                SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY));
        activity.onNewIntent(intent);

        verify(mMockedNotificationManager, atLeastOnce()).cancel(
                eq(CellBroadcastAlertService.NOTIFICATION_ID));
    }

    public void testCopyMessageToClipboard() {
        Context mockContext = mock(Context.class);
        ClipboardManager mockClipboardManager = mock(ClipboardManager.class);
        Resources mockResources = mock(Resources.class);
        doReturn(mockClipboardManager).when(mockContext).getSystemService(
                eq(Context.CLIPBOARD_SERVICE));
        doReturn(mockResources).when(mockContext).getResources();
        CellBroadcastSettings.setUseResourcesForSubId(false);


        SmsCbMessage testMessage =
                CellBroadcastAlertServiceTest.createMessageForCmasMessageClass(12412,
                        mServiceCategory,
                        mCmasMessageClass);

        boolean madeToast = false;
        try {
            CellBroadcastAlertDialog.copyMessageToClipboard(testMessage, mockContext);
        } catch (NullPointerException e) {
            // exception expected from creating the toast at the end of copyMessageToClipboard
            madeToast = true;
        }

        assertTrue(madeToast);
        verify(mockContext).getSystemService(eq(Context.CLIPBOARD_SERVICE));
        verify(mockClipboardManager).setPrimaryClip(any());
    }

    public void testAnimationHandler() throws Throwable {
        CellBroadcastAlertDialog activity = startActivity();
        CellBroadcastSettings.setUseResourcesForSubId(false);

        activity.mAnimationHandler.startIconAnimation(mSubId);

        assertTrue(activity.mAnimationHandler.mWarningIconVisible);

        Message m = Message.obtain();
        m.what = activity.mAnimationHandler.mCount.get();
        activity.mAnimationHandler.handleMessage(m);

        // assert that message count has gone up
        assertEquals(m.what + 1, activity.mAnimationHandler.mCount.get());
    }

    public void testOnResume() throws Throwable {
        Intent intent = createActivityIntent();
        intent.putExtra(CellBroadcastAlertDialog.FROM_NOTIFICATION_EXTRA, true);

        Looper.prepare();
        CellBroadcastAlertDialog activity = startActivity(intent, null, null);

        CellBroadcastAlertDialog.AnimationHandler mockAnimationHandler = mock(
                CellBroadcastAlertDialog.AnimationHandler.class);
        activity.mAnimationHandler = mockAnimationHandler;

        activity.onResume();
        verify(mockAnimationHandler).startIconAnimation(anyInt());
    }

    public void testOnPause() throws Throwable {
        Intent intent = createActivityIntent();
        intent.putExtra(CellBroadcastAlertDialog.FROM_NOTIFICATION_EXTRA, true);

        Looper.prepare();
        CellBroadcastAlertDialog activity = startActivity(intent, null, null);

        CellBroadcastAlertDialog.AnimationHandler mockAnimationHandler = mock(
                CellBroadcastAlertDialog.AnimationHandler.class);
        activity.mAnimationHandler = mockAnimationHandler;

        activity.onPause();
        verify(mockAnimationHandler).stopIconAnimation();
    }

    public void testOnKeyDown() throws Throwable {
        Intent intent = createActivityIntent();
        intent.putExtra(CellBroadcastAlertDialog.FROM_NOTIFICATION_EXTRA, true);

        Looper.prepare();
        CellBroadcastAlertDialog activity = startActivity(intent, null, null);

        assertTrue(activity.onKeyDown(0,
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_FOCUS)));
    }
}
