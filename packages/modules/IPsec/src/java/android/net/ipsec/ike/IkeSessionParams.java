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

import android.annotation.IntDef;
import android.annotation.IntRange;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.SuppressLint;
import android.annotation.SystemApi;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.eap.EapSessionConfig;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeIpv4Pcscf;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttributeIpv6Pcscf;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.IkeConfigAttribute;
import com.android.internal.net.ipsec.ike.message.IkePayload;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.security.PrivateKey;
import java.security.cert.TrustAnchor;
import java.security.cert.X509Certificate;
import java.security.interfaces.RSAPrivateKey;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.TimeUnit;

/**
 * IkeSessionParams contains all user provided configurations for negotiating an {@link IkeSession}.
 *
 * <p>Note that all negotiated configurations will be reused during rekey including SA Proposal and
 * lifetime.
 *
 * @hide
 */
@SystemApi
public final class IkeSessionParams {
    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({IKE_AUTH_METHOD_PSK, IKE_AUTH_METHOD_PUB_KEY_SIGNATURE, IKE_AUTH_METHOD_EAP})
    public @interface IkeAuthMethod {}

    // Constants to describe user configured authentication methods.
    /** @hide */
    public static final int IKE_AUTH_METHOD_PSK = 1;
    /** @hide */
    public static final int IKE_AUTH_METHOD_PUB_KEY_SIGNATURE = 2;
    /** @hide */
    public static final int IKE_AUTH_METHOD_EAP = 3;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({IKE_OPTION_ACCEPT_ANY_REMOTE_ID, IKE_OPTION_EAP_ONLY_AUTH})
    public @interface IkeOption {}

    /**
     * If set, the IKE library will accept any remote (server) identity, even if it does not match
     * the configured remote identity
     *
     * <p>See {@link Builder#setRemoteIdentification(IkeIdentification)}
     */
    public static final int IKE_OPTION_ACCEPT_ANY_REMOTE_ID = 0;
    /**
     * If set, and EAP has been configured as the authentication method, the IKE library will
     * request that the remote (also) use an EAP-only authentication flow.
     *
     * <p>@see {@link Builder#setAuthEap(X509Certificate, EapSessionConfig)}
     */
    public static final int IKE_OPTION_EAP_ONLY_AUTH = 1;

    private static final int MIN_IKE_OPTION = IKE_OPTION_ACCEPT_ANY_REMOTE_ID;
    private static final int MAX_IKE_OPTION = IKE_OPTION_EAP_ONLY_AUTH;

    /** @hide */
    @VisibleForTesting static final int IKE_HARD_LIFETIME_SEC_MINIMUM = 300; // 5 minutes
    /** @hide */
    @VisibleForTesting static final int IKE_HARD_LIFETIME_SEC_MAXIMUM = 86400; // 24 hours
    /** @hide */
    @VisibleForTesting static final int IKE_HARD_LIFETIME_SEC_DEFAULT = 14400; // 4 hours

    /** @hide */
    @VisibleForTesting static final int IKE_SOFT_LIFETIME_SEC_MINIMUM = 120; // 2 minutes
    /** @hide */
    @VisibleForTesting static final int IKE_SOFT_LIFETIME_SEC_DEFAULT = 7200; // 2 hours

    /** @hide */
    @VisibleForTesting
    static final int IKE_LIFETIME_MARGIN_SEC_MINIMUM = (int) TimeUnit.MINUTES.toSeconds(1L);

    /** @hide */
    @VisibleForTesting static final int IKE_DPD_DELAY_SEC_MIN = 20;
    /** @hide */
    @VisibleForTesting static final int IKE_DPD_DELAY_SEC_MAX = 1800; // 30 minutes
    /** @hide */
    @VisibleForTesting static final int IKE_DPD_DELAY_SEC_DEFAULT = 120; // 2 minutes

    /** @hide */
    @VisibleForTesting static final int IKE_RETRANS_TIMEOUT_MS_MIN = 500;
    /** @hide */
    @VisibleForTesting
    static final int IKE_RETRANS_TIMEOUT_MS_MAX = (int) TimeUnit.MINUTES.toMillis(30L);
    /** @hide */
    @VisibleForTesting static final int IKE_RETRANS_MAX_ATTEMPTS_MAX = 10;
    /** @hide */
    @VisibleForTesting
    static final int[] IKE_RETRANS_TIMEOUT_MS_LIST_DEFAULT =
            new int[] {500, 1000, 2000, 4000, 8000};

