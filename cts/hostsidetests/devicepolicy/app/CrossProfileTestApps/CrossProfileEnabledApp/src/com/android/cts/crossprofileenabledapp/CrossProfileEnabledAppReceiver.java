package com.android.cts.crossprofileenabledapp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Slog;

public class CrossProfileEnabledAppReceiver extends BroadcastReceiver {
    private static final String TAG = "CrossProfileEnabledAppReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        Slog.w(TAG, String.format("onReceive(%s)", intent.getAction()));
    }
}