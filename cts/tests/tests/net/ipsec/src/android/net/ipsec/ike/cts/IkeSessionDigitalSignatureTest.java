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

import android.net.InetAddresses;
import android.net.LinkAddress;
import android.net.ipsec.ike.IkeDerAsn1DnIdentification;
import android.net.ipsec.ike.IkeSession;
import android.net.ipsec.ike.IkeSessionParams;
import android.net.ipsec.ike.IkeTrafficSelector;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.internal.net.ipsec.ike.testutils.CertUtils;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.InetAddress;
import java.security.cert.X509Certificate;
import java.security.interfaces.RSAPrivateKey;
import java.util.ArrayList;
import java.util.Arrays;

import javax.security.auth.x500.X500Principal;

/**
 * Explicitly test setting up transport mode Child SA so that devices do not have
 * FEATURE_IPSEC_TUNNELS will be test covered. Tunnel mode Child SA setup has been tested in
 * IkeSessionPskTest and authentication method is orthogonal to Child mode.
 */
@RunWith(AndroidJUnit4.class)
public class IkeSessionDigitalSignatureTest extends IkeSessionTestBase {
    private static final int EXPECTED_AUTH_REQ_FRAG_COUNT = 3;

    private static final String IKE_INIT_RESP =
            "46B8ECA1E0D72A18BF3FA1C2CB1EE86F21202220000000000000015022000030"
                    + "0000002C010100040300000C0100000C800E0100030000080300000503000008"
                    + "0200000400000008040000022800008800020000328451C8A976CE69E407966A"
                    + "50D7320C4197A15A07267CE1B16BAFF9BDBBDEC1FDCDAAF7175ADF9AA8DB55DB"
                    + "2D70C012D01D914C4EDEF6E8B226868EA1D01B2ED0C4C5C86E6BFE566010EC0C"
                    + "33BA1C93666430B88BDA0470D82CC4F4416F49E3E361E3017C9F27811A66718B"
                    + "389E1800915D776D59AA528A7E1D1B7815D35144290000249FE8FABE7F43D917"
                    + "CE370DE2FD9C22BBC082951AC26C1BA26DE795470F2C25BC2900001C00004004"
                    + "AE388EC86D6D1A470D44142D01AB2E85A7AC14182900001C0000400544A235A4"
                    + "171C884286B170F48FFC181DB428D87D290000080000402E290000100000402F"
                    + "00020003000400050000000800004014";
    private static final String IKE_AUTH_RESP_FRAG_1 =
            "46B8ECA1E0D72A18BF3FA1C2CB1EE86F3520232000000001000004E0240004C4"
                    + "00010002DF6750A2D1D5675006F9F6230BB886FFD20CFB973FD04963CFD7A528"
                    + "560598C58CC44178B2FCBBBBB271387AC81A664B7E7F1055B912F8C686E287C9"
                    + "D31684C66339151AB86DA3CF1DA664052FA97687634558A1E9E6B37E16A86BD1"
                    + "68D76DA5E2E1E0B7E98EB662D80D542307015D2BF134EBBBE425D6954FE8C2C4"
                    + "D31D16C16AA0521C3C481F873ECF25BB8B05AC6083775C1821CAAB1E35A3955D"
                    + "85ACC599574142E1DD5B262D6E5365CBF6EBE92FFCC16BC29EC3239456F3B202"
                    + "492551C0F6D752ADCCA56D506D50CC8809EF6BC56EAD005586F7168F76445FD3"
                    + "1366CC62D32C0C19B28210B8F813F97CD6A447C3857EFD6EC483DDA8ACD9870E"
                    + "5A21B9C66F0FA44496C0C3D05E8859A1A4CFC88155D0C411BABC13033DD41FA4"
                    + "AF08CE7734A146687F374F95634D1F26843203CA1FFD05CA3EB150CEA02FBF14"
                    + "712B7A1C9BC7616A086E7FCA059E7D64EFF98DB895B32F8F7002762AF7D12F23"
                    + "31E9DD25174C4CE273E5392BBB48F50B7A3E0187181216265F6A4FC7B91BE0AB"
                    + "C601A580149D4B07411AE99DDB1944B977E86ADC9746605C60A92B569EEFAFFC"
                    + "3A888D187B75D8F13249689FC28EBCD62B5E03AF171F3A561F0DEA3B1A75F531"
                    + "971157DCE1E7BC6E7789FF3E8156015BC9C521EFE48996B41471D33BF09864E4"
                    + "2436E8D7EB6218CDE7716DA754A924B123A63E25585BF27F4AC043A0C4AECE38"
                    + "BB59DD62F5C0EC657206A76CED1BD26262237DA1CA6815435992A825758DEBEC"
                    + "DDF598A22B8242AC4E34E70704DBA7B7B73DC3E067C1C98764F8791F84C99156"
                    + "947D1FFC875F36FCE24B89369C1B5BF1D4C999DCA28E72A528D0E0163C66C067"
                    + "E71B5E0025C13DA93313942F9EDA230B3ADC254821A4CB1A5DC9D0C5F4DC4E8E"
                    + "CE46B7B8C72D3C5923C9B30DF1EF7B4EDEDA8BD05C86CA0162AE1BF8F277878E"
                    + "607401BAA8F06E3EA873FA4C137324C4E0699277CDF649FE7F0F01945EE25FA7"
                    + "0E4A89737E58185B11B4CB52FD5B0497D3E3CD1CEE7B1FBB3E969DB6F4C324A1"
                    + "32DC6A0EA21F41332435FD99140C286F8ABBBA926953ADBEED17D30AAD953909"
                    + "1347EF6D87163D6B1FF32D8B11FFB2E69FAEE7FE913D3826FBA7F9D11E0E3C57"
                    + "27625B37D213710B5DD8965DAEFD3F491E8C029E2BF361039949BADEC31D60AC"
                    + "355F26EE41339C03CC9D9B01C3C7F288F0E9D6DFEE78231BDA9AC10FED135913"
                    + "2836B1A17CE060742B7E5B738A7177CCD59F70337BA251409C377A0FA5333204"
                    + "D8622BA8C06DE0BEF4F32B6D4D77BE9DE977445D8A2A08C5C38341CB7974FBFB"
                    + "22C8F983A7D6CEF068DDB2281E6673453521C831C1826861005AE5F37649BC64"
                    + "0A6360B23284861441A440F1C5AADE1AB53CA63DB17F4C314D493C4C44DE5F20"
                    + "75E084D080F92791F30BDD88373D50AB5A07BC72B0E7FFFA593103964E55603E"
                    + "F7FEB7CA0762A1A7B86B6CCAD88CD6CBC7C6935D21F5F06B2700588A2530E619"
                    + "DA1648AC809F3DDF56ACE5951737568FFEC7E2AB1AA0AE01B03A7F5A29CE73C0"
                    + "5D2801B17CAAD0121082E9952FAB16BA1C386336C62D4CF3A5019CF61609433E"
                    + "1C083237D47C4CF575097F7BF9000EF6B6C497A44E6480154A35669AD276BF05"
                    + "6CC730B4E5962B6AF96CC6D236AE85CEFDA6877173F72D2F614F6696D1F9DF07"
                    + "E107758B0978F69BC9DBE0CCBF252C40A3FDF7CE9104D3344F7B73593CCD73E0";
    private static final String IKE_AUTH_RESP_FRAG_2 =
            "46B8ECA1E0D72A18BF3FA1C2CB1EE86F3520232000000001000000F0000000D4"
                    + "00020002155211EA41B37BC5F20568A6AE57038EEE208F94F9B444004F1EF391"
                    + "2CABFCF857B9CD95FAAA9489ED10A3F5C93510820E22E23FC55ED8049E067D72"
                    + "3645C00E1E08611916CE72D7F0A84123B63A8F3B9E78DBBE39967B7BB074AF4D"
                    + "BF2178D991EDBDD01908A14A266D09236DB963B14AC33D894F0F83A580209EFD"
                    + "61875BB56273AA336C22D6A4D890B93E0D42435667830CC32E4F608500E18569"
                    + "3E6C1D88C0B5AE427333C86468E3474DAA4D1506AAB2A4021309A33DD759D0D0"
                    + "A8C98BF7FBEA8109361A9F194D0FD756";
    private static final String DELETE_IKE_RESP =
            "46B8ECA1E0D72A18BF3FA1C2CB1EE86F2E202520000000020000004C00000030"
                    + "342842D8DA37C8EFB92ED37C4FBB23CBDC90445137D6A0AF489F9F03641DBA9D"
                    + "02F6F59FD8A7A78C7261CEB8";

