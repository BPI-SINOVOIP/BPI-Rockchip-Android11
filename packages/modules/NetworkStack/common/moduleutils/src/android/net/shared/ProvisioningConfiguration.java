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

package android.net.shared;

import static android.net.shared.ParcelableUtil.fromParcelableArray;
import static android.net.shared.ParcelableUtil.toParcelableArray;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.net.INetd;
import android.net.InformationElementParcelable;
import android.net.Network;
import android.net.ProvisioningConfigurationParcelable;
import android.net.ScanResultInfoParcelable;
import android.net.StaticIpConfiguration;
import android.net.apf.ApfCapabilities;
import android.net.ip.IIpClient;
import android.util.Log;

import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
import java.util.StringJoiner;

/**
 * This class encapsulates parameters to be passed to
 * IpClient#startProvisioning(). A defensive copy is made by IpClient
 * and the values specified herein are in force until IpClient#stop()
 * is called.
 *
 * Example use:
 *
 *     final ProvisioningConfiguration config =
 *             new ProvisioningConfiguration.Builder()
 *                     .withPreDhcpAction()
 *                     .withProvisioningTimeoutMs(36 * 1000)
 *                     .build();
 *     mIpClient.startProvisioning(config.toStableParcelable());
 *     ...
 *     mIpClient.stop();
 *
 * The specified provisioning configuration will only be active until
 * IIpClient#stop() is called. Future calls to IIpClient#startProvisioning()
 * must specify the configuration again.
 * @hide
 */
public class ProvisioningConfiguration {
    private static final String TAG = "ProvisioningConfiguration";

    // TODO: Delete this default timeout once those callers that care are
    // fixed to pass in their preferred timeout.
    //
    // We pick 18 seconds so we can send DHCP requests at
    //
    //     t=0, t=1, t=3, t=7, t=16
    //
    // allowing for 10% jitter.
    private static final int DEFAULT_TIMEOUT_MS = 18 * 1000;

    /**
     * Builder to create a {@link ProvisioningConfiguration}.
     */
    public static class Builder {
        protected ProvisioningConfiguration mConfig = new ProvisioningConfiguration();

        /**
         * Specify that the configuration should not enable IPv4. It is enabled by default.
         */
        public Builder withoutIPv4() {
            mConfig.mEnableIPv4 = false;
            return this;
        }

        /**
         * Specify that the configuration should not enable IPv6. It is enabled by default.
         */
        public Builder withoutIPv6() {
            mConfig.mEnableIPv6 = false;
            return this;
        }

        /**
         * Specify that the configuration should not use a MultinetworkPolicyTracker. It is used
         * by default.
         */
        public Builder withoutMultinetworkPolicyTracker() {
            mConfig.mUsingMultinetworkPolicyTracker = false;
            return this;
        }

        /**
         * Specify that the configuration should not use a IpReachabilityMonitor. It is used by
         * default.
         */
        public Builder withoutIpReachabilityMonitor() {
            mConfig.mUsingIpReachabilityMonitor = false;
            return this;
        }

        /**
         * Identical to {@link #withPreDhcpAction(int)}, using a default timeout.
         * @see #withPreDhcpAction(int)
         */
        public Builder withPreDhcpAction() {
            mConfig.mRequestedPreDhcpActionMs = DEFAULT_TIMEOUT_MS;
            return this;
        }

        /**
         * Specify that {@link IpClientCallbacks#onPreDhcpAction()} should be called. Clients must
         * call {@link IIpClient#completedPreDhcpAction()} when the callback called. This behavior
         * is disabled by default.
         * @param dhcpActionTimeoutMs Timeout for clients to call completedPreDhcpAction().
         */
        public Builder withPreDhcpAction(int dhcpActionTimeoutMs) {
            mConfig.mRequestedPreDhcpActionMs = dhcpActionTimeoutMs;
            return this;
        }

