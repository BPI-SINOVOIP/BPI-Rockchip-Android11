/*
 * Copyright (C) 2010 The Android Open Source Project
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

package libcore.java.security;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.security.Security;
import java.security.spec.DSAPrivateKeySpec;
import java.security.spec.DSAPublicKeySpec;
import java.security.spec.ECPrivateKeySpec;
import java.security.spec.ECPublicKeySpec;
import java.security.spec.KeySpec;
import java.security.spec.RSAPrivateCrtKeySpec;
import java.security.spec.RSAPublicKeySpec;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import javax.crypto.spec.DHPrivateKeySpec;
import javax.crypto.spec.DHPublicKeySpec;

/**
 * This class defines expected string names for protocols, key types,
 * client and server auth types, cipher suites.
 *
 * Initially based on "Appendix A: Standard Names" of
 * <a href="http://java.sun.com/j2se/1.5.0/docs/guide/security/jsse/JSSERefGuide.html#AppA">
 * Java &trade; Secure Socket Extension (JSSE) Reference Guide
 * for the Java &trade; 2 Platform Standard Edition 5
 * </a>.
 *
 * Updated based on the
 * <a href="http://download.java.net/jdk8/docs/technotes/guides/security/SunProviders.html">
 * Java &trade; Cryptography Architecture Oracle Providers Documentation
 * for Java &trade; Platform Standard Edition 7
 * </a>.
 * See also the
 * <a href="http://download.java.net/jdk8/docs/technotes/guides/security/StandardNames.html">
 * Java &trade; Cryptography Architecture Standard Algorithm Name Documentation
 * </a>.
 *
 * Further updates based on the
 * <a href=http://java.sun.com/javase/6/docs/technotes/guides/security/p11guide.html">
 * Java &trade; PKCS#11 Reference Guide
 * </a>.
 */
public final class StandardNames {

    public static final boolean IS_RI
            = !"Dalvik Core Library".equals(System.getProperty("java.specification.name"));

    public static final String SECURITY_PROVIDER_NAME = (IS_RI) ? "SUN" : "BC";

    public static final String KEY_STORE_ALGORITHM = (IS_RI) ? "JKS" : "BKS";

    /**
     * RFC 5746's Signaling Cipher Suite Value to indicate a request for secure renegotiation
     */
    private static final String CIPHER_SUITE_SECURE_RENEGOTIATION
            = "TLS_EMPTY_RENEGOTIATION_INFO_SCSV";

    /**
     * A map from algorithm type (e.g. Cipher) to a set of algorithms (e.g. AES, DES, ...)
     */
    static final Map<String,Set<String>> PROVIDER_ALGORITHMS
            = new HashMap<>();

    private static final Map<String,Set<String>> CIPHER_MODES
            = new HashMap<>();

    private static final Map<String,Set<String>> CIPHER_PADDINGS
            = new HashMap<>();