    @NonNull private final String mServerHostname;
    @NonNull private final Network mNetwork;

    @NonNull private final IkeSaProposal[] mSaProposals;

    @NonNull private final IkeIdentification mLocalIdentification;
    @NonNull private final IkeIdentification mRemoteIdentification;

    @NonNull private final IkeAuthConfig mLocalAuthConfig;
    @NonNull private final IkeAuthConfig mRemoteAuthConfig;

    @NonNull private final IkeConfigAttribute[] mConfigRequests;

    @NonNull private final int[] mRetransTimeoutMsList;

    private final long mIkeOptions;

    private final int mHardLifetimeSec;
    private final int mSoftLifetimeSec;

    private final int mDpdDelaySec;

    private final boolean mIsIkeFragmentationSupported;

    private IkeSessionParams(
            @NonNull String serverHostname,
            @NonNull Network network,
            @NonNull IkeSaProposal[] proposals,
            @NonNull IkeIdentification localIdentification,
            @NonNull IkeIdentification remoteIdentification,
            @NonNull IkeAuthConfig localAuthConfig,
            @NonNull IkeAuthConfig remoteAuthConfig,
            @NonNull IkeConfigAttribute[] configRequests,
            @NonNull int[] retransTimeoutMsList,
            long ikeOptions,
            int hardLifetimeSec,
            int softLifetimeSec,
            int dpdDelaySec,
            boolean isIkeFragmentationSupported) {
        mServerHostname = serverHostname;
        mNetwork = network;

        mSaProposals = proposals;

        mLocalIdentification = localIdentification;
        mRemoteIdentification = remoteIdentification;

        mLocalAuthConfig = localAuthConfig;
        mRemoteAuthConfig = remoteAuthConfig;

        mConfigRequests = configRequests;

        mRetransTimeoutMsList = retransTimeoutMsList;

        mIkeOptions = ikeOptions;

        mHardLifetimeSec = hardLifetimeSec;
        mSoftLifetimeSec = softLifetimeSec;

        mDpdDelaySec = dpdDelaySec;

        mIsIkeFragmentationSupported = isIkeFragmentationSupported;
    }

    private static void validateIkeOptionOrThrow(@IkeOption int ikeOption) {
        if (ikeOption < MIN_IKE_OPTION || ikeOption > MAX_IKE_OPTION) {
            throw new IllegalArgumentException("Invalid IKE Option: " + ikeOption);
        }
    }

    private static long getOptionBitValue(int ikeOption) {
        return 1 << ikeOption;
    }

    /**
     * Retrieves the configured server hostname
     *
     * <p>The configured server hostname will be resolved during IKE Session creation.
     */
    @NonNull
    public String getServerHostname() {
        return mServerHostname;
    }

    /** Retrieves the configured {@link Network} */
    @NonNull
    public Network getNetwork() {
        return mNetwork;
    }

    /** Retrieves all ChildSaProposals configured */
    @NonNull
    public List<IkeSaProposal> getSaProposals() {
        return Arrays.asList(mSaProposals);
    }

    /** @hide */
    public IkeSaProposal[] getSaProposalsInternal() {
        return mSaProposals;
    }

    /** Retrieves the local (client) identity */
    @NonNull
    public IkeIdentification getLocalIdentification() {
        return mLocalIdentification;
    }

    /** Retrieves the required remote (server) identity */
    @NonNull
    public IkeIdentification getRemoteIdentification() {
        return mRemoteIdentification;
    }

    /** Retrieves the local (client) authentication configuration */
    @NonNull
    public IkeAuthConfig getLocalAuthConfig() {
        return mLocalAuthConfig;
    }

    /** Retrieves the remote (server) authentication configuration */
    @NonNull
    public IkeAuthConfig getRemoteAuthConfig() {
        return mRemoteAuthConfig;
    }

    /** Retrieves hard lifetime in seconds */
    // Use "second" because smaller unit won't make sense to describe a rekey interval.
    @SuppressLint("MethodNameUnits")
    @IntRange(from = IKE_HARD_LIFETIME_SEC_MINIMUM, to = IKE_HARD_LIFETIME_SEC_MAXIMUM)
    public int getHardLifetimeSeconds() {
        return mHardLifetimeSec;
    }

