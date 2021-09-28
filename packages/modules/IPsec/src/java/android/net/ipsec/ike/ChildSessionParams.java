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

import android.annotation.IntRange;
import android.annotation.NonNull;
import android.annotation.SuppressLint;
import android.annotation.SystemApi;
import android.net.InetAddresses;

import java.net.InetAddress;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * ChildSessionParams is an abstract class that represents proposed configurations for negotiating a
 * Child Session.
 *
 * <p>Note that references to negotiated configurations will be held, and the same parameters will
 * be reused during rekey. This includes SA Proposals, lifetimes and traffic selectors.
 *
 * <p>IKE library will send out KE payload only if user has configured one or more DH groups. The KE
 * payload in a request will use the first DH group from the first user provided SA proposal (or the
 * peer selected SA proposal if it's a rekey request). The KE payload in a response will depend on
 * the SA proposal negotiation result.
 *
 * <p>When requesting the first Child Session in IKE AUTH, IKE library will not propose any DH group
 * even if user has configured it, as per RFC 7296. When rekeying this child session, IKE library
 * will accept DH groups that are configured in its ChildSessionParams. If after rekeying user needs
 * to have the same DH group as that of the IKE Session, then they need to explicitly set the same
 * DH Group in ChildSessionParams.
 *
 * @see {@link TunnelModeChildSessionParams} and {@link TransportModeChildSessionParams}
 * @hide
 */
@SystemApi
public abstract class ChildSessionParams {
    /** @hide */
    protected static final int CHILD_HARD_LIFETIME_SEC_MINIMUM = 300; // 5 minutes
    /** @hide */
    protected static final int CHILD_HARD_LIFETIME_SEC_MAXIMUM = 14400; // 4 hours
    /** @hide */
    protected static final int CHILD_HARD_LIFETIME_SEC_DEFAULT = 7200; // 2 hours

    /** @hide */
    protected static final int CHILD_SOFT_LIFETIME_SEC_MINIMUM = 120; // 2 minutes
    /** @hide */
    protected static final int CHILD_SOFT_LIFETIME_SEC_DEFAULT = 3600; // 1 hour

    /** @hide */
    protected static final int CHILD_LIFETIME_MARGIN_SEC_MINIMUM =
            (int) TimeUnit.MINUTES.toSeconds(1L);

    @NonNull private static final IkeTrafficSelector DEFAULT_TRAFFIC_SELECTOR_IPV4;
    @NonNull private static final IkeTrafficSelector DEFAULT_TRAFFIC_SELECTOR_IPV6;

    static {
        DEFAULT_TRAFFIC_SELECTOR_IPV4 =
                buildDefaultTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE);
        DEFAULT_TRAFFIC_SELECTOR_IPV6 =
                buildDefaultTrafficSelector(
                        IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV6_ADDR_RANGE);
    }

    @NonNull private final IkeTrafficSelector[] mInboundTrafficSelectors;
    @NonNull private final IkeTrafficSelector[] mOutboundTrafficSelectors;
    @NonNull private final ChildSaProposal[] mSaProposals;

    private final int mHardLifetimeSec;
    private final int mSoftLifetimeSec;

    private final boolean mIsTransport;

    /** @hide */
    protected ChildSessionParams(
            IkeTrafficSelector[] inboundTs,
            IkeTrafficSelector[] outboundTs,
            ChildSaProposal[] proposals,
            int hardLifetimeSec,
            int softLifetimeSec,
            boolean isTransport) {
        mInboundTrafficSelectors = inboundTs;
        mOutboundTrafficSelectors = outboundTs;
        mSaProposals = proposals;
        mHardLifetimeSec = hardLifetimeSec;
        mSoftLifetimeSec = softLifetimeSec;
        mIsTransport = isTransport;
    }

    /**
     * Retrieves configured inbound traffic selectors
     *
     * <p>@see {@link
     * TunnelModeChildSessionParams.Builder#addInboundTrafficSelectors(IkeTrafficSelector)} or
     * {@link
     * TransportModeChildSessionParams.Builder#addInboundTrafficSelectors(IkeTrafficSelector)}
     */
    @NonNull
    public List<IkeTrafficSelector> getInboundTrafficSelectors() {
        return Arrays.asList(mInboundTrafficSelectors);
    }

    /**
     * Retrieves configured outbound traffic selectors
     *
     * <p>@see {@link
     * TunnelModeChildSessionParams.Builder#addOutboundTrafficSelectors(IkeTrafficSelector)} or
     * {@link
     * TransportModeChildSessionParams.Builder#addOutboundTrafficSelectors(IkeTrafficSelector)}
     */
    @NonNull
    public List<IkeTrafficSelector> getOutboundTrafficSelectors() {
        return Arrays.asList(mOutboundTrafficSelectors);
    }

    /** Retrieves all ChildSaProposals configured */
    @NonNull
    public List<ChildSaProposal> getSaProposals() {
        return Arrays.asList(mSaProposals);
    }

    /** Retrieves hard lifetime in seconds */
    // Use "second" because smaller unit won't make sense to describe a rekey interval.
    @SuppressLint("MethodNameUnits")
    @IntRange(from = CHILD_HARD_LIFETIME_SEC_MINIMUM, to = CHILD_HARD_LIFETIME_SEC_MAXIMUM)
    public int getHardLifetimeSeconds() {
        return mHardLifetimeSec;
    }

    /** Retrieves soft lifetime in seconds */
    // Use "second" because smaller unit won't make sense to describe a rekey interval.
    @SuppressLint("MethodNameUnits")
    @IntRange(from = CHILD_SOFT_LIFETIME_SEC_MINIMUM, to = CHILD_HARD_LIFETIME_SEC_MAXIMUM)
    public int getSoftLifetimeSeconds() {
        return mSoftLifetimeSec;
    }

    /** @hide */
    public IkeTrafficSelector[] getInboundTrafficSelectorsInternal() {
        return mInboundTrafficSelectors;
    }

    /** @hide */
    public IkeTrafficSelector[] getOutboundTrafficSelectorsInternal() {
        return mOutboundTrafficSelectors;
    }

    /** @hide */
    public ChildSaProposal[] getSaProposalsInternal() {
        return mSaProposals;
    }

    /** @hide */
    public long getHardLifetimeMsInternal() {
        return TimeUnit.SECONDS.toMillis((long) mHardLifetimeSec);
    }

    /** @hide */
    public long getSoftLifetimeMsInternal() {
        return TimeUnit.SECONDS.toMillis((long) mSoftLifetimeSec);
    }

    /** @hide */
    public boolean isTransportMode() {
        return mIsTransport;
    }

    /**
     * This class represents common information for Child Session Parameters Builders.
     *
     * @hide
     */
    protected abstract static class Builder {
        @NonNull protected final List<IkeTrafficSelector> mInboundTsList = new LinkedList<>();
        @NonNull protected final List<IkeTrafficSelector> mOutboundTsList = new LinkedList<>();
        @NonNull protected final List<SaProposal> mSaProposalList = new LinkedList<>();

        protected int mHardLifetimeSec = CHILD_HARD_LIFETIME_SEC_DEFAULT;
        protected int mSoftLifetimeSec = CHILD_SOFT_LIFETIME_SEC_DEFAULT;

        protected void addProposal(@NonNull ChildSaProposal proposal) {
            mSaProposalList.add(proposal);
        }

        protected void addInboundTs(@NonNull IkeTrafficSelector trafficSelector) {
            mInboundTsList.add(trafficSelector);
        }

        protected void addOutboundTs(@NonNull IkeTrafficSelector trafficSelector) {
            mOutboundTsList.add(trafficSelector);
        }

        protected void validateAndSetLifetime(int hardLifetimeSec, int softLifetimeSec) {
            if (hardLifetimeSec < CHILD_HARD_LIFETIME_SEC_MINIMUM
                    || hardLifetimeSec > CHILD_HARD_LIFETIME_SEC_MAXIMUM
                    || softLifetimeSec < CHILD_SOFT_LIFETIME_SEC_MINIMUM
                    || hardLifetimeSec - softLifetimeSec < CHILD_LIFETIME_MARGIN_SEC_MINIMUM) {
                throw new IllegalArgumentException("Invalid lifetime value");
            }
        }

        protected void validateOrThrow() {
            if (mSaProposalList.isEmpty()) {
                throw new IllegalArgumentException(
                        "ChildSessionParams requires at least one Child SA proposal.");
            }
        }

        protected void addDefaultTsIfNotConfigured() {
            if (mInboundTsList.isEmpty()) {
                mInboundTsList.add(DEFAULT_TRAFFIC_SELECTOR_IPV4);
                mInboundTsList.add(DEFAULT_TRAFFIC_SELECTOR_IPV6);
            }

            if (mOutboundTsList.isEmpty()) {
                mOutboundTsList.add(DEFAULT_TRAFFIC_SELECTOR_IPV4);
                mOutboundTsList.add(DEFAULT_TRAFFIC_SELECTOR_IPV6);
            }
        }
    }

    private static IkeTrafficSelector buildDefaultTrafficSelector(
            @IkeTrafficSelector.TrafficSelectorType int tsType) {
        int startPort = IkeTrafficSelector.PORT_NUMBER_MIN;
        int endPort = IkeTrafficSelector.PORT_NUMBER_MAX;
        InetAddress startAddress = null;
        InetAddress endAddress = null;
        switch (tsType) {
            case IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV4_ADDR_RANGE:
                startAddress = InetAddresses.parseNumericAddress("0.0.0.0");
                endAddress = InetAddresses.parseNumericAddress("255.255.255.255");
                break;
            case IkeTrafficSelector.TRAFFIC_SELECTOR_TYPE_IPV6_ADDR_RANGE:
                startAddress = InetAddresses.parseNumericAddress("::");
                endAddress =
                        InetAddresses.parseNumericAddress(
                                "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
                break;
            default:
                throw new IllegalArgumentException("Invalid Traffic Selector type: " + tsType);
        }

        return new IkeTrafficSelector(tsType, startPort, endPort, startAddress, endAddress);
    }
}