    private static void provide(String type, String algorithm) {
        Set<String> algorithms = PROVIDER_ALGORITHMS.get(type);
        if (algorithms == null) {
            algorithms = new HashSet<>();
            PROVIDER_ALGORITHMS.put(type, algorithms);
        }
        assertTrue("Duplicate " + type + " " + algorithm,
                   algorithms.add(algorithm.toUpperCase(Locale.ROOT)));
    }
    private static void unprovide(String type, String algorithm) {
        Set<String> algorithms = PROVIDER_ALGORITHMS.get(type);
        assertNotNull(algorithms);
        assertTrue(algorithm, algorithms.remove(algorithm.toUpperCase(Locale.ROOT)));
        if (algorithms.isEmpty()) {
            assertNotNull(PROVIDER_ALGORITHMS.remove(type));
        }
    }
    private static void provideCipherModes(String algorithm, String newModes[]) {
        Set<String> modes = CIPHER_MODES.get(algorithm);
        if (modes == null) {
            modes = new HashSet<>();
            CIPHER_MODES.put(algorithm, modes);
        }
        modes.addAll(Arrays.asList(newModes));
    }
    private static void provideCipherPaddings(String algorithm, String newPaddings[]) {
        Set<String> paddings = CIPHER_PADDINGS.get(algorithm);
        if (paddings == null) {
            paddings = new HashSet<>();
            CIPHER_PADDINGS.put(algorithm, paddings);
        }
        paddings.addAll(Arrays.asList(newPaddings));
    }
    static {
        provide("AlgorithmParameterGenerator", "DSA");
        provide("AlgorithmParameterGenerator", "DiffieHellman");
        provide("AlgorithmParameters", "AES");
        provide("AlgorithmParameters", "Blowfish");
        provide("AlgorithmParameters", "DES");
        provide("AlgorithmParameters", "DESede");
        provide("AlgorithmParameters", "DSA");
        provide("AlgorithmParameters", "DiffieHellman");
        provide("AlgorithmParameters", "GCM");
        provide("AlgorithmParameters", "OAEP");
        provide("AlgorithmParameters", "PBEWithMD5AndDES");
        provide("AlgorithmParameters", "PBEWithMD5AndTripleDES");
        provide("AlgorithmParameters", "PBEWithSHA1AndDESede");
        provide("AlgorithmParameters", "PBEWithSHA1AndRC2_40");
        provide("AlgorithmParameters", "PSS");
        provide("AlgorithmParameters", "RC2");
        provide("AlgorithmParameters", "PBEWITHHMACSHA1ANDAES_128");
        provide("AlgorithmParameters", "PBEWITHHMACSHA1ANDAES_256");
        provide("AlgorithmParameters", "PBEWITHHMACSHA224ANDAES_128");
        provide("AlgorithmParameters", "PBEWITHHMACSHA224ANDAES_256");
        provide("AlgorithmParameters", "PBEWITHHMACSHA256ANDAES_128");
        provide("AlgorithmParameters", "PBEWITHHMACSHA256ANDAES_256");
        provide("AlgorithmParameters", "PBEWITHHMACSHA384ANDAES_128");
        provide("AlgorithmParameters", "PBEWITHHMACSHA384ANDAES_256");
        provide("AlgorithmParameters", "PBEWITHHMACSHA512ANDAES_128");
        provide("AlgorithmParameters", "PBEWITHHMACSHA512ANDAES_256");
        provide("SecretKeyFactory", "PBEWITHHMACSHA1ANDAES_128");
        provide("SecretKeyFactory", "PBEWITHHMACSHA1ANDAES_256");
        provide("SecretKeyFactory", "PBEWITHHMACSHA224ANDAES_128");
        provide("SecretKeyFactory", "PBEWITHHMACSHA224ANDAES_256");
        provide("SecretKeyFactory", "PBEWITHHMACSHA256ANDAES_128");
        provide("SecretKeyFactory", "PBEWITHHMACSHA256ANDAES_256");
        provide("SecretKeyFactory", "PBEWITHHMACSHA384ANDAES_128");
        provide("SecretKeyFactory", "PBEWITHHMACSHA384ANDAES_256");
        provide("SecretKeyFactory", "PBEWITHHMACSHA512ANDAES_128");
        provide("SecretKeyFactory", "PBEWITHHMACSHA512ANDAES_256");
        provide("CertPathBuilder", "PKIX");
        provide("CertPathValidator", "PKIX");
        provide("CertStore", "Collection");
        provide("CertStore", "LDAP");
        provide("CertificateFactory", "X.509");
        // TODO: provideCipherModes and provideCipherPaddings for other Ciphers
        provide("Cipher", "AES");
        provideCipherModes("AES", new String[] { "CBC", "CFB", "CTR", "CTS", "ECB", "OFB" });
        provideCipherPaddings("AES", new String[] { "NoPadding", "PKCS5Padding" });
        provide("Cipher", "AESWrap");
        provide("Cipher", "ARCFOUR");
        provide("Cipher", "Blowfish");
        provide("Cipher", "DES");
        provide("Cipher", "DESede");
        provide("Cipher", "DESedeWrap");
        provide("Cipher", "PBEWithMD5AndDES");
        provide("Cipher", "PBEWithMD5AndTripleDES");
        provide("Cipher", "PBEWithSHA1AndDESede");
        provide("Cipher", "PBEWithSHA1AndRC2_40");
        provide("Cipher", "RC2");
        provide("Cipher", "RSA");
        // TODO: None?
        provideCipherModes("RSA", new String[] { "ECB" });
        // TODO: OAEPPadding
        provideCipherPaddings("RSA", new String[] { "NoPadding", "PKCS1Padding" });
        provide("Configuration", "JavaLoginConfig");
        provide("KeyAgreement", "DiffieHellman");
        provide("KeyFactory", "DSA");
        provide("KeyFactory", "DiffieHellman");
        provide("KeyFactory", "RSA");
        provide("KeyGenerator", "AES");
        provide("KeyGenerator", "ARCFOUR");
        provide("KeyGenerator", "Blowfish");
        provide("KeyGenerator", "DES");
        provide("KeyGenerator", "DESede");
        provide("KeyGenerator", "HmacMD5");
        provide("KeyGenerator", "HmacSHA1");
        provide("KeyGenerator", "HmacSHA224");
        provide("KeyGenerator", "HmacSHA256");
        provide("KeyGenerator", "HmacSHA384");
        provide("KeyGenerator", "HmacSHA512");
        provide("KeyGenerator", "RC2");
        provide("KeyInfoFactory", "DOM");
        provide("KeyManagerFactory", "PKIX");
        provide("KeyPairGenerator", "DSA");
        provide("KeyPairGenerator", "DiffieHellman");
        provide("KeyPairGenerator", "RSA");
        provide("KeyStore", "JCEKS");
        provide("KeyStore", "JKS");
        provide("KeyStore", "PKCS12");
        provide("Mac", "HmacMD5");
        provide("Mac", "HmacSHA1");
        provide("Mac", "HmacSHA224");
        provide("Mac", "HmacSHA256");
        provide("Mac", "HmacSHA384");
        provide("Mac", "HmacSHA512");
        provide("Mac", "PBEWITHHMACSHA224");
        provide("Mac", "PBEWITHHMACSHA256");
        provide("Mac", "PBEWITHHMACSHA384");
        provide("Mac", "PBEWITHHMACSHA512");
        // If adding a new MessageDigest, consider adding it to JarVerifier
        provide("MessageDigest", "MD2");
        provide("MessageDigest", "MD5");
        provide("MessageDigest", "SHA-224");
        provide("MessageDigest", "SHA-256");
        provide("MessageDigest", "SHA-384");
        provide("MessageDigest", "SHA-512");
        provide("Policy", "JavaPolicy");
        // Android does not support SSLv3
        if (IS_RI) {
            provide("SSLContext", "SSLv3");
        }
        provide("SSLContext", "TLSv1");
        provide("SSLContext", "TLSv1.1");
        provide("SSLContext", "TLSv1.2");
        provide("SSLContext", "TLSv1.3");
        provide("SecretKeyFactory", "DES");
        provide("SecretKeyFactory", "DESede");
        provide("SecretKeyFactory", "PBEWithMD5AndDES");
        provide("SecretKeyFactory", "PBEWithMD5AndTripleDES");
        provide("SecretKeyFactory", "PBEWithSHA1AndDESede");
        provide("SecretKeyFactory", "PBEWithSHA1AndRC2_40");
        provide("SecretKeyFactory", "PBKDF2WithHmacSHA1");
        provide("SecretKeyFactory", "PBKDF2WithHmacSHA224");
        provide("SecretKeyFactory", "PBKDF2WithHmacSHA256");
        provide("SecretKeyFactory", "PBKDF2WithHmacSHA384");
        provide("SecretKeyFactory", "PBKDF2WithHmacSHA512");
        provide("SecretKeyFactory", "PBKDF2WithHmacSHA1And8bit");
        provide("SecureRandom", "SHA1PRNG");
        provide("Signature", "MD2withRSA");
        provide("Signature", "MD5withRSA");
        provide("Signature", "NONEwithDSA");
        provide("Signature", "SHA1withDSA");
        provide("Signature", "SHA224withDSA");
        provide("Signature", "SHA256withDSA");
        provide("Signature", "SHA1withRSA");
        provide("Signature", "SHA224withRSA");
        provide("Signature", "SHA256withRSA");
        provide("Signature", "SHA384withRSA");
        provide("Signature", "SHA512withRSA");
        provide("TerminalFactory", "PC/SC");
        provide("TransformService", "http://www.w3.org/2000/09/xmldsig#base64");
        provide("TransformService", "http://www.w3.org/2000/09/xmldsig#enveloped-signature");
        provide("TransformService", "http://www.w3.org/2001/10/xml-exc-c14n#");
        provide("TransformService", "http://www.w3.org/2001/10/xml-exc-c14n#WithComments");
        provide("TransformService", "http://www.w3.org/2002/06/xmldsig-filter2");
        provide("TransformService", "http://www.w3.org/TR/1999/REC-xpath-19991116");
        provide("TransformService", "http://www.w3.org/TR/1999/REC-xslt-19991116");
        provide("TransformService", "http://www.w3.org/TR/2001/REC-xml-c14n-20010315");
        provide("TransformService", "http://www.w3.org/TR/2001/REC-xml-c14n-20010315#WithComments");
        provide("TrustManagerFactory", "PKIX");
        provide("XMLSignatureFactory", "DOM");

        // Not clearly documented by RI
        provide("GssApiMechanism", "1.2.840.113554.1.2.2");
        provide("GssApiMechanism", "1.3.6.1.5.5.2");

        // Not correctly documented by RI which left off the Factory suffix
        provide("SaslClientFactory", "CRAM-MD5");
        provide("SaslClientFactory", "DIGEST-MD5");
        provide("SaslClientFactory", "EXTERNAL");
        provide("SaslClientFactory", "GSSAPI");
        provide("SaslClientFactory", "PLAIN");
        provide("SaslServerFactory", "CRAM-MD5");
        provide("SaslServerFactory", "DIGEST-MD5");
        provide("SaslServerFactory", "GSSAPI");

        // Documentation seems to list alias instead of actual name
        // provide("MessageDigest", "SHA-1");
        provide("MessageDigest", "SHA");

        // Mentioned in javadoc, not documentation
        provide("SSLContext", "Default");

        // Not documented as in RI 6 but mentioned in Standard Names
        provide("AlgorithmParameters", "PBE");
        provide("SSLContext", "SSL");
        provide("SSLContext", "TLS");

        // Not documented as in RI 6 but that exist in RI 6
        if (IS_RI) {
            provide("CertStore", "com.sun.security.IndexedCollection");
            provide("KeyGenerator", "SunTlsKeyMaterial");
            provide("KeyGenerator", "SunTlsMasterSecret");
            provide("KeyGenerator", "SunTlsPrf");
            provide("KeyGenerator", "SunTlsRsaPremasterSecret");
            provide("KeyStore", "CaseExactJKS");
            provide("Mac", "HmacPBESHA1");
            provide("Mac", "SslMacMD5");
            provide("Mac", "SslMacSHA1");
            provide("SecureRandom", "NativePRNG");
            provide("Signature", "MD5andSHA1withRSA");
            provide("TrustManagerFactory", "SunX509");
        }

        // Only available with the SunPKCS11-NSS provider,
        // which seems to be enabled in OpenJDK 6 but not Oracle Java 6
        if (Security.getProvider("SunPKCS11-NSS") != null) {
            provide("Cipher", "AES/CBC/NOPADDING");
            provide("Cipher", "DES/CBC/NOPADDING");
            provide("Cipher", "DESEDE/CBC/NOPADDING");
            provide("Cipher", "RSA/ECB/PKCS1PADDING");
            provide("KeyAgreement", "DH");
            provide("KeyFactory", "DH");
            provide("KeyPairGenerator", "DH");
            provide("KeyStore", "PKCS11");
            provide("MessageDigest", "SHA1");
            provide("SecretKeyFactory", "AES");
            provide("SecretKeyFactory", "ARCFOUR");
            provide("SecureRandom", "PKCS11");
            provide("Signature", "DSA");
            provide("Signature", "RAWDSA");
        }

        if (Security.getProvider("SunPKCS11-NSS") != null ||
                Security.getProvider("SunEC") != null) {
            provide("AlgorithmParameters", "EC");
            provide("KeyAgreement", "ECDH");
            provide("KeyFactory", "EC");
            provide("KeyPairGenerator", "EC");
            provide("Signature", "NONEWITHECDSA");
            provide("Signature", "SHA1WITHECDSA");
            provide("Signature", "SHA224WITHECDSA");
            provide("Signature", "SHA256WITHECDSA");
            provide("Signature", "SHA384WITHECDSA");
            provide("Signature", "SHA512WITHECDSA");
        }

        // Documented as Standard Names, but do not exit in RI 6
        if (IS_RI) {
            unprovide("SSLContext", "TLSv1.1");
            unprovide("SSLContext", "TLSv1.2");
        }

        // Fixups for the RI
        if (IS_RI) {
            // different names: Standard Names says PKIX, JSSE Reference Guide says SunX509 or NewSunX509
            unprovide("KeyManagerFactory", "PKIX");
            provide("KeyManagerFactory", "SunX509");
            provide("KeyManagerFactory", "NewSunX509");
        }

        // Fixups for dalvik
        if (!IS_RI) {

            // whole types that we do not provide
            PROVIDER_ALGORITHMS.remove("Configuration");
            PROVIDER_ALGORITHMS.remove("GssApiMechanism");
            PROVIDER_ALGORITHMS.remove("KeyInfoFactory");
            PROVIDER_ALGORITHMS.remove("Policy");
            PROVIDER_ALGORITHMS.remove("SaslClientFactory");
            PROVIDER_ALGORITHMS.remove("SaslServerFactory");
            PROVIDER_ALGORITHMS.remove("TerminalFactory");
            PROVIDER_ALGORITHMS.remove("TransformService");
            PROVIDER_ALGORITHMS.remove("XMLSignatureFactory");

            // different names Diffie-Hellman vs DH
            unprovide("AlgorithmParameterGenerator", "DiffieHellman");
            provide("AlgorithmParameterGenerator", "DH");
            unprovide("AlgorithmParameters", "DiffieHellman");
            provide("AlgorithmParameters", "DH");
            unprovide("KeyAgreement", "DiffieHellman");
            provide("KeyAgreement", "DH");
            unprovide("KeyFactory", "DiffieHellman");
            provide("KeyFactory", "DH");
            unprovide("KeyPairGenerator", "DiffieHellman");
            provide("KeyPairGenerator", "DH");

            // different names PBEWithSHA1AndDESede vs PBEWithSHAAnd3-KEYTripleDES-CBC
            unprovide("AlgorithmParameters", "PBEWithSHA1AndDESede");
            unprovide("Cipher", "PBEWithSHA1AndDESede");
            unprovide("SecretKeyFactory", "PBEWithSHA1AndDESede");
            provide("AlgorithmParameters", "PKCS12PBE");
            provide("Cipher", "PBEWithSHAAnd3-KEYTripleDES-CBC");
            provide("SecretKeyFactory", "PBEWithSHAAnd3-KEYTripleDES-CBC");

            // different names: BouncyCastle actually uses the Standard name of SHA-1 vs SHA
            unprovide("MessageDigest", "SHA");
            provide("MessageDigest", "SHA-1");

            // Added to support Android KeyStore operations
            provide("Signature", "NONEwithRSA");
            provide("Cipher", "RSA/ECB/NOPADDING");
            provide("Cipher", "RSA/ECB/PKCS1PADDING");
            provide("Cipher", "RSA/ECB/OAEPPadding");
            provide("Cipher", "RSA/ECB/OAEPWithSHA-1AndMGF1Padding");
            provide("Cipher", "RSA/ECB/OAEPWithSHA-224AndMGF1Padding");
            provide("Cipher", "RSA/ECB/OAEPWithSHA-256AndMGF1Padding");
            provide("Cipher", "RSA/ECB/OAEPWithSHA-384AndMGF1Padding");
            provide("Cipher", "RSA/ECB/OAEPWithSHA-512AndMGF1Padding");
            provide("SecretKeyFactory", "AES");
            provide("SecretKeyFactory", "HmacSHA1");
            provide("SecretKeyFactory", "HmacSHA224");
            provide("SecretKeyFactory", "HmacSHA256");
            provide("SecretKeyFactory", "HmacSHA384");
            provide("SecretKeyFactory", "HmacSHA512");
            provide("Signature", "SHA1withRSA/PSS");
            provide("Signature", "SHA224withRSA/PSS");
            provide("Signature", "SHA256withRSA/PSS");
            provide("Signature", "SHA384withRSA/PSS");
            provide("Signature", "SHA512withRSA/PSS");

            // different names: ARCFOUR vs ARC4
            unprovide("Cipher", "ARCFOUR");
            provide("Cipher", "ARC4");
            unprovide("KeyGenerator", "ARCFOUR");
            provide("KeyGenerator", "ARC4");

            // different case names: Blowfish vs BLOWFISH
            unprovide("AlgorithmParameters", "Blowfish");
            provide("AlgorithmParameters", "BLOWFISH");
            unprovide("Cipher", "Blowfish");
            provide("Cipher", "BLOWFISH");
            unprovide("KeyGenerator", "Blowfish");
            provide("KeyGenerator", "BLOWFISH");

            // Harmony has X.509, BouncyCastle X509
            // TODO remove one, probably Harmony's
            provide("CertificateFactory", "X509");

            // not just different names, but different binary formats
            unprovide("KeyStore", "JKS");
            provide("KeyStore", "BKS");
            unprovide("KeyStore", "JCEKS");
            provide("KeyStore", "BouncyCastle");

            // Noise to support KeyStore.PKCS12
            provide("Cipher", "PBEWITHMD5AND128BITAES-CBC-OPENSSL");
            provide("Cipher", "PBEWITHMD5AND192BITAES-CBC-OPENSSL");
            provide("Cipher", "PBEWITHMD5AND256BITAES-CBC-OPENSSL");
            provide("Cipher", "PBEWITHMD5ANDRC2");
            provide("Cipher", "PBEWITHSHA1ANDDES");
            provide("Cipher", "PBEWITHSHA1ANDRC2");
            provide("Cipher", "PBEWITHSHA256AND128BITAES-CBC-BC");
            provide("Cipher", "PBEWITHSHA256AND192BITAES-CBC-BC");
            provide("Cipher", "PBEWITHSHA256AND256BITAES-CBC-BC");
            provide("Cipher", "PBEWITHSHAAND128BITAES-CBC-BC");
            provide("Cipher", "PBEWITHSHAAND128BITRC2-CBC");
            provide("Cipher", "PBEWITHSHAAND128BITRC4");
            provide("Cipher", "PBEWITHSHAAND192BITAES-CBC-BC");
            provide("Cipher", "PBEWITHSHAAND2-KEYTRIPLEDES-CBC");
            provide("Cipher", "PBEWITHSHAAND256BITAES-CBC-BC");
            provide("Cipher", "PBEWITHSHAAND40BITRC2-CBC");
            provide("Cipher", "PBEWITHSHAAND40BITRC4");
            provide("Cipher", "PBEWITHSHAANDTWOFISH-CBC");
            provide("Cipher", "PBEWithHmacSHA1AndAES_128");
            provide("Cipher", "PBEWithHmacSHA224AndAES_128");
            provide("Cipher", "PBEWithHmacSHA256AndAES_128");
            provide("Cipher", "PBEWithHmacSHA384AndAES_128");
            provide("Cipher", "PBEWithHmacSHA512AndAES_128");
            provide("Cipher", "PBEWithHmacSHA1AndAES_256");
            provide("Cipher", "PBEWithHmacSHA224AndAES_256");
            provide("Cipher", "PBEWithHmacSHA256AndAES_256");
            provide("Cipher", "PBEWithHmacSHA384AndAES_256");
            provide("Cipher", "PBEWithHmacSHA512AndAES_256");
            provide("Mac", "PBEWITHHMACSHA");
            provide("Mac", "PBEWITHHMACSHA1");
            provide("SecretKeyFactory", "PBEWITHHMACSHA1");
            provide("SecretKeyFactory", "PBEWITHMD5AND128BITAES-CBC-OPENSSL");
            provide("SecretKeyFactory", "PBEWITHMD5AND192BITAES-CBC-OPENSSL");
            provide("SecretKeyFactory", "PBEWITHMD5AND256BITAES-CBC-OPENSSL");
            provide("SecretKeyFactory", "PBEWITHMD5ANDRC2");
            provide("SecretKeyFactory", "PBEWITHSHA1ANDDES");
            provide("SecretKeyFactory", "PBEWITHSHA1ANDRC2");
            provide("SecretKeyFactory", "PBEWITHSHA256AND128BITAES-CBC-BC");
            provide("SecretKeyFactory", "PBEWITHSHA256AND192BITAES-CBC-BC");
            provide("SecretKeyFactory", "PBEWITHSHA256AND256BITAES-CBC-BC");
            provide("SecretKeyFactory", "PBEWITHSHAAND128BITAES-CBC-BC");
            provide("SecretKeyFactory", "PBEWITHSHAAND128BITRC2-CBC");
            provide("SecretKeyFactory", "PBEWITHSHAAND128BITRC4");
            provide("SecretKeyFactory", "PBEWITHSHAAND192BITAES-CBC-BC");
            provide("SecretKeyFactory", "PBEWITHSHAAND2-KEYTRIPLEDES-CBC");
            provide("SecretKeyFactory", "PBEWITHSHAAND256BITAES-CBC-BC");
            provide("SecretKeyFactory", "PBEWITHSHAAND40BITRC2-CBC");
            provide("SecretKeyFactory", "PBEWITHSHAAND40BITRC4");
            provide("SecretKeyFactory", "PBEWITHSHAANDTWOFISH-CBC");

            // Needed by our OpenSSL provider
            provide("Cipher", "AES/CBC/NOPADDING");
            provide("Cipher", "AES/CBC/PKCS5PADDING");
            provide("Cipher", "AES/CBC/PKCS7PADDING");
            provide("Cipher", "AES/CFB/NOPADDING");
            provide("Cipher", "AES/CFB/PKCS5PADDING");
            provide("Cipher", "AES/CFB/PKCS7PADDING");
            provide("Cipher", "AES/CTR/NOPADDING");
            provide("Cipher", "AES/CTR/PKCS5PADDING");
            provide("Cipher", "AES/CTR/PKCS7PADDING");
            provide("Cipher", "AES/ECB/NOPADDING");
            provide("Cipher", "AES/ECB/PKCS5PADDING");
            provide("Cipher", "AES/ECB/PKCS7PADDING");
            provide("Cipher", "AES/GCM/NOPADDING");
            provide("Cipher", "AES/GCM-SIV/NOPADDING");
            provide("Cipher", "AES/OFB/NOPADDING");
            provide("Cipher", "AES/OFB/PKCS5PADDING");
            provide("Cipher", "AES/OFB/PKCS7PADDING");
            provide("Cipher", "AES_128/CBC/NOPADDING");
            provide("Cipher", "AES_128/CBC/PKCS5PADDING");
            provide("Cipher", "AES_128/CBC/PKCS7PADDING");
            provide("Cipher", "AES_128/ECB/NOPADDING");
            provide("Cipher", "AES_128/ECB/PKCS5PADDING");
            provide("Cipher", "AES_128/ECB/PKCS7PADDING");
            provide("Cipher", "AES_128/GCM/NOPADDING");
            provide("Cipher", "AES_128/GCM-SIV/NOPADDING");
            provide("Cipher", "AES_256/CBC/NOPADDING");
            provide("Cipher", "AES_256/CBC/PKCS5PADDING");
            provide("Cipher", "AES_256/CBC/PKCS7PADDING");
            provide("Cipher", "AES_256/ECB/NOPADDING");
            provide("Cipher", "AES_256/ECB/PKCS5PADDING");
            provide("Cipher", "AES_256/ECB/PKCS7PADDING");
            provide("Cipher", "AES_256/GCM/NOPADDING");
            provide("Cipher", "AES_256/GCM-SIV/NOPADDING");
            provide("Cipher", "DESEDE/CBC/NOPADDING");
            provide("Cipher", "DESEDE/CBC/PKCS5PADDING");
            provide("Cipher", "DESEDE/CBC/PKCS7PADDING");
            provide("Cipher", "DESEDE/CFB/NOPADDING");
            provide("Cipher", "DESEDE/CFB/PKCS5PADDING");
            provide("Cipher", "DESEDE/CFB/PKCS7PADDING");
            provide("Cipher", "DESEDE/ECB/NOPADDING");
            provide("Cipher", "DESEDE/ECB/PKCS5PADDING");
            provide("Cipher", "DESEDE/ECB/PKCS7PADDING");
            provide("Cipher", "DESEDE/OFB/NOPADDING");
            provide("Cipher", "DESEDE/OFB/PKCS5PADDING");
            provide("Cipher", "DESEDE/OFB/PKCS7PADDING");

            // Provided by our OpenSSL provider
            provide("AlgorithmParameters", "ChaCha20");
            provide("Cipher", "ChaCha20");
            provide("Cipher", "ChaCha20/Poly1305/NoPadding");
            provide("KeyGenerator", "ChaCha20");
            provideCipherPaddings("AES", new String[] { "PKCS7Padding" });

            // removed LDAP
            unprovide("CertStore", "LDAP");

            // removed MD2
            unprovide("MessageDigest", "MD2");
            unprovide("Signature", "MD2withRSA");

            // removed RC2
            // NOTE the implementation remains to support PKCS12 keystores
            unprovide("AlgorithmParameters", "PBEWithSHA1AndRC2_40");
            unprovide("AlgorithmParameters", "RC2");
            unprovide("Cipher", "PBEWithSHA1AndRC2_40");
            unprovide("Cipher", "RC2");
            unprovide("KeyGenerator", "RC2");
            unprovide("SecretKeyFactory", "PBEWithSHA1AndRC2_40");

            // PBEWithMD5AndTripleDES is Sun proprietary
            unprovide("AlgorithmParameters", "PBEWithMD5AndTripleDES");
            unprovide("Cipher", "PBEWithMD5AndTripleDES");
            unprovide("SecretKeyFactory", "PBEWithMD5AndTripleDES");

            // missing from Bouncy Castle
            // Standard Names document says to use specific PBEWith*And*
            unprovide("AlgorithmParameters", "PBE");

            // missing from Bouncy Castle
            // TODO add to JDKAlgorithmParameters perhaps as wrapper on PBES2Parameters
            // For now, can use AlgorithmParametersSpec javax.crypto.spec.PBEParameterSpec instead
            unprovide("AlgorithmParameters", "PBEWithMD5AndDES"); // 1.2.840.113549.1.5.3

            // EC support
            provide("AlgorithmParameters", "EC");
            provide("KeyAgreement", "ECDH");
            provide("KeyFactory", "EC");
            provide("KeyPairGenerator", "EC");
            provide("Signature", "NONEWITHECDSA");
            provide("Signature", "SHA1WITHECDSA");
            provide("Signature", "SHA224WITHECDSA");
            provide("Signature", "SHA256WITHECDSA");
            provide("Signature", "SHA384WITHECDSA");
            provide("Signature", "SHA512WITHECDSA");

            // Android's CA store
            provide("KeyStore", "AndroidCAStore");

            // Android's KeyStore provider
            if (Security.getProvider("AndroidKeyStore") != null) {
                provide("KeyStore", "AndroidKeyStore");
            }

            // TimaKeyStore provider
            if (Security.getProvider("TimaKeyStore") != null) {
                provide("KeyStore", "TimaKeyStore");
            }
            // KnoxAndroidKeyStore provider
            if (Security.getProvider("KnoxAndroidKeyStore") != null) {
                provide("KeyStore", "KnoxAndroidKeyStore");
            }
        }
    }

