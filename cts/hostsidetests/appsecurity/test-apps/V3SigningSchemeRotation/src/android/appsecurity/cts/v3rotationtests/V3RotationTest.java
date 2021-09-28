/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.appsecurity.cts.v3rotationtests;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.test.AndroidTestCase;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * On-device tests for APK Signature Scheme v3 based signing certificate rotation
 */
public class V3RotationTest extends AndroidTestCase {

    private static final String PKG = "android.appsecurity.cts.tinyapp";
    private static final String COMPANION_PKG = "android.appsecurity.cts.tinyapp_companion";
    private static final String PERMISSION_NAME = "android.appsecurity.cts.tinyapp.perm";

    private static final String FIRST_CERT_HEX =
            "308203773082025fa0030201020204357f7a97300d06092a864886f70d01010b0500306c310b3009060355"
            + "040613025553310b3009060355040813024341311630140603550407130d4d6f756e7461696e20566965"
            + "773110300e060355040a1307416e64726f69643110300e060355040b1307556e6b6e6f776e3114301206"
            + "03550403130b477265656e2044726f6964301e170d3138303133313138303331345a170d343530363138"
            + "3138303331345a306c310b3009060355040613025553310b300906035504081302434131163014060355"
            + "0407130d4d6f756e7461696e20566965773110300e060355040a1307416e64726f69643110300e060355"
            + "040b1307556e6b6e6f776e311430120603550403130b477265656e2044726f696430820122300d06092a"
            + "864886f70d01010105000382010f003082010a0282010100bce0517978db6e1eb0255b4704c10fca31b8"
            + "d8b7465d1512a36712f425009143d418743cbde9c9f7df219907fff376e44298462fb14f5dd7d0edd46e"
            + "d764f93e82b277146f100616c4d93e97279aefdcf449a5ff000eb72445816039c7afdcd9340f39fc5f0b"
            + "b4d0e6c4d67a0ec876bf28007cf709667250142385248af89c5fa87b8ef9cc1270577b1721c235bdde97"
            + "87c04536706947193d38d244c8c6590e54c90b3843506b3e19c5f2c22c38a1a2893c749ad4ddce67b86f"
            + "f5dcd7dd0d63265fa50036f4bf31f28f2b4c067fb58c95e09ab6c1686c257c3730616659b352b966ed66"
            + "d93952c60563c9d863aa37c097f646d55c825b450f511eca39198cd70203010001a321301f301d060355"
            + "1d0e04160414831074e3173e51fbd2ac8ed8c6681f61eca81318300d06092a864886f70d01010b050003"
            + "820101001fa412618659f40395e38698cd8b7ca9425f63e231e39a6c8e2909af951bbdc5e6307813e25e"
            + "31502578a56b6e6f08442a400de19449f1c7f87357e55191fd7f30b8735e1298d4c668b9cde563e143d4"
            + "72bf8005443d8ee386f955136ad0aa16dda7f5b977dc0dee858f954aff2ae4d7473259fb1b1acc1d6161"
            + "49b12659371ae0298222974be10817156279c9dd8f6cae5265bf15b63fa9f084e789d06b3a9b86d82749"
            + "c95793acded24b321a31cc20fd12774c7b207325df50c2efcb78a5cf9749f62aafd86d303254880a4476"
            + "a67801452e82d1e6c91aeb97ca837a91bcefd8324fca491de4ef648d80c6d74f4b66533684040ddad550"
            + "0ff140edcdacf0a4";

