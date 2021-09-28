package com.android.cts.modifyquietmodeenabledapp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Slog;

public class ModifyQuietModeEnabledAppReceiver extends BroadcastReceiver {
    private static final String TAG = "ModifyQuietModeEnabledAppReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        Slog.w(TAG, String.format("onReceive(%s)", intent.getAction()));
    }
}