        /**
         * Specify that preconnection feature would be enabled. It's not used by default.
         */
        public Builder withPreconnection() {
            mConfig.mEnablePreconnection = true;
            return this;
        }

        /**
         * Specify the initial provisioning configuration.
         */
        public Builder withInitialConfiguration(InitialConfiguration initialConfig) {
            mConfig.mInitialConfig = initialConfig;
            return this;
        }

        /**
         * Specify a static configuration for provisioning.
         */
        public Builder withStaticConfiguration(StaticIpConfiguration staticConfig) {
            mConfig.mStaticIpConfig = staticConfig;
            return this;
        }

        /**
         * Specify ApfCapabilities.
         */
        public Builder withApfCapabilities(ApfCapabilities apfCapabilities) {
            mConfig.mApfCapabilities = apfCapabilities;
            return this;
        }

        /**
         * Specify the timeout to use for provisioning.
         */
        public Builder withProvisioningTimeoutMs(int timeoutMs) {
            mConfig.mProvisioningTimeoutMs = timeoutMs;
            return this;
        }

        /**
         * Specify that IPv6 address generation should use a random MAC address.
         */
        public Builder withRandomMacAddress() {
            mConfig.mIPv6AddrGenMode = INetd.IPV6_ADDR_GEN_MODE_EUI64;
            return this;
        }

        /**
         * Specify that IPv6 address generation should use a stable MAC address.
         */
        public Builder withStableMacAddress() {
            mConfig.mIPv6AddrGenMode = INetd.IPV6_ADDR_GEN_MODE_STABLE_PRIVACY;
            return this;
        }

        /**
         * Specify the network to use for provisioning.
         */
        public Builder withNetwork(Network network) {
            mConfig.mNetwork = network;
            return this;
        }

        /**
         * Specify the display name that the IpClient should use.
         */
        public Builder withDisplayName(String displayName) {
            mConfig.mDisplayName = displayName;
            return this;
        }

        /**
         * Specify the information elements included in wifi scan result that was obtained
         * prior to connecting to the access point, if this is a WiFi network.
         *
         * <p>The scan result can be used to infer whether the network is metered.
         */
        public Builder withScanResultInfo(ScanResultInfo scanResultInfo) {
            mConfig.mScanResultInfo = scanResultInfo;
            return this;
        }

        /**
         * Specify the L2 information(bssid, l2key and cluster) that the IpClient should use.
         */
        public Builder withLayer2Information(Layer2Information layer2Info) {
            mConfig.mLayer2Info = layer2Info;
            return this;
        }

        /**
         * Build the configuration using previously specified parameters.
         */
        public ProvisioningConfiguration build() {
            return new ProvisioningConfiguration(mConfig);
        }
    }

    /**
     * Class wrapper of {@link android.net.wifi.ScanResult} to encapsulate the SSID and
     * InformationElements fields of ScanResult.
     */
    public static class ScanResultInfo {
        @NonNull
        private final String mSsid;
        @NonNull
        private final String mBssid;
        @NonNull
        private final List<InformationElement> mInformationElements;

       /**
        * Class wrapper of {@link android.net.wifi.ScanResult.InformationElement} to encapsulate
        * the specific IE id and payload fields.
        */
        public static class InformationElement {
            private final int mId;
            @NonNull
            private final byte[] mPayload;

            public InformationElement(int id, @NonNull ByteBuffer payload) {
                mId = id;
                mPayload = convertToByteArray(payload.asReadOnlyBuffer());
            }

           /**
            * Get the element ID of the information element.
            */
            public int getId() {
                return mId;
            }

           /**
            * Get the specific content of the information element.
            */
            @NonNull
            public ByteBuffer getPayload() {
                return ByteBuffer.wrap(mPayload).asReadOnlyBuffer();
            }

            @Override
            public boolean equals(Object o) {
                if (o == this) return true;
                if (!(o instanceof InformationElement)) return false;
                InformationElement other = (InformationElement) o;
                return mId == other.mId && Arrays.equals(mPayload, other.mPayload);
            }

