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

package android.net.ipsec.ike;

import static android.system.OsConstants.AF_INET;
import static android.system.OsConstants.AF_INET6;

import android.annotation.IntRange;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.SystemApi;
import android.net.LinkAddress;

import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeIpv4Address;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeIpv4Dhcp;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeIpv4Dns;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeIpv4Netmask;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeIpv6Address;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeIpv6Dns;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.TunnelModeChildConfigAttribute;

import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Objects;

/**
 * TunnelModeChildSessionParams represents proposed configurations for negotiating a tunnel mode
 * Child Session.
 *
 * @hide
 */
@SystemApi
public final class TunnelModeChildSessionParams extends ChildSessionParams {
    @NonNull private final TunnelModeChildConfigAttribute[] mConfigRequests;

    private TunnelModeChildSessionParams(
            @NonNull IkeTrafficSelector[] inboundTs,
            @NonNull IkeTrafficSelector[] outboundTs,
            @NonNull ChildSaProposal[] proposals,
            @NonNull TunnelModeChildConfigAttribute[] configRequests,
            int hardLifetimeSec,
            int softLifetimeSec) {
        super(
                inboundTs,
                outboundTs,
                proposals,
                hardLifetimeSec,
                softLifetimeSec,
                false /*isTransport*/);
        mConfigRequests = configRequests;
    }

    /** @hide */
    public TunnelModeChildConfigAttribute[] getConfigurationAttributesInternal() {
        return mConfigRequests;
    }

    /** Retrieves the list of Configuration Requests */
    @NonNull
    public List<TunnelModeChildConfigRequest> getConfigurationRequests() {
        return Collections.unmodifiableList(Arrays.asList(mConfigRequests));
    }

    /** Represents a tunnel mode child session configuration request type */
    public interface TunnelModeChildConfigRequest {}

    /** Represents an IPv4 Internal Address request */
    public interface ConfigRequestIpv4Address extends TunnelModeChildConfigRequest {
        /**
         * Retrieves the requested internal IPv4 address
         *
         * @return The requested IPv4 address, or null if no specific internal address was requested
         */
        @Nullable
        Inet4Address getAddress();
    }

    /** Represents an IPv4 DHCP server request */
    public interface ConfigRequestIpv4DhcpServer extends TunnelModeChildConfigRequest {}

    /** Represents an IPv4 DNS Server request */
    public interface ConfigRequestIpv4DnsServer extends TunnelModeChildConfigRequest {}

    /** Represents an IPv4 Netmask request */
    public interface ConfigRequestIpv4Netmask extends TunnelModeChildConfigRequest {}

    /** Represents an IPv6 Internal Address request */
    public interface ConfigRequestIpv6Address extends TunnelModeChildConfigRequest {
        /**
         * Retrieves the requested internal IPv6 address
         *
         * @return The requested IPv6 address, or null if no specific internal address was requested
         */
        @Nullable
        Inet6Address getAddress();

        /**
         * Retrieves the prefix length
         *
         * @return The requested prefix length, or -1 if no specific IPv6 address was requested
         */
        int getPrefixLength();
    }

    /** Represents an IPv6 DNS Server request */
    public interface ConfigRequestIpv6DnsServer extends TunnelModeChildConfigRequest {}

    /** This class can be used to incrementally construct a {@link TunnelModeChildSessionParams}. */
    public static final class Builder extends ChildSessionParams.Builder {
        private static final int IPv4_DEFAULT_PREFIX_LEN = 32;

        private boolean mHasIp4AddressRequest;
        private List<TunnelModeChildConfigAttribute> mConfigRequestList;

        /** Create a Builder for negotiating a transport mode Child Session. */
        public Builder() {
            super();
            mHasIp4AddressRequest = false;
            mConfigRequestList = new LinkedList<>();
        }

