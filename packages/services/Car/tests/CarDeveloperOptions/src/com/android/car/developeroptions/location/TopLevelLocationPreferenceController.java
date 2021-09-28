package com.android.car.developeroptions.location;

import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.LocationManager;
import android.permission.PermissionControllerManager;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.core.BasePreferenceController;
import com.android.settingslib.core.lifecycle.LifecycleObserver;
import com.android.settingslib.core.lifecycle.events.OnStart;
import com.android.settingslib.core.lifecycle.events.OnStop;

import java.util.Arrays;

public class TopLevelLocationPreferenceController extends BasePreferenceController implements
        LifecycleObserver, OnStart, OnStop {
    private static final IntentFilter INTENT_FILTER_LOCATION_MODE_CHANGED =
            new IntentFilter(LocationManager.MODE_CHANGED_ACTION);
    private final LocationManager mLocationManager;
    /** Total number of apps that has location permission. */
    private int mNumTotal = -1;
    private BroadcastReceiver mReceiver;
    private Preference mPreference;

    public TopLevelLocationPreferenceController(Context context, String preferenceKey) {
        super(context, preferenceKey);
        mLocationManager = (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
    }

    @Override
    public int getAvailabilityStatus() {
        return AVAILABLE;
    }

    @Override
    public CharSequence getSummary() {
        if (mLocationManager.isLocationEnabled()) {
            if (mNumTotal == -1) {
                return mContext.getString(R.string.location_settings_loading_app_permission_stats);
            }
            return mContext.getResources().getQuantityString(
                    R.plurals.location_settings_summary_location_on,
                    mNumTotal, mNumTotal);
        } else {
            return mContext.getString(R.string.location_settings_summary_location_off);
        }
    }

    @VisibleForTesting
    void setLocationAppCount(int numApps) {
        mNumTotal = numApps;
        refreshSummary(mPreference);
    }

    @Override
    public void updateState(Preference preference) {
        super.updateState(preference);
        mPreference = preference;
        refreshSummary(preference);
        // Bail out if location has been disabled.
        if (!mLocationManager.isLocationEnabled()) {
            return;
        }
        mContext.getSystemService(PermissionControllerManager.class).countPermissionApps(
                Arrays.asList(ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION), 0,
                (numApps) -> {
                    setLocationAppCount(numApps);
                }, null);
    }

    @Override
    public void onStart() {
        if (mReceiver == null) {
            mReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    refreshLocationMode();
                }
            };
        }
        mContext.registerReceiver(mReceiver, INTENT_FILTER_LOCATION_MODE_CHANGED);
        refreshLocationMode();
    }

    @Override
    public void onStop() {
        mContext.unregisterReceiver(mReceiver);
    }

    private void refreshLocationMode() {
        // 'null' is checked inside updateState(), so no need to check here.
        updateState(mPreference);
    }
}
