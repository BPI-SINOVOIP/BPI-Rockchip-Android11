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
import android.net.eap.EapSessionConfig;
import android.net.ipsec.ike.IkeFqdnIdentification;
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
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Explicitly test setting up transport mode Child SA so that devices do not have
 * FEATURE_IPSEC_TUNNELS will be test covered. Tunnel mode Child SA setup has been tested in
 * IkeSessionPskTest and authentication method is orthogonal to Child mode.
 */
@RunWith(AndroidJUnit4.class)
public class IkeSessionMschapV2Test extends IkeSessionTestBase {
    private static final String IKE_INIT_RESP =
            "46B8ECA1E0D72A1873F643FF94D249A921202220000000000000015022000030"
                    + "0000002C010100040300000C0100000C800E0080030000080300000203000008"
                    + "0200000200000008040000022800008800020000CC6E71E67E32CED6BCE33FBD"
                    + "A74113867E3FA3AE21C7C9AB44A7F8835DF602BFD6F6528B67FEE39821232380"
                    + "C99E8FFC0A5D767F8F38906DA41946C2299DF18C15FA69BAC08D3EDB32E8C8CA"
                    + "28431831561C04CB0CDE393F817151CD8DAF7A311838411F1C39BFDB5EBCF6A6"
                    + "1DF66DEB067362649D64607D599B56C4227819D0290000241197004CF31AD00F"
                    + "5E0C92E198488D8A2B6F6A25C82762AA49F565BCE9D857D72900001C00004004"
                    + "A0D98FEABBFB92A6C0976EE83D2AACFCCF969A6B2900001C0000400575EBF73F"
                    + "8EE5CC73917DE9D3F91FCD4A16A0444D290000080000402E290000100000402F"
                    + "00020003000400050000000800004014";
    private static final String IKE_AUTH_RESP_1_FRAG_1 =
            "46B8ECA1E0D72A1873F643FF94D249A93520232000000001000004E0240004C4"
                    + "00010002C4159CB756773B3F1911F4595107BC505D7A28C72F05182966076679"
                    + "CA68ED92E4BC5CD441C9CB315F2F449A8A521CAFED3C5F285E295FC3791D3415"
                    + "E3BACF66A08410DF4E35F7D88FE40DA28851C91C77A6549E186AC1B7846DF3FA"
                    + "0A347A5ABBCAEE19E70F0EE5966DC6242A115F29523709302EDAD2E36C8F0395"
                    + "CF5C42EC2D2898ECDD8A6AEDD686A70B589A981558667647F32F41E0D8913E94"
                    + "A6693F53E59EA8938037F562CF1DC5E6E2CDC630B5FFB08949E3172249422F7D"
                    + "EA069F9BAD5F96E48BADC7164A9269669AD0DF295A80C54D1D23CEA3F28AC485"
                    + "86D2A9850DA23823037AB7D1577B7B2364C92C36B84238357129EB4A64D33310"
                    + "B95DCD50CD53E78C32EFE7DC1627D9432E9BFDEE130045DE967B19F92A9D1270"
                    + "F1E2C6BFBAA56802F3E63510578EF1ECB6872852F286EEC790AA1FE0CAF391CB"
                    + "E276554922713BA4770CFE71E23F043DC620E22CC02A74F60725D18331B7F2C9"
                    + "276EB6FBB7CBDAA040046D7ECBE1A5D7064E04E542807C5101B941D1C81B9D5E"
                    + "90347B22BD4E638E2EDC98E369B51AA29BDB2CF8AA610D4B893EB83A4650717C"
                    + "38B4D145EE939C18DCEDF6C79933CEB3D7C116B1F188DF9DDD560951B54E4A7D"
                    + "80C999A32AB02BF39D7B498DAD36F1A5CBE2F64557D6401AE9DD6E0CEADA3F90"
                    + "540FE9114BB6B8719C9064796354F4A180A6600CAD092F8302564E409B71ACB7"
                    + "590F19B3AC88E7A606C718D0B97F7E4B4830F11D851C59F2255846DA22E2C805"
                    + "0CA2AF2ACF3B6C769D11B75B5AC9AB82ED3D90014994B1BF6FED58FBEF2D72EF"
                    + "8BDFE51F9A101393A7CA1ACF78FAEBF3E3CC25E09407D1E14AF351A159A13EE3"
                    + "9B919BA8B49942792E7527C2FB6D418C4DF427669A4BF5A1AFBBB973BAF17918"
                    + "9C9D520CAC2283B89A539ECE785EBE48FBB77D880A17D55C84A51F46068A4B87"
                    + "FF48FEEE50E1E034CC8AFF5DA92105F55EC4823E67BDFE942CA8BE0DAECBBD52"
                    + "E8AAF306049DC6C4CF87D987B0AC54FCE92E6AE8507965AAAC6AB8BD3405712F"
                    + "EE170B70BC64BDCBD86D80C7AAAF341131F9A1210D7430B17218413AE1363183"
                    + "5C98FA2428B1E9E987ADC9070E232310A28F4C3163E18366FFB112BADD7C5E0F"
                    + "D13093A7C1428F87856BA0A7E46955589ACA267CE7A04320C4BCDBB60C672404"
                    + "778F8D511AAB09349DAB482445D7F606F28E7FBBB18FC0F4EC0AF04F44C282F9"
                    + "39C6E3B955C84DADEA350667236583069B74F492D600127636FA31F63E560851"
                    + "2FC28B8EA5B4D01D110990B6EA46B9C2E7C7C856C240EF7A8147BA2C4344B85A"
                    + "453C862024B5B6814D13CDEAEF7683D539BB50CAFFC0416F269F2F9EDEC5FA30"
                    + "022FD7B4B186CD2020E7ED8D81ED90822EDD8B76F840DD68F09694CFF9B4F33E"
                    + "11DF4E601A4212881A6D4E9259001705C41E9E23D18A7F3D4A3463649A38211A"
                    + "5A90D0F17739A677C74E23F31C01D60B5A0F1E6A4D44FED9D25BF1E63418E1FC"
                    + "0B19F6F4B71DE53C62B14B82279538A82DD4BE19AB6E00AFC20F124AAB7DF21A"
                    + "42259BE4F40EC69B16917256F23E2C37376311D62E0A3A0EF8C2AD0C090221D5"
                    + "C5ECA08F08178A4D31FFDB150C609827D18AD83C7B0A43AEE0406BD3FB494B53"
                    + "A279FDD6447E234C926AD8CE47FFF779BB45B1FC8457C6E7D257D1359959D977"
                    + "CEF6906A3367DC4D454993EFDC6F1EA94E17EB3DCB00A289346B4CFD7F19B16E";
    private static final String IKE_AUTH_RESP_1_FRAG_2 =
            "46B8ECA1E0D72A1873F643FF94D249A935202320000000010000008000000064"
                    + "00020002C61F66025E821A5E69A4DE1F591A2C32C983C3154A5003660137D685"
                    + "A5262B9FDF5EDC699DE4D8BD38F549E3CBD12024B45B4C86561C36C3EED839DA"
                    + "9860C6AA0B764C662D08F1B6A98F68CF6E3038F737C0B415AD8A8B7D702BD92A";
    private static final String IKE_AUTH_RESP_2 =
            "46B8ECA1E0D72A1873F643FF94D249A92E202320000000020000008C30000070"
                    + "62B90C2229FD23025BC2FD7FE6341E9EE04B17264CD619BCE18975A5F88BE438"
                    + "D4AD4A5310057255AF568C293A29B10107E3EE3675C10AA2B26404D90C0528CC"
                    + "F7605A86C96A1F2635CCC6CFC90EE65E5C2A2262EB33FE520EB708423A83CB63"
                    + "274ECCBB102AF5DF35742657";
    private static final String IKE_AUTH_RESP_3 =
            "46B8ECA1E0D72A1873F643FF94D249A92E202320000000030000004C30000030"
                    + "AB52C3C80123D3432C05AF457CE93C352395F73E861CD49561BA528CFE68D17D"
                    + "78BBF6FC41E81C2B9EA051A2";
    private static final String IKE_AUTH_RESP_4 =
            "46B8ECA1E0D72A1873F643FF94D249A92E20232000000004000000CC270000B0"
                    + "8D3342A7AB2666AC754F4B55C5C6B1A61255E62FBCA53D5CDEEDE60DADB7915C"
                    + "7F962076A58BF7D39A05ED1B60FF349B6DE311AF7CEBC72B4BB9723A728A5D3E"
                    + "9E508B2D7A11843D279B56ADA07E608D61F5CA7638F10372A440AD1DCE44E190"
                    + "7B7B7A68B126EBBB86638D667D5B528D233BA8D32D7E0FAC4E1448E87396EEE6"
                    + "0985B79841E1229D7962AACFD8F872722EC8D5B19D4C82D6C4ADCB276127A1A7"
                    + "3FC84CDF85B2299BC96B64AC";
    private static final String DELETE_IKE_RESP =
            "46B8ECA1E0D72A1873F643FF94D249A92E202520000000050000004C00000030"
                    + "622CE06C8CB132AA00567E9BC83F58B32BD7DB5130C76E385B306434DA227361"
                    + "D50CC19D408A8D4F36F9697F";

