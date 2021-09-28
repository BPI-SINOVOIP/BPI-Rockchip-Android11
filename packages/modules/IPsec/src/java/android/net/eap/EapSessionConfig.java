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

package android.net.eap;

import static com.android.internal.net.eap.message.EapData.EAP_TYPE_AKA;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_AKA_PRIME;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_MSCHAP_V2;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_SIM;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.SystemApi;
import android.telephony.Annotation.UiccAppType;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.net.eap.message.EapData.EapMethod;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * EapSessionConfig represents a container for EAP method configuration.
 *
 * <p>The EAP authentication server decides which EAP method is used, so clients are encouraged to
 * provide configs for several EAP methods.
 *
 * @hide
 */
@SystemApi
public final class EapSessionConfig {
    /** @hide */
    @VisibleForTesting static final byte[] DEFAULT_IDENTITY = new byte[0];

    // IANA -> EapMethodConfig for that method
    /** @hide */
    public final Map<Integer, EapMethodConfig> eapConfigs;
    /** @hide */
    public final byte[] eapIdentity;

    /** @hide */
    @VisibleForTesting
    public EapSessionConfig(Map<Integer, EapMethodConfig> eapConfigs, byte[] eapIdentity) {
        this.eapConfigs = Collections.unmodifiableMap(eapConfigs);
        this.eapIdentity = eapIdentity;
    }

    /** Retrieves client's EAP Identity */
    @NonNull
    public byte[] getEapIdentity() {
        return eapIdentity;
    }

    /**
     * Retrieves configuration for EAP SIM
     *
     * @return the configuration for EAP SIM, or null if it was not set
     */
    @Nullable
    public EapSimConfig getEapSimConfig() {
        return (EapSimConfig) eapConfigs.get(EAP_TYPE_SIM);
    }

    /**
     * Retrieves configuration for EAP AKA
     *
     * @return the configuration for EAP AKA, or null if it was not set
     */
    @Nullable
    public EapAkaConfig getEapAkaConfig() {
        return (EapAkaConfig) eapConfigs.get(EAP_TYPE_AKA);
    }

    /**
     * Retrieves configuration for EAP AKA'
     *
     * @return the configuration for EAP AKA', or null if it was not set
     */
    @Nullable
    public EapAkaPrimeConfig getEapAkaPrimeConfig() {
        return (EapAkaPrimeConfig) eapConfigs.get(EAP_TYPE_AKA_PRIME);
    }

    /**
     * Retrieves configuration for EAP MSCHAPV2
     *
     * @return the configuration for EAP MSCHAPV2, or null if it was not set
     */
    @Nullable
    public EapMsChapV2Config getEapMsChapV2onfig() {
        return (EapMsChapV2Config) eapConfigs.get(EAP_TYPE_MSCHAP_V2);
    }

    /** This class can be used to incrementally construct an {@link EapSessionConfig}. */
    public static final class Builder {
        private final Map<Integer, EapMethodConfig> mEapConfigs;
        private byte[] mEapIdentity;

        /** Constructs and returns a new Builder for constructing an {@link EapSessionConfig}. */
        public Builder() {
            mEapConfigs = new HashMap<>();
            mEapIdentity = DEFAULT_IDENTITY;
        }

        /**
         * Sets the client's EAP Identity.
         *
         * @param eapIdentity byte[] representing the client's EAP Identity.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setEapIdentity(@NonNull byte[] eapIdentity) {
            this.mEapIdentity = eapIdentity.clone();
            return this;
        }

        /**
         * Sets the configuration for EAP SIM.
         *
         * @param subId int the client's subId to be authenticated.
         * @param apptype the {@link UiccAppType} apptype to be used for authentication.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setEapSimConfig(int subId, @UiccAppType int apptype) {
            mEapConfigs.put(EAP_TYPE_SIM, new EapSimConfig(subId, apptype));
            return this;
        }

        /**
         * Sets the configuration for EAP AKA.
         *
         * @param subId int the client's subId to be authenticated.
         * @param apptype the {@link UiccAppType} apptype to be used for authentication.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setEapAkaConfig(int subId, @UiccAppType int apptype) {
            mEapConfigs.put(EAP_TYPE_AKA, new EapAkaConfig(subId, apptype));
            return this;
        }

        /**
         * Sets the configuration for EAP AKA'.
         *
         * @param subId int the client's subId to be authenticated.
         * @param apptype the {@link UiccAppType} apptype to be used for authentication.
         * @param networkName String the network name to be used for authentication.
         * @param allowMismatchedNetworkNames indicates whether the EAP library can ignore potential
         *     mismatches between the given network name and that received in an EAP-AKA' session.
         *     If false, mismatched network names will be handled as an Authentication Reject
         *     message.
         * @return Builder this, to facilitate chaining.
         */
        @NonNull
        public Builder setEapAkaPrimeConfig(
                int subId,
                @UiccAppType int apptype,
                @NonNull String networkName,
                boolean allowMismatchedNetworkNames) {
            mEapConfigs.put(
                    EAP_TYPE_AKA_PRIME,
                    new EapAkaPrimeConfig(
                            subId, apptype, networkName, allowMismatchedNetworkNames));
            return this;
        }

        /**
         * Sets the configuration for EAP MSCHAPv2.
         *
         * @param username String the client account's username to be authenticated.
         * @param password String the client account's password to be authenticated.
         * @return Builder this, to faciliate chaining.
         */
        @NonNull
        public Builder setEapMsChapV2Config(@NonNull String username, @NonNull String password) {
            mEapConfigs.put(EAP_TYPE_MSCHAP_V2, new EapMsChapV2Config(username, password));
            return this;
        }

