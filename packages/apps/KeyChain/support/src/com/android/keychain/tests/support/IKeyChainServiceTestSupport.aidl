/*
 * Copyright (C) 2011 The Android Open Source Project
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
package com.android.keychain.tests.support;

import android.accounts.Account;
import android.security.keystore.ParcelableKeyGenParameterSpec;

/**
 * Service that runs as the system user for the use of the
 * KeyChainServiceTest which needs to run as a regular app user, but
 * needs to automate some steps only permissable to the system
 * user. In particular, revokeAppPermission and grantAppPermission
 * must be run within the system_server itself. In a real application,
 * the user is prompted to perform these steps via the
 * com.android.credentials.UNLOCK Intent and
 * KeyChainActivity.
 *
 * @hide
 */
interface IKeyChainServiceTestSupport {
    boolean keystoreReset();
    boolean keystoreSetPassword(String password);
    boolean keystorePut(String key, in byte[] value);
    boolean keystoreImportKey(String key, in byte[] value);
    void revokeAppPermission(int uid, String alias);
    void grantAppPermission(int uid, String alias);
    boolean installKeyPair(in byte[] privateKey, in byte[] userCert, in byte[] certChain, String alias);
    boolean removeKeyPair(String alias);
    void setUserSelectable(String alias, boolean isUserSelectable);
    int generateKeyPair(in String algorithm, in ParcelableKeyGenParameterSpec spec);
    int attestKey(in String alias, in byte[] challenge, in int[] idAttestationFlags);
    boolean setKeyPairCertificate(String alias, in byte[] userCert, in byte[] certChain);
}