    // This value is align with the test vectors hex that are generated in an IPv4 environment
    private static final IkeTrafficSelector TRANSPORT_MODE_INBOUND_TS =
            new IkeTrafficSelector(
                    MIN_PORT,
                    MAX_PORT,
                    InetAddresses.parseNumericAddress("172.58.35.67"),
                    InetAddresses.parseNumericAddress("172.58.35.67"));

    private static final EapSessionConfig EAP_CONFIG =
            new EapSessionConfig.Builder()
                    .setEapIdentity(EAP_IDENTITY)
                    .setEapMsChapV2Config(EAP_MSCHAPV2_USERNAME, EAP_MSCHAPV2_PASSWORD)
                    .build();

    private static X509Certificate sServerCaCert;

    @BeforeClass
    public static void setUpCertBeforeClass() throws Exception {
        sServerCaCert = CertUtils.createCertFromPemFile("server-a-self-signed-ca.pem");
    }

    private IkeSession openIkeSessionWithRemoteAddress(InetAddress remoteAddress) {
        IkeSessionParams ikeParams =
                new IkeSessionParams.Builder(sContext)
                        .setNetwork(mTunNetwork)
                        .setServerHostname(remoteAddress.getHostAddress())
                        .addSaProposal(SaProposalTest.buildIkeSaProposalWithNormalModeCipher())
                        .addSaProposal(SaProposalTest.buildIkeSaProposalWithCombinedModeCipher())
                        .setLocalIdentification(new IkeFqdnIdentification(LOCAL_HOSTNAME))
                        .setRemoteIdentification(new IkeFqdnIdentification(REMOTE_HOSTNAME))
                        .setAuthEap(sServerCaCert, EAP_CONFIG)
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
        int expectedMsgId = 0;
        mTunUtils.awaitReqAndInjectResp(
                IKE_DETERMINISTIC_INITIATOR_SPI,
                expectedMsgId++,
                false /* expectedUseEncap */,
                IKE_INIT_RESP);

        mTunUtils.awaitReqAndInjectResp(
                IKE_DETERMINISTIC_INITIATOR_SPI,
                expectedMsgId++,
                true /* expectedUseEncap */,
                IKE_AUTH_RESP_1_FRAG_1,
                IKE_AUTH_RESP_1_FRAG_2);

        mTunUtils.awaitReqAndInjectResp(
                IKE_DETERMINISTIC_INITIATOR_SPI,
                expectedMsgId++,
                true /* expectedUseEncap */,
                IKE_AUTH_RESP_2);
        mTunUtils.awaitReqAndInjectResp(
                IKE_DETERMINISTIC_INITIATOR_SPI,
                expectedMsgId++,
                true /* expectedUseEncap */,
                IKE_AUTH_RESP_3);
        mTunUtils.awaitReqAndInjectResp(
                IKE_DETERMINISTIC_INITIATOR_SPI,
                expectedMsgId++,
                true /* expectedUseEncap */,
                IKE_AUTH_RESP_4);

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