    /** Retrieves soft lifetime in seconds */
    // Use "second" because smaller unit does not make sense to a rekey interval.
    @SuppressLint("MethodNameUnits")
    @IntRange(from = IKE_SOFT_LIFETIME_SEC_MINIMUM, to = IKE_HARD_LIFETIME_SEC_MAXIMUM)
    public int getSoftLifetimeSeconds() {
        return mSoftLifetimeSec;
    }

    /** Retrieves the Dead Peer Detection(DPD) delay in seconds */
    @IntRange(from = IKE_DPD_DELAY_SEC_MIN, to = IKE_DPD_DELAY_SEC_MAX)
    public int getDpdDelaySeconds() {
        return mDpdDelaySec;
    }

    /**
     * Retrieves the relative retransmission timeout list in milliseconds
     *
     * <p>@see {@link Builder#setRetransmissionTimeoutsMillis(int[])}
     */
    public int[] getRetransmissionTimeoutsMillis() {
        return mRetransTimeoutMsList;
    }

    /** Checks if the given IKE Session negotiation option is set */
    public boolean hasIkeOption(@IkeOption int ikeOption) {
        validateIkeOptionOrThrow(ikeOption);
        return (mIkeOptions & getOptionBitValue(ikeOption)) != 0;
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
    public boolean isIkeFragmentationSupported() {
        return mIsIkeFragmentationSupported;
    }

    /** @hide */
    public IkeConfigAttribute[] getConfigurationAttributesInternal() {
        return mConfigRequests;
    }

    /** Retrieves the list of Configuration Requests */
    @NonNull
    public List<IkeConfigRequest> getConfigurationRequests() {
        return Collections.unmodifiableList(Arrays.asList(mConfigRequests));
    }

    /** Represents an IKE session configuration request type */
    public interface IkeConfigRequest {}

    /** Represents an IPv4 P_CSCF request */
    public interface ConfigRequestIpv4PcscfServer extends IkeConfigRequest {
        /**
         * Retrieves the requested IPv4 P_CSCF server address
         *
         * @return The requested P_CSCF server address, or null if no specific P_CSCF server was
         *     requested
         */
        @Nullable
        Inet4Address getAddress();
    }

    /** Represents an IPv6 P_CSCF request */
    public interface ConfigRequestIpv6PcscfServer extends IkeConfigRequest {
        /**
         * Retrieves the requested IPv6 P_CSCF server address
         *
         * @return The requested P_CSCF server address, or null if no specific P_CSCF server was
         *     requested
         */
        @Nullable
        Inet6Address getAddress();
    }

    /** This class contains common information of an IKEv2 authentication configuration. */
    public abstract static class IkeAuthConfig {
        /** @hide */
        @IkeAuthMethod public final int mAuthMethod;

        /** @hide */
        IkeAuthConfig(@IkeAuthMethod int authMethod) {
            mAuthMethod = authMethod;
        }
    }

    /**
     * This class represents the configuration to support IKEv2 pre-shared-key-based authentication
     * of local or remote side.
     */
    public static class IkeAuthPskConfig extends IkeAuthConfig {
        /** @hide */
        @NonNull public final byte[] mPsk;

        private IkeAuthPskConfig(byte[] psk) {
            super(IKE_AUTH_METHOD_PSK);
            mPsk = psk;
        }

        /** Retrieves the pre-shared key */
        @NonNull
        public byte[] getPsk() {
            return Arrays.copyOf(mPsk, mPsk.length);
        }
    }

    /**
     * This class represents the configuration to support IKEv2 public-key-signature-based
     * authentication of the remote side.
     */
    public static class IkeAuthDigitalSignRemoteConfig extends IkeAuthConfig {
        /** @hide */
        @Nullable public final TrustAnchor mTrustAnchor;

        /**
         * If a certificate is provided, it MUST be the root CA used by the remote (server), or
         * authentication will fail. If no certificate is provided, any root CA in the system's
         * truststore is considered acceptable.
         */
        private IkeAuthDigitalSignRemoteConfig(@Nullable X509Certificate caCert) {
            super(IKE_AUTH_METHOD_PUB_KEY_SIGNATURE);
            if (caCert == null) {
                mTrustAnchor = null;
            } else {
                // The name constraints extension, defined in RFC 5280, indicates a name space
                // within which all subject names in subsequent certificates in a certification path
                // MUST be located.
                mTrustAnchor = new TrustAnchor(caCert, null /*nameConstraints*/);

                // TODO: Investigate if we need to support the name constraints extension.
            }
        }

        /** Retrieves the provided CA certificate for validating the remote certificate(s) */
        @Nullable
        public X509Certificate getRemoteCaCert() {
            if (mTrustAnchor == null) return null;
            return mTrustAnchor.getTrustedCert();
        }
    }

    /**
     * This class represents the configuration to support IKEv2 public-key-signature-based
     * authentication of the local side.
     */
    public static class IkeAuthDigitalSignLocalConfig extends IkeAuthConfig {
        /** @hide */
        @NonNull public final X509Certificate mEndCert;

        /** @hide */
        @NonNull public final List<X509Certificate> mIntermediateCerts;

        /** @hide */
        @NonNull public final PrivateKey mPrivateKey;

        private IkeAuthDigitalSignLocalConfig(
                @NonNull X509Certificate clientEndCert,
                @NonNull List<X509Certificate> clientIntermediateCerts,
                @NonNull PrivateKey privateKey) {
            super(IKE_AUTH_METHOD_PUB_KEY_SIGNATURE);
            mEndCert = clientEndCert;
            mIntermediateCerts = clientIntermediateCerts;
            mPrivateKey = privateKey;
        }

        /** Retrieves the client end certificate */
        @NonNull
        public X509Certificate getClientEndCertificate() {
            return mEndCert;
        }

        /** Retrieves the intermediate certificates */
        @NonNull
        public List<X509Certificate> getIntermediateCertificates() {
            return mIntermediateCerts;
        }

        /** Retrieves the private key */
        @NonNull
        public PrivateKey getPrivateKey() {
            return mPrivateKey;
        }
    }

    /**
     * This class represents the configuration to support EAP authentication of the local side.
     *
     * <p>@see {@link IkeSessionParams.Builder#setAuthEap(X509Certificate, EapSessionConfig)}
     */
    public static class IkeAuthEapConfig extends IkeAuthConfig {
        /** @hide */
        @NonNull public final EapSessionConfig mEapConfig;

        private IkeAuthEapConfig(EapSessionConfig eapConfig) {
            super(IKE_AUTH_METHOD_EAP);

            mEapConfig = eapConfig;
        }

        /** Retrieves EAP configuration */
        @NonNull
        public EapSessionConfig getEapConfig() {
            return mEapConfig;
        }
    }

    /** This class can be used to incrementally construct a {@link IkeSessionParams}. */
    public static final class Builder {
        @NonNull private final ConnectivityManager mConnectivityManager;

        @NonNull private final List<IkeSaProposal> mSaProposalList = new LinkedList<>();
        @NonNull private final List<IkeConfigAttribute> mConfigRequestList = new ArrayList<>();

        @NonNull
        private int[] mRetransTimeoutMsList =
                Arrays.copyOf(
                        IKE_RETRANS_TIMEOUT_MS_LIST_DEFAULT,
                        IKE_RETRANS_TIMEOUT_MS_LIST_DEFAULT.length);

        @NonNull private String mServerHostname;
        @Nullable private Network mNetwork;

        @Nullable private IkeIdentification mLocalIdentification;
        @Nullable private IkeIdentification mRemoteIdentification;

        @Nullable private IkeAuthConfig mLocalAuthConfig;
        @Nullable private IkeAuthConfig mRemoteAuthConfig;

        private long mIkeOptions = 0;

        private int mHardLifetimeSec = IKE_HARD_LIFETIME_SEC_DEFAULT;
        private int mSoftLifetimeSec = IKE_SOFT_LIFETIME_SEC_DEFAULT;

        private int mDpdDelaySec = IKE_DPD_DELAY_SEC_DEFAULT;

        private boolean mIsIkeFragmentationSupported = false;

        /**
         * Construct Builder
         *
         * @param context a valid {@link Context} instance.
         */
        public Builder(@NonNull Context context) {
            this((ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE));
        }

        /** @hide */
        @VisibleForTesting
        public Builder(ConnectivityManager connectManager) {
            mConnectivityManager = connectManager;
        }

        /**
         * Sets the server hostname for the {@link IkeSessionParams} being built.
         *
         * @param serverHostname the hostname of the IKE server, such as "ike.android.com".
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setServerHostname(@NonNull String serverHostname) {
            Objects.requireNonNull(serverHostname, "Required argument not provided");

            mServerHostname = serverHostname;
            return this;
        }

        /**
         * Sets the {@link Network} for the {@link IkeSessionParams} being built.
         *
         * <p>If no {@link Network} is provided, the default Network (as per {@link
         * ConnectivityManager#getActiveNetwork()}) will be used.
         *
         * @param network the {@link Network} that IKE Session will use.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setNetwork(@NonNull Network network) {
            if (network == null) {
                throw new NullPointerException("Required argument not provided");
            }

            mNetwork = network;
            return this;
        }

        /**
         * Sets local IKE identification for the {@link IkeSessionParams} being built.
         *
         * <p>It is not allowed to use KEY ID together with digital-signature-based authentication
         * as per RFC 7296.
         *
         * @param identification the local IKE identification.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setLocalIdentification(@NonNull IkeIdentification identification) {
            if (identification == null) {
                throw new NullPointerException("Required argument not provided");
            }

            mLocalIdentification = identification;
            return this;
        }

        /**
         * Sets remote IKE identification for the {@link IkeSessionParams} being built.
         *
         * @param identification the remote IKE identification.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setRemoteIdentification(@NonNull IkeIdentification identification) {
            if (identification == null) {
                throw new NullPointerException("Required argument not provided");
            }

            mRemoteIdentification = identification;
            return this;
        }

        /**
         * Adds an IKE SA proposal to the {@link IkeSessionParams} being built.
         *
         * @param proposal IKE SA proposal.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addSaProposal(@NonNull IkeSaProposal proposal) {
            if (proposal == null) {
                throw new NullPointerException("Required argument not provided");
            }

            if (proposal.getProtocolId() != IkePayload.PROTOCOL_ID_IKE) {
                throw new IllegalArgumentException(
                        "Expected IKE SA Proposal but received Child SA proposal");
            }
            mSaProposalList.add(proposal);
            return this;
        }

        /**
         * Configures the {@link IkeSession} to use pre-shared-key-based authentication.
         *
         * <p>Both client and server MUST be authenticated using the provided shared key. IKE
         * authentication will fail if the remote peer tries to use other authentication methods.
         *
         * <p>Callers MUST declare only one authentication method. Calling this function will
         * override the previously set authentication configuration.
         *
         * <p>Callers SHOULD NOT use this if any other authentication methods can be used; PSK-based
         * authentication is generally considered insecure.
         *
         * @param sharedKey the shared key.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setAuthPsk(@NonNull byte[] sharedKey) {
            if (sharedKey == null) {
                throw new NullPointerException("Required argument not provided");
            }

            mLocalAuthConfig = new IkeAuthPskConfig(sharedKey);
            mRemoteAuthConfig = new IkeAuthPskConfig(sharedKey);
            return this;
        }

        /**
         * Configures the {@link IkeSession} to use EAP authentication.
         *
         * <p>Not all EAP methods provide mutual authentication. As such EAP MUST be used in
         * conjunction with a public-key-signature-based authentication of the remote server, unless
         * EAP-Only authentication is enabled.
         *
         * <p>Callers may enable EAP-Only authentication by setting {@link
         * IKE_OPTION_EAP_ONLY_AUTH}, which will make IKE library request the remote to use EAP-Only
         * authentication. The remote may opt to reject the request, at which point the received
         * certificates and authentication payload WILL be validated with the provided root CA or
         * system's truststore as usual. Only safe EAP methods as listed in RFC 5998 will be
         * accepted for EAP-Only authentication.
         *
         * <p>If {@link IKE_OPTION_EAP_ONLY_AUTH} is set, callers MUST configure EAP as the
         * authentication method and all EAP methods set in EAP Session configuration MUST be safe
         * methods that are accepted for EAP-Only authentication. Otherwise callers will get an
         * exception when building the {@link IkeSessionParams}
         *
         * <p>Callers MUST declare only one authentication method. Calling this function will
         * override the previously set authentication configuration.
         *
         * @see <a href="https://tools.ietf.org/html/rfc5280">RFC 5280, Internet X.509 Public Key
         *     Infrastructure Certificate and Certificate Revocation List (CRL) Profile</a>
         * @see <a href="https://tools.ietf.org/html/rfc5998">RFC 5998, An Extension for EAP-Only
         *     Authentication in IKEv2
         * @param serverCaCert the CA certificate for validating the received server certificate(s).
         *     If a certificate is provided, it MUST be the root CA used by the server, or
         *     authentication will fail. If no certificate is provided, any root CA in the system's
         *     truststore is considered acceptable.
         * @return Builder this, to facilitate chaining.
         */
        // TODO(b/151667921): Consider also supporting configuring EAP method that is not accepted
        // by EAP-Only when {@link IKE_OPTION_EAP_ONLY_AUTH} is set
        @NonNull
        public Builder setAuthEap(
                @Nullable X509Certificate serverCaCert, @NonNull EapSessionConfig eapConfig) {
            if (eapConfig == null) {
                throw new NullPointerException("Required argument not provided");
            }

            mLocalAuthConfig = new IkeAuthEapConfig(eapConfig);
            mRemoteAuthConfig = new IkeAuthDigitalSignRemoteConfig(serverCaCert);

            return this;
        }

        /**
         * Configures the {@link IkeSession} to use public-key-signature-based authentication.
         *
         * <p>The public key included by the client end certificate and the private key used for
         * signing MUST be a matching key pair.
         *
         * <p>The IKE library will use the strongest signature algorithm supported by both sides.
         *
         * <p>Currenly only RSA digital signature is supported.
         *
         * @param serverCaCert the CA certificate for validating the received server certificate(s).
         *     If a certificate is provided, it MUST be the root CA used by the server, or
         *     authentication will fail. If no certificate is provided, any root CA in the system's
         *     truststore is considered acceptable.
         * @param clientEndCert the end certificate for remote server to verify the locally
         *     generated signature.
         * @param clientPrivateKey private key to generate outbound digital signature. Only {@link
         *     RSAPrivateKey} is supported.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setAuthDigitalSignature(
                @Nullable X509Certificate serverCaCert,
                @NonNull X509Certificate clientEndCert,
                @NonNull PrivateKey clientPrivateKey) {
            return setAuthDigitalSignature(
                    serverCaCert,
                    clientEndCert,
                    new LinkedList<X509Certificate>(),
                    clientPrivateKey);
        }

        /**
         * Configures the {@link IkeSession} to use public-key-signature-based authentication.
         *
         * <p>The public key included by the client end certificate and the private key used for
         * signing MUST be a matching key pair.
         *
         * <p>The IKE library will use the strongest signature algorithm supported by both sides.
         *
         * <p>Currenly only RSA digital signature is supported.
         *
         * @param serverCaCert the CA certificate for validating the received server certificate(s).
         *     If a null value is provided, IKE library will try all default CA certificates stored
         *     in Android system to do the validation. Otherwise, it will only use the provided CA
         *     certificate.
         * @param clientEndCert the end certificate for remote server to verify locally generated
         *     signature.
         * @param clientIntermediateCerts intermediate certificates for the remote server to
         *     validate the end certificate.
         * @param clientPrivateKey private key to generate outbound digital signature. Only {@link
         *     RSAPrivateKey} is supported.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setAuthDigitalSignature(
                @Nullable X509Certificate serverCaCert,
                @NonNull X509Certificate clientEndCert,
                @NonNull List<X509Certificate> clientIntermediateCerts,
                @NonNull PrivateKey clientPrivateKey) {
            if (clientEndCert == null
                    || clientIntermediateCerts == null
                    || clientPrivateKey == null) {
                throw new NullPointerException("Required argument not provided");
            }

            if (!(clientPrivateKey instanceof RSAPrivateKey)) {
                throw new IllegalArgumentException("Unsupported private key type");
            }

            mLocalAuthConfig =
                    new IkeAuthDigitalSignLocalConfig(
                            clientEndCert, clientIntermediateCerts, clientPrivateKey);
            mRemoteAuthConfig = new IkeAuthDigitalSignRemoteConfig(serverCaCert);

            return this;
        }

        /**
         * Adds a specific internal P_CSCF server request to the {@link IkeSessionParams} being
         * built.
         *
         * @param address the requested P_CSCF address.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addPcscfServerRequest(@NonNull InetAddress address) {
            if (address == null) {
                throw new NullPointerException("Required argument not provided");
            }

            if (address instanceof Inet4Address) {
                mConfigRequestList.add(new ConfigAttributeIpv4Pcscf((Inet4Address) address));
            } else if (address instanceof Inet6Address) {
                mConfigRequestList.add(new ConfigAttributeIpv6Pcscf((Inet6Address) address));
            } else {
                throw new IllegalArgumentException("Invalid address family");
            }
            return this;
        }

        /**
         * Adds a internal P_CSCF server request to the {@link IkeSessionParams} being built.
         *
         * @param addressFamily the address family. Only {@link OsConstants.AF_INET} and {@link
         *     OsConstants.AF_INET6} are allowed.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addPcscfServerRequest(int addressFamily) {
            if (addressFamily == AF_INET) {
                mConfigRequestList.add(new ConfigAttributeIpv4Pcscf());
                return this;
            } else if (addressFamily == AF_INET6) {
                mConfigRequestList.add(new ConfigAttributeIpv6Pcscf());
                return this;
            } else {
                throw new IllegalArgumentException("Invalid address family: " + addressFamily);
            }
        }

        /**
         * Sets hard and soft lifetimes.
         *
         * <p>Lifetimes will not be negotiated with the remote IKE server.
         *
         * @param hardLifetimeSeconds number of seconds after which IKE SA will expire. Defaults to
         *     14400 seconds (4 hours). MUST be a value from 300 seconds (5 minutes) to 86400
         *     seconds (24 hours), inclusive.
         * @param softLifetimeSeconds number of seconds after which IKE SA will request rekey.
         *     Defaults to 7200 seconds (2 hours). MUST be at least 120 seconds (2 minutes), and at
         *     least 60 seconds (1 minute) shorter than the hard lifetime.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setLifetimeSeconds(
                @IntRange(from = IKE_HARD_LIFETIME_SEC_MINIMUM, to = IKE_HARD_LIFETIME_SEC_MAXIMUM)
                        int hardLifetimeSeconds,
                @IntRange(from = IKE_SOFT_LIFETIME_SEC_MINIMUM, to = IKE_HARD_LIFETIME_SEC_MAXIMUM)
                        int softLifetimeSeconds) {
            if (hardLifetimeSeconds < IKE_HARD_LIFETIME_SEC_MINIMUM
                    || hardLifetimeSeconds > IKE_HARD_LIFETIME_SEC_MAXIMUM
                    || softLifetimeSeconds < IKE_SOFT_LIFETIME_SEC_MINIMUM
                    || hardLifetimeSeconds - softLifetimeSeconds
                            < IKE_LIFETIME_MARGIN_SEC_MINIMUM) {
                throw new IllegalArgumentException("Invalid lifetime value");
            }

            mHardLifetimeSec = hardLifetimeSeconds;
            mSoftLifetimeSec = softLifetimeSeconds;
            return this;
        }

        /**
         * Sets the Dead Peer Detection(DPD) delay in seconds.
         *
         * @param dpdDelaySeconds number of seconds after which IKE SA will initiate DPD if no
         *     inbound cryptographically protected IKE message was received. Defaults to 120
         *     seconds. MUST be a value from 20 seconds to 1800 seconds, inclusive.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setDpdDelaySeconds(
                @IntRange(from = IKE_DPD_DELAY_SEC_MIN, to = IKE_DPD_DELAY_SEC_MAX)
                        int dpdDelaySeconds) {
            if (dpdDelaySeconds < IKE_DPD_DELAY_SEC_MIN
                    || dpdDelaySeconds > IKE_DPD_DELAY_SEC_MAX) {
                throw new IllegalArgumentException("Invalid DPD delay value");
            }
            mDpdDelaySec = dpdDelaySeconds;
            return this;
        }

        /**
         * Sets the retransmission timeout list in milliseconds.
         *
         * <p>Configures the retransmission by providing an array of relative retransmission
         * timeouts in milliseconds, where each timeout is the waiting time before next retry,
         * except the last timeout is the waiting time before terminating the IKE Session. Each
         * element in the array MUST be a value from 500 ms to 1800000 ms (30 minutes). The length
         * of the array MUST NOT exceed 10. This retransmission timeout list defaults to {0.5s, 1s,
         * 2s, 4s, 8s}
         *
         * @param retransTimeoutMillisList the array of relative retransmission timeout in
         *     milliseconds.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setRetransmissionTimeoutsMillis(@NonNull int[] retransTimeoutMillisList) {
            boolean isValid = true;
            if (retransTimeoutMillisList == null
                    || retransTimeoutMillisList.length == 0
                    || retransTimeoutMillisList.length > IKE_RETRANS_MAX_ATTEMPTS_MAX) {
                isValid = false;
            }
            for (int t : retransTimeoutMillisList) {
                if (t < IKE_RETRANS_TIMEOUT_MS_MIN || t > IKE_RETRANS_TIMEOUT_MS_MAX) {
                    isValid = false;
                }
            }
            if (!isValid) throw new IllegalArgumentException("Invalid retransmission timeout list");

            mRetransTimeoutMsList = retransTimeoutMillisList;
            return this;
        }

        /**
         * Sets the specified IKE Option as enabled.
         *
         * @param ikeOption the option to be enabled.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder addIkeOption(@IkeOption int ikeOption) {
            validateIkeOptionOrThrow(ikeOption);
            mIkeOptions |= getOptionBitValue(ikeOption);
            return this;
        }

        /**
         * Resets (disables) the specified IKE Option.
         *
         * @param ikeOption the option to be disabled.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder removeIkeOption(@IkeOption int ikeOption) {
            validateIkeOptionOrThrow(ikeOption);
            mIkeOptions &= ~getOptionBitValue(ikeOption);
            return this;
        }

        /**
         * Validates and builds the {@link IkeSessionParams}.
         *
         * @return IkeSessionParams the validated IkeSessionParams.
         */
        @NonNull
        public IkeSessionParams build() {
            if (mSaProposalList.isEmpty()) {
                throw new IllegalArgumentException("IKE SA proposal not found");
            }

            Network network = mNetwork != null ? mNetwork : mConnectivityManager.getActiveNetwork();
            if (network == null) {
                throw new IllegalArgumentException("Network not found");
            }

            if (mServerHostname == null
                    || mLocalIdentification == null
                    || mRemoteIdentification == null
                    || mLocalAuthConfig == null
                    || mRemoteAuthConfig == null) {
                throw new IllegalArgumentException("Necessary parameter missing.");
            }

            if ((mIkeOptions & getOptionBitValue(IKE_OPTION_EAP_ONLY_AUTH)) != 0) {
                if (!(mLocalAuthConfig instanceof IkeAuthEapConfig)) {
                    throw new IllegalArgumentException(
                            "If IKE_OPTION_EAP_ONLY_AUTH is set,"
                                    + " eap authentication needs to be configured.");
                }

                IkeAuthEapConfig ikeAuthEapConfig = (IkeAuthEapConfig) mLocalAuthConfig;
                if (!ikeAuthEapConfig.getEapConfig().areAllMethodsEapOnlySafe()) {
                    throw new IllegalArgumentException(
                            "Only EAP-only safe method allowed" + " when using EAP-only option.");
                }
            }

            if (mLocalAuthConfig.mAuthMethod == IKE_AUTH_METHOD_PUB_KEY_SIGNATURE
                    && mLocalIdentification.idType == IkeIdentification.ID_TYPE_KEY_ID) {
                throw new IllegalArgumentException(
                        "It is not allowed to use KEY_ID as local ID when local authentication"
                                + " method is digital-signature-based");
            }

            return new IkeSessionParams(
                    mServerHostname,
                    network,
                    mSaProposalList.toArray(new IkeSaProposal[0]),
                    mLocalIdentification,
                    mRemoteIdentification,
                    mLocalAuthConfig,
                    mRemoteAuthConfig,
                    mConfigRequestList.toArray(new IkeConfigAttribute[0]),
                    mRetransTimeoutMsList,
                    mIkeOptions,
                    mHardLifetimeSec,
                    mSoftLifetimeSec,
                    mDpdDelaySec,
                    mIsIkeFragmentationSupported);
        }

        // TODO: add methods for supporting IKE fragmentation.
    }
}