    // Using IPv4 for transport mode Child SA. IPv6 is currently infeasible because the IKE server
    // that generates the test vectors is running in an IPv4 only network.
    private static final IkeTrafficSelector TRANSPORT_MODE_INBOUND_TS =
            new IkeTrafficSelector(
                    MIN_PORT,
                    MAX_PORT,
                    InetAddresses.parseNumericAddress("172.58.35.103"),
                    InetAddresses.parseNumericAddress("172.58.35.103"));

    // TODO(b/157510502): Add test for IKE Session setup with transport mode Child in IPv6 network

    private static final String LOCAL_ID_ASN1_DN =
            "CN=client.test.ike.android.net, O=Android, C=US";
    private static final String REMOTE_ID_ASN1_DN =
            "CN=server.test.ike.android.net, O=Android, C=US";

    private static X509Certificate sServerCaCert;
    private static X509Certificate sClientEndCert;
    private static X509Certificate sClientIntermediateCaCertOne;
    private static X509Certificate sClientIntermediateCaCertTwo;
    private static RSAPrivateKey sClientPrivateKey;

    @BeforeClass
    public static void setUpCertsBeforeClass() throws Exception {
        sServerCaCert = CertUtils.createCertFromPemFile("server-a-self-signed-ca.pem");
        sClientEndCert = CertUtils.createCertFromPemFile("client-a-end-cert.pem");
        sClientIntermediateCaCertOne =
                CertUtils.createCertFromPemFile("client-a-intermediate-ca-one.pem");
        sClientIntermediateCaCertTwo =
                CertUtils.createCertFromPemFile("client-a-intermediate-ca-two.pem");
        sClientPrivateKey = CertUtils.createRsaPrivateKeyFromKeyFile("client-a-private-key.key");
    }

