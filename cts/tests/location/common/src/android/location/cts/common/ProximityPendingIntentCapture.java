package android.location.cts.common;

import static android.location.LocationManager.KEY_PROXIMITY_ENTERING;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.LocationManager;
import android.os.Looper;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public class ProximityPendingIntentCapture extends BroadcastCapture {

    private static final String ACTION = "android.location.cts.LOCATION_BROADCAST";
    private static final AtomicInteger sRequestCode = new AtomicInteger(0);

    private final LocationManager mLocationManager;
    private final PendingIntent mPendingIntent;
    private final LinkedBlockingQueue<Boolean> mProximityChanges;

    public ProximityPendingIntentCapture(Context context) {
        super(context);

        mLocationManager = context.getSystemService(LocationManager.class);
        mPendingIntent = PendingIntent.getBroadcast(context, sRequestCode.getAndIncrement(),
                new Intent(ACTION).setPackage(context.getPackageName()),
                PendingIntent.FLAG_CANCEL_CURRENT);
        mProximityChanges = new LinkedBlockingQueue<>();

        register(ACTION);
    }

    public PendingIntent getPendingIntent() {
        return mPendingIntent;
    }

    public Boolean getNextProximityChange(long timeoutMs) throws InterruptedException {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            throw new AssertionError("getNextProximityChange() called from main thread");
        }

        return mProximityChanges.poll(timeoutMs, TimeUnit.MILLISECONDS);
    }

    @Override
    public void close() {
        super.close();
        mLocationManager.removeProximityAlert(mPendingIntent);
        mPendingIntent.cancel();
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        super.onReceive(context, intent);
        if (intent.hasExtra(KEY_PROXIMITY_ENTERING)) {
            mProximityChanges.add(intent.getBooleanExtra(KEY_PROXIMITY_ENTERING, false));
        }
    }
}