    public static final Set<String> KEY_TYPES = new HashSet<>(Arrays.asList(
            "RSA",
            "DSA",
            "DH_RSA",
            "DH_DSA",
            "EC",
            "EC_EC",
            "EC_RSA"));
    static {
        if (IS_RI) {
            // DH_* are specified by standard names, but do not seem to be supported by RI
            KEY_TYPES.remove("DH_RSA");
            KEY_TYPES.remove("DH_DSA");
        }
    }

    /**
     * Valid values for X509TrustManager.checkClientTrusted authType,
     * either the algorithm of the public key or UNKNOWN.
     */
    public static final Set<String> CLIENT_AUTH_TYPES = new HashSet<>(Arrays.asList(
            "RSA",
            "DSA",
            "EC",
            "UNKNOWN"));

    /**
     * Valid values for X509TrustManager.checkServerTrusted authType,
     * either key exchange algorithm part of the cipher suite, UNKNOWN,
     * or GENERIC (for TLS 1.3 cipher suites that don't imply a specific
     * key exchange method).
     */
    public static final Set<String> SERVER_AUTH_TYPES = new HashSet<>(Arrays.asList(
            "DHE_DSS",
            "DHE_DSS_EXPORT",
            "DHE_RSA",
            "DHE_RSA_EXPORT",
            "DH_DSS_EXPORT",
            "DH_RSA_EXPORT",
            "DH_anon",
            "DH_anon_EXPORT",
            "KRB5",
            "KRB5_EXPORT",
            "RSA",
            "RSA_EXPORT",
            "RSA_EXPORT1024",
            "ECDH_ECDSA",
            "ECDH_RSA",
            "ECDHE_ECDSA",
            "ECDHE_RSA",
            "UNKNOWN",
            "GENERIC"));

