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

import com.android.internal.net.ipsec.ike.message.IkePayload;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.DhGroupTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.EncryptionTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.EsnTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IntegrityTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.Transform;

import java.util.Arrays;
import java.util.List;

/**
 * ChildSaProposal represents a proposed configuration to negotiate a Child SA.
 *
 * <p>ChildSaProposal will contain cryptograhic algorithms and key generation materials for the
 * negotiation of a Child SA.
 *
 * <p>User must provide at least one valid ChildSaProposal when they are creating a new Child SA.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#section-3.3">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 * @hide
 */
@SystemApi
public final class ChildSaProposal extends SaProposal {
    private final EsnTransform[] mEsns;

    /**
     * Construct an instance of ChildSaProposal.
     *
     * <p>This constructor is either called by ChildSaPayload for building an inbound proposal from
     * a decoded packet, or called by the inner Builder to build an outbound proposal from user
     * provided parameters
     *
     * @param encryptionAlgos encryption algorithms
     * @param integrityAlgos integrity algorithms
     * @param dhGroups Diffie-Hellman Groups
     * @param esns ESN policies
     * @hide
     */
    public ChildSaProposal(
            EncryptionTransform[] encryptionAlgos,
            IntegrityTransform[] integrityAlgos,
            DhGroupTransform[] dhGroups,
            EsnTransform[] esns) {
        super(IkePayload.PROTOCOL_ID_ESP, encryptionAlgos, integrityAlgos, dhGroups);
        mEsns = esns;
    }

    /**
     * Gets all ESN policies.
     *
     * @hide
     */
    public EsnTransform[] getEsnTransforms() {
        return mEsns;
    }

    /**
     * Gets a copy of proposal without all proposed DH groups.
     *
     * <p>This is used to avoid negotiating DH Group for negotiating first Child SA.
     *
     * @hide
     */
    public ChildSaProposal getCopyWithoutDhTransform() {
        return new ChildSaProposal(
                getEncryptionTransforms(),
                getIntegrityTransforms(),
                new DhGroupTransform[0],
                getEsnTransforms());
    }

    /** @hide */
    @Override
    public Transform[] getAllTransforms() {
        List<Transform> transformList = getAllTransformsAsList();
        transformList.addAll(Arrays.asList(mEsns));

        return transformList.toArray(new Transform[transformList.size()]);
    }

    /** @hide */
    @Override
    public boolean isNegotiatedFrom(SaProposal reqProposal) {
        return super.isNegotiatedFrom(reqProposal)
                && isTransformSelectedFrom(mEsns, ((ChildSaProposal) reqProposal).mEsns);
    }

    /** @hide */
    public boolean isNegotiatedFromExceptDhGroup(SaProposal saProposal) {
        return getProtocolId() == saProposal.getProtocolId()
                && isTransformSelectedFrom(
                        getEncryptionTransforms(), saProposal.getEncryptionTransforms())
                && isTransformSelectedFrom(
                        getIntegrityTransforms(), saProposal.getIntegrityTransforms())
                && isTransformSelectedFrom(mEsns, ((ChildSaProposal) saProposal).mEsns);
    }

    /** @hide */
    public ChildSaProposal getCopyWithAdditionalDhTransform(int dhGroup) {
        return new ChildSaProposal(
                getEncryptionTransforms(),
                getIntegrityTransforms(),
                new DhGroupTransform[] {new DhGroupTransform(dhGroup)},
                getEsnTransforms());
    }

    /**
     * This class is used to incrementally construct a ChildSaProposal. ChildSaProposal instances
     * are immutable once built.
     */
    public static final class Builder extends SaProposal.Builder {
        // TODO: Support users to add algorithms from most preferred to least preferred.

        /**
         * Adds an encryption algorithm with a specific key length to the SA proposal being built.
         *
         * @param algorithm encryption algorithm to add to ChildSaProposal.
         * @param keyLength key length of algorithm. For algorithms that have fixed key length (e.g.
         *     3DES) only {@link SaProposal.KEY_LEN_UNUSED} is allowed.
         * @return Builder of ChildSaProposal.
         */
        @NonNull
        public Builder addEncryptionAlgorithm(@EncryptionAlgorithm int algorithm, int keyLength) {
            validateAndAddEncryptAlgo(algorithm, keyLength);
            return this;
        }

        /**
         * Adds an integrity algorithm to the SA proposal being built.
         *
         * @param algorithm integrity algorithm to add to ChildSaProposal.
         * @return Builder of ChildSaProposal.
         */
        @NonNull
        public Builder addIntegrityAlgorithm(@IntegrityAlgorithm int algorithm) {
            addIntegrityAlgo(algorithm);
            return this;
        }

        /**
         * Adds a Diffie-Hellman Group to the SA proposal being built.
         *
         * @param dhGroup to add to ChildSaProposal.
         * @return Builder of ChildSaProposal.
         */
        @NonNull
        public Builder addDhGroup(@DhGroup int dhGroup) {
            addDh(dhGroup);
            return this;
        }

        private IntegrityTransform[] buildIntegAlgosOrThrow() {
            // When building Child SA Proposal with normal-mode ciphers, there is no contraint on
            // integrity algorithm. When building Child SA Proposal with combined-mode ciphers,
            // mProposedIntegrityAlgos must be either empty or only have INTEGRITY_ALGORITHM_NONE.
            for (IntegrityTransform transform : mProposedIntegrityAlgos) {
                if (transform.id != INTEGRITY_ALGORITHM_NONE && mHasAead) {
                    throw new IllegalArgumentException(
                            ERROR_TAG
                                    + "Only INTEGRITY_ALGORITHM_NONE can be"
                                    + " proposed with combined-mode ciphers in any proposal.");
                }
            }

            return mProposedIntegrityAlgos.toArray(
                    new IntegrityTransform[mProposedIntegrityAlgos.size()]);
        }

        /**
         * Validates and builds the ChildSaProposal.
         *
         * @return the validated ChildSaProposal.
         */
        @NonNull
        public ChildSaProposal build() {
            EncryptionTransform[] encryptionTransforms = buildEncryptAlgosOrThrow();
            IntegrityTransform[] integrityTransforms = buildIntegAlgosOrThrow();

            return new ChildSaProposal(
                    encryptionTransforms,
                    integrityTransforms,
                    mProposedDhGroups.toArray(new DhGroupTransform[mProposedDhGroups.size()]),
                    new EsnTransform[] {new EsnTransform()});
        }
    }
}