    private static final String SECOND_CERT_HEX =
            "3082037b30820263a00302010202044c4a644a300d06092a864886f70d01010b0500306e310b3009060355"
            + "040613025553310b3009060355040813024341311630140603550407130d4d6f756e7461696e20566965"
            + "773110300e060355040a1307416e64726f69643110300e060355040b1307556e6b6e6f776e3116301406"
            + "03550403130d477265656e65722044726f6964301e170d3138303133313138303533385a170d34353036"
            + "31383138303533385a306e310b3009060355040613025553310b30090603550408130243413116301406"
            + "03550407130d4d6f756e7461696e20566965773110300e060355040a1307416e64726f69643110300e06"
            + "0355040b1307556e6b6e6f776e311630140603550403130d477265656e65722044726f69643082012230"
            + "0d06092a864886f70d01010105000382010f003082010a0282010100a4f3741edf04e32e7047aaadc2ce"
            + "de0164a2847149aa7fad054dab59e70c1078dd2d5946cec64809f79551395478b08f3e0286d452fa39b9"
            + "e6804e965a44210a61bd5cfe3c8ac0ec86017c0e91260248daa1fd7a11b1b1b519216a40d02db49e754a"
            + "6380357c45ef1531996e4c37c13e2f507f3a9296eb52235248d0172efe4ed7bac537fe2435f03d66c5e3"
            + "e5cbf60d77f3088bc22718ded09009d6414106d1301d1a5abf71aa5dc6469bb73b29b2cc9c09b9c42e8f"
            + "8e81e7fc9b24ad1cb0c3e2e2832d0570bdeb87a3ff8f49aae05b03164fb3c225745b70134c6b7aaf081f"
            + "514058776bd28a0c0a152bf45b3eddc7f2c4aaed4eace0cab67ef4650213303f0203010001a321301f30"
            + "1d0603551d0e04160414f0c3dacf02d9583200d4d386a40341aee82dd859300d06092a864886f70d0101"
            + "0b050003820101003a63a93be986740158574239310e987cdcdc86f735867789c2b56f8ffa7ce901a9f2"
            + "2b0c374d345aa996a37c7474824dbf72186d1eedaa6aae0574fd1996f501def32e62703ee4010a7058df"
            + "f0d608df1176bdbe93ebd3910172fc146307d0961a57d95806eeabc1e54b81273d9449f0f68511288377"
            + "c82f87a032d1379260365ef83c93f1bf4c0af959b426794c558c0357fe61271ed0fb054e916e0bab8600"
            + "3bcc16ce53f7d2c8d05b83072115b0fb2c671c2266813f1a407ca911c47f27665debfa63bc4e4071730a"
            + "a477acbe6eecc8d575511b2b77a3551f040ccf21792f74cd95c84e3ff87ad03851db81d164c836830e31"
            + "8208a52dcdc77e376f73b96d";

    // These are the hex encodings of the der form of the pkgsigverify/ec-p256[_2] certs used to
    // sign APKs that are part of this test.
    private static final String EC_P256_FIRST_CERT_HEX =
            "3082016c30820111a003020102020900ca0fb64dfb66e772300a06082a86"
                    + "48ce3d04030230123110300e06035504030c0765632d70323536301e170d"
                    + "3136303333313134353830365a170d3433303831373134353830365a3012"
                    + "3110300e06035504030c0765632d703235363059301306072a8648ce3d02"
                    + "0106082a8648ce3d03010703420004a65f113d22cb4913908307ac31ee2b"
                    + "a0e9138b785fac6536d14ea2ce90d2b4bfe194b50cdc8e169f54a73a991e"
                    + "f0fa76329825be078cc782740703da44b4d7eba350304e301d0603551d0e"
                    + "04160414d4133568b95b30158b322071ea8c43ff5b05ccc8301f0603551d"
                    + "23041830168014d4133568b95b30158b322071ea8c43ff5b05ccc8300c06"
                    + "03551d13040530030101ff300a06082a8648ce3d04030203490030460221"
                    + "00f504a0866caef029f417142c5cb71354c79ffcd1d640618dfca4f19e16"
                    + "db78d6022100f8eea4829799c06cad08c6d3d2d2ec05e0574154e747ea0f"
                    + "dbb8042cb655aadd";

    private static final String EC_P256_SECOND_CERT_HEX =
            "3082016d30820113a0030201020209008855bd1dd2b2b225300a06082a86"
                    + "48ce3d04030230123110300e06035504030c0765632d70323536301e170d"
                    + "3138303731333137343135315a170d3238303731303137343135315a3014"
                    + "3112301006035504030c0965632d703235365f323059301306072a8648ce"
                    + "3d020106082a8648ce3d030107034200041d4cca0472ad97ee3cecef0da9"
                    + "3d62b450c6788333b36e7553cde9f74ab5df00bbba6ba950e68461d70bbc"
                    + "271b62151dad2de2bf6203cd2076801c7a9d4422e1a350304e301d060355"
                    + "1d0e041604147991d92b0208fc448bf506d4efc9fff428cb5e5f301f0603"
                    + "551d23041830168014d4133568b95b30158b322071ea8c43ff5b05ccc830"
                    + "0c0603551d13040530030101ff300a06082a8648ce3d0403020348003045"
                    + "02202769abb1b49fc2f53479c4ae92a6631dabfd522c9acb0bba2b43ebeb"
                    + "99c63011022100d260fb1d1f176cf9b7fa60098bfd24319f4905a3e5fda1"
                    + "00a6fe1a2ab19ff09e";

    public void testHasPerm() throws Exception {
        PackageManager pm = getContext().getPackageManager();
        assertTrue(PERMISSION_NAME + " not granted to " + COMPANION_PKG,
                pm.checkPermission(PERMISSION_NAME, COMPANION_PKG)
                        == PackageManager.PERMISSION_GRANTED);
    }

