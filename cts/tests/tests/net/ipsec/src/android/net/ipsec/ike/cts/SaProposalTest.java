/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.net.ipsec.ike.cts;

import static android.net.ipsec.ike.SaProposal.DH_GROUP_1024_BIT_MODP;
import static android.net.ipsec.ike.SaProposal.DH_GROUP_2048_BIT_MODP;
import static android.net.ipsec.ike.SaProposal.DH_GROUP_NONE;
import static android.net.ipsec.ike.SaProposal.ENCRYPTION_ALGORITHM_3DES;
import static android.net.ipsec.ike.SaProposal.ENCRYPTION_ALGORITHM_AES_CBC;
import static android.net.ipsec.ike.SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_12;
import static android.net.ipsec.ike.SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_16;
import static android.net.ipsec.ike.SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_8;
import static android.net.ipsec.ike.SaProposal.INTEGRITY_ALGORITHM_AES_XCBC_96;
import static android.net.ipsec.ike.SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96;
import static android.net.ipsec.ike.SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA2_256_128;
import static android.net.ipsec.ike.SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA2_384_192;
import static android.net.ipsec.ike.SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA2_512_256;
import static android.net.ipsec.ike.SaProposal.INTEGRITY_ALGORITHM_NONE;
import static android.net.ipsec.ike.SaProposal.KEY_LEN_AES_128;
import static android.net.ipsec.ike.SaProposal.KEY_LEN_AES_192;
import static android.net.ipsec.ike.SaProposal.KEY_LEN_AES_256;
import static android.net.ipsec.ike.SaProposal.KEY_LEN_UNUSED;
import static android.net.ipsec.ike.SaProposal.PSEUDORANDOM_FUNCTION_AES128_XCBC;
import static android.net.ipsec.ike.SaProposal.PSEUDORANDOM_FUNCTION_HMAC_SHA1;
import static android.net.ipsec.ike.SaProposal.PSEUDORANDOM_FUNCTION_SHA2_256;
import static android.net.ipsec.ike.SaProposal.PSEUDORANDOM_FUNCTION_SHA2_384;
import static android.net.ipsec.ike.SaProposal.PSEUDORANDOM_FUNCTION_SHA2_512;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.net.ipsec.ike.ChildSaProposal;
import android.net.ipsec.ike.IkeSaProposal;
import android.util.Pair;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class SaProposalTest {
    private static final List<Pair<Integer, Integer>> NORMAL_MODE_CIPHERS = new ArrayList<>();
    private static final List<Pair<Integer, Integer>> COMBINED_MODE_CIPHERS = new ArrayList<>();
    private static final List<Integer> INTEGRITY_ALGOS = new ArrayList<>();
    private static final List<Integer> DH_GROUPS = new ArrayList<>();
    private static final List<Integer> DH_GROUPS_WITH_NONE = new ArrayList<>();
    private static final List<Integer> PRFS = new ArrayList<>();

    static {
        NORMAL_MODE_CIPHERS.add(new Pair<>(ENCRYPTION_ALGORITHM_3DES, KEY_LEN_UNUSED));
        NORMAL_MODE_CIPHERS.add(new Pair<>(ENCRYPTION_ALGORITHM_AES_CBC, KEY_LEN_AES_128));
        NORMAL_MODE_CIPHERS.add(new Pair<>(ENCRYPTION_ALGORITHM_AES_CBC, KEY_LEN_AES_192));
        NORMAL_MODE_CIPHERS.add(new Pair<>(ENCRYPTION_ALGORITHM_AES_CBC, KEY_LEN_AES_256));

        COMBINED_MODE_CIPHERS.add(new Pair<>(ENCRYPTION_ALGORITHM_AES_GCM_8, KEY_LEN_AES_128));
        COMBINED_MODE_CIPHERS.add(new Pair<>(ENCRYPTION_ALGORITHM_AES_GCM_12, KEY_LEN_AES_192));
        COMBINED_MODE_CIPHERS.add(new Pair<>(ENCRYPTION_ALGORITHM_AES_GCM_16, KEY_LEN_AES_256));

        INTEGRITY_ALGOS.add(INTEGRITY_ALGORITHM_HMAC_SHA1_96);
        INTEGRITY_ALGOS.add(INTEGRITY_ALGORITHM_AES_XCBC_96);
        INTEGRITY_ALGOS.add(INTEGRITY_ALGORITHM_HMAC_SHA2_256_128);
        INTEGRITY_ALGOS.add(INTEGRITY_ALGORITHM_HMAC_SHA2_384_192);
        INTEGRITY_ALGOS.add(INTEGRITY_ALGORITHM_HMAC_SHA2_512_256);

        DH_GROUPS.add(DH_GROUP_1024_BIT_MODP);
        DH_GROUPS.add(DH_GROUP_2048_BIT_MODP);

        DH_GROUPS_WITH_NONE.add(DH_GROUP_NONE);
        DH_GROUPS_WITH_NONE.addAll(DH_GROUPS);

        PRFS.add(PSEUDORANDOM_FUNCTION_HMAC_SHA1);
        PRFS.add(PSEUDORANDOM_FUNCTION_AES128_XCBC);
        PRFS.add(PSEUDORANDOM_FUNCTION_SHA2_256);
        PRFS.add(PSEUDORANDOM_FUNCTION_SHA2_384);
        PRFS.add(PSEUDORANDOM_FUNCTION_SHA2_512);
    }

    // Package private
    static IkeSaProposal buildIkeSaProposalWithNormalModeCipher() {
        return buildIkeSaProposal(NORMAL_MODE_CIPHERS, INTEGRITY_ALGOS, PRFS, DH_GROUPS);
    }

    // Package private
    static IkeSaProposal buildIkeSaProposalWithCombinedModeCipher() {
        return buildIkeSaProposalWithCombinedModeCipher(true /* hasIntegrityNone */);
    }

    private static IkeSaProposal buildIkeSaProposalWithCombinedModeCipher(
            boolean hasIntegrityNone) {
        List<Integer> integerAlgos = new ArrayList<>();
        if (hasIntegrityNone) {
            integerAlgos.add(INTEGRITY_ALGORITHM_NONE);
        }
        return buildIkeSaProposal(COMBINED_MODE_CIPHERS, integerAlgos, PRFS, DH_GROUPS);
    }

    private static IkeSaProposal buildIkeSaProposal(
            List<Pair<Integer, Integer>> ciphers,
            List<Integer> integrityAlgos,
            List<Integer> prfs,
            List<Integer> dhGroups) {
        IkeSaProposal.Builder builder = new IkeSaProposal.Builder();

        for (Pair<Integer, Integer> pair : ciphers) {
            builder.addEncryptionAlgorithm(pair.first, pair.second);
        }
        for (int algo : integrityAlgos) {
            builder.addIntegrityAlgorithm(algo);
        }
        for (int algo : prfs) {
            builder.addPseudorandomFunction(algo);
        }
        for (int algo : dhGroups) {
            builder.addDhGroup(algo);
        }

        return builder.build();
    }

    // Package private
    static ChildSaProposal buildChildSaProposalWithNormalModeCipher() {
        return buildChildSaProposal(NORMAL_MODE_CIPHERS, INTEGRITY_ALGOS, DH_GROUPS_WITH_NONE);
    }

    // Package private
    static ChildSaProposal buildChildSaProposalWithCombinedModeCipher() {
        return buildChildSaProposalWithCombinedModeCipher(true /* hasIntegrityNone */);
    }

    private static ChildSaProposal buildChildSaProposalWithCombinedModeCipher(
            boolean hasIntegrityNone) {
        List<Integer> integerAlgos = new ArrayList<>();
        if (hasIntegrityNone) {
            integerAlgos.add(INTEGRITY_ALGORITHM_NONE);
        }

        return buildChildSaProposal(COMBINED_MODE_CIPHERS, integerAlgos, DH_GROUPS_WITH_NONE);
    }

    private static ChildSaProposal buildChildSaProposal(
            List<Pair<Integer, Integer>> ciphers,
            List<Integer> integrityAlgos,
            List<Integer> dhGroups) {
        ChildSaProposal.Builder builder = new ChildSaProposal.Builder();

        for (Pair<Integer, Integer> pair : ciphers) {
            builder.addEncryptionAlgorithm(pair.first, pair.second);
        }
        for (int algo : integrityAlgos) {
            builder.addIntegrityAlgorithm(algo);
        }
        for (int algo : dhGroups) {
            builder.addDhGroup(algo);
        }

        return builder.build();
    }

    // Package private
    static ChildSaProposal buildChildSaProposalWithOnlyCiphers() {
        return buildChildSaProposal(
                COMBINED_MODE_CIPHERS, Collections.EMPTY_LIST, Collections.EMPTY_LIST);
    }

    @Test
    public void testBuildIkeSaProposalWithNormalModeCipher() {
        IkeSaProposal saProposal = buildIkeSaProposalWithNormalModeCipher();

        assertEquals(NORMAL_MODE_CIPHERS, saProposal.getEncryptionAlgorithms());
        assertEquals(INTEGRITY_ALGOS, saProposal.getIntegrityAlgorithms());
        assertEquals(PRFS, saProposal.getPseudorandomFunctions());
        assertEquals(DH_GROUPS, saProposal.getDhGroups());
    }

    @Test
    public void testBuildIkeSaProposalWithCombinedModeCipher() {
        IkeSaProposal saProposal =
                buildIkeSaProposalWithCombinedModeCipher(false /* hasIntegrityNone */);

        assertEquals(COMBINED_MODE_CIPHERS, saProposal.getEncryptionAlgorithms());
        assertEquals(PRFS, saProposal.getPseudorandomFunctions());
        assertEquals(DH_GROUPS, saProposal.getDhGroups());
        assertTrue(saProposal.getIntegrityAlgorithms().isEmpty());
    }

    @Test
    public void testBuildIkeSaProposalWithCombinedModeCipherAndIntegrityNone() {
        IkeSaProposal saProposal =
                buildIkeSaProposalWithCombinedModeCipher(true /* hasIntegrityNone */);

        assertEquals(COMBINED_MODE_CIPHERS, saProposal.getEncryptionAlgorithms());
        assertEquals(PRFS, saProposal.getPseudorandomFunctions());
        assertEquals(DH_GROUPS, saProposal.getDhGroups());
        assertEquals(Arrays.asList(INTEGRITY_ALGORITHM_NONE), saProposal.getIntegrityAlgorithms());
    }

    @Test
    public void testBuildChildSaProposalWithNormalModeCipher() {
        ChildSaProposal saProposal = buildChildSaProposalWithNormalModeCipher();

        assertEquals(NORMAL_MODE_CIPHERS, saProposal.getEncryptionAlgorithms());
        assertEquals(INTEGRITY_ALGOS, saProposal.getIntegrityAlgorithms());
        assertEquals(DH_GROUPS_WITH_NONE, saProposal.getDhGroups());
    }

    @Test
    public void testBuildChildProposalWithCombinedModeCipher() {
        ChildSaProposal saProposal =
                buildChildSaProposalWithCombinedModeCipher(false /* hasIntegrityNone */);

        assertEquals(COMBINED_MODE_CIPHERS, saProposal.getEncryptionAlgorithms());
        assertTrue(saProposal.getIntegrityAlgorithms().isEmpty());
        assertEquals(DH_GROUPS_WITH_NONE, saProposal.getDhGroups());
    }

    @Test
    public void testBuildChildProposalWithCombinedModeCipherAndIntegrityNone() {
        ChildSaProposal saProposal =
                buildChildSaProposalWithCombinedModeCipher(true /* hasIntegrityNone */);

        assertEquals(COMBINED_MODE_CIPHERS, saProposal.getEncryptionAlgorithms());
        assertEquals(Arrays.asList(INTEGRITY_ALGORITHM_NONE), saProposal.getIntegrityAlgorithms());
        assertEquals(DH_GROUPS_WITH_NONE, saProposal.getDhGroups());
    }

    @Test
    public void testBuildChildSaProposalWithOnlyCiphers() {
        ChildSaProposal saProposal = buildChildSaProposalWithOnlyCiphers();

        assertEquals(COMBINED_MODE_CIPHERS, saProposal.getEncryptionAlgorithms());
        assertTrue(saProposal.getIntegrityAlgorithms().isEmpty());
        assertTrue(saProposal.getDhGroups().isEmpty());
    }

    // TODO(b/148689509): Test throwing exception when algorithm combination is invalid
}
