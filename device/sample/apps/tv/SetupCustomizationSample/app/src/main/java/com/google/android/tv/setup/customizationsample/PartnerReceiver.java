package com.google.android.tv.setup.customizationsample;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * This no-op BroadcastReceiver marks this application as a provider of partner resources for
 * Android TV Setup. See AndroidManifest.xml for more details.
 */
public class PartnerReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        // Do nothing.
    }
}