    private IkeSession openIkeSessionWithRemoteAddress(InetAddress remoteAddress) {
        IkeSessionParams ikeParams =
                new IkeSessionParams.Builder(sContext)
                        .setNetwork(mTunNetwork)
                        .setServerHostname(remoteAddress.getHostAddress())
                        .addSaProposal(SaProposalTest.buildIkeSaProposalWithNormalModeCipher())
                        .addSaProposal(SaProposalTest.buildIkeSaProposalWithCombinedModeCipher())
                        .setLocalIdentification(
                                new IkeDerAsn1DnIdentification(new X500Principal(LOCAL_ID_ASN1_DN)))
                        .setRemoteIdentification(
                                new IkeDerAsn1DnIdentification(
                                        new X500Principal(REMOTE_ID_ASN1_DN)))
                        .setAuthDigitalSignature(
                                sServerCaCert,
                                sClientEndCert,
                                Arrays.asList(
                                        sClientIntermediateCaCertOne, sClientIntermediateCaCertTwo),
                                sClientPrivateKey)
                        .build();

        return new IkeSession(
                sContext,
                ikeParams,
                buildTransportModeChildParamsWithTs(
                        TRANSPORT_MODE_INBOUND_TS, TRANSPORT_MODE_OUTBOUND_TS),
                mUserCbExecutor,
                mIkeSessionCallback,
                mFirstChildSessionCallback);
    }

    @Test
    public void testIkeSessionSetupAndChildSessionSetupWithTransportMode() throws Exception {
        // Open IKE Session
        IkeSession ikeSession = openIkeSessionWithRemoteAddress(mRemoteAddress);
        performSetupIkeAndFirstChildBlocking(
                IKE_INIT_RESP,
                EXPECTED_AUTH_REQ_FRAG_COUNT /* expectedReqPktCnt */,
                true /* expectedAuthUseEncap */,
                IKE_AUTH_RESP_FRAG_1,
                IKE_AUTH_RESP_FRAG_2);

        // IKE INIT and IKE AUTH takes two exchanges. Message ID starts from 2
        int expectedMsgId = 2;

        verifyIkeSessionSetupBlocking();
        verifyChildSessionSetupBlocking(
                mFirstChildSessionCallback,
                Arrays.asList(TRANSPORT_MODE_INBOUND_TS),
                Arrays.asList(TRANSPORT_MODE_OUTBOUND_TS),
                new ArrayList<LinkAddress>());
        IpSecTransformCallRecord firstTransformRecordA =
                mFirstChildSessionCallback.awaitNextCreatedIpSecTransform();
        IpSecTransformCallRecord firstTransformRecordB =
                mFirstChildSessionCallback.awaitNextCreatedIpSecTransform();
        verifyCreateIpSecTransformPair(firstTransformRecordA, firstTransformRecordB);

        // Close IKE Session
        ikeSession.close();
        performCloseIkeBlocking(expectedMsgId++, DELETE_IKE_RESP);
        verifyCloseIkeAndChildBlocking(firstTransformRecordA, firstTransformRecordB);
    }
}
