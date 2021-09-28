package com.android.server.telecom.tests;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.res.Resources;
import android.net.Uri;
import android.os.UserHandle;
import android.telecom.DisconnectCause;
import android.telecom.PhoneAccountHandle;
import android.telephony.TelephonyManager;

import androidx.test.filters.SmallTest;

import com.android.server.telecom.Call;
import com.android.server.telecom.CallState;
import com.android.server.telecom.CallerInfoLookupHelper;
import com.android.server.telecom.CallsManager;
import com.android.server.telecom.ui.DisconnectedCallNotifier;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;

import java.util.Collections;

public class DisconnectedCallNotifierTest extends TelecomTestCase {

    private static final PhoneAccountHandle PHONE_ACCOUNT_HANDLE = new PhoneAccountHandle(
            new ComponentName("com.android.server.telecom.tests", "DisconnectedCallNotifierTest"),
            "testId");
    private static final Uri TEL_CALL_HANDLE = Uri.parse("tel:+11915552620");

    @Mock private CallsManager mCallsManager;
    @Mock private CallerInfoLookupHelper mCallerInfoLookupHelper;

    private NotificationManager mNotificationManager;

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        mContext = mComponentContextFixture.getTestDouble().getApplicationContext();

        mNotificationManager = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        TelephonyManager fakeTelephonyManager = (TelephonyManager) mContext.getSystemService(
                Context.TELEPHONY_SERVICE);
        when(fakeTelephonyManager.getNetworkCountryIso()).thenReturn("US");
        doReturn(mCallerInfoLookupHelper).when(mCallsManager).getCallerInfoLookupHelper();
    }

    @After
    @Override
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @Test
    @SmallTest
    public void testNotificationShownAfterEmergencyCall() {
        Call call = createCall(new DisconnectCause(DisconnectCause.LOCAL,
                DisconnectCause.REASON_EMERGENCY_CALL_PLACED));

        DisconnectedCallNotifier notifier = new DisconnectedCallNotifier(mContext, mCallsManager);
        notifier.onCallStateChanged(call, CallState.NEW, CallState.DIALING);
        notifier.onCallStateChanged(call, CallState.DIALING, CallState.DISCONNECTED);
        verify(mNotificationManager, never()).notifyAsUser(anyString(), anyInt(),
                any(Notification.class), any(UserHandle.class));

        doReturn(Collections.EMPTY_LIST).when(mCallsManager).getCalls();
        notifier.onCallRemoved(call);
        ArgumentCaptor<Notification> captor = ArgumentCaptor.forClass(Notification.class);
        verify(mNotificationManager).notifyAsUser(anyString(), anyInt(),
                captor.capture(), any(UserHandle.class));
        Notification notification = captor.getValue();
        assertNotNull(notification.contentIntent);
        assertEquals(2, notification.actions.length);
    }

    @Test
    @SmallTest
    public void testNotificationShownForDisconnectedEmergencyCall() {
        Call call = createCall(new DisconnectCause(DisconnectCause.LOCAL,
                DisconnectCause.REASON_EMERGENCY_CALL_PLACED));
        when(call.isEmergencyCall()).thenReturn(true);

        DisconnectedCallNotifier notifier = new DisconnectedCallNotifier(mContext, mCallsManager);
        notifier.onCallStateChanged(call, CallState.NEW, CallState.DIALING);
        notifier.onCallStateChanged(call, CallState.DIALING, CallState.DISCONNECTED);
        verify(mNotificationManager, never()).notifyAsUser(anyString(), anyInt(),
                any(Notification.class), any(UserHandle.class));

        doReturn(Collections.EMPTY_LIST).when(mCallsManager).getCalls();
        notifier.onCallRemoved(call);
        ArgumentCaptor<Notification> captor = ArgumentCaptor.forClass(Notification.class);
        verify(mNotificationManager).notifyAsUser(anyString(), anyInt(),
                captor.capture(), any(UserHandle.class));
        Notification notification = captor.getValue();
        assertNull(notification.contentIntent);
        if (notification.actions != null) {
            assertEquals(0, notification.actions.length);
        }
    }

    @Test
    @SmallTest
    public void testNotificationNotShownAfterCall() {
        Call call = createCall(new DisconnectCause(DisconnectCause.LOCAL));

        DisconnectedCallNotifier notifier = new DisconnectedCallNotifier(mContext, mCallsManager);
        notifier.onCallStateChanged(call, CallState.DIALING, CallState.DISCONNECTED);
        verify(mNotificationManager, never()).notifyAsUser(anyString(), anyInt(),
                any(Notification.class), any(UserHandle.class));

        doReturn(Collections.EMPTY_LIST).when(mCallsManager).getCalls();
        notifier.onCallRemoved(call);
        verify(mNotificationManager, never()).notifyAsUser(anyString(), anyInt(),
                any(Notification.class), any(UserHandle.class));
    }

    @Test
    @SmallTest
    public void testNotificationClearedForEmergencyCall() {
        Call call = createCall(new DisconnectCause(DisconnectCause.LOCAL,
                DisconnectCause.REASON_EMERGENCY_CALL_PLACED));

        DisconnectedCallNotifier notifier = new DisconnectedCallNotifier(mContext, mCallsManager);
        notifier.onCallStateChanged(call, CallState.DIALING, CallState.DISCONNECTED);
        verify(mNotificationManager).cancelAsUser(anyString(), anyInt(), any());
    }

    private Call createCall(DisconnectCause cause) {
        Call call = mock(Call.class);
        when(call.getDisconnectCause()).thenReturn(cause);
        when(call.getTargetPhoneAccount()).thenReturn(PHONE_ACCOUNT_HANDLE);
        when(call.getHandle()).thenReturn(TEL_CALL_HANDLE);
        return call;
    }
}
