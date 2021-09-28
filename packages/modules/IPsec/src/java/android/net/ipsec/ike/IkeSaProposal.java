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

import android.annotation.NonNull;
import android.annotation.SystemApi;
import android.util.ArraySet;

import com.android.internal.net.ipsec.ike.message.IkePayload;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.DhGroupTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.EncryptionTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IntegrityTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.PrfTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.Transform;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/**
 * IkeSaProposal represents a proposed configuration to negotiate an IKE SA.
 *
 * <p>IkeSaProposal will contain cryptograhic algorithms and key generation materials for the
 * negotiation of an IKE SA.
 *
 * <p>User must provide at least one valid IkeSaProposal when they are creating a new IKE SA.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#section-3.3">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 * @hide
 */
@SystemApi
public final class IkeSaProposal extends SaProposal {
    private final PrfTransform[] mPseudorandomFunctions;

    /**
     * Construct an instance of IkeSaProposal.
     *
     * <p>This constructor is either called by IkeSaPayload for building an inbound proposal from a
     * decoded packet, or called by the inner Builder to build an outbound proposal from user
     * provided parameters
     *
     * @param encryptionAlgos encryption algorithms
     * @param prfs pseudorandom functions
     * @param integrityAlgos integrity algorithms
     * @param dhGroups Diffie-Hellman Groups
     * @hide
     */
    public IkeSaProposal(
            EncryptionTransform[] encryptionAlgos,
            PrfTransform[] prfs,
            IntegrityTransform[] integrityAlgos,
            DhGroupTransform[] dhGroups) {
        super(IkePayload.PROTOCOL_ID_IKE, encryptionAlgos, integrityAlgos, dhGroups);
        mPseudorandomFunctions = prfs;
    }

    /**
     * Gets all proposed Pseudorandom Functions
     *
     * @return A list of the IANA-defined IDs for the proposed Pseudorandom Functions
     */
    @NonNull
    public List<Integer> getPseudorandomFunctions() {
        final List<Integer> result = new ArrayList<>();
        for (Transform transform : mPseudorandomFunctions) {
            result.add(transform.id);
        }
        return result;
    }

    /**
     * Gets all PRF Transforms
     *
     * @hide
     */
    public PrfTransform[] getPrfTransforms() {
        return mPseudorandomFunctions;
    }

    /** @hide */
    @Override
    public Transform[] getAllTransforms() {
        List<Transform> transformList = getAllTransformsAsList();
        transformList.addAll(Arrays.asList(mPseudorandomFunctions));

        return transformList.toArray(new Transform[transformList.size()]);
    }

    /** @hide */
    @Override
    public boolean isNegotiatedFrom(SaProposal reqProposal) {
        return super.isNegotiatedFrom(reqProposal)
                && isTransformSelectedFrom(
                        mPseudorandomFunctions,
                        ((IkeSaProposal) reqProposal).mPseudorandomFunctions);
    }

    /**
     * This class is used to incrementally construct a IkeSaProposal. IkeSaProposal instances are
     * immutable once built.
     */
    public static final class Builder extends SaProposal.Builder {
        // TODO: Support users to add algorithms from most preferred to least preferred.

        // Use set to avoid adding repeated algorithms.
        private final Set<PrfTransform> mProposedPrfs = new ArraySet<>();

        /**
         * Adds an encryption algorithm with a specific key length to the SA proposal being built.
         *
         * @param algorithm encryption algorithm to add to IkeSaProposal.
         * @param keyLength key length of algorithm. For algorithms that have fixed key length (e.g.
         *     3DES) only {@link SaProposal.KEY_LEN_UNUSED} is allowed.
         * @return Builder of IkeSaProposal.
         */
        @NonNull
        public Builder addEncryptionAlgorithm(@EncryptionAlgorithm int algorithm, int keyLength) {
            validateAndAddEncryptAlgo(algorithm, keyLength);
            return this;
        }