    /**
     * Cipher suites that are only supported with TLS 1.3.
     */
    public static final List<String> CIPHER_SUITES_TLS13 = Arrays.asList(
            "TLS_AES_128_GCM_SHA256",
            "TLS_AES_256_GCM_SHA384",
            "TLS_CHACHA20_POLY1305_SHA256");

    // NOTE: This list needs to be kept in sync with Javadoc of javax.net.ssl.SSLSocket and
    // javax.net.ssl.SSLEngine.
    private static final List<String> CIPHER_SUITES_ANDROID_AES_HARDWARE = Arrays.asList(
            "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",
            "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384",
            "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256",
            "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
            "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
            "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256",
            "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA",
            "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA",
            "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",
            "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",
            "TLS_RSA_WITH_AES_128_GCM_SHA256",
            "TLS_RSA_WITH_AES_256_GCM_SHA384",
            "TLS_RSA_WITH_AES_128_CBC_SHA",
            "TLS_RSA_WITH_AES_256_CBC_SHA",
            CIPHER_SUITE_SECURE_RENEGOTIATION
    );

    // NOTE: This list needs to be kept in sync with Javadoc of javax.net.ssl.SSLSocket and
    // javax.net.ssl.SSLEngine.
    private static final List<String> CIPHER_SUITES_ANDROID_SOFTWARE = Arrays.asList(
            "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256",
            "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",
            "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384",
            "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256",
            "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
            "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",
            "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA",
            "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA",
            "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",
            "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",
            "TLS_RSA_WITH_AES_128_GCM_SHA256",
            "TLS_RSA_WITH_AES_256_GCM_SHA384",
            "TLS_RSA_WITH_AES_128_CBC_SHA",
            "TLS_RSA_WITH_AES_256_CBC_SHA",
            CIPHER_SUITE_SECURE_RENEGOTIATION
    );

