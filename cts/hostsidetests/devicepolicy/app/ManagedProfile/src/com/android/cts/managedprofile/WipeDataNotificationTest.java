package com.android.cts.managedprofile;

import android.annotation.NonNull;
import android.app.Notification;
import android.os.Bundle;
import android.test.AndroidTestCase;
import android.text.TextUtils;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.SystemUtil;
import com.android.internal.notification.SystemNotificationChannels;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Test wipeDataWithReason() has indeed shown the notification.
 * The function wipeDataWithReason() is called and executed in another test.
 */
public class WipeDataNotificationTest extends AndroidTestCase {

    private static final String WIPE_DATA_TITLE = "Work profile deleted";
    private static final int NOTIF_TIMEOUT_SECONDS = 5;

    public void testWipeDataWithReasonVerification() throws Exception {
        final int profileId = getProfileId();
        assertTrue("Invalid profileId", profileId > 0);

        CountDownLatch notificationLatch = initWipeNotificationLatch(WIPE_DATA_TITLE,
                WipeDataReceiver.TEST_WIPE_DATA_REASON);
        sendWipeProfileBroadcast(WipeDataReceiver.ACTION_WIPE_DATA_WITH_REASON, profileId);

        assertTrue("Did not receive notification for profile wipe",
                notificationLatch.await(NOTIF_TIMEOUT_SECONDS, TimeUnit.SECONDS));
        NotificationListener.getInstance().clearListeners();
    }

    public void testWipeDataWithEmptyReasonVerification() throws Exception {
        final int profileId = getProfileId();
        assertTrue("Invalid profileId", profileId > 0);

        // if reason is empty, DPM will fill it with internal strings: ignore
        CountDownLatch notificationLatch = initWipeNotificationLatch(WIPE_DATA_TITLE, "");
        sendWipeProfileBroadcast(WipeDataReceiver.ACTION_WIPE_DATA, profileId);

        assertTrue("Did not receive notification for profile wipe",
                notificationLatch.await(NOTIF_TIMEOUT_SECONDS, TimeUnit.SECONDS));
        NotificationListener.getInstance().clearListeners();
    }

    public void testWipeDataWithoutReasonVerification() throws Exception {
        int profileId = getProfileId();
        assertTrue("Invalid profileId", profileId > 0);

        CountDownLatch notificationLatch = initWipeNotificationLatch("", "");
        sendWipeProfileBroadcast(WipeDataReceiver.ACTION_WIPE_DATA_WITHOUT_REASON, profileId);

        assertFalse("Wipe notification found",
                notificationLatch.await(NOTIF_TIMEOUT_SECONDS, TimeUnit.SECONDS));
        NotificationListener.getInstance().clearListeners();
    }

    private int getProfileId() {
        int profileId = -1;
        final Bundle testArguments = InstrumentationRegistry.getArguments();
        if (testArguments.containsKey("profileId")) {
            profileId = Integer.parseInt(testArguments.getString("profileId"));
        }
        return profileId;
    }

    private CountDownLatch initWipeNotificationLatch(@NonNull String notificationTitle,
            @NonNull String notificationText) {
        CountDownLatch notificationCounterLatch = new CountDownLatch(1);
        NotificationListener.getInstance().addListener((notification) -> {
            if (notification.getNotification().getChannelId().equals(
                    SystemNotificationChannels.DEVICE_ADMIN)) {
                assertEquals("Wipe notification title not found", notificationTitle,
                        notification.getNotification().extras.getString(Notification.EXTRA_TITLE));
                // do not check reason text if param is empty == ignore notif. text
                if (!TextUtils.isEmpty(notificationText)) {
                    assertEquals("Wipe notification content not found", notificationText,
                            notification.getNotification().extras.getString(
                                    Notification.EXTRA_TEXT));
                }
                notificationCounterLatch.countDown();
            }
        });
        return notificationCounterLatch;
    }

    private void sendWipeProfileBroadcast(String action, int profileId) throws Exception {
        final String cmd = "am broadcast --receiver-foreground --user " + profileId
                + " -a " + action
                + " com.android.cts.managedprofile/.WipeDataReceiver";
        SystemUtil.runShellCommand(cmd);
    }
}
