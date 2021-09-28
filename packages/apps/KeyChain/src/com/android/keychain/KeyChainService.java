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

package com.android.keychain;

import static android.app.admin.SecurityLog.TAG_CERT_AUTHORITY_INSTALLED;
import static android.app.admin.SecurityLog.TAG_CERT_AUTHORITY_REMOVED;

import android.annotation.Nullable;
import android.app.BroadcastOptions;
import android.app.IntentService;
import android.app.admin.SecurityLog;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.StringParceledListSlice;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.Process;
import android.os.UserHandle;
import android.security.Credentials;
import android.security.IKeyChainService;
import android.security.KeyChain;
import android.security.KeyStore;
import android.security.keymaster.KeymasterArguments;
import android.security.keymaster.KeymasterCertificateChain;
import android.security.keystore.AttestationUtils;
import android.security.keystore.DeviceIdAttestationException;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.ParcelableKeyGenParameterSpec;
import android.security.keystore.StrongBoxUnavailableException;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.widget.LockPatternUtils;
import com.android.keychain.internal.ExistingKeysProvider;
import com.android.keychain.internal.GrantsDatabase;
import com.android.org.conscrypt.TrustedCertificateStore;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.security.InvalidAlgorithmParameterException;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.cert.Certificate;
import java.security.cert.CertificateEncodingException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.security.auth.x500.X500Principal;

public class KeyChainService extends IntentService {

    private static final String TAG = "KeyChain";
    private static final String CERT_INSTALLER_PACKAGE = "com.android.certinstaller";
    private final Set<Integer> ALLOWED_UIDS = Collections.unmodifiableSet(
            new HashSet(Arrays.asList(KeyStore.UID_SELF, Process.WIFI_UID)));

    /** created in onCreate(), closed in onDestroy() */
    private GrantsDatabase mGrantsDb;
    private Injector mInjector;
    private final KeyStore mKeyStore = KeyStore.getInstance();

    public KeyChainService() {
        super(KeyChainService.class.getSimpleName());
        mInjector = new Injector();
    }

    @Override public void onCreate() {
        super.onCreate();
        mGrantsDb = new GrantsDatabase(this, new KeyStoreAliasesProvider(mKeyStore));
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mGrantsDb.destroy();
        mGrantsDb = null;
    }

    private static class KeyStoreAliasesProvider implements ExistingKeysProvider {
        private final KeyStore mKeyStore;

        KeyStoreAliasesProvider(KeyStore keyStore) {
            mKeyStore = keyStore;
        }

        @Override
        public List<String> getExistingKeyAliases() {
            List<String> aliases = new ArrayList<String>();
            String[] keyStoreAliases = mKeyStore.list(Credentials.USER_PRIVATE_KEY);
            if (keyStoreAliases == null) {
                return aliases;
            }

            for (String alias: keyStoreAliases) {
                Log.w(TAG, "Got Alias from KeyStore: " + alias);
                String unPrefixedAlias = alias.replaceFirst("^" + Credentials.USER_PRIVATE_KEY, "");
                if (!unPrefixedAlias.startsWith(LockPatternUtils.SYNTHETIC_PASSWORD_KEY_PREFIX)) {
                    aliases.add(unPrefixedAlias);
                }
            }
            return aliases;
        }
    }