        /**
         * Adds an integrity algorithm to the SA proposal being built.
         *
         * @param algorithm integrity algorithm to add to IkeSaProposal.
         * @return Builder of IkeSaProposal.
         */
        @NonNull
        public Builder addIntegrityAlgorithm(@IntegrityAlgorithm int algorithm) {
            addIntegrityAlgo(algorithm);
            return this;
        }

        /**
         * Adds a Diffie-Hellman Group to the SA proposal being built.
         *
         * @param dhGroup to add to IkeSaProposal.
         * @return Builder of IkeSaProposal.
         */
        @NonNull
        public Builder addDhGroup(@DhGroup int dhGroup) {
            addDh(dhGroup);
            return this;
        }

        /**
         * Adds a pseudorandom function to the SA proposal being built.
         *
         * @param algorithm pseudorandom function to add to IkeSaProposal.
         * @return Builder of IkeSaProposal.
         */
        @NonNull
        public Builder addPseudorandomFunction(@PseudorandomFunction int algorithm) {
            // Construct PrfTransform and validate proposed algorithm during construction.
            mProposedPrfs.add(new PrfTransform(algorithm));
            return this;
        }

        private IntegrityTransform[] buildIntegAlgosOrThrow() {
            // When building IKE SA Proposal with normal-mode ciphers, mProposedIntegrityAlgos must
            // not be empty and must not have INTEGRITY_ALGORITHM_NONE. When building IKE SA
            // Proposal with combined-mode ciphers, mProposedIntegrityAlgos must be either empty or
            // only have INTEGRITY_ALGORITHM_NONE.
            if (mProposedIntegrityAlgos.isEmpty() && !mHasAead) {
                throw new IllegalArgumentException(
                        ERROR_TAG
                                + "Integrity algorithm "
                                + "must be proposed with normal ciphers in IKE proposal.");
            }

            for (IntegrityTransform transform : mProposedIntegrityAlgos) {
                if ((transform.id == INTEGRITY_ALGORITHM_NONE) != mHasAead) {
                    throw new IllegalArgumentException(
                            ERROR_TAG
                                    + "Invalid integrity algorithm configuration"
                                    + " for this SA Proposal");
                }
            }

            return mProposedIntegrityAlgos.toArray(
                    new IntegrityTransform[mProposedIntegrityAlgos.size()]);
        }

        private DhGroupTransform[] buildDhGroupsOrThrow() {
            if (mProposedDhGroups.isEmpty()) {
                throw new IllegalArgumentException(
                        ERROR_TAG + "DH group must be proposed in IKE SA proposal.");
            }

            for (DhGroupTransform transform : mProposedDhGroups) {
                if (transform.id == DH_GROUP_NONE) {
                    throw new IllegalArgumentException(
                            ERROR_TAG + "None-value DH group invalid in IKE SA proposal");
                }
            }

            return mProposedDhGroups.toArray(new DhGroupTransform[mProposedDhGroups.size()]);
        }

        private PrfTransform[] buildPrfsOrThrow() {
            if (mProposedPrfs.isEmpty()) {
                throw new IllegalArgumentException(
                        ERROR_TAG + "PRF must be proposed in IKE SA proposal.");
            }
            return mProposedPrfs.toArray(new PrfTransform[mProposedPrfs.size()]);
        }

        /**
         * Validates and builds the IkeSaProposal.
         *
         * @return the validated IkeSaProposal.
         */
        @NonNull
        public IkeSaProposal build() {
            EncryptionTransform[] encryptionTransforms = buildEncryptAlgosOrThrow();
            PrfTransform[] prfTransforms = buildPrfsOrThrow();
            IntegrityTransform[] integrityTransforms = buildIntegAlgosOrThrow();
            DhGroupTransform[] dhGroupTransforms = buildDhGroupsOrThrow();

            return new IkeSaProposal(
                    encryptionTransforms, prfTransforms, integrityTransforms, dhGroupTransforms);
        }
    }
}
