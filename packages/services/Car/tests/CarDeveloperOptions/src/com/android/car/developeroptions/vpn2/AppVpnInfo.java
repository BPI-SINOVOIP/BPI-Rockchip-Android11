package com.android.car.developeroptions.vpn2;

import android.annotation.NonNull;

import java.util.Objects;

/**
 * Holds packageName:userId pairs without any heavyweight fields.
 * {@see ApplicationInfo}
 */
class AppVpnInfo implements Comparable {
    public final int userId;
    public final String packageName;

    public AppVpnInfo(int userId, @NonNull String packageName) {
        this.userId = userId;
        this.packageName = Objects.requireNonNull(packageName);
    }

    @Override
    public int compareTo(Object other) {
        AppVpnInfo that = (AppVpnInfo) other;

        int result = packageName.compareTo(that.packageName);
        if (result == 0) {
            result = that.userId - userId;
        }
        return result;
    }

    @Override
    public boolean equals(Object other) {
        if (other instanceof AppVpnInfo) {
            AppVpnInfo that = (AppVpnInfo) other;
            return userId == that.userId && Objects.equals(packageName, that.packageName);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(packageName, userId);
    }
}