    private final IKeyChainService.Stub mIKeyChainService = new IKeyChainService.Stub() {
        private final TrustedCertificateStore mTrustedCertificateStore
                = new TrustedCertificateStore();
        private final Context mContext = KeyChainService.this;

        @Override
        public String requestPrivateKey(String alias) {
            if (!hasGrant(alias)) {
                return null;
            }

            final String keystoreAlias = Credentials.USER_PRIVATE_KEY + alias;
            final int uid = mInjector.getCallingUid();
            Log.i(TAG, String.format("UID %d will be granted access to %s", uid, keystoreAlias));
            return mKeyStore.grant(keystoreAlias, uid);
        }

        @Override public byte[] getCertificate(String alias) {
            if (!hasGrant(alias)) {
                return null;
            }
            return mKeyStore.get(Credentials.USER_CERTIFICATE + alias);
        }

        @Override public byte[] getCaCertificates(String alias) {
            if (!hasGrant(alias)) {
                return null;
            }
            return mKeyStore.get(Credentials.CA_CERTIFICATE + alias);
        }

        @Override public boolean isUserSelectable(String alias) {
            validateAlias(alias);
            return mGrantsDb.isUserSelectable(alias);
        }

        @Override public void setUserSelectable(String alias, boolean isUserSelectable) {
            validateAlias(alias);
            checkSystemCaller();
            Log.i(TAG, String.format("Marking certificate %s as user-selectable: %b", alias,
                    isUserSelectable));
            mGrantsDb.setIsUserSelectable(alias, isUserSelectable);
        }

        @Override public int generateKeyPair(
                String algorithm, ParcelableKeyGenParameterSpec parcelableSpec) {
            checkSystemCaller();
            final KeyGenParameterSpec spec = parcelableSpec.getSpec();
            final String alias = spec.getKeystoreAlias();

            Log.i(TAG, String.format("About to generate key with alias %s, algorithm %s",
                    alias, algorithm));

            if (KeyChain.KEY_ALIAS_SELECTION_DENIED.equals(alias)) {
                throw new IllegalArgumentException("The alias specified for the key denotes "
                        + "a reserved value and cannot be used to name a key");
            }
            // Validate the alias here to avoid relying on KeyGenParameterSpec c'tor preventing
            // the creation of a KeyGenParameterSpec instance with a non-empty alias.
            if (TextUtils.isEmpty(alias) || spec.getUid() != KeyStore.UID_SELF) {
                Log.e(TAG, "Cannot generate key pair with empty alias or specified uid.");
                return KeyChain.KEY_GEN_MISSING_ALIAS;
            }

            if (spec.getAttestationChallenge() != null) {
                Log.e(TAG, "Key generation request should not include an Attestation challenge.");
                return KeyChain.KEY_GEN_SUPERFLUOUS_ATTESTATION_CHALLENGE;
            }

            if (!removeKeyPair(alias)) {
                Log.e(TAG, "Failed to remove previously-installed alias " + alias);
                //TODO: Introduce a different error code in R to distinguish the failure to remove
                // old keys from other failures.
                return KeyChain.KEY_GEN_FAILURE;
            }

            try {
                KeyPairGenerator generator = KeyPairGenerator.getInstance(
                        algorithm, "AndroidKeyStore");
                // Do not prepend USER_PRIVATE_KEY to the alias because
                // AndroidKeyStoreKeyPairGeneratorSpi will helpfully prepend that in
                // generateKeyPair.
                generator.initialize(spec);
                KeyPair kp = generator.generateKeyPair();
                if (kp == null) {
                    Log.e(TAG, "Key generation failed.");
                    return KeyChain.KEY_GEN_FAILURE;
                }
                return KeyChain.KEY_GEN_SUCCESS;
            } catch (NoSuchAlgorithmException e) {
                Log.e(TAG, "Invalid algorithm requested", e);
                return KeyChain.KEY_GEN_NO_SUCH_ALGORITHM;
            } catch (InvalidAlgorithmParameterException e) {
                Log.e(TAG, "Invalid algorithm params", e);
                return KeyChain.KEY_GEN_INVALID_ALGORITHM_PARAMETERS;
            } catch (NoSuchProviderException e) {
                Log.e(TAG, "Could not find Keystore.", e);
                return KeyChain.KEY_GEN_NO_KEYSTORE_PROVIDER;
            } catch (StrongBoxUnavailableException e) {
                Log.e(TAG, "StrongBox unavailable.", e);
                return KeyChain.KEY_GEN_STRONGBOX_UNAVAILABLE;
            }
        }

        @Override public int attestKey(
                String alias, byte[] attestationChallenge,
                int[] idAttestationFlags,
                KeymasterCertificateChain attestationChain) {
            checkSystemCaller();
            validateAlias(alias);

            if (attestationChallenge == null) {
                Log.e(TAG, String.format("Missing attestation challenge for alias %s", alias));
                return KeyChain.KEY_ATTESTATION_MISSING_CHALLENGE;
            }

            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, String.format("About to attest key alias %s, challenge %s, flags %s",
                        alias, Base64.encodeToString(attestationChallenge, Base64.NO_WRAP),
                        Arrays.toString(idAttestationFlags)));
            }