        /**
         * Constructs and returns an EapSessionConfig with the configurations applied to this
         * Builder.
         *
         * @return the EapSessionConfig constructed by this Builder.
         */
        @NonNull
        public EapSessionConfig build() {
            if (mEapConfigs.isEmpty()) {
                throw new IllegalStateException("Must have at least one EAP method configured");
            }

            return new EapSessionConfig(mEapConfigs, mEapIdentity);
        }
    }

    /**
     * EapMethodConfig represents a generic EAP method configuration.
     */
    public abstract static class EapMethodConfig {
        /** @hide */
        @EapMethod public final int methodType;

        /** @hide */
        EapMethodConfig(@EapMethod int methodType) {
            this.methodType = methodType;
        }

        /**
         * Retrieves the EAP method type
         *
         * @return the IANA-defined EAP method constant
         */
        public int getMethodType() {
            return methodType;
        }

        /**
         * Check if this is EAP-only safe method.
         *
         * @return whether the method is EAP-only safe
         *
         * @see <a href="https://tools.ietf.org/html/rfc5998">RFC 5998#section 4, for safe eap
         * methods</a>
         *
         * @hide
         */
        public boolean isEapOnlySafeMethod() {
            return false;
        }
    }

    /**
     * EapUiccConfig represents the configs needed for EAP methods that rely on UICC cards for
     * authentication.
     */
    public abstract static class EapUiccConfig extends EapMethodConfig {
        /** @hide */
        public final int subId;
        /** @hide */
        public final int apptype;

        private EapUiccConfig(@EapMethod int methodType, int subId, @UiccAppType int apptype) {
            super(methodType);
            this.subId = subId;
            this.apptype = apptype;
        }

        /**
         * Retrieves the subId
         *
         * @return the subId
         */
        public int getSubId() {
            return subId;
        }

        /**
         * Retrieves the UICC app type
         *
         * @return the {@link UiccAppType} constant
         */
        public int getAppType() {
            return apptype;
        }

        /** @hide */
        @Override
        public boolean isEapOnlySafeMethod() {
            return true;
        }
    }

    /**
     * EapSimConfig represents the configs needed for an EAP SIM session.
     */
    public static class EapSimConfig extends EapUiccConfig {
        /** @hide */
        @VisibleForTesting
        public EapSimConfig(int subId, @UiccAppType int apptype) {
            super(EAP_TYPE_SIM, subId, apptype);
        }
    }

    /**
     * EapAkaConfig represents the configs needed for an EAP AKA session.
     */
    public static class EapAkaConfig extends EapUiccConfig {
        /** @hide */
        @VisibleForTesting
        public EapAkaConfig(int subId, @UiccAppType int apptype) {
            this(EAP_TYPE_AKA, subId, apptype);
        }

        /** @hide */
        EapAkaConfig(int methodType, int subId, @UiccAppType int apptype) {
            super(methodType, subId, apptype);
        }
    }

    /**
     * EapAkaPrimeConfig represents the configs needed for an EAP-AKA' session.
     */
    public static class EapAkaPrimeConfig extends EapAkaConfig {
        /** @hide */
        @NonNull public final String networkName;
        /** @hide */
        public final boolean allowMismatchedNetworkNames;

        /** @hide */
        @VisibleForTesting
        public EapAkaPrimeConfig(
                int subId,
                @UiccAppType int apptype,
                @NonNull String networkName,
                boolean allowMismatchedNetworkNames) {
            super(EAP_TYPE_AKA_PRIME, subId, apptype);

            if (networkName == null) {
                throw new IllegalArgumentException("NetworkName was null");
            }

            this.networkName = networkName;
            this.allowMismatchedNetworkNames = allowMismatchedNetworkNames;
        }

        /**
         * Retrieves the UICC app type
         *
         * @return the {@link UiccAppType} constant
         */
        @NonNull
        public String getNetworkName() {
            return networkName;
        }

        /**
         * Checks if mismatched network names are allowed
         *
         * @return whether network name mismatches are allowed
         */
        public boolean allowsMismatchedNetworkNames() {
            return allowMismatchedNetworkNames;
        }
    }

    /**
     * EapMsChapV2Config represents the configs needed for an EAP MSCHAPv2 session.
     */
    public static class EapMsChapV2Config extends EapMethodConfig {
        /** @hide */
        @NonNull public final String username;
        /** @hide */
        @NonNull public final String password;

        /** @hide */
        @VisibleForTesting
        public EapMsChapV2Config(String username, String password) {
            super(EAP_TYPE_MSCHAP_V2);

            if (username == null || password == null) {
                throw new IllegalArgumentException("Username or password was null");
            }

            this.username = username;
            this.password = password;
        }

        /**
         * Retrieves the username
         *
         * @return the username to be used by MSCHAPV2
         */
        @NonNull
        public String getUsername() {
            return username;
        }

        /**
         * Retrieves the password
         *
         * @return the password to be used by MSCHAPV2
         */
        @NonNull
        public String getPassword() {
            return password;
        }
    }

    /**
     * Checks if all the methods in the session are EAP-only safe
     *
     * @return whether all the methods in the session are EAP-only safe
     *
     * @see <a href="https://tools.ietf.org/html/rfc5998">RFC 5998#section 4, for safe eap
     * methods</a>
     *
     * @hide
     */
    public boolean areAllMethodsEapOnlySafe() {
        for(Map.Entry<Integer, EapMethodConfig> eapConfigsEntry : eapConfigs.entrySet()) {
            if (!eapConfigsEntry.getValue().isEapOnlySafeMethod()) {
                return false;
            }
        }

        return true;
    }
}