    // NOTE: This list needs to be kept in sync with Javadoc of javax.net.ssl.SSLSocket and
    // javax.net.ssl.SSLEngine.
    public static final List<String> CIPHER_SUITES_DEFAULT = (IS_RI)
            ? Arrays.asList("TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384",
                            "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384",
                            "TLS_RSA_WITH_AES_256_CBC_SHA256",
                            "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384",
                            "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384",
                            "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256",
                            "TLS_DHE_DSS_WITH_AES_256_CBC_SHA256",
                            "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA",
                            "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",
                            "TLS_RSA_WITH_AES_256_CBC_SHA",
                            "TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA",
                            "TLS_ECDH_RSA_WITH_AES_256_CBC_SHA",
                            "TLS_DHE_RSA_WITH_AES_256_CBC_SHA",
                            "TLS_DHE_DSS_WITH_AES_256_CBC_SHA",
                            "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256",
                            "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256",
                            "TLS_RSA_WITH_AES_128_CBC_SHA256",
                            "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256",
                            "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256",
                            "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256",
                            "TLS_DHE_DSS_WITH_AES_128_CBC_SHA256",
                            "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA",
                            "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",
                            "TLS_RSA_WITH_AES_128_CBC_SHA",
                            "TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA",
                            "TLS_ECDH_RSA_WITH_AES_128_CBC_SHA",
                            "TLS_DHE_RSA_WITH_AES_128_CBC_SHA",
                            "TLS_DHE_DSS_WITH_AES_128_CBC_SHA",
                            "TLS_ECDHE_ECDSA_WITH_RC4_128_SHA",
                            "TLS_ECDHE_RSA_WITH_RC4_128_SHA",
                            "SSL_RSA_WITH_RC4_128_SHA",
                            "TLS_ECDH_ECDSA_WITH_RC4_128_SHA",
                            "TLS_ECDH_RSA_WITH_RC4_128_SHA",
                            "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA",
                            "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA",
                            "SSL_RSA_WITH_3DES_EDE_CBC_SHA",
                            "TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA",
                            "TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA",
                            "SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA",
                            "SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA",
                            "SSL_RSA_WITH_RC4_128_MD5",
                            "TLS_EMPTY_RENEGOTIATION_INFO_SCSV")
            : CpuFeatures.isAesHardwareAccelerated() ? CIPHER_SUITES_ANDROID_AES_HARDWARE
                    : CIPHER_SUITES_ANDROID_SOFTWARE;