    public void testHasNoPerm() throws Exception {
        PackageManager pm = getContext().getPackageManager();
        assertFalse(PERMISSION_NAME + " granted to " + COMPANION_PKG,
                pm.checkPermission(PERMISSION_NAME, COMPANION_PKG)
                        == PackageManager.PERMISSION_GRANTED);
    }

    public void testGetSignaturesShowsOld() throws Exception {
        // to prevent breakage due to signing certificate rotation, we report the oldest signing
        // certificate in the now-deprecated PackageInfo signatures field.  Make sure that when
        // rotating, it still displays the older cert.
        PackageManager pm = getContext().getPackageManager();
        PackageInfo pi = pm.getPackageInfo(PKG, PackageManager.GET_SIGNATURES);
        assertNotNull("Failed to get signatures in PackageInfo of " + PKG, pi.signatures);
        assertEquals("PackageInfo for " + PKG + "contains multiple entries",
                1, pi.signatures.length);
        assertEquals("signature mismatch for " + PKG + ", expected old signing certificate",
                pi.signatures[0].toCharsString(), FIRST_CERT_HEX);
    }

    public void testGetSigningCertificatesShowsAll() throws Exception {
        // make sure our shiny new GET_SIGNATURES replacement does its job.  It should return all of
        // the certificates in an app's provided history, including its current one
        PackageManager pm = getContext().getPackageManager();
        PackageInfo pi = pm.getPackageInfo(PKG, PackageManager.GET_SIGNING_CERTIFICATES);
        assertNotNull("Failed to get signatures in PackageInfo of " + PKG,
                pi.signingInfo);
        assertFalse("Multiple signing certificates found in signing certificate history for " + PKG,
                pi.signingInfo.hasMultipleSigners());
        Signature[] signingHistory = pi.signingInfo.getSigningCertificateHistory();
        int numSigningSets = signingHistory.length;
        assertEquals("PackageInfo for " + PKG + "contains the wrong number of signing certificat "
                + " rotations.  Expected 2 (corresponding to one rotation). Found: "
                + numSigningSets, 2, numSigningSets);
        // check to see if we match the certs for which we're looking
        boolean matchedFirst = false;
        boolean matchedSecond = false;
        for (int i = 0; i < numSigningSets; i++) {
            String reportedCert = signingHistory[i].toCharsString();
            if (FIRST_CERT_HEX.equals(reportedCert)) {
                matchedFirst = true;
            } else if (SECOND_CERT_HEX.equals(reportedCert)) {
                matchedSecond = true;
            }
        }
        assertTrue("Old signing certificate not found for " + PKG + " expected "
                + FIRST_CERT_HEX, matchedFirst);
        assertTrue("Current signing certificate not found for " + PKG + " expected "
                + SECOND_CERT_HEX, matchedSecond);
    }

    public void testGetApkContentsSignersShowsCurrent() throws Exception {
        // The SigningInfo instance returned from GET_SIGNING_CERTIFICATES provides an option to
        // obtain only the current signer through getApkContentsSigners.
        PackageManager pm = getContext().getPackageManager();
        PackageInfo pi = pm.getPackageInfo(PKG, PackageManager.GET_SIGNING_CERTIFICATES);
        assertNotNull("Failed to get signatures in Package Info of " + PKG, pi.signingInfo);
        assertFalse("Multiple signing certificates found in signing certificate history for " + PKG,
                pi.signingInfo.hasMultipleSigners());
        assertExpectedSignatures(pi.signingInfo.getApkContentsSigners(), EC_P256_SECOND_CERT_HEX);
    }

    public void testGetApkContentsSignersShowsMultipleSigners() throws Exception {
        // Similar to the test above when GET_SIGNING_CERTIFICATES is used to obtain the signers
        // getApkContentSigners should return all of the current signatures when there are multiple
        // V1 / V2 signers.
        PackageManager pm = getContext().getPackageManager();
        PackageInfo pi = pm.getPackageInfo(PKG, PackageManager.GET_SIGNING_CERTIFICATES);
        assertNotNull("Failed to get signatures in PackageInfo of " + PKG,
                pi.signingInfo);
        assertTrue("Multiple signing certificates should have been reported for " + PKG,
                pi.signingInfo.hasMultipleSigners());
        assertExpectedSignatures(pi.signingInfo.getApkContentsSigners(), EC_P256_FIRST_CERT_HEX,
                EC_P256_SECOND_CERT_HEX);
    }

    public void testHasSigningCertificate() throws Exception {
        // make sure that hasSigningCertificate() reports that both certificates in the signing
        // history are present
        PackageManager pm = getContext().getPackageManager();
        byte[] firstCertBytes = fromHexToByteArray(FIRST_CERT_HEX);
        assertTrue("Old signing certificate not found for " + PKG,
               pm.hasSigningCertificate(PKG, firstCertBytes, PackageManager.CERT_INPUT_RAW_X509));
        byte[] secondCertBytes = fromHexToByteArray(SECOND_CERT_HEX);
        assertTrue("Current signing certificate not found for " + PKG,
                pm.hasSigningCertificate(PKG, secondCertBytes, PackageManager.CERT_INPUT_RAW_X509));
    }