            @Override
            public int hashCode() {
                return Objects.hash(mId, mPayload);
            }

            @Override
            public String toString() {
                return "ID: " + mId + ", " + Arrays.toString(mPayload);
            }

            /**
             * Convert this InformationElement to a {@link InformationElementParcelable}.
             */
            public InformationElementParcelable toStableParcelable() {
                final InformationElementParcelable p = new InformationElementParcelable();
                p.id = mId;
                p.payload = mPayload != null ? mPayload.clone() : null;
                return p;
            }

            /**
             * Create an instance of {@link InformationElement} based on the contents of the
             * specified {@link InformationElementParcelable}.
             */
            @Nullable
            public static InformationElement fromStableParcelable(InformationElementParcelable p) {
                if (p == null) return null;
                return new InformationElement(p.id,
                        ByteBuffer.wrap(p.payload.clone()).asReadOnlyBuffer());
            }
        }

        public ScanResultInfo(@NonNull String ssid, @NonNull String bssid,
                @NonNull List<InformationElement> informationElements) {
            Objects.requireNonNull(ssid, "ssid must not be null.");
            Objects.requireNonNull(bssid, "bssid must not be null.");
            mSsid = ssid;
            mBssid = bssid;
            mInformationElements =
                    Collections.unmodifiableList(new ArrayList<>(informationElements));
        }

        /**
         * Get the scanned network name.
         */
        @NonNull
        public String getSsid() {
            return mSsid;
        }

        /**
         * Get the address of the access point.
         */
        @NonNull
        public String getBssid() {
            return mBssid;
        }

        /**
         * Get all information elements found in the beacon.
         */
        @NonNull
        public List<InformationElement> getInformationElements() {
            return mInformationElements;
        }

        @Override
        public String toString() {
            StringBuffer str = new StringBuffer();
            str.append("SSID: ").append(mSsid);
            str.append(", BSSID: ").append(mBssid);
            str.append(", Information Elements: {");
            for (InformationElement ie : mInformationElements) {
                str.append("[").append(ie.toString()).append("]");
            }
            str.append("}");
            return str.toString();
        }

        @Override
        public boolean equals(Object o) {
            if (o == this) return true;
            if (!(o instanceof ScanResultInfo)) return false;
            ScanResultInfo other = (ScanResultInfo) o;
            return Objects.equals(mSsid, other.mSsid)
                    && Objects.equals(mBssid, other.mBssid)
                    && mInformationElements.equals(other.mInformationElements);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mSsid, mBssid, mInformationElements);
        }

        /**
         * Convert this ScanResultInfo to a {@link ScanResultInfoParcelable}.
         */
        public ScanResultInfoParcelable toStableParcelable() {
            final ScanResultInfoParcelable p = new ScanResultInfoParcelable();
            p.ssid = mSsid;
            p.bssid = mBssid;
            p.informationElements = toParcelableArray(mInformationElements,
                    InformationElement::toStableParcelable, InformationElementParcelable.class);
            return p;
        }

        /**
         * Create an instance of {@link ScanResultInfo} based on the contents of the specified
         * {@link ScanResultInfoParcelable}.
         */
        public static ScanResultInfo fromStableParcelable(ScanResultInfoParcelable p) {
            if (p == null) return null;
            final List<InformationElement> ies = new ArrayList<InformationElement>();
            ies.addAll(fromParcelableArray(p.informationElements,
                    InformationElement::fromStableParcelable));
            return new ScanResultInfo(p.ssid, p.bssid, ies);
        }

