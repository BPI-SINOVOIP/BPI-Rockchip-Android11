package com.android.settingslib.development;

import android.app.ActivityManager;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.os.UserManager;
import android.os.SystemProperties;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.preference.SwitchPreference;
import androidx.preference.TwoStatePreference;


import android.text.TextUtils;

import com.android.settingslib.core.ConfirmationDialogController;

import android.net.ConnectivityManager;
import android.net.LinkProperties;
import java.io.IOException;
import java.io.InputStream;
import java.net.InetAddress;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.net.Inet4Address;

import com.android.settingslib.R;

public abstract class AbstractEnableInternetAdbPreferenceController extends
        DeveloperOptionsPreferenceController implements ConfirmationDialogController {
    private static final String KEY_ENABLE_INTERNET_ADB = "enable_internet_adb";
    public static final String ACTION_ENABLE_INTERNET_ADB_STATE_CHANGED =
            "com.android.settingslib.development.AbstractEnableInternetAdbController."
                    + "ENABLE_ADB_STATE_CHANGED";

    public static final int ADB_SETTING_ON = 1;
    public static final int ADB_SETTING_OFF = 0;


    protected SwitchPreference mPreference;
    private ConnectivityManager mConnectivityManager;
    public AbstractEnableInternetAdbPreferenceController(Context context) {
        super(context);
        mConnectivityManager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    @Override
    public void displayPreference(PreferenceScreen screen) {
        super.displayPreference(screen);
        if (isAvailable()) {
            mPreference = (SwitchPreference) screen.findPreference(KEY_ENABLE_INTERNET_ADB);
        }
    }

    @Override
    public boolean isAvailable() {
        final UserManager um = mContext.getSystemService(UserManager.class);
        return um != null && (um.isAdminUser() || um.isDemoUser());
    }

    @Override
    public String getPreferenceKey() {
        return KEY_ENABLE_INTERNET_ADB;
    }

    private boolean isInternetAdbEnabled() {
        String internetADB = SystemProperties.get("persist.internet_adb_enable", "0");
        return "1".equals(internetADB);
    }


    @Override
    public void updateState(Preference preference) {
        boolean enabled = isInternetAdbEnabled();
        int port  = SystemProperties.getInt("service.adb.tcp.port", 0) ;
        ((TwoStatePreference) preference).setChecked(isInternetAdbEnabled());
        String ipAddress = null;
        if (enabled) {
            ipAddress = getDefaultIpV4Addresses(mConnectivityManager);
        }

        if (ipAddress != null) {
            preference.setSummary(ipAddress + ":" + String.valueOf(port));
        }else{
            preference.setSummary(R.string.enable_internet_adb_summary);
        }

    }



    public void enablePreference(boolean enabled) {
        if (isAvailable()) {
            mPreference.setEnabled(enabled);
        }
    }

    public void resetPreference() {
        if (mPreference.isChecked()) {
            mPreference.setChecked(false);
            handlePreferenceTreeClick(mPreference);
        }
    }

    public boolean haveDebugSettings() {
        return isInternetAdbEnabled();
    }

    @Override
    public boolean handlePreferenceTreeClick(Preference preference) {
        if (isUserAMonkey()) {
            return false;
        }

        if (TextUtils.equals(KEY_ENABLE_INTERNET_ADB, preference.getKey())) {
            if (!isInternetAdbEnabled()) {
                showConfirmationDialog(preference);
            } else {
                writeInternetAdbSetting(false);
                updateState(preference);
            }
            return true;
        } else {
            return false;
        }
    }

    protected void writeInternetAdbSetting(boolean enabled) {
        SystemProperties.set("persist.internet_adb_enable", enabled?"1":"0");
        notifyStateChanged();
    }

    private void notifyStateChanged() {
        LocalBroadcastManager.getInstance(mContext)
                .sendBroadcast(new Intent(ACTION_ENABLE_INTERNET_ADB_STATE_CHANGED));
    }

    @VisibleForTesting
    boolean isUserAMonkey() {
        return ActivityManager.isUserAMonkey();
    }

    public static String getDefaultIpV4Addresses(ConnectivityManager cm) {
        LinkProperties prop = cm.getActiveLinkProperties();
        return formatIpv4Addresses(prop);
    }

    private static String formatIpv4Addresses(LinkProperties prop) {
        if (prop == null) return null;
        Iterator<InetAddress> iter = prop.getAllAddresses().iterator();
        if (!iter.hasNext()) return null;
         String addresses = "";
          while (iter.hasNext()) {
              InetAddress inetAddress = iter.next();
              if(inetAddress instanceof Inet4Address)
              {
                    addresses += inetAddress.getHostAddress();
                    if (iter.hasNext()) addresses += "\n";
              }
          }
          return addresses;
    }
}