    public void testHasSigningCertificateSha256() throws Exception {
        // make sure that hasSigningCertificate() reports that both certificates in the signing
        // history are present
        PackageManager pm = getContext().getPackageManager();
        byte[] firstCertBytes = computeSha256DigestBytes(fromHexToByteArray(FIRST_CERT_HEX));

        assertTrue("Old signing certificate not found for " + PKG,
                pm.hasSigningCertificate(PKG, firstCertBytes, PackageManager.CERT_INPUT_SHA256));
        byte[] secondCertBytes = computeSha256DigestBytes(fromHexToByteArray(SECOND_CERT_HEX));
        assertTrue("Current signing certificate not found for " + PKG,
                pm.hasSigningCertificate(PKG, secondCertBytes, PackageManager.CERT_INPUT_SHA256));
    }

    public void testHasSigningCertificateByUid() throws Exception {
        // make sure that hasSigningCertificate() reports that both certificates in the signing
        // history are present
        PackageManager pm = getContext().getPackageManager();
        int uid = pm.getPackageUid(PKG, 0);
        byte[] firstCertBytes = fromHexToByteArray(FIRST_CERT_HEX);
        assertTrue("Old signing certificate not found for " + PKG,
                pm.hasSigningCertificate(uid, firstCertBytes, PackageManager.CERT_INPUT_RAW_X509));
        byte[] secondCertBytes = fromHexToByteArray(SECOND_CERT_HEX);
        assertTrue("Current signing certificate not found for " + PKG,
                pm.hasSigningCertificate(uid, secondCertBytes, PackageManager.CERT_INPUT_RAW_X509));
    }

    public void testHasSigningCertificateByUidSha256() throws Exception {
        // make sure that hasSigningCertificate() reports that both certificates in the signing
        // history are present
        PackageManager pm = getContext().getPackageManager();
        int uid = pm.getPackageUid(PKG, 0);
        byte[] firstCertBytes = computeSha256DigestBytes(fromHexToByteArray(FIRST_CERT_HEX));

        assertTrue("Old signing certificate not found for " + PKG,
                pm.hasSigningCertificate(uid, firstCertBytes, PackageManager.CERT_INPUT_SHA256));
        byte[] secondCertBytes = computeSha256DigestBytes(fromHexToByteArray(SECOND_CERT_HEX));
        assertTrue("Current signing certificate not found for " + PKG,
                pm.hasSigningCertificate(uid, secondCertBytes, PackageManager.CERT_INPUT_SHA256));
    }

    private  static byte[] fromHexToByteArray(String str) {
        if (str == null || str.length() == 0 || str.length() % 2 != 0) {
            return null;
        }

        final char[] chars = str.toCharArray();
        final int charLength = chars.length;
        final byte[] bytes = new byte[charLength / 2];

        for (int i = 0; i < bytes.length; i++) {
            bytes[i] =
                    (byte)(((getIndex(chars[i * 2]) << 4) & 0xF0)
                            | (getIndex(chars[i * 2 + 1]) & 0x0F));
        }
        return bytes;
    }

    // copy of ByteStringUtils - lowercase version (to match inputs)
    private static int getIndex(char c) {
        final char[] HEX_ARRAY = "0123456789abcdef".toCharArray();
        for (int i = 0; i < HEX_ARRAY.length; i++) {
            if (HEX_ARRAY[i] == c) {
                return i;
            }
        }
        return -1;
    }

    private static byte[] computeSha256DigestBytes(byte[] data) throws NoSuchAlgorithmException {
        MessageDigest messageDigest = MessageDigest.getInstance("SHA256");
        messageDigest.update(data);
        return messageDigest.digest();
    }

    private static void assertExpectedSignatures(Signature[] signatures,
            String... expectedSignatures) throws Exception {
        int numSigners = signatures.length;
        assertEquals("An unexpected number of signatures was returned, expected "
                        + expectedSignatures.length + ", but received " + signatures.length,
                expectedSignatures.length, numSigners);
        Set<String> expectedSignatureSet = new HashSet<>(Arrays.asList(expectedSignatures));
        for (int i = 0; i < numSigners; i++) {
            String reportedCert = signatures[i].toCharsString();
            // Remove the reported certificate from the set to ensure duplicates are not matched.
            if (!expectedSignatureSet.remove(reportedCert)) {
                fail("Received an unexpected signature during the test: " + reportedCert);
            }
        }
    }
}
