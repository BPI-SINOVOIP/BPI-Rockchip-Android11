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

import static com.android.internal.util.HexDump.hexStringToByteArray;

import android.net.InetAddresses;
import android.net.LinkAddress;
import android.net.ipsec.ike.IkeFqdnIdentification;
import android.net.ipsec.ike.IkeSession;
import android.net.ipsec.ike.IkeSessionParams;
import android.net.ipsec.ike.IkeTrafficSelector;
import android.net.ipsec.ike.cts.IkeTunUtils.PortPair;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Explicitly test transport mode Child SA so that devices without FEATURE_IPSEC_TUNNELS can be test
 * covered. Tunnel mode Child SA setup has been tested in IkeSessionPskTest. Rekeying process is
 * independent from Child SA mode.
 */
@RunWith(AndroidJUnit4.class)
public class IkeSessionRekeyTest extends IkeSessionTestBase {
    // This value is align with the test vectors hex that are generated in an IPv4 environment
    private static final IkeTrafficSelector TRANSPORT_MODE_INBOUND_TS =
            new IkeTrafficSelector(
                    MIN_PORT,
                    MAX_PORT,
                    InetAddresses.parseNumericAddress("172.58.35.40"),
                    InetAddresses.parseNumericAddress("172.58.35.40"));

    private IkeSession openIkeSessionWithRemoteAddress(InetAddress remoteAddress) {
        IkeSessionParams ikeParams =
                new IkeSessionParams.Builder(sContext)
                        .setNetwork(mTunNetwork)
                        .setServerHostname(remoteAddress.getHostAddress())
                        .addSaProposal(SaProposalTest.buildIkeSaProposalWithNormalModeCipher())
                        .addSaProposal(SaProposalTest.buildIkeSaProposalWithCombinedModeCipher())
                        .setLocalIdentification(new IkeFqdnIdentification(LOCAL_HOSTNAME))
                        .setRemoteIdentification(new IkeFqdnIdentification(REMOTE_HOSTNAME))
                        .setAuthPsk(IKE_PSK)
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

    private byte[] buildInboundPkt(PortPair outPktSrcDestPortPair, String inboundDataHex)
            throws Exception {
        // Build inbound packet by flipping the outbound packet addresses and ports
        return IkeTunUtils.buildIkePacket(
                mRemoteAddress,
                mLocalAddress,
                outPktSrcDestPortPair.dstPort,
                outPktSrcDestPortPair.srcPort,
                true /* useEncap */,
                hexStringToByteArray(inboundDataHex));
    }

    @Test
    public void testRekeyIke() throws Exception {
        final String ikeInitResp =
                "46B8ECA1E0D72A1866B5248CF6C7472D21202220000000000000015022000030"
                        + "0000002C010100040300000C0100000C800E0100030000080300000C03000008"
                        + "0200000500000008040000022800008800020000920D3E830E7276908209212D"
                        + "E5A7F2A48706CFEF1BE8CB6E3B173B8B4E0D8C2DC626271FF1B13A88619E569E"
                        + "7B03C3ED2C127390749CDC7CDC711D0A8611E4457FFCBC4F0981B3288FBF58EA"
                        + "3E8B70E27E76AE70117FBBCB753660ADDA37EB5EB3A81BED6A374CCB7E132C2A"
                        + "94BFCE402DC76B19C158B533F6B1F2ABF01ACCC329000024B302CA2FB85B6CF4"
                        + "02313381246E3C53828D787F6DFEA6BD62D6405254AEE6242900001C00004004"
                        + "7A1682B06B58596533D00324886EF1F20EF276032900001C00004005BF633E31"
                        + "F9984B29A62E370BB2770FC09BAEA665290000080000402E290000100000402F"
                        + "00020003000400050000000800004014";
        final String ikeAuthResp =
                "46B8ECA1E0D72A1866B5248CF6C7472D2E20232000000001000000F0240000D4"
                        + "10166CA8647F56123DE74C17FA5E256043ABF73216C812EE32EE1BB01EAF4A82"
                        + "DC107AB3ADBFEE0DEA5EEE10BDD5D43178F4C975C7C775D252273BB037283C7F"
                        + "236FE34A6BCE4833816897075DB2055B9FFD66DFA45A0A89A8F70AFB59431EED"
                        + "A20602FB614369D12906D3355CF7298A5D25364ABBCC75A9D88E0E6581449FCD"
                        + "4E361A39E00EFD1FD0A69651F63DB46C12470226AA21BA5EFF48FAF0B6DDF61C"
                        + "B0A69392CE559495EEDB4D1C1D80688434D225D57210A424C213F7C993D8A456"
                        + "38153FBD194C5E247B592D1D048DB4C8";
        final String rekeyIkeCreateReq =
                "46B8ECA1E0D72A1866B5248CF6C7472D2E202400000000000000013021000114"
                        + "13743670039E308A8409BA5FD47B67F956B36FEE88AC3B70BB5D789B8218A135"
                        + "1B3D83E260E87B3EDB1BF064F09D4DC2611AEDBC99951B4B2DE767BD4AA2ACC3"
                        + "3653549CFC66B75869DF003CDC9A137A9CC27776AD5732B34203E74BE8CA4858"
                        + "1D5C0D9C9CA52D680EB299B4B21C7FA25FFEE174D57015E0FF2EAED653AAD95C"
                        + "071ABE269A8C2C9FBC1188E07550EB992F910D4CA9689E44BA66DE0FABB2BDF9"
                        + "8DD377186DBB25EF9B68B027BB2A27981779D8303D88D7CE860010A42862D50B"
                        + "1E0DBFD3D27C36F14809D7F493B2B96A65534CF98B0C32AD5219AD77F681AC04"
                        + "9D5CB89A0230A91A243FA7F16251B0D9B4B65E7330BEEAC9663EF4578991EAC8"
                        + "46C19EBB726E7D113F1D0D601102C05E";
        final String rekeyIkeDeleteReq =
                "46B8ECA1E0D72A1866B5248CF6C7472D2E20250000000001000000502A000034"
                        + "02E40C0C7B1ED977729F705BB9B643FAC513A1070A6EB28ECD2AEA8A441ADC05"
                        + "7841382A7967BBF116AE52496590B2AD";
        final String deleteIkeReq =
                "7D3DEDC65407D1FC9361C8CF8C47162A2E20250800000000000000502A000034"
                        + "201915C9E4E9173AA9EE79F3E02FE2D4954B22085C66D164762C34D347C16E9F"
                        + "FC5F7F114428C54D8D915860C57B1BC1";
        final long newIkeDeterministicInitSpi = Long.parseLong("7D3DEDC65407D1FC", 16);

        // Open IKE Session
        IkeSession ikeSession = openIkeSessionWithRemoteAddress(mRemoteAddress);
        PortPair localRemotePorts = performSetupIkeAndFirstChildBlocking(ikeInitResp, ikeAuthResp);

        // Local request message ID starts from 2 because there is one IKE_INIT message and a single
        // IKE_AUTH message.
        int expectedReqMsgId = 2;
        int expectedRespMsgId = 0;

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

        // Inject rekey IKE requests
        mTunUtils.injectPacket(buildInboundPkt(localRemotePorts, rekeyIkeCreateReq));
        mTunUtils.awaitResp(
                IKE_DETERMINISTIC_INITIATOR_SPI, expectedRespMsgId++, true /* expectedUseEncap */);
        mTunUtils.injectPacket(buildInboundPkt(localRemotePorts, rekeyIkeDeleteReq));
        mTunUtils.awaitResp(
                IKE_DETERMINISTIC_INITIATOR_SPI, expectedRespMsgId++, true /* expectedUseEncap */);

        // IKE has been rekeyed, reset message IDs
        expectedReqMsgId = 0;
        expectedRespMsgId = 0;

        // Inject delete IKE request
        mTunUtils.injectPacket(buildInboundPkt(localRemotePorts, deleteIkeReq));
        mTunUtils.awaitResp(
                newIkeDeterministicInitSpi, expectedRespMsgId++, true /* expectedUseEncap */);

        verifyDeleteIpSecTransformPair(
                mFirstChildSessionCallback, firstTransformRecordA, firstTransformRecordB);
        mFirstChildSessionCallback.awaitOnClosed();
        mIkeSessionCallback.awaitOnClosed();
    }

    @Test
    public void testRekeyTransportModeChildSa() throws Exception {
        final String ikeInitResp =
                "46B8ECA1E0D72A18CECD871146CF83A121202220000000000000015022000030"
                        + "0000002C010100040300000C0100000C800E0100030000080300000C03000008"
                        + "0200000500000008040000022800008800020000C4904458957746BCF1C12972"
                        + "1D4E19EB8A584F78DE673053396D167CE0F34552DBC69BA63FE7C673B4CF4A99"
                        + "62481518EE985357876E8C47BAAA0DBE9C40AE47B12E52165874703586E8F786"
                        + "045F72EEEB238C5D1823352BED44B71B3214609276ADC0B3D42DAC820168C4E2"
                        + "660730DAAC92492403288805EBB9053F1AB060DA290000242D9364ACB93519FF"
                        + "8F8B019BAA43A40D699F59714B327B8382216EF427ED52282900001C00004004"
                        + "06D91438A0D6B734E152F76F5CC55A72A2E38A0A2900001C000040052EFF78B3"
                        + "55B37F3CE75AFF26C721B050F892C0D6290000080000402E290000100000402F"
                        + "00020003000400050000000800004014";
        final String ikeAuthResp =
                "46B8ECA1E0D72A18CECD871146CF83A12E20232000000001000000F0240000D4"
                        + "A17BC258BA2714CF536663639DD5F665A60C75E93557CD5141990A8CEEDD2017"
                        + "93F5B181C8569FBCD6C2A00198EC2B62D42BEFAC016B8B6BF6A7BC9CEDE3413A"
                        + "6C495A6B8EC941864DC3E08F57D015EA6520C4B05884960B85478FCA53DA5F17"
                        + "9628BB1097DA77461C71837207A9EB80720B3E6E661816EE4E14AC995B5E8441"
                        + "A4C3F9097CC148142BA300076C94A23EC4ADE82B1DD2B121F7E9102860A8C3BF"
                        + "58DDC207285A3176E924C44DE820322524E1AA438EFDFBA781B36084AED80846"
                        + "3B77FCED9682B6E4E476408EF3F1037E";
        final String rekeyChildCreateReq =
                "46B8ECA1E0D72A18CECD871146CF83A12E202400000000000000015029000134"
                        + "319D74B6B155B86942143CEC1D29D21F073F24B7BEDC9BFE0F0FDD8BDB5458C0"
                        + "8DB93506E1A43DD0640FE7370C97F9B34FF4EC9B2DB7257A87B75632301FB68A"
                        + "86B54871249534CA3D01C9BEB127B669F46470E1C8AAF72574C3CEEC15B901CF"
                        + "5A0D6ADAE59C3CA64AC8C86689C860FAF9500E608DFE63F2DCD30510FD6FFCD5"
                        + "A50838574132FD1D069BCACD4C7BAF45C9B1A7689FAD132E3F56DBCFAF905A8C"
                        + "4145D4BA1B74A54762F8F43308D94DE05649C49D885121CE30681D51AC1E3E68"
                        + "AB82F9A19B99579AFE257F32DBD1037814DA577379E4F42DEDAC84502E49C933"
                        + "9EA83F6F5DB4401B660CB1681B023B8603D205DFDD1DE86AD8DE22B6B754F30D"
                        + "05EAE81A709C2CEE81386133DC3DC7B5EF8F166E48E54A0722DD0C64F4D00638"
                        + "40F272144C47F6ECED72A248180645DB";
        final String rekeyChildDeleteReq =
                "46B8ECA1E0D72A18CECD871146CF83A12E20250000000001000000502A000034"
                        + "02D98DAF0432EBD991CA4F2D89C1E0EFABC6E91A3327A85D8914FB2F1485BE1B"
                        + "8D3415D548F7CE0DC4224E7E9D0D3355";
        final String deleteIkeReq =
                "46B8ECA1E0D72A18CECD871146CF83A12E20250000000002000000502A000034"
                        + "095041F4026B4634F04B0AB4F9349484F7BE9AEF03E3733EEE293330043B75D2"
                        + "ABF5F965ED51127629585E1B1BBA787F";

        // Open IKE Session
        IkeSession ikeSession = openIkeSessionWithRemoteAddress(mRemoteAddress);
        PortPair localRemotePorts = performSetupIkeAndFirstChildBlocking(ikeInitResp, ikeAuthResp);

        // IKE INIT and IKE AUTH takes two exchanges. Local request message ID starts from 2
        int expectedReqMsgId = 2;
        int expectedRespMsgId = 0;

        verifyIkeSessionSetupBlocking();
        verifyChildSessionSetupBlocking(
                mFirstChildSessionCallback,
                Arrays.asList(TRANSPORT_MODE_INBOUND_TS),
                Arrays.asList(TRANSPORT_MODE_OUTBOUND_TS),
                new ArrayList<LinkAddress>());
        IpSecTransformCallRecord oldTransformRecordA =
                mFirstChildSessionCallback.awaitNextCreatedIpSecTransform();
        IpSecTransformCallRecord oldTransformRecordB =
                mFirstChildSessionCallback.awaitNextCreatedIpSecTransform();
        verifyCreateIpSecTransformPair(oldTransformRecordA, oldTransformRecordB);

        // Inject rekey Child requests
        mTunUtils.injectPacket(buildInboundPkt(localRemotePorts, rekeyChildCreateReq));
        mTunUtils.awaitResp(
                IKE_DETERMINISTIC_INITIATOR_SPI, expectedRespMsgId++, true /* expectedUseEncap */);
        mTunUtils.injectPacket(buildInboundPkt(localRemotePorts, rekeyChildDeleteReq));
        mTunUtils.awaitResp(
                IKE_DETERMINISTIC_INITIATOR_SPI, expectedRespMsgId++, true /* expectedUseEncap */);

        // Verify IpSecTransforms are renewed
        IpSecTransformCallRecord newTransformRecordA =
                mFirstChildSessionCallback.awaitNextCreatedIpSecTransform();
        IpSecTransformCallRecord newTransformRecordB =
                mFirstChildSessionCallback.awaitNextCreatedIpSecTransform();
        verifyCreateIpSecTransformPair(newTransformRecordA, newTransformRecordB);
        verifyDeleteIpSecTransformPair(
                mFirstChildSessionCallback, oldTransformRecordA, oldTransformRecordB);

        // Inject delete IKE request
        mTunUtils.injectPacket(buildInboundPkt(localRemotePorts, deleteIkeReq));
        mTunUtils.awaitResp(
                IKE_DETERMINISTIC_INITIATOR_SPI, expectedRespMsgId++, true /* expectedUseEncap */);

        verifyDeleteIpSecTransformPair(
                mFirstChildSessionCallback, newTransformRecordA, newTransformRecordB);
        mFirstChildSessionCallback.awaitOnClosed();
        mIkeSessionCallback.awaitOnClosed();
    }
}
