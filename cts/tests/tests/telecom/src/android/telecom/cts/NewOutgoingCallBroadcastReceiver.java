package android.telecom.cts;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

/**
 * Broadcast receiver for {@link Intent#ACTION_NEW_OUTGOING_CALL}.
 */
public class NewOutgoingCallBroadcastReceiver extends BroadcastReceiver {
    private static boolean sIsNewOutgoingCallBroadcastReceived = false;
    private static Uri sReceivedPhoneNumber = null;

    @Override
    public void onReceive(Context context, Intent intent) {
        if (Intent.ACTION_NEW_OUTGOING_CALL.equals(intent.getAction())) {
            sIsNewOutgoingCallBroadcastReceived = true;
            sReceivedPhoneNumber = intent.getData();
        }
    }

    public static boolean isNewOutgoingCallBroadcastReceived() {
        return sIsNewOutgoingCallBroadcastReceived;
    }

    public static Uri getReceivedNumber() {
        return sReceivedPhoneNumber;
    }

    public static void reset() {
        sIsNewOutgoingCallBroadcastReceived = false;
        sReceivedPhoneNumber = null;
    }
}
