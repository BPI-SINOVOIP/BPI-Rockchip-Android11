package com.android.tv.settings.vpn;

import android.content.Context;
import androidx.preference.Preference;
import android.text.TextUtils;
import android.view.View;

import com.android.internal.net.VpnProfile;
import com.android.tv.settings.R;

import static com.android.internal.net.LegacyVpnInfo.STATE_CONNECTED;

/**
 * {@link androidx.preference.Preference} tracks the underlying legacy vpn profile and
 * its connection state.
 */
public class LegacyVpnPreference extends ManageablePreference {
    private VpnProfile mProfile;

    public LegacyVpnPreference(Context context) {
        super(context, null /* attrs */);
        setIcon(R.drawable.ic_launcher_settings);
    }

    public VpnProfile getProfile() {
        return mProfile;
    }

    public void setProfile(VpnProfile profile) {
        final String oldLabel = (mProfile != null ? mProfile.name : null);
        final String newLabel = (profile != null ? profile.name : null);
        if (!TextUtils.equals(oldLabel, newLabel)) {
            setTitle(newLabel);
            notifyHierarchyChanged();
        }
        mProfile = profile;
    }

    @Override
    public int compareTo(Preference preference) {
        if (preference instanceof LegacyVpnPreference) {
            LegacyVpnPreference another = (LegacyVpnPreference) preference;
            int result;
            if ((result = another.mState - mState) == 0 &&
                    (result = mProfile.name.compareToIgnoreCase(another.mProfile.name)) == 0 &&
                    (result = mProfile.type - another.mProfile.type) == 0) {
                result = mProfile.key.compareTo(another.mProfile.key);
            }
            return result;
        } /*else if (preference instanceof AppPreference) {
            // Try to sort connected VPNs first
            AppPreference another = (AppPreference) preference;
            if (mState != STATE_CONNECTED && another.getState() == AppPreference.STATE_CONNECTED) {
                return 1;
            }
            // Show configured VPNs before app VPNs
            return -1;
        } */ else {
            return super.compareTo(preference);
        }
    }

    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.settings_button && isDisabledByAdmin()) {
            performClick();
            return;
        }
        super.onClick(v);
    }
}
