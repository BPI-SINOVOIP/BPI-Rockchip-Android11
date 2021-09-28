package com.android.cts.hotspot;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.wifi.WifiManager;
import android.os.Handler;

public class Notify extends BroadcastReceiver {

    private static final String EXTRA_HOTSPOT_KEY = "HOTSPOT";
    private static WifiManager.LocalOnlyHotspotReservation mReservation;

    @Override
    public void onReceive(Context context, Intent intent) {
        if ("com.android.cts.hotspot.TEST_ACTION".equals(intent.getAction())) {
            if (intent.hasExtra(EXTRA_HOTSPOT_KEY)) {
                if ("turnOn".equals(intent.getStringExtra(EXTRA_HOTSPOT_KEY))) {
                    turnOnHotspot(context);
                } else if ("turnOff".equals(intent.getStringExtra(EXTRA_HOTSPOT_KEY))) {
                    turnOffHotspot();
                }
            }
        }
    }

    private void turnOnHotspot(Context x) {
        WifiManager manager = (WifiManager) x.getSystemService(Context.WIFI_SERVICE);

        manager.startLocalOnlyHotspot(
                new WifiManager.LocalOnlyHotspotCallback() {

                    @Override
                    public void onStarted(WifiManager.LocalOnlyHotspotReservation reservation) {
                        mReservation = reservation;
                        super.onStarted(reservation);
                    }

                    @Override
                    public void onStopped() {
                        super.onStopped();
                    }

                    @Override
                    public void onFailed(int reason) {
                        super.onFailed(reason);
                    }
                },
                new Handler());
    }

    private void turnOffHotspot() {
        if (mReservation != null) {
            mReservation.close();
        }
    }
}