    private static final Map<String, Class<? extends KeySpec>> PRIVATE_KEY_SPEC_CLASSES;
    private static final Map<String, Class<? extends KeySpec>> PUBLIC_KEY_SPEC_CLASSES;
    private static final Map<String, Integer> MINIMUM_KEY_SIZE;
    static {
        PRIVATE_KEY_SPEC_CLASSES = new HashMap<>();
        PUBLIC_KEY_SPEC_CLASSES = new HashMap<>();
        MINIMUM_KEY_SIZE = new HashMap<>();
        PRIVATE_KEY_SPEC_CLASSES.put("RSA", RSAPrivateCrtKeySpec.class);
        PUBLIC_KEY_SPEC_CLASSES.put("RSA", RSAPublicKeySpec.class);
        MINIMUM_KEY_SIZE.put("RSA", 512);
        PRIVATE_KEY_SPEC_CLASSES.put("DSA", DSAPrivateKeySpec.class);
        PUBLIC_KEY_SPEC_CLASSES.put("DSA", DSAPublicKeySpec.class);
        MINIMUM_KEY_SIZE.put("DSA", 512);
        PRIVATE_KEY_SPEC_CLASSES.put("DH", DHPrivateKeySpec.class);
        PUBLIC_KEY_SPEC_CLASSES.put("DH", DHPublicKeySpec.class);
        MINIMUM_KEY_SIZE.put("DH", 256);
        PRIVATE_KEY_SPEC_CLASSES.put("EC", ECPrivateKeySpec.class);
        PUBLIC_KEY_SPEC_CLASSES.put("EC", ECPublicKeySpec.class);
        MINIMUM_KEY_SIZE.put("EC", 256);
    }

    public static Class<? extends KeySpec> getPrivateKeySpecClass(String algName) {
        return PRIVATE_KEY_SPEC_CLASSES.get(algName);
    }

    public static Class<? extends KeySpec> getPublicKeySpecClass(String algName) {
        return PUBLIC_KEY_SPEC_CLASSES.get(algName);
    }

    public static int getMinimumKeySize(String algName) {
        return MINIMUM_KEY_SIZE.get(algName);
    }

}
