/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.networkstack.apishim.api30;

import android.net.IpPrefix;
import android.net.LinkProperties;
import android.net.NetworkCapabilities;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.android.networkstack.apishim.common.CaptivePortalDataShim;
import com.android.networkstack.apishim.common.NetworkInformationShim;
import com.android.networkstack.apishim.common.ShimUtils;

import java.net.Inet4Address;

/**
 * Compatibility implementation of {@link NetworkInformationShim}.
 */
public class NetworkInformationShimImpl extends
        com.android.networkstack.apishim.api29.NetworkInformationShimImpl {
    protected NetworkInformationShimImpl() {}

    /**
     * Get a new instance of {@link NetworkInformationShim}.
     */
    public static NetworkInformationShim newInstance() {
        if (!useApiAboveQ()) {
            return com.android.networkstack.apishim.api29.NetworkInformationShimImpl.newInstance();
        }
        return new NetworkInformationShimImpl();
    }

    /**
     * Indicates whether the shim can use APIs above the Q SDK.
     */
    @VisibleForTesting
    public static boolean useApiAboveQ() {
        return ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q);
    }

    @Nullable
    @Override
    public Uri getCaptivePortalApiUrl(@Nullable LinkProperties lp) {
        if (lp == null) return null;
        return lp.getCaptivePortalApiUrl();
    }

    @Override
    public void setCaptivePortalApiUrl(@NonNull LinkProperties lp, @Nullable Uri url) {
        lp.setCaptivePortalApiUrl(url);
    }

    @Nullable
    @Override
    public CaptivePortalDataShim getCaptivePortalData(@Nullable LinkProperties lp) {
        if (lp == null || lp.getCaptivePortalData() == null) return null;
        return new CaptivePortalDataShimImpl(lp.getCaptivePortalData());
    }

    @Nullable
    @Override
    public IpPrefix getNat64Prefix(@NonNull LinkProperties lp) {
        return lp.getNat64Prefix();
    }

    @Override
    public void setNat64Prefix(@NonNull LinkProperties lp, @Nullable IpPrefix prefix) {
        lp.setNat64Prefix(prefix);
    }

    @Nullable
    @Override
    public String getSsid(@Nullable NetworkCapabilities nc) {
        if (nc == null) return null;
        return nc.getSsid();
    }

    @NonNull
    @Override
    public LinkProperties makeSensitiveFieldsParcelingCopy(@NonNull final LinkProperties lp) {
        return new LinkProperties(lp, true);
    }

    @Override
    public void setDhcpServerAddress(@NonNull LinkProperties lp,
            @NonNull Inet4Address serverAddress) {
        lp.setDhcpServerAddress(serverAddress);
    }
}