        /**
         * Adds an Child SA proposal to the {@link TunnelModeChildSessionParams} being built.
         *
         * @param proposal Child SA proposal.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addSaProposal(@NonNull ChildSaProposal proposal) {
            if (proposal == null) {
                throw new NullPointerException("Required argument not provided");
            }

            addProposal(proposal);
            return this;
        }

        /**
         * Adds an inbound {@link IkeTrafficSelector} to the {@link TunnelModeChildSessionParams}
         * being built.
         *
         * <p>This method allows callers to limit the inbound traffic transmitted over the Child
         * Session to the given range. The IKE server may further narrow the range. Callers should
         * refer to {@link ChildSessionConfiguration} for the negotiated traffic selectors.
         *
         * <p>If no inbound {@link IkeTrafficSelector} is provided, a default value will be used
         * that covers all IP addresses and ports.
         *
         * @param trafficSelector the inbound {@link IkeTrafficSelector}.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addInboundTrafficSelectors(@NonNull IkeTrafficSelector trafficSelector) {
            Objects.requireNonNull(trafficSelector, "Required argument not provided");
            addInboundTs(trafficSelector);
            return this;
        }

        /**
         * Adds an outbound {@link IkeTrafficSelector} to the {@link TunnelModeChildSessionParams}
         * being built.
         *
         * <p>This method allows callers to limit the outbound traffic transmitted over the Child
         * Session to the given range. The IKE server may further narrow the range. Callers should
         * refer to {@link ChildSessionConfiguration} for the negotiated traffic selectors.
         *
         * <p>If no outbound {@link IkeTrafficSelector} is provided, a default value will be used
         * that covers all IP addresses and ports.
         *
         * @param trafficSelector the outbound {@link IkeTrafficSelector}.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addOutboundTrafficSelectors(@NonNull IkeTrafficSelector trafficSelector) {
            Objects.requireNonNull(trafficSelector, "Required argument not provided");
            addOutboundTs(trafficSelector);
            return this;
        }

        /**
         * Sets hard and soft lifetimes.
         *
         * <p>Lifetimes will not be negotiated with the remote IKE server.
         *
         * @param hardLifetimeSeconds number of seconds after which Child SA will expire. Defaults
         *     to 7200 seconds (2 hours). Considering IPsec packet lifetime, IKE library requires
         *     hard lifetime to be a value from 300 seconds (5 minutes) to 14400 seconds (4 hours),
         *     inclusive.
         * @param softLifetimeSeconds number of seconds after which Child SA will request rekey.
         *     Defaults to 3600 seconds (1 hour). MUST be at least 120 seconds (2 minutes), and at
         *     least 60 seconds (1 minute) shorter than the hard lifetime.
         */
        @NonNull
        public Builder setLifetimeSeconds(
                @IntRange(
                                from = CHILD_HARD_LIFETIME_SEC_MINIMUM,
                                to = CHILD_HARD_LIFETIME_SEC_MAXIMUM)
                        int hardLifetimeSeconds,
                @IntRange(
                                from = CHILD_SOFT_LIFETIME_SEC_MINIMUM,
                                to = CHILD_HARD_LIFETIME_SEC_MAXIMUM)
                        int softLifetimeSeconds) {
            validateAndSetLifetime(hardLifetimeSeconds, softLifetimeSeconds);
            mHardLifetimeSec = hardLifetimeSeconds;
            mSoftLifetimeSec = softLifetimeSeconds;
            return this;
        }

