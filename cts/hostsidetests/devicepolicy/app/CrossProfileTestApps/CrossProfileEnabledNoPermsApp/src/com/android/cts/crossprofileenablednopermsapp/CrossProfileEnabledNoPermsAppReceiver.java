package com.android.cts.crossprofileenablednopermsapp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Slog;

public class CrossProfileEnabledNoPermsAppReceiver extends BroadcastReceiver {
    private static final String TAG = "CrossProfileEnabledNoPermsAppReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        Slog.w(TAG, String.format("onReceive(%s)", intent.getAction()));
    }
}