        private static byte[] convertToByteArray(@NonNull final ByteBuffer buffer) {
            final byte[] bytes = new byte[buffer.limit()];
            final ByteBuffer copy = buffer.asReadOnlyBuffer();
            try {
                copy.position(0);
                copy.get(bytes);
            } catch (BufferUnderflowException e) {
                Log.wtf(TAG, "Buffer under flow exception should never happen.");
            } finally {
                return bytes;
            }
        }
    }

    public boolean mEnableIPv4 = true;
    public boolean mEnableIPv6 = true;
    public boolean mEnablePreconnection = false;
    public boolean mUsingMultinetworkPolicyTracker = true;
    public boolean mUsingIpReachabilityMonitor = true;
    public int mRequestedPreDhcpActionMs;
    public InitialConfiguration mInitialConfig;
    public StaticIpConfiguration mStaticIpConfig;
    public ApfCapabilities mApfCapabilities;
    public int mProvisioningTimeoutMs = DEFAULT_TIMEOUT_MS;
    public int mIPv6AddrGenMode = INetd.IPV6_ADDR_GEN_MODE_STABLE_PRIVACY;
    public Network mNetwork = null;
    public String mDisplayName = null;
    public ScanResultInfo mScanResultInfo;
    public Layer2Information mLayer2Info;

    public ProvisioningConfiguration() {} // used by Builder

    public ProvisioningConfiguration(ProvisioningConfiguration other) {
        mEnableIPv4 = other.mEnableIPv4;
        mEnableIPv6 = other.mEnableIPv6;
        mEnablePreconnection = other.mEnablePreconnection;
        mUsingMultinetworkPolicyTracker = other.mUsingMultinetworkPolicyTracker;
        mUsingIpReachabilityMonitor = other.mUsingIpReachabilityMonitor;
        mRequestedPreDhcpActionMs = other.mRequestedPreDhcpActionMs;
        mInitialConfig = InitialConfiguration.copy(other.mInitialConfig);
        mStaticIpConfig = other.mStaticIpConfig == null
                ? null
                : new StaticIpConfiguration(other.mStaticIpConfig);
        mApfCapabilities = other.mApfCapabilities;
        mProvisioningTimeoutMs = other.mProvisioningTimeoutMs;
        mIPv6AddrGenMode = other.mIPv6AddrGenMode;
        mNetwork = other.mNetwork;
        mDisplayName = other.mDisplayName;
        mScanResultInfo = other.mScanResultInfo;
        mLayer2Info = other.mLayer2Info;
    }

    /**
     * Create a ProvisioningConfigurationParcelable from this ProvisioningConfiguration.
     */
    public ProvisioningConfigurationParcelable toStableParcelable() {
        final ProvisioningConfigurationParcelable p = new ProvisioningConfigurationParcelable();
        p.enableIPv4 = mEnableIPv4;
        p.enableIPv6 = mEnableIPv6;
        p.enablePreconnection = mEnablePreconnection;
        p.usingMultinetworkPolicyTracker = mUsingMultinetworkPolicyTracker;
        p.usingIpReachabilityMonitor = mUsingIpReachabilityMonitor;
        p.requestedPreDhcpActionMs = mRequestedPreDhcpActionMs;
        p.initialConfig = mInitialConfig == null ? null : mInitialConfig.toStableParcelable();
        p.staticIpConfig = mStaticIpConfig == null
                ? null
                : new StaticIpConfiguration(mStaticIpConfig);
        p.apfCapabilities = mApfCapabilities; // ApfCapabilities is immutable
        p.provisioningTimeoutMs = mProvisioningTimeoutMs;
        p.ipv6AddrGenMode = mIPv6AddrGenMode;
        p.network = mNetwork;
        p.displayName = mDisplayName;
        p.scanResultInfo = mScanResultInfo == null ? null : mScanResultInfo.toStableParcelable();
        p.layer2Info = mLayer2Info == null ? null : mLayer2Info.toStableParcelable();
        return p;
    }

    /**
     * Create a ProvisioningConfiguration from a ProvisioningConfigurationParcelable.
     */
    public static ProvisioningConfiguration fromStableParcelable(
            @Nullable ProvisioningConfigurationParcelable p) {
        if (p == null) return null;
        final ProvisioningConfiguration config = new ProvisioningConfiguration();
        config.mEnableIPv4 = p.enableIPv4;
        config.mEnableIPv6 = p.enableIPv6;
        config.mEnablePreconnection = p.enablePreconnection;
        config.mUsingMultinetworkPolicyTracker = p.usingMultinetworkPolicyTracker;
        config.mUsingIpReachabilityMonitor = p.usingIpReachabilityMonitor;
        config.mRequestedPreDhcpActionMs = p.requestedPreDhcpActionMs;
        config.mInitialConfig = InitialConfiguration.fromStableParcelable(p.initialConfig);
        config.mStaticIpConfig = p.staticIpConfig == null
                ? null
                : new StaticIpConfiguration(p.staticIpConfig);
        config.mApfCapabilities = p.apfCapabilities; // ApfCapabilities is immutable
        config.mProvisioningTimeoutMs = p.provisioningTimeoutMs;
        config.mIPv6AddrGenMode = p.ipv6AddrGenMode;
        config.mNetwork = p.network;
        config.mDisplayName = p.displayName;
        config.mScanResultInfo = ScanResultInfo.fromStableParcelable(p.scanResultInfo);
        config.mLayer2Info = Layer2Information.fromStableParcelable(p.layer2Info);
        return config;
    }

    @Override
    public String toString() {
        return new StringJoiner(", ", getClass().getSimpleName() + "{", "}")
                .add("mEnableIPv4: " + mEnableIPv4)
                .add("mEnableIPv6: " + mEnableIPv6)
                .add("mEnablePreconnection: " + mEnablePreconnection)
                .add("mUsingMultinetworkPolicyTracker: " + mUsingMultinetworkPolicyTracker)
                .add("mUsingIpReachabilityMonitor: " + mUsingIpReachabilityMonitor)
                .add("mRequestedPreDhcpActionMs: " + mRequestedPreDhcpActionMs)
                .add("mInitialConfig: " + mInitialConfig)
                .add("mStaticIpConfig: " + mStaticIpConfig)
                .add("mApfCapabilities: " + mApfCapabilities)
                .add("mProvisioningTimeoutMs: " + mProvisioningTimeoutMs)
                .add("mIPv6AddrGenMode: " + mIPv6AddrGenMode)
                .add("mNetwork: " + mNetwork)
                .add("mDisplayName: " + mDisplayName)
                .add("mScanResultInfo: " + mScanResultInfo)
                .add("mLayer2Info: " + mLayer2Info)
                .toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof ProvisioningConfiguration)) return false;
        final ProvisioningConfiguration other = (ProvisioningConfiguration) obj;
        return mEnableIPv4 == other.mEnableIPv4
                && mEnableIPv6 == other.mEnableIPv6
                && mEnablePreconnection == other.mEnablePreconnection
                && mUsingMultinetworkPolicyTracker == other.mUsingMultinetworkPolicyTracker
                && mUsingIpReachabilityMonitor == other.mUsingIpReachabilityMonitor
                && mRequestedPreDhcpActionMs == other.mRequestedPreDhcpActionMs
                && Objects.equals(mInitialConfig, other.mInitialConfig)
                && Objects.equals(mStaticIpConfig, other.mStaticIpConfig)
                && Objects.equals(mApfCapabilities, other.mApfCapabilities)
                && mProvisioningTimeoutMs == other.mProvisioningTimeoutMs
                && mIPv6AddrGenMode == other.mIPv6AddrGenMode
                && Objects.equals(mNetwork, other.mNetwork)
                && Objects.equals(mDisplayName, other.mDisplayName)
                && Objects.equals(mScanResultInfo, other.mScanResultInfo)
                && Objects.equals(mLayer2Info, other.mLayer2Info);
    }

    public boolean isValid() {
        return (mInitialConfig == null) || mInitialConfig.isValid();
    }
}
