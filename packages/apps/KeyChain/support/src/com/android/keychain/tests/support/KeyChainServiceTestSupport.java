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

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.RemoteException;
import android.security.IKeyChainService;
import android.security.KeyChain;
import android.security.KeyStore;
import android.security.keymaster.KeymasterCertificateChain;
import android.security.keystore.ParcelableKeyGenParameterSpec;
import android.util.Log;

public class KeyChainServiceTestSupport extends Service {
    private static final String TAG = "KeyChainServiceTest";

    private final KeyStore mKeyStore = KeyStore.getInstance();

    private final IKeyChainServiceTestSupport.Stub mIKeyChainServiceTestSupport
            = new IKeyChainServiceTestSupport.Stub() {
        @Override public boolean keystoreReset() {
            Log.d(TAG, "keystoreReset");
            for (String key : mKeyStore.list("")) {
                if (!mKeyStore.delete(key, KeyStore.UID_SELF)) {
                    return false;
                }
            }
            return true;
        }
        @Override public boolean keystoreSetPassword(String password) {
            Log.d(TAG, "keystoreSetPassword");
            return mKeyStore.onUserPasswordChanged(password);
        }
        @Override public boolean keystorePut(String key, byte[] value) {
            Log.d(TAG, "keystorePut");
            return mKeyStore.put(key, value, KeyStore.UID_SELF, KeyStore.FLAG_NONE);
        }
        @Override public boolean keystoreImportKey(String key, byte[] value) {
            Log.d(TAG, "keystoreImport");
            return mKeyStore.importKey(key, value, KeyStore.UID_SELF, KeyStore.FLAG_NONE);
        }

        @Override public void revokeAppPermission(final int uid, final String alias)
                throws RemoteException {
            Log.d(TAG, "revokeAppPermission");
            blockingSetGrantPermission(uid, alias, false);
        }

        @Override public void grantAppPermission(final int uid, final String alias)
                throws RemoteException {
            Log.d(TAG, "grantAppPermission");
            blockingSetGrantPermission(uid, alias, true);
        }

        @Override public boolean installKeyPair(
                byte[] privateKey, byte[] userCert, byte[] certChain, String alias)
                throws RemoteException {
            Log.d(TAG, "installKeyPair");
            return performBlockingKeyChainCall(keyChainService -> {
                return keyChainService.installKeyPair(
                        privateKey, userCert, certChain, alias, KeyStore.UID_SELF);
            });
        }

        @Override public boolean removeKeyPair(String alias) throws RemoteException {
            Log.d(TAG, "removeKeyPair");
            return performBlockingKeyChainCall(keyChainService -> {
                return keyChainService.removeKeyPair(alias);
            });
        }

        @Override public void setUserSelectable(String alias, boolean isUserSelectable)
                throws RemoteException {
            Log.d(TAG, "setUserSelectable");
            KeyChainAction<Void> action = service -> {
                service.setUserSelectable(alias, isUserSelectable);
                return null;
            };
            performBlockingKeyChainCall(action);
        }

        @Override public int generateKeyPair(String algorithm, ParcelableKeyGenParameterSpec spec)
                throws RemoteException {
            return performBlockingKeyChainCall(keyChainService -> {
                return keyChainService.generateKeyPair(algorithm, spec);
            });
        }

        @Override public int attestKey(
                String alias, byte[] attestationChallenge,
                int[] idAttestationFlags) throws RemoteException {
            KeymasterCertificateChain attestationChain = new KeymasterCertificateChain();
            return performBlockingKeyChainCall(keyChainService -> {
                return keyChainService.attestKey(alias, attestationChallenge, idAttestationFlags,
                        attestationChain);
            });
        }

        @Override public boolean setKeyPairCertificate(String alias, byte[] userCertificate,
                byte[] userCertificateChain) throws RemoteException {
            return performBlockingKeyChainCall(keyChainService -> {
                return keyChainService.setKeyPairCertificate(alias, userCertificate,
                        userCertificateChain);
            });
        }

        /**
         * Binds to the KeyChainService and requests that permission for the sender to
         * access the specified alias is granted/revoked.
         * This method blocks so it must not be called from the UI thread.
         * @param senderUid
         * @param alias
         */
        private void blockingSetGrantPermission(int senderUid, String alias, boolean value)
                throws RemoteException {
            KeyChainAction<Void> action = new KeyChainAction<Void>() {
                public Void run(IKeyChainService service) throws RemoteException {
                    service.setGrant(senderUid, alias, value);
                    return null;
                };
            };
            performBlockingKeyChainCall(action);
        }
    };

    @Override public IBinder onBind(Intent intent) {
        return mIKeyChainServiceTestSupport;
    }

    public interface KeyChainAction<T> {
        T run(IKeyChainService service) throws RemoteException;
    }

    private <T> T performBlockingKeyChainCall(KeyChainAction<T> action) throws RemoteException {
        try (KeyChain.KeyChainConnection connection =
        KeyChain.bind(KeyChainServiceTestSupport.this)) {
            return action.run(connection.getService());
        } catch (InterruptedException e) {
            // should never happen.
            Log.e(TAG, "interrupted while running action");
            Thread.currentThread().interrupt();
        }
        return null;
    }
}