        /**
         * Adds an internal IP address request to the {@link TunnelModeChildSessionParams} being
         * built.
         *
         * @param addressFamily the address family. Only {@link OsConstants.AF_INET} and {@link
         *     OsConstants.AF_INET6} are allowed.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addInternalAddressRequest(int addressFamily) {
            if (addressFamily == AF_INET) {
                mHasIp4AddressRequest = true;
                mConfigRequestList.add(new ConfigAttributeIpv4Address());
                return this;
            } else if (addressFamily == AF_INET6) {
                mConfigRequestList.add(new ConfigAttributeIpv6Address());
                return this;
            } else {
                throw new IllegalArgumentException("Invalid address family: " + addressFamily);
            }
        }

        /**
         * Adds a specific internal IPv4 address request to the {@link TunnelModeChildSessionParams}
         * being built.
         *
         * @param address the requested IPv4 address.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addInternalAddressRequest(@NonNull Inet4Address address) {
            if (address == null) {
                throw new NullPointerException("Required argument not provided");
            }

            mHasIp4AddressRequest = true;
            mConfigRequestList.add(new ConfigAttributeIpv4Address((Inet4Address) address));
            return this;
        }

        /**
         * Adds a specific internal IPv6 address request to the {@link TunnelModeChildSessionParams}
         * being built.
         *
         * @param address the requested IPv6 address.
         * @param prefixLen length of the IPv6 address prefix length.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addInternalAddressRequest(@NonNull Inet6Address address, int prefixLen) {
            if (address == null) {
                throw new NullPointerException("Required argument not provided");
            }

            mConfigRequestList.add(
                    new ConfigAttributeIpv6Address(new LinkAddress(address, prefixLen)));
            return this;
        }

        /**
         * Adds an internal DNS server request to the {@link TunnelModeChildSessionParams} being
         * built.
         *
         * @param addressFamily the address family. Only {@link OsConstants.AF_INET} and {@link
         *     OsConstants.AF_INET6} are allowed.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addInternalDnsServerRequest(int addressFamily) {
            if (addressFamily == AF_INET) {
                mConfigRequestList.add(new ConfigAttributeIpv4Dns());
                return this;
            } else if (addressFamily == AF_INET6) {
                mConfigRequestList.add(new ConfigAttributeIpv6Dns());
                return this;
            } else {
                throw new IllegalArgumentException("Invalid address family: " + addressFamily);
            }
        }

        /**
         * Adds a specific internal DNS server request to the {@link TunnelModeChildSessionParams}
         * being built.
         *
         * @param address the requested DNS server address.
         * @return Builder this, to facilitate chaining.
         * @hide
         */
        @NonNull
        public Builder addInternalDnsServerRequest(@NonNull InetAddress address) {
            if (address == null) {
                throw new NullPointerException("Required argument not provided");
            }

            if (address instanceof Inet4Address) {
                mConfigRequestList.add(new ConfigAttributeIpv4Dns((Inet4Address) address));
                return this;
            } else if (address instanceof Inet6Address) {
                mConfigRequestList.add(new ConfigAttributeIpv6Dns((Inet6Address) address));
                return this;
            } else {
                throw new IllegalArgumentException("Invalid address " + address);
            }
        }

        /**
         * Adds internal DHCP server requests to the {@link TunnelModeChildSessionParams} being
         * built.
         *
         * <p>Only DHCPv4 server requests are supported.
         *
         * @param addressFamily the address family. Only {@link OsConstants.AF_INET} is allowed.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addInternalDhcpServerRequest(int addressFamily) {
            if (addressFamily == AF_INET) {
                mConfigRequestList.add(new ConfigAttributeIpv4Dhcp());
                return this;
            } else {
                throw new IllegalArgumentException("Invalid address family: " + addressFamily);
            }
        }

        /**
         * Adds a specific internal DHCP server request to the {@link TunnelModeChildSessionParams}
         * being built.
         *
         * <p>Only DHCPv4 server requests are supported.
         *
         * @param address the requested DHCP server address.
         * @return Builder this, to facilitate chaining.
         * @hide
         */
        @NonNull
        public Builder addInternalDhcpServerRequest(@NonNull InetAddress address) {
            if (address == null) {
                throw new NullPointerException("Required argument not provided");
            }

            if (address instanceof Inet4Address) {
                mConfigRequestList.add(new ConfigAttributeIpv4Dhcp((Inet4Address) address));
                return this;
            } else {
                throw new IllegalArgumentException("Invalid address " + address);
            }
        }

        /**
         * Validates and builds the {@link TunnelModeChildSessionParams}.
         *
         * @return the validated {@link TunnelModeChildSessionParams}.
         */
        @NonNull
        public TunnelModeChildSessionParams build() {
            addDefaultTsIfNotConfigured();
            validateOrThrow();

            if (mHasIp4AddressRequest) {
                mConfigRequestList.add(new ConfigAttributeIpv4Netmask());
            }

            return new TunnelModeChildSessionParams(
                    mInboundTsList.toArray(new IkeTrafficSelector[0]),
                    mOutboundTsList.toArray(new IkeTrafficSelector[0]),
                    mSaProposalList.toArray(new ChildSaProposal[0]),
                    mConfigRequestList.toArray(new TunnelModeChildConfigAttribute[0]),
                    mHardLifetimeSec,
                    mSoftLifetimeSec);
        }
    }
}