            KeymasterArguments attestArgs;
            try {
                attestArgs = AttestationUtils.prepareAttestationArguments(
                        mContext, idAttestationFlags, attestationChallenge);
            } catch (DeviceIdAttestationException e) {
                Log.e(TAG, "Failed collecting attestation data", e);
                return KeyChain.KEY_ATTESTATION_CANNOT_COLLECT_DATA;
            }
            int errorCode = checkKeyChainStatus(alias, attestationChain, attestArgs);
            if (errorCode == KeyChain.KEY_ATTESTATION_CANNOT_ATTEST_IDS) {
                // b/69471841: id attestation might fail due to incorrect provisioning of device
                try {
                    attestArgs =
                            AttestationUtils.prepareAttestationArgumentsIfMisprovisioned(
                            mContext, idAttestationFlags, attestationChallenge);
                    if (attestArgs == null) {
                        return errorCode;
                    }
                } catch (DeviceIdAttestationException e) {
                    Log.e(TAG, "Failed collecting attestation data "
                            + "during second attempt on misprovisioned device", e);
                    return KeyChain.KEY_ATTESTATION_CANNOT_COLLECT_DATA;
                }
            }

            return checkKeyChainStatus(alias, attestationChain, attestArgs);
        }

        private int checkKeyChainStatus(
                String alias,
                KeymasterCertificateChain attestationChain,
                KeymasterArguments attestArgs) {

            final String keystoreAlias = Credentials.USER_PRIVATE_KEY + alias;
            final int errorCode = mKeyStore.attestKey(keystoreAlias, attestArgs, attestationChain);
            if (errorCode != KeyStore.NO_ERROR) {
                Log.e(TAG, String.format("Failure attesting for key %s: %d", alias, errorCode));
                if (errorCode == KeyStore.CANNOT_ATTEST_IDS) {
                    return KeyChain.KEY_ATTESTATION_CANNOT_ATTEST_IDS;
                } else {
                    // General failure, cannot discern which.
                    return KeyChain.KEY_ATTESTATION_FAILURE;
                }
            }

            return KeyChain.KEY_ATTESTATION_SUCCESS;
        }

        @Override public boolean setKeyPairCertificate(String alias, byte[] userCertificate,
                byte[] userCertificateChain) {
            checkSystemCaller();
            if (!mKeyStore.put(Credentials.USER_CERTIFICATE + alias, userCertificate,
                        KeyStore.UID_SELF, KeyStore.FLAG_NONE)) {
                Log.e(TAG, "Failed to import user certificate " + userCertificate);
                return false;
            }

            if (userCertificateChain != null && userCertificateChain.length > 0) {
                if (!mKeyStore.put(Credentials.CA_CERTIFICATE + alias, userCertificateChain,
                            KeyStore.UID_SELF, KeyStore.FLAG_NONE)) {
                    Log.e(TAG, "Failed to import certificate chain" + userCertificateChain);
                    if (!mKeyStore.delete(Credentials.USER_CERTIFICATE + alias)) {
                        Log.e(TAG, "Failed to clean up key chain after certificate chain"
                                + " importing failed");
                    }
                    return false;
                }
            } else {
                if (!mKeyStore.delete(Credentials.CA_CERTIFICATE + alias)) {
                    Log.e(TAG, "Failed to remove CA certificate chain for alias " + alias);
                }
            }

            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, String.format("Set certificate for key alias %s : user %s CA chain: %s",
                        alias, emptyOrBase64Encoded(userCertificate),
                        emptyOrBase64Encoded(userCertificateChain)));
            }
            broadcastKeychainChange();
            broadcastLegacyStorageChange();
            return true;
        }

        private void validateAlias(String alias) {
            if (alias == null) {
                throw new NullPointerException("alias == null");
            }
        }

        private boolean hasGrant(String alias) {
            validateAlias(alias);

            final int callingUid = mInjector.getCallingUid();
            if (!mGrantsDb.hasGrant(callingUid, alias)) {
                Log.w(TAG, String.format(
                        "uid %d doesn't have permission to access the requested alias %s",
                        callingUid, alias));
                return false;
            }

            return true;
        }

        @Override public String installCaCertificate(byte[] caCertificate) {
            checkCertInstallerOrSystemCaller();
            final String alias;
            String subject = null;
            final boolean isSecurityLoggingEnabled = mInjector.isSecurityLoggingEnabled();
            try {
                final X509Certificate cert = parseCertificate(caCertificate);

                final boolean isDebugLoggable = Log.isLoggable(TAG, Log.DEBUG);
                subject = cert.getSubjectX500Principal().getName(X500Principal.CANONICAL);
                if (isDebugLoggable) {
                    Log.d(TAG, String.format("Installing CA certificate: %s", subject));
                }

                synchronized (mTrustedCertificateStore) {
                    mTrustedCertificateStore.installCertificate(cert);
                    alias = mTrustedCertificateStore.getCertificateAlias(cert);
                }
            } catch (IOException | CertificateException e) {
                Log.w(TAG, "Failed installing CA certificate", e);
                if (isSecurityLoggingEnabled && subject != null) {
                    mInjector.writeSecurityEvent(
                            TAG_CERT_AUTHORITY_INSTALLED, 0 /*result*/, subject,
                            UserHandle.myUserId());
                }
                throw new IllegalStateException(e);
            }
            if (isSecurityLoggingEnabled && subject != null) {
                mInjector.writeSecurityEvent(
                        TAG_CERT_AUTHORITY_INSTALLED, 1 /*result*/, subject,
                        UserHandle.myUserId());
            }

            // If the caller is the cert installer, install the CA certificate into KeyStore.
            // This is a temporary solution to enable CA certificates to be used as VPN trust
            // anchors. Ultimately, the user should explicitly choose to install the VPN trust
            // anchor separately and independently of CA certificates, at which point this code
            // should be removed.
            if (CERT_INSTALLER_PACKAGE.equals(callingPackage())) {
                final boolean result = mKeyStore.put(
                        String.format("%s%s %s", Credentials.CA_CERTIFICATE, subject, alias),
                        caCertificate, Process.SYSTEM_UID,
                        KeyStore.FLAG_NONE);
                Log.d(TAG, String.format(
                        "Attempted installing %s (subject: %s) to KeyStore. Result: %b", alias,
                        subject, result));
            }

            broadcastLegacyStorageChange();
            broadcastTrustStoreChange();
            return alias;
        }

        /**
         * Install a key pair to the keystore.
         *
         * @param privateKey The private key associated with the client certificate
         * @param userCertificate The client certificate to be installed
         * @param userCertificateChain The rest of the chain for the client certificate
         * @param alias The alias under which the key pair is installed. It is invalid to pass
         *              {@code KeyChain.KEY_ALIAS_SELECTION_DENIED}.
         * @param uid Can be only one of two values: Either {@code KeyStore.UID_SELF} to indicate
         *            installation into the current user's system Keystore instance, or
         *            {@code Process.WIFI_UID} to indicate installation into the main user's
         *            WiFi Keystore instance. It is only valid to pass {@code Process.WIFI_UID} to
         *            the KeyChain service on user 0.
         * @return Whether the operation succeeded or not.
         */
        @Override public boolean installKeyPair(@Nullable byte[] privateKey,
                @Nullable byte[] userCertificate, @Nullable byte[] userCertificateChain,
                String alias, int uid) {
            checkCertInstallerOrSystemCaller();
            if (KeyChain.KEY_ALIAS_SELECTION_DENIED.equals(alias)) {
                throw new IllegalArgumentException("The alias specified for the key denotes "
                        + "a reserved value and cannot be used to name a key");
            }
            if (!ALLOWED_UIDS.contains(uid)) {
                Log.e(TAG,
                        String.format("Installing alias %s as UID %d is now allowed.", alias, uid));
                return false;
            }

            if (privateKey == null && userCertificate == null && userCertificateChain == null) {
                Log.e(TAG, String.format("Nothing to install for alias %s", alias));
                return false;
            }

            if (uid == Process.WIFI_UID && UserHandle.myUserId() != UserHandle.USER_SYSTEM) {
                Log.e(TAG, String.format(
                        "Installation into the WiFi Keystore should be called from the primary "
                                + "user, not user %d",
                        UserHandle.myUserId()));
                return false;
            }

            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, String.format("Installing certificate and key to alias %s to uid %d: "
                                + "user cert %s CA chain: %s", alias, uid,
                                emptyOrBase64Encoded(userCertificate),
                                emptyOrBase64Encoded(userCertificateChain)));
            }

            if (!removeKeyPair(alias)) {
                return false;
            }
            if (privateKey != null && !mKeyStore.importKey(
                    Credentials.USER_PRIVATE_KEY + alias, privateKey, uid, KeyStore.FLAG_NONE)) {
                Log.e(TAG, "Failed to import private key " + alias);
                return false;
            }
            if (userCertificate != null &&
                    !mKeyStore.put(Credentials.USER_CERTIFICATE + alias, userCertificate,
                        uid, KeyStore.FLAG_NONE)) {
                Log.e(TAG, "Failed to import user certificate " + userCertificate);
                if (!mKeyStore.delete(Credentials.USER_PRIVATE_KEY + alias)) {
                    Log.e(TAG, "Failed to delete private key after certificate importing failed");
                }
                return false;
            }
            if (userCertificateChain != null && userCertificateChain.length > 0) {
                if (!mKeyStore.put(Credentials.CA_CERTIFICATE + alias, userCertificateChain, uid,
                        KeyStore.FLAG_NONE)) {
                    Log.e(TAG, "Failed to import certificate chain" + userCertificateChain);
                    if (!removeKeyPair(alias)) {
                        Log.e(TAG, "Failed to clean up key chain after certificate chain"
                                + " importing failed");
                    }
                    return false;
                }
            }
            broadcastKeychainChange();
            broadcastLegacyStorageChange();
            return true;
        }

        @Override public boolean removeKeyPair(String alias) {
            checkCertInstallerOrSystemCaller();
            if (!Credentials.deleteAllTypesForAlias(mKeyStore, alias)) {
                return false;
            }
            Log.w(TAG, String.format(
                    "WARNING: Removing alias %s, existing grants will be revoked.", alias));
            mGrantsDb.removeAliasInformation(alias);
            broadcastKeychainChange();
            broadcastLegacyStorageChange();
            return true;
        }

        private X509Certificate parseCertificate(byte[] bytes) throws CertificateException {
            CertificateFactory cf = CertificateFactory.getInstance("X.509");
            return (X509Certificate) cf.generateCertificate(new ByteArrayInputStream(bytes));
        }

        @Override public boolean reset() {
            // only Settings should be able to reset
            checkSystemCaller();
            mGrantsDb.removeAllAliasesInformation();
            boolean ok = true;
            synchronized (mTrustedCertificateStore) {
                // delete user-installed CA certs
                for (String alias : mTrustedCertificateStore.aliases()) {
                    if (TrustedCertificateStore.isUser(alias)) {
                        if (!deleteCertificateEntry(alias)) {
                            ok = false;
                        }
                    }
                }
            }
            broadcastTrustStoreChange();
            broadcastKeychainChange();
            broadcastLegacyStorageChange();
            return ok;
        }

        @Override public boolean deleteCaCertificate(String alias) {
            // only Settings should be able to delete
            checkSystemCaller();
            boolean ok = true;
            Log.i(TAG, String.format("Deleting CA certificate %s", alias));
            synchronized (mTrustedCertificateStore) {
                ok = deleteCertificateEntry(alias);
            }
            broadcastTrustStoreChange();
            broadcastLegacyStorageChange();
            return ok;
        }

        private boolean deleteCertificateEntry(String alias) {
            String subjectForAudit = null;
            if (mInjector.isSecurityLoggingEnabled()) {
                final Certificate cert = mTrustedCertificateStore.getCertificate(alias);
                if (cert instanceof X509Certificate) {
                    subjectForAudit = ((X509Certificate) cert)
                            .getSubjectX500Principal().getName(X500Principal.CANONICAL);
                }
            }

            try {
                mTrustedCertificateStore.deleteCertificateEntry(alias);
                if (subjectForAudit != null) {
                    mInjector.writeSecurityEvent(
                            TAG_CERT_AUTHORITY_REMOVED, 1 /*result*/, subjectForAudit,
                            UserHandle.myUserId());
                }
                return true;
            } catch (IOException | CertificateException e) {
                Log.w(TAG, "Problem removing CA certificate " + alias, e);
                if (subjectForAudit != null) {
                    mInjector.writeSecurityEvent(
                            TAG_CERT_AUTHORITY_REMOVED, 0 /*result*/, subjectForAudit,
                            UserHandle.myUserId());
                }
                return false;
            }
        }

        private void checkCertInstallerOrSystemCaller() {
            final String caller = callingPackage();
            if (!isCallerWithSystemUid() && !CERT_INSTALLER_PACKAGE.equals(caller)) {
                throw new SecurityException("Not system or cert installer package: " + caller);
            }
        }

        private void checkSystemCaller() {
            if (!isCallerWithSystemUid()) {
                throw new SecurityException("Not system package: " + callingPackage());
            }
        }

        private boolean isCallerWithSystemUid() {
            return UserHandle.isSameApp(mInjector.getCallingUid(), Process.SYSTEM_UID);
        }

        private String callingPackage() {
            return getPackageManager().getNameForUid(mInjector.getCallingUid());
        }

        @Override public boolean hasGrant(int uid, String alias) {
            checkSystemCaller();
            return mGrantsDb.hasGrant(uid, alias);
        }

        @Override public void setGrant(int uid, String alias, boolean value) {
            checkSystemCaller();
            mGrantsDb.setGrant(uid, alias, value);
            broadcastPermissionChange(uid, alias, value);
            broadcastLegacyStorageChange();
        }

        @Override
        public StringParceledListSlice getUserCaAliases() {
            synchronized (mTrustedCertificateStore) {
                return new StringParceledListSlice(new ArrayList<String>(
                        mTrustedCertificateStore.userAliases()));
            }
        }

        @Override
        public StringParceledListSlice getSystemCaAliases() {
            synchronized (mTrustedCertificateStore) {
                return new StringParceledListSlice(new ArrayList<String>(
                        mTrustedCertificateStore.allSystemAliases()));
            }
        }

        @Override
        public boolean containsCaAlias(String alias) {
            return mTrustedCertificateStore.containsAlias(alias);
        }

        @Override
        public byte[] getEncodedCaCertificate(String alias, boolean includeDeletedSystem) {
            synchronized (mTrustedCertificateStore) {
                X509Certificate certificate = (X509Certificate) mTrustedCertificateStore
                        .getCertificate(alias, includeDeletedSystem);
                if (certificate == null) {
                    Log.w(TAG, "Could not find CA certificate " + alias);
                    return null;
                }
                try {
                    return certificate.getEncoded();
                } catch (CertificateEncodingException e) {
                    Log.w(TAG, "Error while encoding CA certificate " + alias);
                    return null;
                }
            }
        }

        @Override
        public List<String> getCaCertificateChainAliases(String rootAlias,
                boolean includeDeletedSystem) {
            synchronized (mTrustedCertificateStore) {
                X509Certificate root = (X509Certificate) mTrustedCertificateStore.getCertificate(
                        rootAlias, includeDeletedSystem);
                try {
                    List<X509Certificate> chain = mTrustedCertificateStore.getCertificateChain(
                            root);
                    List<String> aliases = new ArrayList<String>(chain.size());
                    final int n = chain.size();
                    for (int i = 0; i < n; ++i) {
                        String alias = mTrustedCertificateStore.getCertificateAlias(chain.get(i),
                                true);
                        if (alias != null) {
                            aliases.add(alias);
                        }
                    }
                    return aliases;
                } catch (CertificateException e) {
                    Log.w(TAG, "Error retrieving cert chain for root " + rootAlias);
                    return Collections.emptyList();
                }
            }
        }
    };

    @Override public IBinder onBind(Intent intent) {
        if (IKeyChainService.class.getName().equals(intent.getAction())) {
            return mIKeyChainService;
        }
        return null;
    }

    @Override
    protected void onHandleIntent(final Intent intent) {
        if (Intent.ACTION_PACKAGE_REMOVED.equals(intent.getAction())) {
            mGrantsDb.purgeOldGrants(getPackageManager());
        }
    }

    private void broadcastLegacyStorageChange() {
        Intent intent = new Intent(KeyChain.ACTION_STORAGE_CHANGED);
        BroadcastOptions opts = BroadcastOptions.makeBasic();
        opts.setMaxManifestReceiverApiLevel(Build.VERSION_CODES.N_MR1);
        sendBroadcastAsUser(intent, UserHandle.of(UserHandle.myUserId()), null, opts.toBundle());
    }

    private void broadcastKeychainChange() {
        Intent intent = new Intent(KeyChain.ACTION_KEYCHAIN_CHANGED);
        sendBroadcastAsUser(intent, UserHandle.of(UserHandle.myUserId()));
    }

    private void broadcastTrustStoreChange() {
        Intent intent = new Intent(KeyChain.ACTION_TRUST_STORE_CHANGED);
        sendBroadcastAsUser(intent, UserHandle.of(UserHandle.myUserId()));
    }

    private void broadcastPermissionChange(int uid, String alias, boolean access) {
        // Since the permission change only impacts one uid only send to that uid's packages.
        final PackageManager packageManager = getPackageManager();
        String[] packages = packageManager.getPackagesForUid(uid);
        if (packages == null) {
            return;
        }
        for (String pckg : packages) {
            Intent intent = new Intent(KeyChain.ACTION_KEY_ACCESS_CHANGED);
            intent.putExtra(KeyChain.EXTRA_KEY_ALIAS, alias);
            intent.putExtra(KeyChain.EXTRA_KEY_ACCESSIBLE, access);
            intent.setPackage(pckg);
            sendBroadcastAsUser(intent, UserHandle.of(UserHandle.myUserId()));
        }
    }

    private static String emptyOrBase64Encoded(byte[] cert) {
        if (cert == null) {
            return "";
        }
        return Base64.encodeToString(cert, Base64.NO_WRAP);
    }

    @VisibleForTesting
    void setInjector(Injector injector) {
        mInjector = injector;
    }

    /**
     * Injector for mocking out dependencies in tests.
     */
    @VisibleForTesting
    static class Injector {
        public boolean isSecurityLoggingEnabled() {
            return SecurityLog.isLoggingEnabled();
        }

        public void writeSecurityEvent(int tag, Object... payload) {
            SecurityLog.writeEvent(tag, payload);
        }

        public int getCallingUid() {
            return Binder.getCallingUid();
        }
    }
}
