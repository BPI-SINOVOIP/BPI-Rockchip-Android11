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

package com.android.internal.net.ipsec.ike;

import static android.net.ipsec.ike.IkeSessionConfiguration.EXTENSION_TYPE_FRAGMENTATION;
import static android.net.ipsec.ike.IkeSessionParams.IKE_OPTION_EAP_ONLY_AUTH;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_AUTHENTICATION_FAILED;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_CHILD_SA_NOT_FOUND;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_INTERNAL_ADDRESS_FAILURE;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_INVALID_SYNTAX;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_NO_ADDITIONAL_SAS;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_NO_PROPOSAL_CHOSEN;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_TEMPORARY_FAILURE;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_UNSUPPORTED_CRITICAL_PAYLOAD;
import static android.system.OsConstants.AF_INET;
import static android.system.OsConstants.AF_INET6;

import static com.android.internal.net.TestUtils.createMockRandomFactory;
import static com.android.internal.net.ipsec.ike.AbstractSessionStateMachine.RETRY_INTERVAL_MS;
import static com.android.internal.net.ipsec.ike.IkeSessionStateMachine.CMD_FORCE_TRANSITION;
import static com.android.internal.net.ipsec.ike.IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET;
import static com.android.internal.net.ipsec.ike.IkeSessionStateMachine.IKE_EXCHANGE_SUBTYPE_DELETE_CHILD;
import static com.android.internal.net.ipsec.ike.IkeSessionStateMachine.IKE_EXCHANGE_SUBTYPE_REKEY_CHILD;
import static com.android.internal.net.ipsec.ike.IkeSessionStateMachine.RETRY_INTERVAL_MS;
import static com.android.internal.net.ipsec.ike.IkeSessionStateMachine.TEMP_FAILURE_RETRY_TIMEOUT_MS;
import static com.android.internal.net.ipsec.ike.message.IkeConfigPayload.CONFIG_ATTR_APPLICATION_VERSION;
import static com.android.internal.net.ipsec.ike.message.IkeConfigPayload.CONFIG_ATTR_INTERNAL_IP4_ADDRESS;
import static com.android.internal.net.ipsec.ike.message.IkeConfigPayload.CONFIG_ATTR_INTERNAL_IP4_NETMASK;
import static com.android.internal.net.ipsec.ike.message.IkeConfigPayload.CONFIG_ATTR_INTERNAL_IP6_ADDRESS;
import static com.android.internal.net.ipsec.ike.message.IkeConfigPayload.CONFIG_ATTR_IP4_PCSCF;
import static com.android.internal.net.ipsec.ike.message.IkeConfigPayload.CONFIG_ATTR_IP6_PCSCF;
import static com.android.internal.net.ipsec.ike.message.IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA;
import static com.android.internal.net.ipsec.ike.message.IkeHeader.EXCHANGE_TYPE_INFORMATIONAL;
import static com.android.internal.net.ipsec.ike.message.IkeNotifyPayload.NOTIFY_TYPE_EAP_ONLY_AUTHENTICATION;
import static com.android.internal.net.ipsec.ike.message.IkeNotifyPayload.NOTIFY_TYPE_IKEV2_FRAGMENTATION_SUPPORTED;
import static com.android.internal.net.ipsec.ike.message.IkeNotifyPayload.NOTIFY_TYPE_NAT_DETECTION_DESTINATION_IP;
import static com.android.internal.net.ipsec.ike.message.IkeNotifyPayload.NOTIFY_TYPE_NAT_DETECTION_SOURCE_IP;
import static com.android.internal.net.ipsec.ike.message.IkeNotifyPayload.NOTIFY_TYPE_SIGNATURE_HASH_ALGORITHMS;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_AUTH;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_NOTIFY;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_SA;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyLong;
import static org.mockito.Matchers.anyObject;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.argThat;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.AlarmManager;
import android.content.Context;
import android.net.eap.EapSessionConfig;
import android.net.ipsec.ike.ChildSaProposal;
import android.net.ipsec.ike.ChildSessionCallback;
import android.net.ipsec.ike.ChildSessionParams;
import android.net.ipsec.ike.IkeFqdnIdentification;
import android.net.ipsec.ike.IkeIdentification;
import android.net.ipsec.ike.IkeIpv4AddrIdentification;
import android.net.ipsec.ike.IkeManager;
import android.net.ipsec.ike.IkeSaProposal;
import android.net.ipsec.ike.IkeSessionCallback;
import android.net.ipsec.ike.IkeSessionConfiguration;
import android.net.ipsec.ike.IkeSessionConnectionInfo;
import android.net.ipsec.ike.IkeSessionParams;
import android.net.ipsec.ike.SaProposal;
import android.net.ipsec.ike.TunnelModeChildSessionParams;
import android.net.ipsec.ike.exceptions.IkeException;
import android.net.ipsec.ike.exceptions.IkeInternalException;
import android.net.ipsec.ike.exceptions.IkeProtocolException;
import android.os.Looper;
import android.os.test.TestLooper;
import android.telephony.TelephonyManager;

import com.android.internal.net.TestUtils;
import com.android.internal.net.eap.EapAuthenticator;
import com.android.internal.net.eap.IEapCallback;
import com.android.internal.net.ipsec.ike.ChildSessionStateMachine.IChildSessionSmCallback;
import com.android.internal.net.ipsec.ike.ChildSessionStateMachineFactory.ChildSessionFactoryHelper;
import com.android.internal.net.ipsec.ike.ChildSessionStateMachineFactory.IChildSessionFactoryHelper;
import com.android.internal.net.ipsec.ike.IkeLocalRequestScheduler.ChildLocalRequest;
import com.android.internal.net.ipsec.ike.IkeLocalRequestScheduler.IkeLocalRequest;
import com.android.internal.net.ipsec.ike.IkeSessionStateMachine.ReceivedIkePacket;
import com.android.internal.net.ipsec.ike.SaRecord.ISaRecordHelper;
import com.android.internal.net.ipsec.ike.SaRecord.IkeSaRecord;
import com.android.internal.net.ipsec.ike.SaRecord.IkeSaRecordConfig;
import com.android.internal.net.ipsec.ike.SaRecord.SaLifetimeAlarmScheduler;
import com.android.internal.net.ipsec.ike.SaRecord.SaRecordHelper;
import com.android.internal.net.ipsec.ike.crypto.IkeCipher;
import com.android.internal.net.ipsec.ike.crypto.IkeMacIntegrity;
import com.android.internal.net.ipsec.ike.crypto.IkeMacPrf;
import com.android.internal.net.ipsec.ike.exceptions.AuthenticationFailedException;
import com.android.internal.net.ipsec.ike.exceptions.InvalidSyntaxException;
import com.android.internal.net.ipsec.ike.exceptions.NoValidProposalChosenException;
import com.android.internal.net.ipsec.ike.exceptions.UnsupportedCriticalPayloadException;
import com.android.internal.net.ipsec.ike.message.IkeAuthDigitalSignPayload;
import com.android.internal.net.ipsec.ike.message.IkeAuthPayload;
import com.android.internal.net.ipsec.ike.message.IkeAuthPskPayload;
import com.android.internal.net.ipsec.ike.message.IkeCertX509CertPayload;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload;
import com.android.internal.net.ipsec.ike.message.IkeDeletePayload;
import com.android.internal.net.ipsec.ike.message.IkeEapPayload;
import com.android.internal.net.ipsec.ike.message.IkeHeader;
import com.android.internal.net.ipsec.ike.message.IkeIdPayload;
import com.android.internal.net.ipsec.ike.message.IkeInformationalPayload;
import com.android.internal.net.ipsec.ike.message.IkeKePayload;
import com.android.internal.net.ipsec.ike.message.IkeMessage;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResult;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResultOk;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResultPartial;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResultProtectedError;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResultUnprotectedError;
import com.android.internal.net.ipsec.ike.message.IkeMessage.IIkeMessageHelper;
import com.android.internal.net.ipsec.ike.message.IkeMessage.IkeMessageHelper;
import com.android.internal.net.ipsec.ike.message.IkeNoncePayload;
import com.android.internal.net.ipsec.ike.message.IkeNotifyPayload;
import com.android.internal.net.ipsec.ike.message.IkePayload;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.DhGroupTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.EncryptionTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IntegrityTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.PrfTransform;
import com.android.internal.net.ipsec.ike.message.IkeSkfPayload;
import com.android.internal.net.ipsec.ike.message.IkeTestUtils;
import com.android.internal.net.ipsec.ike.message.IkeTsPayload;
import com.android.internal.net.ipsec.ike.testmode.DeterministicSecureRandom;
import com.android.internal.net.ipsec.ike.testutils.CertUtils;
import com.android.internal.net.ipsec.ike.utils.IkeSecurityParameterIndex;
import com.android.internal.net.ipsec.ike.utils.IkeSpiGenerator;
import com.android.internal.net.ipsec.ike.utils.IpSecSpiGenerator;
import com.android.internal.net.ipsec.ike.utils.RandomnessFactory;
import com.android.internal.net.ipsec.ike.utils.State;
import com.android.internal.net.utils.Log;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.invocation.InvocationOnMock;

import java.io.IOException;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.security.GeneralSecurityException;
import java.security.PrivateKey;
import java.security.SecureRandom;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;

public final class IkeSessionStateMachineTest extends IkeSessionTestBase {
    private static final String TAG = "IkeSessionStateMachineTest";

    private static final String IKE_INIT_RESP_HEX_STRING =
            "5f54bf6d8b48e6e1909232b3d1edcb5c21202220000000000000014c220000300000"
                    + "002c010100040300000c0100000c800e008003000008030000020300000802000002"
                    + "00000008040000022800008800020000fe014fefed55a4229928bfa3dad1ea6ffaca"
                    + "abfb5f5bdd71790e99a192530e3f849d3a3d96dc6e0a7a10ff6f72a6162103ac573c"
                    + "acd41d08b7a034cad8f5eab09c14ced5a9e4af5692dff028f21c1119dd75226b6af6"
                    + "b2f009245369c9892cc5742e5c94a254ebff052470771fb2cb4f29a35d8953e18a1a"
                    + "6c6fbc56acc188a5290000249756112ca539f5c25abacc7ee92b73091942a9c06950"
                    + "f98848f1af1694c4ddff2900001c00004004c53f054b976a25d75fde72dbf1c7b6c8"
                    + "c9aa9ca12900001c00004005b16d79b21c1bc89ca7350f42de805be0227e2ed62b00"
                    + "00080000401400000014882fe56d6fd20dbc2251613b2ebe5beb";
    private static final String IKE_SA_PAYLOAD_HEX_STRING =
            "220000300000002c010100040300000c0100000c800e00800300000803000002030"
                    + "00008020000020000000804000002";
    private static final String IKE_REKEY_SA_PAYLOAD_HEX_STRING =
            "22000038000000340101080400000000000000FF0300000c0100000c800e0080030"
                    + "000080300000203000008020000020000000804000002";
    private static final String IKE_REKEY_UNACCEPTABLE_SA_PAYLOAD_HEX_STRING =
            "22000038000000340101080400000000000000FF0300000c0100000c800e0080030"
                    + "00008030000020300000802000002000000080400000e";
    private static final int IKE_REKEY_SA_INITIATOR_SPI = 0xff;
    private static final String KE_PAYLOAD_HEX_STRING =
            "2800008800020000b4a2faf4bb54878ae21d638512ece55d9236fc50"
                    + "46ab6cef82220f421f3ce6361faf36564ecb6d28798a94aa"
                    + "d7b2b4b603ddeaaa5630adb9ece8ac37534036040610ebdd"
                    + "92f46bef84f0be7db860351843858f8acf87056e272377f7"
                    + "0c9f2d81e29c7b0ce4f291a3a72476bb0b278fd4b7b0a4c2"
                    + "6bbeb08214c7071376079587";
    private static final String INVALID_KE_PAYLOAD_HEX_STRING = "0000000a00000011000e";
    private static final String NONCE_INIT_PAYLOAD_HEX_STRING =
            "29000024c39b7f368f4681b89fa9b7be6465abd7c5f68b6ed5d3b4c72cb4240eb5c46412";
    private static final String NONCE_RESP_PAYLOAD_HEX_STRING =
            "290000249756112ca539f5c25abacc7ee92b73091942a9c06950f98848f1af1694c4ddff";
    private static final String NONCE_INIT_HEX_STRING =
            "c39b7f368f4681b89fa9b7be6465abd7c5f68b6ed5d3b4c72cb4240eb5c46412";
    private static final String NONCE_RESP_HEX_STRING =
            "9756112ca539f5c25abacc7ee92b73091942a9c06950f98848f1af1694c4ddff";
    private static final String NAT_DETECTION_SOURCE_PAYLOAD_HEX_STRING =
            "2900001c00004004e54f73b7d83f6beb881eab2051d8663f421d10b0";
    private static final String NAT_DETECTION_DESTINATION_PAYLOAD_HEX_STRING =
            "2900001c00004005d915368ca036004cb578ae3e3fb268509aeab190";
    private static final String FRAGMENTATION_SUPPORTED_PAYLOAD_HEX_STRING = "290000080000402e";
    private static final String SIGNATURE_HASH_SUPPORTED_PAYLOAD_HEX_STRING =
            "2b00000F0000402f0001000200030004";
    private static final String DELETE_IKE_PAYLOAD_HEX_STRING = "0000000801000000";
    private static final String NOTIFY_REKEY_IKE_PAYLOAD_HEX_STRING = "2100000800004009";
    private static final String ID_PAYLOAD_INITIATOR_HEX_STRING =
            "290000180200000031313233343536373839414243444546";
    private static final String ID_PAYLOAD_RESPONDER_HEX_STRING = "2700000c010000007f000001";
    private static final String ID_PAYLOAD_RESPONDER_FQDN_HEX_STRING =
            "2700001702000000696B652E616E64726F69642E6E6574";
    private static final String PSK_AUTH_RESP_PAYLOAD_HEX_STRING =
            "2100001c0200000058f36412e9b7b38df817a9f7779b7a008dacdd25";
    private static final String GENERIC_DIGITAL_SIGN_AUTH_RESP_HEX_STRING =
            "300000580e0000000f300d06092a864886f70d01010b05006f76af4150d653c5d413"
                    + "6b9f69d905849bf075c563e6d14ccda42361ec3e7d12c72e2dece5711ea1d952f7b8e"
                    + "12c5d982aa4efdaeac36a02b222aa96242cc424";
    private static final String CHILD_SA_PAYLOAD_HEX_STRING =
            "2c00002c0000002801030403cae7019f0300000c0100000c800e008003000008030"
                    + "000020000000805000000";
    private static final String TS_INIT_PAYLOAD_HEX_STRING =
            "2d00001801000000070000100000ffff00000000ffffffff";
    private static final String TS_RESP_PAYLOAD_HEX_STRING =
            "2900001801000000070000100000ffff000000000fffffff";
    private static final String VENDOR_ID_PAYLOAD_HEX_STRING =
            "0000001852656d6f74652056656e646f72204944204f6e65";

    private static final String PSK_HEX_STRING = "6A756E69706572313233";

    private static final String PRF_KEY_INIT_HEX_STRING =
            "094787780EE466E2CB049FA327B43908BC57E485";
    private static final String PRF_KEY_RESP_HEX_STRING =
            "A30E6B08BE56C0E6BFF4744143C75219299E1BEB";

    private static final String CP_PAYLOAD_HEX_STRING =
            "210000810200000000080011260010111067a17d000000000a"
                    + "f68e8640000a0010200148880067ff000643000d000000000"
                    + "00a0010200148880066ff000645000d0000000000150010200"
                    + "148880006713a00f10104000000050015001020014888000671"
                    + "3a00f101040000008900150010200148880005713a00e00104000000c9";
    private static final String PCSCF_IPV6_ADDRESS1 = "2001:4888:6:713a:f1:104:0:5";
    private static final String PCSCF_IPV6_ADDRESS2 = "2001:4888:6:713a:f1:104:0:89";
    private static final String PCSCF_IPV6_ADDRESS3 = "2001:4888:5:713a:e0:104:0:c9";

    private static final byte[] EAP_DUMMY_MSG = "EAP Message".getBytes();
    private static final byte[] REMOTE_VENDOR_ID_ONE = "Remote Vendor ID One".getBytes();
    private static final byte[] REMOTE_VENDOR_ID_TWO = "Remote Vendor ID Two".getBytes();

    private static final IkeIdentification LOCAL_ID_IPV4 =
            new IkeIpv4AddrIdentification((Inet4Address) LOCAL_ADDRESS);
    private static final IkeIdentification REMOTE_ID_FQDN =
            new IkeFqdnIdentification("server.test.android.net");
    private static final IkeIdentification REMOTE_ID_IPV4 =
            new IkeIpv4AddrIdentification((Inet4Address) REMOTE_ADDRESS);

    private static final int KEY_LEN_IKE_INTE = 20;
    private static final int KEY_LEN_IKE_ENCR = 16;
    private static final int KEY_LEN_IKE_PRF = 20;
    private static final int KEY_LEN_IKE_SKD = KEY_LEN_IKE_PRF;

    private static final int CHILD_SPI_LOCAL = 0x2ad4c0a2;
    private static final int CHILD_SPI_REMOTE = 0xcae7019f;

    private static final int EAP_SIM_SUB_ID = 1;

    private static final int PAYLOAD_TYPE_UNSUPPORTED = 127;

    private static final long RETRANSMIT_BACKOFF_TIMEOUT_MS = 5000L;

    private static final IkeSpiGenerator IKE_SPI_GENERATOR =
            new IkeSpiGenerator(createMockRandomFactory());

    private IkeUdpEncapSocket mSpyIkeUdpEncapSocket;
    private IkeUdp4Socket mSpyIkeUdp4Socket;
    private IkeUdp6Socket mSpyIkeUdp6Socket;
    private IkeSocket mSpyCurrentIkeSocket;

    private TestLooper mLooper;
    private IkeSessionStateMachine mIkeSessionStateMachine;

    private byte[] mPsk;

    private ChildSessionParams mChildSessionParams;

    private Executor mSpyUserCbExecutor;
    private IkeSessionCallback mMockIkeSessionCallback;
    private ChildSessionCallback mMockChildSessionCallback;

    private EncryptionTransform mIkeEncryptionTransform;
    private IntegrityTransform mIkeIntegrityTransform;
    private PrfTransform mIkePrfTransform;
    private DhGroupTransform mIkeDhGroupTransform;

    private IIkeMessageHelper mMockIkeMessageHelper;
    private ISaRecordHelper mMockSaRecordHelper;

    private ChildSessionStateMachine mMockChildSessionStateMachine;
    private IChildSessionFactoryHelper mMockChildSessionFactoryHelper;
    private IChildSessionSmCallback mDummyChildSmCallback;

    private IkeSaRecord mSpyCurrentIkeSaRecord;
    private IkeSaRecord mSpyLocalInitIkeSaRecord;
    private IkeSaRecord mSpyRemoteInitIkeSaRecord;

    private Log mSpyIkeLog;

    private int mExpectedCurrentSaLocalReqMsgId;
    private int mExpectedCurrentSaRemoteReqMsgId;

    private EapSessionConfig mEapSessionConfig;
    private IkeEapAuthenticatorFactory mMockEapAuthenticatorFactory;
    private EapAuthenticator mMockEapAuthenticator;

    private X509Certificate mRootCertificate;
    private X509Certificate mServerEndCertificate;
    private PrivateKey mUserPrivateKey;
    private X509Certificate mUserEndCert;

    private ArgumentCaptor<IkeMessage> mIkeMessageCaptor =
            ArgumentCaptor.forClass(IkeMessage.class);
    private ArgumentCaptor<IkeSaRecordConfig> mIkeSaRecordConfigCaptor =
            ArgumentCaptor.forClass(IkeSaRecordConfig.class);
    private ArgumentCaptor<IChildSessionSmCallback> mChildSessionSmCbCaptor =
            ArgumentCaptor.forClass(IChildSessionSmCallback.class);
    private ArgumentCaptor<List<IkePayload>> mPayloadListCaptor =
            ArgumentCaptor.forClass(List.class);

    private ReceivedIkePacket makeDummyReceivedIkeInitRespPacket(
            long initiatorSpi,
            long responderSpi,
            @IkeHeader.ExchangeType int eType,
            boolean isResp,
            boolean fromIkeInit,
            List<Integer> payloadTypeList,
            List<String> payloadHexStringList)
            throws Exception {

        List<IkePayload> payloadList =
                hexStrListToIkePayloadList(payloadTypeList, payloadHexStringList, isResp);
        // Build a remotely generated NAT_DETECTION_SOURCE_IP payload to mock a remote node's
        // network that is not behind NAT.
        IkePayload sourceNatPayload =
                new IkeNotifyPayload(
                        NOTIFY_TYPE_NAT_DETECTION_SOURCE_IP,
                        IkeNotifyPayload.generateNatDetectionData(
                                initiatorSpi,
                                responderSpi,
                                REMOTE_ADDRESS,
                                IkeSocket.SERVER_PORT_UDP_ENCAPSULATED));
        payloadList.add(sourceNatPayload);
        return makeDummyUnencryptedReceivedIkePacket(
                initiatorSpi, responderSpi, eType, isResp, fromIkeInit, payloadList);
    }

    private ReceivedIkePacket makeDummyUnencryptedReceivedIkePacket(
            long initiatorSpi,
            long responderSpi,
            @IkeHeader.ExchangeType int eType,
            boolean isResp,
            boolean fromIkeInit,
            List<IkePayload> payloadList)
            throws Exception {
        IkeMessage dummyIkeMessage =
                makeDummyIkeMessageForTest(
                        initiatorSpi,
                        responderSpi,
                        eType,
                        isResp,
                        fromIkeInit,
                        0,
                        false /*isEncrypted*/,
                        payloadList);

        byte[] dummyIkePacketBytes = new byte[0];
        doReturn(new DecodeResultOk(dummyIkeMessage, dummyIkePacketBytes))
                .when(mMockIkeMessageHelper)
                .decode(0, dummyIkeMessage.ikeHeader, dummyIkePacketBytes);

        return new ReceivedIkePacket(dummyIkeMessage.ikeHeader, dummyIkePacketBytes);
    }

    private ReceivedIkePacket makeDummyEncryptedReceivedIkePacket(
            IkeSaRecord ikeSaRecord,
            @IkeHeader.ExchangeType int eType,
            boolean isResp,
            List<Integer> payloadTypeList,
            List<String> payloadHexStringList)
            throws Exception {
        List<IkePayload> payloadList =
                hexStrListToIkePayloadList(payloadTypeList, payloadHexStringList, isResp);
        return makeDummyEncryptedReceivedIkePacketWithPayloadList(
                ikeSaRecord, eType, isResp, payloadList);
    }

    private ReceivedIkePacket makeDummyEncryptedReceivedIkePacketWithPayloadList(
            IkeSaRecord ikeSaRecord,
            @IkeHeader.ExchangeType int eType,
            boolean isResp,
            List<IkePayload> payloadList)
            throws Exception {
        return makeDummyEncryptedReceivedIkePacketWithPayloadList(
                ikeSaRecord,
                eType,
                isResp,
                isResp
                        ? ikeSaRecord.getLocalRequestMessageId()
                        : ikeSaRecord.getRemoteRequestMessageId(),
                payloadList,
                new byte[0] /*dummyIkePacketBytes*/);
    }

    private ReceivedIkePacket makeDummyEncryptedReceivedIkePacketWithPayloadList(
            IkeSaRecord ikeSaRecord,
            @IkeHeader.ExchangeType int eType,
            boolean isResp,
            int msgId,
            List<IkePayload> payloadList,
            byte[] dummyIkePacketBytes)
            throws Exception {
        boolean fromIkeInit = !ikeSaRecord.isLocalInit;
        IkeMessage dummyIkeMessage =
                makeDummyIkeMessageForTest(
                        ikeSaRecord.getInitiatorSpi(),
                        ikeSaRecord.getResponderSpi(),
                        eType,
                        isResp,
                        fromIkeInit,
                        msgId,
                        true /*isEncyprted*/,
                        payloadList);

        setDecodeEncryptedPacketResult(
                ikeSaRecord,
                dummyIkeMessage.ikeHeader,
                null /*collectedFrags*/,
                new DecodeResultOk(dummyIkeMessage, dummyIkePacketBytes));

        return new ReceivedIkePacket(dummyIkeMessage.ikeHeader, dummyIkePacketBytes);
    }

    private ReceivedIkePacket makeDummyReceivedIkePacketWithInvalidSyntax(
            IkeSaRecord ikeSaRecord, boolean isResp, int eType) {
        return makeDummyReceivedIkePacketWithDecodingError(
                ikeSaRecord, isResp, eType, new InvalidSyntaxException("IkeStateMachineTest"));
    }

    private ReceivedIkePacket makeDummyReceivedIkePacketWithDecodingError(
            IkeSaRecord ikeSaRecord, boolean isResp, int eType, IkeProtocolException exception) {
        IkeHeader header =
                makeDummyIkeHeader(ikeSaRecord, isResp, eType, IkePayload.PAYLOAD_TYPE_SK);
        byte[] dummyPacket = new byte[0];
        doReturn(new DecodeResultProtectedError(exception, dummyPacket))
                .when(mMockIkeMessageHelper)
                .decode(anyInt(), any(), any(), eq(ikeSaRecord), eq(header), any(), any());

        return new ReceivedIkePacket(header, dummyPacket);
    }

    private ReceivedIkePacket makeDummyReceivedIkePacketWithUnprotectedError(
            IkeSaRecord ikeSaRecord, boolean isResp, int eType, IkeException exception) {
        IkeHeader header =
                makeDummyIkeHeader(ikeSaRecord, isResp, eType, IkePayload.PAYLOAD_TYPE_SK);
        byte[] dummyPacket = new byte[0];
        doReturn(new DecodeResultUnprotectedError(exception))
                .when(mMockIkeMessageHelper)
                .decode(anyInt(), any(), any(), eq(ikeSaRecord), eq(header), any(), any());

        return new ReceivedIkePacket(header, dummyPacket);
    }

    private ReceivedIkePacket makeDummyReceivedIkeFragmentPacket(
            IkeSaRecord ikeSaRecord,
            boolean isResp,
            int eType,
            IkeSkfPayload skfPayload,
            int nextPayloadType,
            DecodeResultPartial collectedFrags) {
        IkeHeader header =
                makeDummyIkeHeader(ikeSaRecord, isResp, eType, IkePayload.PAYLOAD_TYPE_SKF);

        byte[] dummyPacket = new byte[0];
        DecodeResultPartial resultFrags =
                new DecodeResultPartial(
                        header, dummyPacket, skfPayload, nextPayloadType, collectedFrags);
        setDecodeEncryptedPacketResult(ikeSaRecord, header, collectedFrags, resultFrags);

        return new ReceivedIkePacket(header, dummyPacket);
    }

    private ReceivedIkePacket makeDummyReceivedLastIkeFragmentPacketOk(
            IkeSaRecord ikeSaRecord,
            boolean isResp,
            int eType,
            DecodeResultPartial collectedFrags,
            List<IkePayload> payloadList,
            byte[] firstFragBytes) {
        IkeHeader header =
                makeDummyIkeHeader(ikeSaRecord, isResp, eType, IkePayload.PAYLOAD_TYPE_SKF);

        IkeMessage completeMessage = new IkeMessage(header, payloadList);

        setDecodeEncryptedPacketResult(
                ikeSaRecord,
                header,
                collectedFrags,
                new DecodeResultOk(completeMessage, firstFragBytes));

        return new ReceivedIkePacket(header, new byte[0] /*dummyIkePacketBytes*/);
    }

    private ReceivedIkePacket makeDummyReceivedLastIkeFragmentPacketError(
            IkeSaRecord ikeSaRecord,
            boolean isResp,
            int eType,
            DecodeResultPartial collectedFrags,
            IkeException exception) {
        IkeHeader header =
                makeDummyIkeHeader(ikeSaRecord, isResp, eType, IkePayload.PAYLOAD_TYPE_SKF);

        byte[] dummyIkePacketBytes = new byte[0];
        setDecodeEncryptedPacketResult(
                ikeSaRecord,
                header,
                collectedFrags,
                new DecodeResultProtectedError(exception, dummyIkePacketBytes));

        return new ReceivedIkePacket(header, dummyIkePacketBytes);
    }

    private IkeHeader makeDummyIkeHeader(
            IkeSaRecord ikeSaRecord, boolean isResp, int eType, int firstPayloadType) {
        return new IkeHeader(
                ikeSaRecord.getInitiatorSpi(),
                ikeSaRecord.getResponderSpi(),
                firstPayloadType,
                eType,
                isResp,
                !ikeSaRecord.isLocalInit,
                isResp
                        ? ikeSaRecord.getLocalRequestMessageId()
                        : ikeSaRecord.getRemoteRequestMessageId());
    }

    private void setDecodeEncryptedPacketResult(
            IkeSaRecord ikeSaRecord,
            IkeHeader header,
            DecodeResultPartial collectedFrags,
            DecodeResult result) {
        doReturn(result)
                .when(mMockIkeMessageHelper)
                .decode(
                        anyInt(),
                        any(),
                        any(),
                        eq(ikeSaRecord),
                        eq(header),
                        any(),
                        eq(collectedFrags));
    }

    private IkeMessage makeDummyIkeMessageForTest(
            long initSpi,
            long respSpi,
            @IkeHeader.ExchangeType int eType,
            boolean isResp,
            boolean fromikeInit,
            int messageId,
            boolean isEncrypted,
            List<IkePayload> payloadList)
            throws Exception {
        int firstPayloadType =
                isEncrypted ? IkePayload.PAYLOAD_TYPE_SK : IkePayload.PAYLOAD_TYPE_NO_NEXT;

        IkeHeader header =
                new IkeHeader(
                        initSpi, respSpi, firstPayloadType, eType, isResp, fromikeInit, messageId);

        return new IkeMessage(header, payloadList);
    }

    private static List<IkePayload> hexStrListToIkePayloadList(
            List<Integer> payloadTypeList, List<String> payloadHexStringList, boolean isResp)
            throws Exception {
        List<IkePayload> payloadList = new LinkedList<>();
        for (int i = 0; i < payloadTypeList.size(); i++) {
            payloadList.add(
                    IkeTestUtils.hexStringToIkePayload(
                            payloadTypeList.get(i), isResp, payloadHexStringList.get(i)));
        }
        return payloadList;
    }

    private void verifyDecodeEncryptedMessage(IkeSaRecord record, ReceivedIkePacket rcvPacket)
            throws Exception {
        verify(mMockIkeMessageHelper)
                .decode(
                        anyInt(),
                        any(),
                        any(),
                        eq(record),
                        eq(rcvPacket.ikeHeader),
                        eq(rcvPacket.ikePacketBytes),
                        eq(null));
    }

    private static IkeSaRecord makeDummyIkeSaRecord(long initSpi, long respSpi, boolean isLocalInit)
            throws IOException {
        Inet4Address initAddress = isLocalInit ? LOCAL_ADDRESS : REMOTE_ADDRESS;
        Inet4Address respAddress = isLocalInit ? REMOTE_ADDRESS : LOCAL_ADDRESS;

        return new IkeSaRecord(
                IKE_SPI_GENERATOR.allocateSpi(initAddress, initSpi),
                IKE_SPI_GENERATOR.allocateSpi(respAddress, respSpi),
                isLocalInit,
                TestUtils.hexStringToByteArray(NONCE_INIT_HEX_STRING),
                TestUtils.hexStringToByteArray(NONCE_RESP_HEX_STRING),
                new byte[KEY_LEN_IKE_SKD],
                new byte[KEY_LEN_IKE_INTE],
                new byte[KEY_LEN_IKE_INTE],
                new byte[KEY_LEN_IKE_ENCR],
                new byte[KEY_LEN_IKE_ENCR],
                TestUtils.hexStringToByteArray(PRF_KEY_INIT_HEX_STRING),
                TestUtils.hexStringToByteArray(PRF_KEY_RESP_HEX_STRING),
                mock(SaLifetimeAlarmScheduler.class));
    }

    private void mockScheduleRekey(SaLifetimeAlarmScheduler mockSaLifetimeAlarmScheduler) {
        IkeLocalRequest rekeyReq =
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE);
        doAnswer(
                (invocation) -> {
                        mIkeSessionStateMachine.sendMessageDelayed(
                                IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE,
                                rekeyReq,
                                mIkeSessionStateMachine.mIkeSessionParams
                                        .getSoftLifetimeMsInternal());
                        return null;
                })
                .when(mockSaLifetimeAlarmScheduler)
                .scheduleLifetimeExpiryAlarm(anyString());
    }

    @Before
    public void setUp() throws Exception {
        super.setUp();

        mSpyIkeLog = TestUtils.makeSpyLogThrowExceptionForWtf(TAG);
        IkeManager.setIkeLog(mSpyIkeLog);

        mEapSessionConfig =
                new EapSessionConfig.Builder()
                        .setEapSimConfig(EAP_SIM_SUB_ID, TelephonyManager.APPTYPE_USIM)
                        .build();

        mMockEapAuthenticatorFactory = mock(IkeEapAuthenticatorFactory.class);
        mMockEapAuthenticator = mock(EapAuthenticator.class);
        doReturn(mMockEapAuthenticator)
                .when(mMockEapAuthenticatorFactory)
                .newEapAuthenticator(
                        any(Looper.class),
                        any(IEapCallback.class),
                        any(Context.class),
                        any(EapSessionConfig.class),
                        any(RandomnessFactory.class));

        mRootCertificate = CertUtils.createCertFromPemFile("self-signed-ca-a.pem");
        mServerEndCertificate = CertUtils.createCertFromPemFile("end-cert-a.pem");
        mUserEndCert = CertUtils.createCertFromPemFile("end-cert-b.pem");
        mUserPrivateKey = CertUtils.createRsaPrivateKeyFromKeyFile("end-cert-key-b.key");

        mPsk = TestUtils.hexStringToByteArray(PSK_HEX_STRING);

        mChildSessionParams = buildChildSessionParams();

        mIkeEncryptionTransform =
                new EncryptionTransform(
                        SaProposal.ENCRYPTION_ALGORITHM_AES_CBC, SaProposal.KEY_LEN_AES_128);
        mIkeIntegrityTransform =
                new IntegrityTransform(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96);
        mIkePrfTransform = new PrfTransform(SaProposal.PSEUDORANDOM_FUNCTION_HMAC_SHA1);
        mIkeDhGroupTransform = new DhGroupTransform(SaProposal.DH_GROUP_1024_BIT_MODP);

        mSpyUserCbExecutor =
                spy(
                        (command) -> {
                            command.run();
                        });

        mMockIkeSessionCallback = mock(IkeSessionCallback.class);
        mMockChildSessionCallback = mock(ChildSessionCallback.class);

        mLooper = new TestLooper();

        mMockChildSessionStateMachine = mock(ChildSessionStateMachine.class);
        mMockChildSessionFactoryHelper = mock(IChildSessionFactoryHelper.class);
        ChildSessionStateMachineFactory.setChildSessionFactoryHelper(
                mMockChildSessionFactoryHelper);
        setupChildStateMachineFactory(mMockChildSessionStateMachine);

        // Setup state machine
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsPsk(mPsk));

        mMockIkeMessageHelper = mock(IkeMessage.IIkeMessageHelper.class);
        IkeMessage.setIkeMessageHelper(mMockIkeMessageHelper);
        resetMockIkeMessageHelper();

        mMockSaRecordHelper = mock(SaRecord.ISaRecordHelper.class);
        SaRecord.setSaRecordHelper(mMockSaRecordHelper);

        mSpyCurrentIkeSaRecord = spy(makeDummyIkeSaRecord(11, 12, true));
        mSpyLocalInitIkeSaRecord = spy(makeDummyIkeSaRecord(21, 22, true));
        mSpyRemoteInitIkeSaRecord = spy(makeDummyIkeSaRecord(31, 32, false));

        mExpectedCurrentSaLocalReqMsgId = 0;
        mExpectedCurrentSaRemoteReqMsgId = 0;
    }

    @After
    public void tearDown() throws Exception {
        mIkeSessionStateMachine.killSession();
        mLooper.dispatchAll();
        mIkeSessionStateMachine.setDbg(false);

        mSpyCurrentIkeSaRecord.close();
        mSpyLocalInitIkeSaRecord.close();
        mSpyRemoteInitIkeSaRecord.close();

        IkeManager.resetIkeLog();
        IkeMessage.setIkeMessageHelper(new IkeMessageHelper());
        SaRecord.setSaRecordHelper(new SaRecordHelper());
        ChildSessionStateMachineFactory.setChildSessionFactoryHelper(
                new ChildSessionFactoryHelper());
    }

    private IkeSessionStateMachine makeAndStartIkeSession(IkeSessionParams ikeParams)
            throws Exception {
        IkeSessionStateMachine ikeSession =
                new IkeSessionStateMachine(
                        mLooper.getLooper(),
                        mSpyContext,
                        mIpSecManager,
                        ikeParams,
                        mChildSessionParams,
                        mSpyUserCbExecutor,
                        mMockIkeSessionCallback,
                        mMockChildSessionCallback,
                        mMockEapAuthenticatorFactory);
        ikeSession.setDbg(true);

        mLooper.dispatchAll();
        ikeSession.mLocalAddress = LOCAL_ADDRESS;
        assertEquals(REMOTE_ADDRESS, ikeSession.mRemoteAddress);

        // Setup socket instances used by the IkeSessionStateMachine
        // TODO: rename these from spy to mock.
        mSpyIkeUdp4Socket = mock(IkeUdp4Socket.class);
        mSpyIkeUdp6Socket = mock(IkeUdp6Socket.class);
        mSpyIkeUdpEncapSocket = mock(IkeUdpEncapSocket.class);

        doNothing().when(mSpyIkeUdp4Socket).sendIkePacket(any(), any());
        doNothing().when(mSpyIkeUdp6Socket).sendIkePacket(any(), any());
        doNothing().when(mSpyIkeUdpEncapSocket).sendIkePacket(any(), any());

        // Always start with unencap'd socket.
        mSpyCurrentIkeSocket = mSpyIkeUdp4Socket;
        ikeSession.mIkeSocket = mSpyCurrentIkeSocket;
        return ikeSession;
    }

    public static IkeSaProposal buildSaProposal() throws Exception {
        return buildSaProposalCommon().addDhGroup(SaProposal.DH_GROUP_2048_BIT_MODP).build();
    }

    private static IkeSaProposal buildNegotiatedSaProposal() throws Exception {
        return buildSaProposalCommon().build();
    }

    private static IkeSaProposal.Builder buildSaProposalCommon() throws Exception {
        return new IkeSaProposal.Builder()
                .addEncryptionAlgorithm(
                        SaProposal.ENCRYPTION_ALGORITHM_AES_CBC, SaProposal.KEY_LEN_AES_128)
                .addIntegrityAlgorithm(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96)
                .addPseudorandomFunction(SaProposal.PSEUDORANDOM_FUNCTION_HMAC_SHA1)
                .addDhGroup(SaProposal.DH_GROUP_1024_BIT_MODP);
    }

    private IkeSessionParams.Builder buildIkeSessionParamsCommon() throws Exception {
        return new IkeSessionParams.Builder(mMockConnectManager)
                .setServerHostname(REMOTE_ADDRESS.getHostAddress())
                .addSaProposal(buildSaProposal())
                .setLocalIdentification(LOCAL_ID_IPV4)
                .setRemoteIdentification(REMOTE_ID_FQDN)
                .addPcscfServerRequest(AF_INET)
                .addPcscfServerRequest(AF_INET6)
                .setRetransmissionTimeoutsMillis(
                        new int[] {5000, 10000, 20000, 30000, 40000, 50000});
    }

    private IkeSessionParams buildIkeSessionParams() throws Exception {
        return buildIkeSessionParamsCommon().setAuthPsk(mPsk).build();
    }

    private IkeSessionParams buildIkeSessionParamsPsk(byte[] psk) throws Exception {
        return buildIkeSessionParamsCommon().setAuthPsk(psk).build();
    }

    private IkeSessionParams buildIkeSessionParamsEap() throws Exception {
        return buildIkeSessionParamsCommon()
                .setAuthEap(mRootCertificate, mEapSessionConfig)
                .build();
    }

    private IkeSessionParams buildIkeSessionParamsDigitalSignature() throws Exception {
        return buildIkeSessionParamsCommon()
                .setAuthDigitalSignature(mRootCertificate, mUserEndCert, mUserPrivateKey)
                .build();
    }

    private ChildSessionParams buildChildSessionParams() throws Exception {
        ChildSaProposal saProposal =
                new ChildSaProposal.Builder()
                        .addEncryptionAlgorithm(
                                SaProposal.ENCRYPTION_ALGORITHM_AES_CBC, SaProposal.KEY_LEN_AES_128)
                        .addIntegrityAlgorithm(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96)
                        .build();

        return new TunnelModeChildSessionParams.Builder()
                .addSaProposal(saProposal)
                .addInternalAddressRequest(AF_INET)
                .addInternalAddressRequest(AF_INET6)
                .build();
    }

    private ReceivedIkePacket makeIkeInitResponse() throws Exception {
        // TODO: Build real IKE INIT response when IKE INIT response validation is implemented.
        List<Integer> payloadTypeList = new LinkedList<>();
        List<String> payloadHexStringList = new LinkedList<>();

        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_SA);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_KE);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_NONCE);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_NOTIFY);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_NOTIFY);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_NOTIFY);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_NOTIFY);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_VENDOR);

        payloadHexStringList.add(IKE_SA_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(KE_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(NONCE_RESP_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(NAT_DETECTION_SOURCE_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(NAT_DETECTION_DESTINATION_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(FRAGMENTATION_SUPPORTED_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(SIGNATURE_HASH_SUPPORTED_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(VENDOR_ID_PAYLOAD_HEX_STRING);

        // In each test assign different IKE responder SPI in IKE INIT response to avoid remote SPI
        // collision during response validation.
        // STOPSHIP: b/131617794 allow #mockIkeSetup to be independent in each test after we can
        // support IkeSession cleanup.
        return makeDummyReceivedIkeInitRespPacket(
                1L /*initiator SPI*/,
                2L /*responder SPI*/,
                IkeHeader.EXCHANGE_TYPE_IKE_SA_INIT,
                true /*isResp*/,
                false /*fromIkeInit*/,
                payloadTypeList,
                payloadHexStringList);
    }

    private List<IkePayload> getIkeAuthPayloadListWithChildPayloads(
            List<IkePayload> authRelatedPayloads) throws Exception {
        List<Integer> payloadTypeList = new LinkedList<>();
        List<String> payloadHexStringList = new LinkedList<>();

        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_SA);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_TS_INITIATOR);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_TS_RESPONDER);

        payloadHexStringList.add(CHILD_SA_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(TS_INIT_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(TS_RESP_PAYLOAD_HEX_STRING);

        List<IkePayload> payloadList =
                hexStrListToIkePayloadList(payloadTypeList, payloadHexStringList, true /*isResp*/);
        payloadList.addAll(authRelatedPayloads);

        return payloadList;
    }

    private ReceivedIkePacket makeIkeAuthRespWithChildPayloads(List<IkePayload> authRelatedPayloads)
            throws Exception {
        List<IkePayload> payloadList = getIkeAuthPayloadListWithChildPayloads(authRelatedPayloads);

        return makeDummyEncryptedReceivedIkePacketWithPayloadList(
                mSpyCurrentIkeSaRecord,
                IkeHeader.EXCHANGE_TYPE_IKE_AUTH,
                true /*isResp*/,
                payloadList);
    }

    private ReceivedIkePacket makeIkeAuthRespWithoutChildPayloads(
            List<IkePayload> authRelatedPayloads) throws Exception {
        return makeDummyEncryptedReceivedIkePacketWithPayloadList(
                mSpyCurrentIkeSaRecord,
                IkeHeader.EXCHANGE_TYPE_IKE_AUTH,
                true /*isResp*/,
                authRelatedPayloads);
    }

    private ReceivedIkePacket makeCreateChildCreateMessage(boolean isResp) throws Exception {
        return makeDummyEncryptedReceivedIkePacketWithPayloadList(
                mSpyCurrentIkeSaRecord,
                IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA,
                isResp,
                makeCreateChildPayloadList(isResp));
    }

    private ReceivedIkePacket makeRekeyChildCreateMessage(boolean isResp, int spi)
            throws Exception {
        IkeNotifyPayload rekeyPayload =
                new IkeNotifyPayload(
                        IkePayload.PROTOCOL_ID_ESP,
                        spi,
                        IkeNotifyPayload.NOTIFY_TYPE_REKEY_SA,
                        new byte[0]);

        List<IkePayload> payloadList = makeCreateChildPayloadList(isResp);
        payloadList.add(rekeyPayload);

        return makeDummyEncryptedReceivedIkePacketWithPayloadList(
                mSpyCurrentIkeSaRecord,
                IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA,
                isResp,
                payloadList);
    }

    private List<IkePayload> makeCreateChildPayloadList(boolean isResp) throws Exception {
        List<Integer> payloadTypeList = new LinkedList<>();
        List<String> payloadHexStringList = new LinkedList<>();

        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_SA);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_NONCE);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_TS_INITIATOR);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_TS_RESPONDER);

        payloadHexStringList.add(CHILD_SA_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(NONCE_RESP_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(TS_INIT_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(TS_RESP_PAYLOAD_HEX_STRING);

        return hexStrListToIkePayloadList(payloadTypeList, payloadHexStringList, isResp);
    }

    private ReceivedIkePacket makeDeleteChildPacket(IkeDeletePayload[] payloads, boolean isResp)
            throws Exception {
        return makeDummyEncryptedReceivedIkePacketWithPayloadList(
                mSpyCurrentIkeSaRecord,
                IkeHeader.EXCHANGE_TYPE_INFORMATIONAL,
                isResp,
                Arrays.asList(payloads));
    }

    private ReceivedIkePacket makeRekeyIkeResponse() throws Exception {
        List<Integer> payloadTypeList = new LinkedList<>();
        List<String> payloadHexStringList = new LinkedList<>();

        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_SA);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_KE);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_NONCE);

        payloadHexStringList.add(IKE_REKEY_SA_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(KE_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(NONCE_RESP_PAYLOAD_HEX_STRING);

        return makeDummyEncryptedReceivedIkePacket(
                mSpyCurrentIkeSaRecord,
                IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA,
                true /*isResp*/,
                payloadTypeList,
                payloadHexStringList);
    }

    private ReceivedIkePacket makeDeleteIkeResponse(IkeSaRecord ikeSaRecord) throws Exception {
        return makeDummyEncryptedReceivedIkePacket(
                ikeSaRecord,
                IkeHeader.EXCHANGE_TYPE_INFORMATIONAL,
                true /*isResp*/,
                new LinkedList<>(),
                new LinkedList<>());
    }

    private ReceivedIkePacket makeDpdIkeRequest(IkeSaRecord saRecord) throws Exception {
        return makeDummyEncryptedReceivedIkePacket(
                saRecord,
                IkeHeader.EXCHANGE_TYPE_INFORMATIONAL,
                false /*isResp*/,
                new LinkedList<>(),
                new LinkedList<>());
    }

    private ReceivedIkePacket makeDpdIkeRequest(int msgId, byte[] dummyIkePacketBytes)
            throws Exception {
        return makeDummyEncryptedReceivedIkePacketWithPayloadList(
                mSpyCurrentIkeSaRecord,
                EXCHANGE_TYPE_INFORMATIONAL,
                false /*isResp*/,
                msgId,
                new LinkedList<>(),
                dummyIkePacketBytes);
    }

    private ReceivedIkePacket makeRekeyIkeRequest() throws Exception {
        IkeSaPayload saPayload =
                (IkeSaPayload)
                        IkeTestUtils.hexStringToIkePayload(
                                IkePayload.PAYLOAD_TYPE_SA,
                                false /*isResp*/,
                                IKE_REKEY_SA_PAYLOAD_HEX_STRING);
        return makeRekeyIkeRequest(saPayload);
    }

    private ReceivedIkePacket makeRekeyIkeRequestWithUnacceptableProposal() throws Exception {
        IkeSaPayload saPayload =
                (IkeSaPayload)
                        IkeTestUtils.hexStringToIkePayload(
                                IkePayload.PAYLOAD_TYPE_SA,
                                false /*isResp*/,
                                IKE_REKEY_UNACCEPTABLE_SA_PAYLOAD_HEX_STRING);
        return makeRekeyIkeRequest(saPayload);
    }

    private ReceivedIkePacket makeRekeyIkeRequest(IkeSaPayload saPayload) throws Exception {
        List<Integer> payloadTypeList = new LinkedList<>();
        List<String> payloadHexStringList = new LinkedList<>();

        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_KE);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_NONCE);

        payloadHexStringList.add(KE_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(NONCE_INIT_PAYLOAD_HEX_STRING);

        List<IkePayload> payloadList =
                hexStrListToIkePayloadList(payloadTypeList, payloadHexStringList, false /*isResp*/);
        payloadList.add(saPayload);

        return makeDummyEncryptedReceivedIkePacketWithPayloadList(
                mSpyCurrentIkeSaRecord,
                IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA,
                false /*isResp*/,
                payloadList);
    }

    private ReceivedIkePacket makeDeleteIkeRequest(IkeSaRecord saRecord) throws Exception {
        List<Integer> payloadTypeList = new LinkedList<>();
        List<String> payloadHexStringList = new LinkedList<>();

        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_DELETE);

        payloadHexStringList.add(DELETE_IKE_PAYLOAD_HEX_STRING);

        return makeDummyEncryptedReceivedIkePacket(
                saRecord,
                IkeHeader.EXCHANGE_TYPE_INFORMATIONAL,
                false /*isResp*/,
                payloadTypeList,
                payloadHexStringList);
    }

    private ReceivedIkePacket makeResponseWithErrorNotify(IkeNotifyPayload notify)
            throws Exception {
        List<IkePayload> payloads = new LinkedList<>();
        payloads.add(notify);
        return makeDummyEncryptedReceivedIkePacketWithPayloadList(
                mSpyCurrentIkeSaRecord, EXCHANGE_TYPE_INFORMATIONAL, true /*isResp*/, payloads);
    }

    private static boolean isIkePayloadExist(
            List<IkePayload> payloadList, @IkePayload.PayloadType int payloadType) {
        for (IkePayload payload : payloadList) {
            if (payload.payloadType == payloadType) return true;
        }
        return false;
    }

    private static boolean isNotifyExist(
            List<IkePayload> payloadList, @IkeNotifyPayload.NotifyType int notifyType) {
        for (IkeNotifyPayload notify :
                IkePayload.getPayloadListForTypeInProvidedList(
                        PAYLOAD_TYPE_NOTIFY, IkeNotifyPayload.class, payloadList)) {
            if (notify.notifyType == notifyType) return true;
        }
        return false;
    }

    private static void assertByteArrayListEquals(
            List<byte[]> expectedList, List<byte[]> resultList) {
        assertEquals(expectedList.size(), resultList.size());
        for (int i = 0; i < expectedList.size(); i++) {
            assertArrayEquals(expectedList.get(i), resultList.get(i));
        }
    }

    private void verifyIncrementLocaReqMsgId() {
        assertEquals(
                ++mExpectedCurrentSaLocalReqMsgId,
                mSpyCurrentIkeSaRecord.getLocalRequestMessageId());
    }

    private void verifyIncrementRemoteReqMsgId() {
        assertEquals(
                ++mExpectedCurrentSaRemoteReqMsgId,
                mSpyCurrentIkeSaRecord.getRemoteRequestMessageId());
    }

    private void verifyRetransmissionStarted() {
        assertTrue(
                mIkeSessionStateMachine
                        .getHandler()
                        .hasMessages(IkeSessionStateMachine.CMD_RETRANSMIT));
    }

    private void verifyRetransmissionStopped() {
        assertFalse(
                mIkeSessionStateMachine
                        .getHandler()
                        .hasMessages(IkeSessionStateMachine.CMD_RETRANSMIT));
    }

    private IkeMessage verifyEncryptAndEncodeAndGetMessage(IkeSaRecord ikeSaRecord) {
        verify(mMockIkeMessageHelper)
                .encryptAndEncode(
                        anyObject(),
                        anyObject(),
                        eq(ikeSaRecord),
                        mIkeMessageCaptor.capture(),
                        anyBoolean(),
                        anyInt());
        return mIkeMessageCaptor.getValue();
    }

    private void verifyEncryptAndEncodeNeverCalled(IkeSaRecord ikeSaRecord) {
        verify(mMockIkeMessageHelper, never())
                .encryptAndEncode(
                        anyObject(),
                        anyObject(),
                        eq(ikeSaRecord),
                        any(IkeMessage.class),
                        anyBoolean(),
                        anyInt());
    }

    private void verifyEncryptAndEncodeNeverCalled() {
        verify(mMockIkeMessageHelper, never())
                .encryptAndEncode(
                        anyObject(),
                        anyObject(),
                        any(IkeSaRecord.class),
                        any(IkeMessage.class),
                        anyBoolean(),
                        anyInt());
    }

    private void resetMockIkeMessageHelper() {
        reset(mMockIkeMessageHelper);
        doReturn(new byte[0]).when(mMockIkeMessageHelper).encode(any());
        doReturn(new byte[1][0])
                .when(mMockIkeMessageHelper)
                .encryptAndEncode(any(), any(), any(), any(), anyBoolean(), anyInt());
    }

    @Test
    public void testQuit() {
        mIkeSessionStateMachine.quit();
        mLooper.dispatchAll();

        verify(mSpyCurrentIkeSocket).releaseReference(eq(mIkeSessionStateMachine));
        verify(mMockBusyWakelock).release();
    }

    @Test
    public void testAllocateIkeSpi() throws Exception {
        // Test randomness.
        IkeSecurityParameterIndex ikeSpiOne = IKE_SPI_GENERATOR.allocateSpi(LOCAL_ADDRESS);
        IkeSecurityParameterIndex ikeSpiTwo = IKE_SPI_GENERATOR.allocateSpi(LOCAL_ADDRESS);

        assertNotEquals(ikeSpiOne.getSpi(), ikeSpiTwo.getSpi());
        ikeSpiTwo.close();

        // Test duplicate SPIs.
        long spiValue = ikeSpiOne.getSpi();
        try {
            IKE_SPI_GENERATOR.allocateSpi(LOCAL_ADDRESS, spiValue);
            fail("Expected to fail because duplicate SPI was assigned to the same address.");
        } catch (IOException expected) {

        }

        ikeSpiOne.close();
        IkeSecurityParameterIndex ikeSpiThree =
                IKE_SPI_GENERATOR.allocateSpi(LOCAL_ADDRESS, spiValue);
        ikeSpiThree.close();
    }

    private void setupFirstIkeSa() throws Exception {
        // Inject IkeSaRecord and release IKE SPI resource since we will lose their references
        // later.
        when(mMockSaRecordHelper.makeFirstIkeSaRecord(any(), any(), any()))
                .thenAnswer(
                        (invocation) -> {
                            captureAndReleaseIkeSpiResource(invocation, 2);
                            mockScheduleRekey(mSpyCurrentIkeSaRecord.mSaLifetimeAlarmScheduler);
                            mSpyCurrentIkeSaRecord.mSaLifetimeAlarmScheduler
                                    .scheduleLifetimeExpiryAlarm(anyString());
                            return mSpyCurrentIkeSaRecord;
                        });
    }

    private void setupRekeyedIkeSa(IkeSaRecord rekeySaRecord) throws Exception {
        // Inject IkeSaRecord and release IKE SPI resource since we will lose their references
        // later.
        when(mMockSaRecordHelper.makeRekeyedIkeSaRecord(
                        eq(mSpyCurrentIkeSaRecord), any(), any(), any(), any()))
                .thenAnswer(
                        (invocation) -> {
                            captureAndReleaseIkeSpiResource(invocation, 4);
                            mockScheduleRekey(rekeySaRecord.mSaLifetimeAlarmScheduler);
                            rekeySaRecord.mSaLifetimeAlarmScheduler.scheduleLifetimeExpiryAlarm(
                                    anyString());
                            return rekeySaRecord;
                        });
    }

    private void throwExceptionWhenMakeRekeyIkeSa(Exception exception) throws Exception {
        // Inject IkeSaRecord and release IKE SPI resource since we will lose their references
        // later.
        when(mMockSaRecordHelper.makeRekeyedIkeSaRecord(
                        eq(mSpyCurrentIkeSaRecord), any(), any(), any(), any()))
                .thenAnswer(
                        (invocation) -> {
                            captureAndReleaseIkeSpiResource(invocation, 4);
                            throw exception;
                        });
    }

    private void captureAndReleaseIkeSpiResource(InvocationOnMock invocation, int ikeConfigIndex) {
        IkeSaRecordConfig config = (IkeSaRecordConfig) invocation.getArguments()[ikeConfigIndex];
        config.initSpi.close();
        config.respSpi.close();
    }

    @Test
    public void testResolveRemoteHostName() throws Exception {
        mIkeSessionStateMachine.quitNow();

        IkeSessionParams ikeParams =
                buildIkeSessionParamsCommon()
                        .setAuthPsk(mPsk)
                        .setServerHostname(REMOTE_HOSTNAME)
                        .build();
        mIkeSessionStateMachine = makeAndStartIkeSession(ikeParams);

        verify(mMockDefaultNetwork).getByName(REMOTE_HOSTNAME);
    }

    @Test
    public void testEnableTestMode() throws Exception {
        doReturn(true)
                .when(mMockNetworkCapabilities)
                .hasTransport(RandomnessFactory.TRANSPORT_TEST);

        IkeSessionStateMachine ikeSession = makeAndStartIkeSession(buildIkeSessionParams());

        SecureRandom random = ikeSession.mRandomFactory.getRandom();
        assertNotNull(random);
        assertTrue(random instanceof DeterministicSecureRandom);
    }

    @Test
    public void testDisableTestMode() throws Exception {
        doReturn(false)
                .when(mMockNetworkCapabilities)
                .hasTransport(RandomnessFactory.TRANSPORT_TEST);

        IkeSessionStateMachine ikeSession = makeAndStartIkeSession(buildIkeSessionParams());

        SecureRandom random = ikeSession.mRandomFactory.getRandom();
        assertNull(random);
    }

    @Test
    public void testCreateIkeLocalIkeInitNegotiatesDhGroup() throws Exception {
        setupFirstIkeSa();
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_LOCAL_REQUEST_CREATE_IKE);
        mLooper.dispatchAll();

        // Verify we started with the top proposed DH group
        assertEquals(
                SaProposal.DH_GROUP_1024_BIT_MODP, mIkeSessionStateMachine.mPeerSelectedDhGroup);

        // Send back a INVALID_KE_PAYLOAD, and verify that the selected DH group changes
        ReceivedIkePacket resp =
                makeDummyReceivedIkeInitRespPacket(
                        1L /*initiator SPI*/,
                        2L /*responder SPI*/,
                        IkeHeader.EXCHANGE_TYPE_IKE_SA_INIT,
                        true /*isResp*/,
                        false /*fromIkeInit*/,
                        Arrays.asList(IkePayload.PAYLOAD_TYPE_NOTIFY),
                        Arrays.asList(INVALID_KE_PAYLOAD_HEX_STRING));
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, resp);
        mLooper.dispatchAll();

        assertEquals(
                SaProposal.DH_GROUP_2048_BIT_MODP, mIkeSessionStateMachine.mPeerSelectedDhGroup);
    }

    @Test
    public void testCreateIkeLocalIkeInitSwitchesToEncapPorts() throws Exception {
        setupFirstIkeSa();
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_LOCAL_REQUEST_CREATE_IKE);
        mLooper.dispatchAll();

        // Receive IKE INIT response
        ReceivedIkePacket dummyReceivedIkePacket = makeIkeInitResponse();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyReceivedIkePacket);
        mLooper.dispatchAll();

        // Validate socket switched
        assertTrue(mIkeSessionStateMachine.mIkeSocket instanceof IkeUdpEncapSocket);
        verify(mSpyIkeUdp4Socket).unregisterIke(anyLong());
    }

    @Ignore
    public void disableTestCreateIkeLocalIkeInit() throws Exception {
        setupFirstIkeSa();

        // Send IKE INIT request
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_LOCAL_REQUEST_CREATE_IKE);
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        // Receive IKE INIT response
        ReceivedIkePacket dummyReceivedIkePacket = makeIkeInitResponse();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyReceivedIkePacket);
        mLooper.dispatchAll();
        verifyIncrementLocaReqMsgId();

        // Validate outbound IKE INIT request
        verify(mMockIkeMessageHelper, times(2)).encode(mIkeMessageCaptor.capture());
        IkeMessage ikeInitReqMessage = mIkeMessageCaptor.getValue();

        IkeHeader ikeHeader = ikeInitReqMessage.ikeHeader;
        assertEquals(IkeHeader.EXCHANGE_TYPE_IKE_SA_INIT, ikeHeader.exchangeType);
        assertFalse(ikeHeader.isResponseMsg);
        assertTrue(ikeHeader.fromIkeInitiator);

        List<IkePayload> payloadList = ikeInitReqMessage.ikePayloadList;
        assertTrue(isIkePayloadExist(payloadList, IkePayload.PAYLOAD_TYPE_SA));
        assertTrue(isIkePayloadExist(payloadList, IkePayload.PAYLOAD_TYPE_KE));
        assertTrue(isIkePayloadExist(payloadList, IkePayload.PAYLOAD_TYPE_NONCE));
        assertTrue(isNotifyExist(payloadList, NOTIFY_TYPE_NAT_DETECTION_SOURCE_IP));
        assertTrue(isNotifyExist(payloadList, NOTIFY_TYPE_NAT_DETECTION_DESTINATION_IP));
        assertTrue(isNotifyExist(payloadList, NOTIFY_TYPE_IKEV2_FRAGMENTATION_SUPPORTED));
        assertTrue(isNotifyExist(payloadList, NOTIFY_TYPE_SIGNATURE_HASH_ALGORITHMS));

        verify(mSpyCurrentIkeSocket)
                .registerIke(eq(mSpyCurrentIkeSaRecord.getLocalSpi()), eq(mIkeSessionStateMachine));

        verify(mMockIkeMessageHelper)
                .decode(0, dummyReceivedIkePacket.ikeHeader, dummyReceivedIkePacket.ikePacketBytes);
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.CreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();

        // Validate negotiated SA proposal.
        IkeSaProposal negotiatedProposal = mIkeSessionStateMachine.mSaProposal;
        assertNotNull(negotiatedProposal);

        assertEquals(
                new EncryptionTransform[] {mIkeEncryptionTransform},
                negotiatedProposal.getEncryptionTransforms());
        assertEquals(
                new IntegrityTransform[] {mIkeIntegrityTransform},
                negotiatedProposal.getIntegrityTransforms());
        assertEquals(new PrfTransform[] {mIkePrfTransform}, negotiatedProposal.getPrfTransforms());

        // Validate current IkeSaRecord.
        verify(mMockSaRecordHelper)
                .makeFirstIkeSaRecord(
                        any(IkeMessage.class),
                        any(IkeMessage.class),
                        mIkeSaRecordConfigCaptor.capture());

        IkeSaRecordConfig ikeSaRecordConfig = mIkeSaRecordConfigCaptor.getValue();
        assertEquals(KEY_LEN_IKE_PRF, ikeSaRecordConfig.prf.getKeyLength());
        assertEquals(KEY_LEN_IKE_INTE, ikeSaRecordConfig.integrityKeyLength);
        assertEquals(KEY_LEN_IKE_ENCR, ikeSaRecordConfig.encryptionKeyLength);
        assertNotNull(ikeSaRecordConfig.saLifetimeAlarmScheduler);

        // Validate NAT detection
        assertTrue(mIkeSessionStateMachine.mIsLocalBehindNat);
        assertFalse(mIkeSessionStateMachine.mIsRemoteBehindNat);

        // Validate vendor IDs
        assertByteArrayListEquals(
                Arrays.asList(REMOTE_VENDOR_ID_ONE), mIkeSessionStateMachine.mRemoteVendorIds);

        // Validate fragmentation support negotiation
        assertTrue(mIkeSessionStateMachine.mSupportFragment);
        assertEquals(
                Arrays.asList(EXTENSION_TYPE_FRAGMENTATION),
                mIkeSessionStateMachine.mEnabledExtensions);
    }

    private void setIkeInitResults() throws Exception {
        mIkeSessionStateMachine.mIkeCipher = mock(IkeCipher.class);
        mIkeSessionStateMachine.mIkeIntegrity = mock(IkeMacIntegrity.class);
        mIkeSessionStateMachine.mIkePrf = mock(IkeMacPrf.class);
        mIkeSessionStateMachine.mSaProposal = buildNegotiatedSaProposal();
        mIkeSessionStateMachine.mCurrentIkeSaRecord = mSpyCurrentIkeSaRecord;
        mIkeSessionStateMachine.mLocalAddress = LOCAL_ADDRESS;
        mIkeSessionStateMachine.mIsLocalBehindNat = true;
        mIkeSessionStateMachine.mIsRemoteBehindNat = false;
        mIkeSessionStateMachine.mSupportFragment = true;
        mIkeSessionStateMachine.mRemoteVendorIds =
                Arrays.asList(REMOTE_VENDOR_ID_ONE, REMOTE_VENDOR_ID_TWO);
        mIkeSessionStateMachine.mEnabledExtensions = Arrays.asList(EXTENSION_TYPE_FRAGMENTATION);
        mIkeSessionStateMachine.addIkeSaRecord(mSpyCurrentIkeSaRecord);

        mSpyCurrentIkeSocket = mSpyIkeUdpEncapSocket;
        mIkeSessionStateMachine.mIkeSocket = mSpyCurrentIkeSocket;
    }

    /** Initializes the mIkeSessionStateMachine in the IDLE state. */
    private void setupIdleStateMachine() throws Exception {
        verify(mMockBusyWakelock).acquire();

        setIkeInitResults();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION, mIkeSessionStateMachine.mIdle);
        mLooper.dispatchAll();

        mDummyChildSmCallback =
                createChildAndGetChildSessionSmCallback(
                        mMockChildSessionStateMachine, CHILD_SPI_REMOTE, mMockChildSessionCallback);

        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);

        verify(mMockBusyWakelock).release();

        // For convenience to verify wakelocks in all other places.
        reset(mMockBusyWakelock);
    }

    private void mockIkeInitAndTransitionToIkeAuth(State authState) throws Exception {
        setIkeInitResults();

        // Need to create a real IkeMacPrf instance for authentication because we cannot inject a
        // method stub for IkeMacPrf#signBytes. IkeMacPrf#signBytes is inheritted from a package
        // protected class IkePrf. We don't have the visibility to mock it.
        mIkeSessionStateMachine.mIkePrf =
                IkeMacPrf.create(new PrfTransform(SaProposal.PSEUDORANDOM_FUNCTION_HMAC_SHA1));

        mIkeSessionStateMachine.mIkeInitRequestBytes = new byte[0];
        mIkeSessionStateMachine.mIkeInitResponseBytes = new byte[0];
        mIkeSessionStateMachine.mIkeInitNoncePayload =
                new IkeNoncePayload(createMockRandomFactory());
        mIkeSessionStateMachine.mIkeRespNoncePayload =
                new IkeNoncePayload(createMockRandomFactory());

        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_FORCE_TRANSITION, authState);
        mLooper.dispatchAll();
    }

    private void setupChildStateMachineFactory(ChildSessionStateMachine child) {
        // After state machine start, add to the callback->statemachine map
        when(mMockChildSessionFactoryHelper.makeChildSessionStateMachine(
                        eq(mLooper.getLooper()),
                        eq(mSpyContext),
                        anyInt(),
                        any(AlarmManager.class),
                        any(RandomnessFactory.class),
                        any(IpSecSpiGenerator.class),
                        eq(mChildSessionParams),
                        eq(mSpyUserCbExecutor),
                        any(ChildSessionCallback.class),
                        any(IChildSessionSmCallback.class)))
                .thenReturn(child);
    }

    /**
     * Utility to register a new callback -> state machine mapping.
     *
     * <p>Must be used if IkeSessionStateMachine.openChildSession() is not called, but commands
     * injected instead.
     *
     * @param callback The callback to be used for the mapping
     * @param sm The ChildSessionStateMachine instance to be used.
     */
    private void registerChildStateMachine(
            ChildSessionCallback callback, ChildSessionStateMachine sm) {
        setupChildStateMachineFactory(sm);
        mIkeSessionStateMachine.registerChildSessionCallback(
                mChildSessionParams, callback, false /*isFirstChild*/);
    }

    @Test
    public void testCreateAdditionalChild() throws Exception {
        setupIdleStateMachine();

        ChildSessionCallback childCallback = mock(ChildSessionCallback.class);
        ChildSessionStateMachine childStateMachine = mock(ChildSessionStateMachine.class);
        registerChildStateMachine(childCallback, childStateMachine);

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new ChildLocalRequest(
                        IkeSessionStateMachine.CMD_LOCAL_REQUEST_CREATE_CHILD,
                        childCallback,
                        mChildSessionParams));
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);
        verify(childStateMachine)
                .createChildSession(
                        eq(LOCAL_ADDRESS),
                        eq(REMOTE_ADDRESS),
                        any(), // udpEncapSocket
                        eq(mIkeSessionStateMachine.mIkePrf),
                        any()); // sk_d

        // Once for initial child, a second time for the additional child.
        verify(mMockChildSessionFactoryHelper)
                .makeChildSessionStateMachine(
                        eq(mLooper.getLooper()),
                        eq(mSpyContext),
                        anyInt(),
                        any(AlarmManager.class),
                        any(RandomnessFactory.class),
                        any(IpSecSpiGenerator.class),
                        eq(mChildSessionParams),
                        eq(mSpyUserCbExecutor),
                        eq(childCallback),
                        mChildSessionSmCbCaptor.capture());
        IChildSessionSmCallback cb = mChildSessionSmCbCaptor.getValue();

        // Mocking sending request
        cb.onOutboundPayloadsReady(
                IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA,
                false /*isResp*/,
                new LinkedList<>(),
                childStateMachine);
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        IkeMessage createChildRequest = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);

        IkeHeader ikeHeader = createChildRequest.ikeHeader;
        assertEquals(IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA, ikeHeader.exchangeType);
        assertFalse(ikeHeader.isResponseMsg);
        assertTrue(ikeHeader.fromIkeInitiator);
        assertEquals(mSpyCurrentIkeSaRecord.getLocalRequestMessageId(), ikeHeader.messageId);
        assertTrue(createChildRequest.ikePayloadList.isEmpty());

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);

        // Mocking receiving response
        ReceivedIkePacket dummyCreateChildResp = makeCreateChildCreateMessage(true /*isResp*/);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyCreateChildResp);
        mLooper.dispatchAll();

        verifyIncrementLocaReqMsgId();
        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyCreateChildResp);

        verify(childStateMachine)
                .receiveResponse(
                        eq(IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA), mPayloadListCaptor.capture());

        List<IkePayload> childRespList = mPayloadListCaptor.getValue();
        assertTrue(isIkePayloadExist(childRespList, IkePayload.PAYLOAD_TYPE_SA));
        assertTrue(isIkePayloadExist(childRespList, IkePayload.PAYLOAD_TYPE_TS_INITIATOR));
        assertTrue(isIkePayloadExist(childRespList, IkePayload.PAYLOAD_TYPE_TS_RESPONDER));
        assertTrue(isIkePayloadExist(childRespList, IkePayload.PAYLOAD_TYPE_NONCE));

        // Mock finishing procedure
        cb.onProcedureFinished(childStateMachine);
        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
        verifyRetransmissionStopped();
    }

    @Test
    public void testTriggerDeleteChildLocal() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new ChildLocalRequest(
                        IkeSessionStateMachine.CMD_LOCAL_REQUEST_DELETE_CHILD,
                        mMockChildSessionCallback,
                        null /*childParams*/));
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);
        verify(mMockChildSessionStateMachine).deleteChildSession();
        verify(mMockBusyWakelock).acquire();
    }

    @Test
    public void testHandleDeleteChildBeforeCreation() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new ChildLocalRequest(
                        IkeSessionStateMachine.CMD_LOCAL_REQUEST_DELETE_CHILD,
                        mock(ChildSessionCallback.class),
                        null /*childParams*/));
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Test
    public void testTriggerRekeyChildLocal() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new ChildLocalRequest(
                        IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_CHILD,
                        mMockChildSessionCallback,
                        null /*childParams*/));
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);
        verify(mMockChildSessionStateMachine).rekeyChildSession();
        verify(mMockBusyWakelock).acquire();
    }

    private IChildSessionSmCallback createChildAndGetChildSessionSmCallback(
            ChildSessionStateMachine child, int remoteSpi) throws Exception {
        return createChildAndGetChildSessionSmCallback(
                child, remoteSpi, mock(ChildSessionCallback.class));
    }

    private IChildSessionSmCallback createChildAndGetChildSessionSmCallback(
            ChildSessionStateMachine child, int remoteSpi, ChildSessionCallback childCallback)
            throws Exception {
        registerChildStateMachine(childCallback, child);

        IChildSessionSmCallback cb = mIkeSessionStateMachine.new ChildSessionSmCallback();
        cb.onChildSaCreated(remoteSpi, child);
        mLooper.dispatchAll();

        return cb;
    }

    private void transitionToChildProcedureOngoing() {
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION,
                mIkeSessionStateMachine.mChildProcedureOngoing);
        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);
    }

    private void verifyChildReceiveDeleteRequest(
            ChildSessionStateMachine child, IkeDeletePayload[] expectedDelPayloads) {
        verify(child)
                .receiveRequest(
                        eq(IKE_EXCHANGE_SUBTYPE_DELETE_CHILD),
                        eq(EXCHANGE_TYPE_INFORMATIONAL),
                        mPayloadListCaptor.capture());
        List<IkePayload> reqPayloads = mPayloadListCaptor.getValue();

        int numExpectedDelPayloads = expectedDelPayloads.length;
        assertEquals(numExpectedDelPayloads, reqPayloads.size());

        for (int i = 0; i < numExpectedDelPayloads; i++) {
            assertEquals(expectedDelPayloads[i], (IkeDeletePayload) reqPayloads.get(i));
        }
    }

    private void outboundDeleteChildPayloadsReady(
            IChildSessionSmCallback childSmCb,
            IkeDeletePayload delPayload,
            boolean isResp,
            ChildSessionStateMachine child) {
        List<IkePayload> outPayloadList = new LinkedList<>();
        outPayloadList.add(delPayload);
        childSmCb.onOutboundPayloadsReady(
                IkeHeader.EXCHANGE_TYPE_INFORMATIONAL, isResp, outPayloadList, child);
        mLooper.dispatchAll();
    }

    private List<IkePayload> verifyOutInfoMsgHeaderAndGetPayloads(boolean isResp) {
        IkeMessage deleteChildMessage = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);

        IkeHeader ikeHeader = deleteChildMessage.ikeHeader;
        assertEquals(mSpyCurrentIkeSaRecord.getInitiatorSpi(), ikeHeader.ikeInitiatorSpi);
        assertEquals(mSpyCurrentIkeSaRecord.getResponderSpi(), ikeHeader.ikeResponderSpi);
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, ikeHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_INFORMATIONAL, ikeHeader.exchangeType);
        assertEquals(mSpyCurrentIkeSaRecord.isLocalInit, ikeHeader.fromIkeInitiator);
        assertEquals(isResp, ikeHeader.isResponseMsg);

        return deleteChildMessage.ikePayloadList;
    }

    @Test
    public void testDeferChildRequestToChildProcedureOngoing() throws Exception {
        setupIdleStateMachine();

        IkeDeletePayload[] inboundDelPayloads =
                new IkeDeletePayload[] {new IkeDeletePayload(new int[] {CHILD_SPI_REMOTE})};
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeDeleteChildPacket(inboundDelPayloads, false /*isResp*/));
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);
        verifyChildReceiveDeleteRequest(mMockChildSessionStateMachine, inboundDelPayloads);
    }

    @Test
    public void testRemoteDeleteOneChild() throws Exception {
        setupIdleStateMachine();
        transitionToChildProcedureOngoing();

        // Receive Delete Child Request
        IkeDeletePayload[] inboundDelPayloads =
                new IkeDeletePayload[] {new IkeDeletePayload(new int[] {CHILD_SPI_REMOTE})};
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeDeleteChildPacket(inboundDelPayloads, false /*isResp*/));
        mLooper.dispatchAll();

        // Verify received payloads
        verifyChildReceiveDeleteRequest(mMockChildSessionStateMachine, inboundDelPayloads);

        // Outbound payload list ready
        IkeDeletePayload outDelPayload = new IkeDeletePayload(new int[] {CHILD_SPI_LOCAL});
        outboundDeleteChildPayloadsReady(
                mDummyChildSmCallback,
                outDelPayload,
                true /*isResp*/,
                mMockChildSessionStateMachine);

        // Verify outbound response
        List<IkePayload> payloadList = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(1, payloadList.size());
        assertEquals(outDelPayload, ((IkeDeletePayload) payloadList.get(0)));
        verify(mMockBusyWakelock).acquire();
    }

    @Test
    public void testRemoteDeleteMultipleChildSession() throws Exception {
        ChildSessionStateMachine childOne = mock(ChildSessionStateMachine.class);
        int childOneRemoteSpi = 11;
        int childOneLocalSpi = 12;

        ChildSessionStateMachine childTwo = mock(ChildSessionStateMachine.class);
        int childTwoRemoteSpi = 21;
        int childTwoLocalSpi = 22;

        setupIdleStateMachine();
        IChildSessionSmCallback childSmCbOne =
                createChildAndGetChildSessionSmCallback(childOne, childOneRemoteSpi);
        IChildSessionSmCallback childSmCbTwo =
                createChildAndGetChildSessionSmCallback(childTwo, childTwoRemoteSpi);

        transitionToChildProcedureOngoing();

        // Receive Delete Child Request
        IkeDeletePayload[] inboundDelPayloads =
                new IkeDeletePayload[] {
                    new IkeDeletePayload(new int[] {childOneRemoteSpi, childTwoRemoteSpi})
                };
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeDeleteChildPacket(inboundDelPayloads, false /*isResp*/));
        mLooper.dispatchAll();

        // Verify received payloads
        verifyChildReceiveDeleteRequest(childOne, inboundDelPayloads);
        verifyChildReceiveDeleteRequest(childTwo, inboundDelPayloads);

        // childOne outbound payload list ready
        IkeDeletePayload outDelPayloadOne = new IkeDeletePayload(new int[] {childOneLocalSpi});
        outboundDeleteChildPayloadsReady(childSmCbOne, outDelPayloadOne, true /*isResp*/, childOne);
        mLooper.dispatchAll();

        // Verify that no response is sent
        verifyEncryptAndEncodeNeverCalled(mSpyCurrentIkeSaRecord);

        // childTwo outbound payload list ready
        IkeDeletePayload outDelPayloadTwo = new IkeDeletePayload(new int[] {childTwoLocalSpi});
        outboundDeleteChildPayloadsReady(childSmCbTwo, outDelPayloadTwo, true /*isResp*/, childTwo);
        mLooper.dispatchAll();

        // Verify outbound response
        List<IkePayload> payloadList = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(2, payloadList.size());
        assertEquals(outDelPayloadOne, ((IkeDeletePayload) payloadList.get(0)));
        assertEquals(outDelPayloadTwo, ((IkeDeletePayload) payloadList.get(1)));
    }

    @Test
    public void testRemoteDeleteMultipleChildSaInSameSession() throws Exception {
        int newChildRemoteSpi = 21;
        int newChildLocalSpi = 22;

        setupIdleStateMachine();
        mDummyChildSmCallback.onChildSaCreated(newChildRemoteSpi, mMockChildSessionStateMachine);

        transitionToChildProcedureOngoing();

        // Receive Delete Child Request
        IkeDeletePayload[] inboundDelPayloads =
                new IkeDeletePayload[] {
                    new IkeDeletePayload(new int[] {CHILD_SPI_REMOTE}),
                    new IkeDeletePayload(new int[] {newChildRemoteSpi})
                };
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeDeleteChildPacket(inboundDelPayloads, false /*isResp*/));
        mLooper.dispatchAll();

        // Verify received payloads
        verifyChildReceiveDeleteRequest(mMockChildSessionStateMachine, inboundDelPayloads);

        // child outbound payload list ready
        IkeDeletePayload outDelPayload =
                new IkeDeletePayload(new int[] {CHILD_SPI_LOCAL, newChildLocalSpi});
        outboundDeleteChildPayloadsReady(
                mDummyChildSmCallback,
                outDelPayload,
                true /*isResp*/,
                mMockChildSessionStateMachine);
        mLooper.dispatchAll();

        // Verify outbound response
        List<IkePayload> payloadList = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(1, payloadList.size());
        assertEquals(outDelPayload, ((IkeDeletePayload) payloadList.get(0)));
    }

    @Test
    public void testIgnoreUnrecognizedChildSpi() throws Exception {
        int unrecognizedSpi = 2;

        setupIdleStateMachine();
        transitionToChildProcedureOngoing();

        // Receive Delete Child Request
        IkeDeletePayload[] inboundDelPayloads =
                new IkeDeletePayload[] {
                    new IkeDeletePayload(new int[] {unrecognizedSpi, CHILD_SPI_REMOTE})
                };
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeDeleteChildPacket(inboundDelPayloads, false /*isResp*/));
        mLooper.dispatchAll();

        // Verify received payloads
        verifyChildReceiveDeleteRequest(mMockChildSessionStateMachine, inboundDelPayloads);

        // child outbound payload list ready
        IkeDeletePayload outPayload = new IkeDeletePayload(new int[] {CHILD_SPI_LOCAL});
        outboundDeleteChildPayloadsReady(
                mDummyChildSmCallback, outPayload, true /*isResp*/, mMockChildSessionStateMachine);
        mLooper.dispatchAll();

        // Verify outbound response
        List<IkePayload> payloadList = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(1, payloadList.size());
        assertEquals(outPayload, ((IkeDeletePayload) payloadList.get(0)));
    }

    @Test
    public void testRemoteDeleteChildHandlesReqWithNoRecognizedSpi() throws Exception {
        int unrecognizedSpi = 2;

        setupIdleStateMachine();

        // Receive Delete Child Request without any recognized SPI
        IkeDeletePayload[] inboundDelPayloads =
                new IkeDeletePayload[] {new IkeDeletePayload(new int[] {unrecognizedSpi})};
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeDeleteChildPacket(inboundDelPayloads, false /*isResp*/));
        mLooper.dispatchAll();

        // Verify outbound empty response was sent
        List<IkePayload> payloadList = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertTrue(payloadList.isEmpty());

        // Verify IKE Session was back to Idle
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Test
    public void testRemoteCreateChild() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                CMD_RECEIVE_IKE_PACKET, makeCreateChildCreateMessage(false /*isResp*/));

        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);

        List<IkePayload> ikePayloadList = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(1, ikePayloadList.size());
        assertEquals(
                ERROR_TYPE_NO_ADDITIONAL_SAS,
                ((IkeNotifyPayload) ikePayloadList.get(0)).notifyType);
    }

    @Test
    public void testTriggerRemoteRekeyChild() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                CMD_RECEIVE_IKE_PACKET,
                makeRekeyChildCreateMessage(false /*isResp*/, CHILD_SPI_REMOTE));
        mLooper.dispatchAll();

        verify(mMockChildSessionStateMachine)
                .receiveRequest(
                        eq(IKE_EXCHANGE_SUBTYPE_REKEY_CHILD),
                        eq(EXCHANGE_TYPE_CREATE_CHILD_SA),
                        any(List.class));
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);
    }

    @Test
    public void testHandleRekeyChildReqWithUnrecognizedSpi() throws Exception {
        int unrecognizedSpi = 2;

        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                CMD_RECEIVE_IKE_PACKET,
                makeRekeyChildCreateMessage(false /*isResp*/, unrecognizedSpi));
        mLooper.dispatchAll();

        verify(mMockChildSessionStateMachine, never()).receiveRequest(anyInt(), anyInt(), any());
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);

        List<IkePayload> ikePayloadList = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(1, ikePayloadList.size());
        IkeNotifyPayload notifyPayload = (IkeNotifyPayload) ikePayloadList.get(0);
        assertEquals(ERROR_TYPE_CHILD_SA_NOT_FOUND, notifyPayload.notifyType);
        assertEquals(unrecognizedSpi, notifyPayload.spi);
    }

    private void verifyNotifyUserCloseSession() {
        verify(mSpyUserCbExecutor).execute(any(Runnable.class));
        verify(mMockIkeSessionCallback).onClosed();
    }

    @Test
    public void testRcvRemoteDeleteIkeWhenChildProcedureOngoing() throws Exception {
        setupIdleStateMachine();
        transitionToChildProcedureOngoing();

        mIkeSessionStateMachine.sendMessage(
                CMD_RECEIVE_IKE_PACKET, makeDeleteIkeRequest(mSpyCurrentIkeSaRecord));

        mLooper.dispatchAll();

        verifyNotifyUserCloseSession();

        // Verify state machine quit properly
        assertNull(mIkeSessionStateMachine.getCurrentState());

        List<IkePayload> ikePayloadList = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertTrue(ikePayloadList.isEmpty());
    }

    @Test
    public void testRcvRemoteRekeyIkeWhenChildProcedureOngoing() throws Exception {
        setupIdleStateMachine();
        transitionToChildProcedureOngoing();

        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, makeRekeyIkeRequest());

        mLooper.dispatchAll();

        // Since we have forced state machine to transition to ChildProcedureOngoing state without
        // really starting any Child procedure, it should transition to Idle at this time.
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);

        List<IkePayload> ikePayloadList = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(1, ikePayloadList.size());
        assertEquals(
                ERROR_TYPE_TEMPORARY_FAILURE,
                ((IkeNotifyPayload) ikePayloadList.get(0)).notifyType);
    }

    @Test
    public void testKillChildSessions() throws Exception {
        setupIdleStateMachine();

        ChildSessionStateMachine childOne = mock(ChildSessionStateMachine.class);
        ChildSessionStateMachine childTwo = mock(ChildSessionStateMachine.class);
        registerChildStateMachine(mock(ChildSessionCallback.class), childOne);
        registerChildStateMachine(mock(ChildSessionCallback.class), childTwo);

        mIkeSessionStateMachine.mCurrentIkeSaRecord = null;

        mIkeSessionStateMachine.quitNow();

        mLooper.dispatchAll();

        verify(childOne).killSession();
        verify(childTwo).killSession();
    }

    private IkeMessage verifyAuthReqAndGetMsg() {
        IkeMessage ikeAuthReqMessage = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);

        IkeHeader ikeHeader = ikeAuthReqMessage.ikeHeader;
        assertEquals(IkeHeader.EXCHANGE_TYPE_IKE_AUTH, ikeHeader.exchangeType);
        assertFalse(ikeHeader.isResponseMsg);
        assertTrue(ikeHeader.fromIkeInitiator);

        return ikeAuthReqMessage;
    }

    private IkeMessage verifyAuthReqWithChildPayloadsAndGetMsg() {
        IkeMessage ikeAuthReqMessage = verifyAuthReqAndGetMsg();

        assertNotNull(
                ikeAuthReqMessage.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_ID_INITIATOR, IkeIdPayload.class));
        assertNotNull(
                ikeAuthReqMessage.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_ID_RESPONDER, IkeIdPayload.class));
        assertNotNull(
                ikeAuthReqMessage.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_SA, IkeSaPayload.class));
        assertNotNull(
                ikeAuthReqMessage.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_TS_INITIATOR, IkeTsPayload.class));
        assertNotNull(
                ikeAuthReqMessage.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_TS_RESPONDER, IkeTsPayload.class));

        IkeConfigPayload configPayload =
                ikeAuthReqMessage.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_CP, IkeConfigPayload.class);
        assertNotNull(configPayload);

        Map<Integer, Integer> expectedAttributes =
                Map.of(
                        CONFIG_ATTR_IP4_PCSCF,
                        1,
                        CONFIG_ATTR_IP6_PCSCF,
                        1,
                        CONFIG_ATTR_INTERNAL_IP4_ADDRESS,
                        1,
                        CONFIG_ATTR_INTERNAL_IP6_ADDRESS,
                        1,
                        CONFIG_ATTR_APPLICATION_VERSION,
                        1,
                        CONFIG_ATTR_INTERNAL_IP4_NETMASK,
                        1);
        Map<Integer, Integer> actualAttributes =
                buildAttributeMap(configPayload.recognizedAttributeList);
        assertEquals(expectedAttributes, actualAttributes);

        IkeNoncePayload noncePayload =
                ikeAuthReqMessage.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_NONCE, IkeNoncePayload.class);
        assertNull(noncePayload);

        return ikeAuthReqMessage;
    }

    private Map<Integer, Integer> buildAttributeMap(
            List<IkeConfigPayload.ConfigAttribute> recognizedAttributeList) {
        Map<Integer, Integer> attrCountMap = new HashMap<>();
        for (IkeConfigPayload.ConfigAttribute attr : recognizedAttributeList) {
            attrCountMap.compute(attr.attributeType, (key, val) -> (val == null) ? 1 : val + 1);
        }

        return attrCountMap;
    }

    private void verifyDigitalSignatureAuthentication(
            IkeAuthDigitalSignPayload spyAuthPayload,
            X509Certificate certificate,
            IkeIdPayload respIdPayload,
            List<IkePayload> authRelatedPayloads,
            boolean hasChildPayloads,
            boolean hasConfigPayloadInResp)
            throws Exception {
        IkeMessage ikeAuthReqMessage =
                verifyAuthenticationCommonAndGetIkeMessage(
                        respIdPayload,
                        authRelatedPayloads,
                        hasChildPayloads,
                        hasConfigPayloadInResp);

        verify(spyAuthPayload)
                .verifyInboundSignature(
                        certificate,
                        mIkeSessionStateMachine.mIkeInitRequestBytes,
                        mSpyCurrentIkeSaRecord.nonceInitiator,
                        respIdPayload.getEncodedPayloadBody(),
                        mIkeSessionStateMachine.mIkePrf,
                        mSpyCurrentIkeSaRecord.getSkPr());

        assertNotNull(
                ikeAuthReqMessage.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_AUTH, IkeAuthDigitalSignPayload.class));
    }

    private void verifySharedKeyAuthentication(
            IkeAuthPskPayload spyAuthPayload,
            IkeIdPayload respIdPayload,
            List<IkePayload> authRelatedPayloads,
            boolean hasChildPayloads,
            boolean hasConfigPayloadInResp)
            throws Exception {
        IkeMessage ikeAuthReqMessage =
                verifyAuthenticationCommonAndGetIkeMessage(
                        respIdPayload,
                        authRelatedPayloads,
                        hasChildPayloads,
                        hasConfigPayloadInResp);

        // Validate authentication is done. Cannot use matchers because IkeAuthPskPayload is final.
        verify(spyAuthPayload)
                .verifyInboundSignature(
                        mPsk,
                        mIkeSessionStateMachine.mIkeInitRequestBytes,
                        mSpyCurrentIkeSaRecord.nonceInitiator,
                        respIdPayload.getEncodedPayloadBody(),
                        mIkeSessionStateMachine.mIkePrf,
                        mSpyCurrentIkeSaRecord.getSkPr());

        assertNotNull(
                ikeAuthReqMessage.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_AUTH, IkeAuthPskPayload.class));
    }

    private IkeMessage verifyAuthenticationCommonAndGetIkeMessage(
            IkeIdPayload respIdPayload,
            List<IkePayload> authRelatedPayloads,
            boolean hasChildPayloads,
            boolean hasConfigPayloadInResp)
            throws Exception {
        // Send IKE AUTH response to IKE state machine
        ReceivedIkePacket authResp = makeIkeAuthRespWithChildPayloads(authRelatedPayloads);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, authResp);
        mLooper.dispatchAll();

        // Validate outbound IKE AUTH request
        IkeMessage ikeAuthReqMessage;
        if (hasChildPayloads) {
            ikeAuthReqMessage = verifyAuthReqWithChildPayloadsAndGetMsg();
        } else {
            ikeAuthReqMessage = verifyAuthReqAndGetMsg();
        }

        // Validate that there is no EAP only notify payload
        List<IkeNotifyPayload> notifyPayloads =
                ikeAuthReqMessage.getPayloadListForType(
                        IkePayload.PAYLOAD_TYPE_NOTIFY, IkeNotifyPayload.class);
        assertFalse(hasEapOnlyNotifyPayload(notifyPayloads));

        // Validate inbound IKE AUTH response
        verifyIncrementLocaReqMsgId();
        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, authResp);

        // Validate that user has been notified
        verify(mSpyUserCbExecutor).execute(any(Runnable.class));

        // Verify IkeSessionConfiguration
        ArgumentCaptor<IkeSessionConfiguration> ikeSessionConfigurationArgumentCaptor =
                ArgumentCaptor.forClass(IkeSessionConfiguration.class);
        verify(mMockIkeSessionCallback).onOpened(ikeSessionConfigurationArgumentCaptor.capture());

        IkeSessionConfiguration sessionConfig = ikeSessionConfigurationArgumentCaptor.getValue();
        assertNotNull(sessionConfig);
        if (hasConfigPayloadInResp) {
            List<InetAddress> pcscfAddressList = sessionConfig.getPcscfServers();
            assertEquals(3, pcscfAddressList.size());
            assertTrue(pcscfAddressList.contains(InetAddress.getByName(PCSCF_IPV6_ADDRESS1)));
            assertTrue(pcscfAddressList.contains(InetAddress.getByName(PCSCF_IPV6_ADDRESS2)));
            assertTrue(pcscfAddressList.contains(InetAddress.getByName(PCSCF_IPV6_ADDRESS3)));
        } else {
            assertTrue(sessionConfig.getPcscfServers().size() == 0);
        }

        assertEquals(
                "" /*expected application version*/, sessionConfig.getRemoteApplicationVersion());
        assertByteArrayListEquals(
                Arrays.asList(REMOTE_VENDOR_ID_ONE, REMOTE_VENDOR_ID_TWO),
                sessionConfig.getRemoteVendorIds());
        assertTrue(
                sessionConfig.isIkeExtensionEnabled(
                        IkeSessionConfiguration.EXTENSION_TYPE_FRAGMENTATION));

        IkeSessionConnectionInfo ikeConnInfo = sessionConfig.getIkeSessionConnectionInfo();
        assertEquals(LOCAL_ADDRESS, ikeConnInfo.getLocalAddress());
        assertEquals(REMOTE_ADDRESS, ikeConnInfo.getRemoteAddress());
        assertEquals(mMockDefaultNetwork, ikeConnInfo.getNetwork());

        // Verify payload list pair for first Child negotiation
        ArgumentCaptor<List<IkePayload>> mReqPayloadListCaptor =
                ArgumentCaptor.forClass(List.class);
        ArgumentCaptor<List<IkePayload>> mRespPayloadListCaptor =
                ArgumentCaptor.forClass(List.class);
        verify(mMockChildSessionStateMachine)
                .handleFirstChildExchange(
                        mReqPayloadListCaptor.capture(),
                        mRespPayloadListCaptor.capture(),
                        eq(LOCAL_ADDRESS),
                        eq(REMOTE_ADDRESS),
                        any(), // udpEncapSocket
                        eq(mIkeSessionStateMachine.mIkePrf),
                        any()); // sk_d
        List<IkePayload> childReqList = mReqPayloadListCaptor.getValue();
        List<IkePayload> childRespList = mRespPayloadListCaptor.getValue();

        assertTrue(isIkePayloadExist(childReqList, IkePayload.PAYLOAD_TYPE_SA));
        assertTrue(isIkePayloadExist(childReqList, IkePayload.PAYLOAD_TYPE_TS_INITIATOR));
        assertTrue(isIkePayloadExist(childReqList, IkePayload.PAYLOAD_TYPE_TS_RESPONDER));
        assertTrue(isIkePayloadExist(childReqList, IkePayload.PAYLOAD_TYPE_NONCE));
        IkeSaPayload reqSaPayload =
                IkePayload.getPayloadForTypeInProvidedList(
                        IkePayload.PAYLOAD_TYPE_SA, IkeSaPayload.class, childReqList);
        assertFalse(reqSaPayload.isSaResponse);

        assertTrue(isIkePayloadExist(childRespList, IkePayload.PAYLOAD_TYPE_SA));
        assertTrue(isIkePayloadExist(childRespList, IkePayload.PAYLOAD_TYPE_TS_INITIATOR));
        assertTrue(isIkePayloadExist(childRespList, IkePayload.PAYLOAD_TYPE_TS_RESPONDER));
        assertTrue(isIkePayloadExist(childRespList, IkePayload.PAYLOAD_TYPE_NONCE));
        if (hasConfigPayloadInResp) {
            assertTrue(isIkePayloadExist(childRespList, IkePayload.PAYLOAD_TYPE_CP));
        } else {
            assertFalse(isIkePayloadExist(childRespList, IkePayload.PAYLOAD_TYPE_CP));
        }
        IkeSaPayload respSaPayload =
                IkePayload.getPayloadForTypeInProvidedList(
                        IkePayload.PAYLOAD_TYPE_SA, IkeSaPayload.class, childRespList);
        assertTrue(respSaPayload.isSaResponse);

        // Mock finishing first Child SA negotiation.
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);

        verify(mMockChildSessionFactoryHelper)
                .makeChildSessionStateMachine(
                        eq(mLooper.getLooper()),
                        eq(mSpyContext),
                        anyInt(),
                        any(AlarmManager.class),
                        any(RandomnessFactory.class),
                        any(IpSecSpiGenerator.class),
                        eq(mChildSessionParams),
                        eq(mSpyUserCbExecutor),
                        eq(mMockChildSessionCallback),
                        mChildSessionSmCbCaptor.capture());
        IChildSessionSmCallback cb = mChildSessionSmCbCaptor.getValue();

        cb.onProcedureFinished(mMockChildSessionStateMachine);
        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);

        return ikeAuthReqMessage;
    }

    private IkeAuthPskPayload makeSpyRespPskPayload() throws Exception {
        IkeAuthPskPayload spyAuthPayload =
                spy(
                        (IkeAuthPskPayload)
                                IkeTestUtils.hexStringToIkePayload(
                                        IkePayload.PAYLOAD_TYPE_AUTH,
                                        true /*isResp*/,
                                        PSK_AUTH_RESP_PAYLOAD_HEX_STRING));

        doNothing()
                .when(spyAuthPayload)
                .verifyInboundSignature(any(), any(), any(), any(), any(), any());
        return spyAuthPayload;
    }

    private IkeAuthDigitalSignPayload makeSpyDigitalSignAuthPayload() throws Exception {
        IkeAuthDigitalSignPayload spyAuthPayload =
                spy(
                        (IkeAuthDigitalSignPayload)
                                IkeTestUtils.hexStringToIkePayload(
                                        IkePayload.PAYLOAD_TYPE_AUTH,
                                        true /*isResp*/,
                                        GENERIC_DIGITAL_SIGN_AUTH_RESP_HEX_STRING));
        doNothing()
                .when(spyAuthPayload)
                .verifyInboundSignature(any(), any(), any(), any(), any(), any());
        return spyAuthPayload;
    }

    private IkeIdPayload makeRespIdPayload() {
        return makeRespIdPayload(REMOTE_ID_FQDN);
    }

    private IkeIdPayload makeRespIdPayload(IkeIdentification ikeId) {
        return new IkeIdPayload(false /* isInitiator */, ikeId);
    }

    private void verifyEmptyInformationalSent(int count, boolean expectedResp) {
        verify(mMockIkeMessageHelper, times(count))
                .encryptAndEncode(
                        anyObject(),
                        anyObject(),
                        eq(mSpyCurrentIkeSaRecord),
                        argThat(
                                msg -> {
                                    return msg.ikePayloadList.isEmpty()
                                            && msg.ikeHeader.isResponseMsg == expectedResp
                                            && msg.ikeHeader.fromIkeInitiator
                                            && msg.ikeHeader.exchangeType
                                                    == IkeHeader.EXCHANGE_TYPE_INFORMATIONAL;
                                }),
                        anyBoolean(),
                        anyInt());
    }

    @Test
    public void testCreateIkeLocalIkeAuthDefersOtherMessages() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();

        // Build IKE AUTH response with Auth-PSK, ID-Responder and config payloads.
        List<IkePayload> authRelatedPayloads = new LinkedList<>();
        IkeAuthPskPayload spyAuthPayload = makeSpyRespPskPayload();
        authRelatedPayloads.add(spyAuthPayload);

        ReceivedIkePacket req =
                makeDummyEncryptedReceivedIkePacket(
                        mSpyCurrentIkeSaRecord,
                        IkeHeader.EXCHANGE_TYPE_INFORMATIONAL,
                        false,
                        Collections.emptyList(),
                        Collections.emptyList());
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, req);

        verifyEmptyInformationalSent(0, true /* expcetedResp*/);

        // Send IKE AUTH response to IKE state machine to trigger moving to next state
        IkeIdPayload respIdPayload = makeRespIdPayload();
        authRelatedPayloads.add(respIdPayload);
        authRelatedPayloads.add(makeConfigPayload());

        ReceivedIkePacket authResp = makeIkeAuthRespWithChildPayloads(authRelatedPayloads);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, authResp);
        mLooper.dispatchAll();

        verifyEmptyInformationalSent(1, true /* expcetedResp*/);
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);
    }

    @Test
    public void testCreateIkeLocalIkeAuthDigitalSignature() throws Exception {
        // Quit and restart IKE Session with Digital Signature Auth params
        mIkeSessionStateMachine.quitNow();
        reset(mMockChildSessionFactoryHelper);
        setupChildStateMachineFactory(mMockChildSessionStateMachine);
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsDigitalSignature());
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);

        // Build IKE AUTH response with Digital Signature Auth, ID-Responder and config payloads.
        List<IkePayload> authRelatedPayloads = new LinkedList<>();
        IkeAuthDigitalSignPayload spyAuthPayload = makeSpyDigitalSignAuthPayload();
        authRelatedPayloads.add(spyAuthPayload);
        authRelatedPayloads.add(new IkeCertX509CertPayload(mServerEndCertificate));

        IkeIdPayload respIdPayload = makeRespIdPayload();
        authRelatedPayloads.add(respIdPayload);
        authRelatedPayloads.add(makeConfigPayload());

        verifyDigitalSignatureAuthentication(
                spyAuthPayload,
                mServerEndCertificate,
                respIdPayload,
                authRelatedPayloads,
                true /*hasChildPayloads*/,
                true /*hasConfigPayloadInResp*/);
        verifyRetransmissionStopped();
    }

    @Test
    public void testCreateIkeLocalIkeAuthPsk() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();

        // Build IKE AUTH response with Auth-PSK, ID-Responder and config payloads.
        List<IkePayload> authRelatedPayloads = new LinkedList<>();
        IkeAuthPskPayload spyAuthPayload = makeSpyRespPskPayload();
        authRelatedPayloads.add(spyAuthPayload);

        IkeIdPayload respIdPayload = makeRespIdPayload();
        authRelatedPayloads.add(respIdPayload);
        authRelatedPayloads.add(makeConfigPayload());

        verifySharedKeyAuthentication(
                spyAuthPayload,
                respIdPayload,
                authRelatedPayloads,
                true /*hasChildPayloads*/,
                true /*hasConfigPayloadInResp*/);
        verifyRetransmissionStopped();
    }

    @Test
    public void testCreateIkeLocalIkeAuthDigitalSignatureIdMismatch() throws Exception {
        // Quit and restart IKE Session with Digital Signature Auth params
        mIkeSessionStateMachine.quitNow();
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsDigitalSignature());
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        resetMockIkeMessageHelper();

        // Build IKE AUTH response with Digital Signature Auth, ID-Responder and config payloads.
        List<IkePayload> authRelatedPayloads = new LinkedList<>();
        IkeAuthDigitalSignPayload spyAuthPayload = makeSpyDigitalSignAuthPayload();
        authRelatedPayloads.add(spyAuthPayload);
        authRelatedPayloads.add(new IkeCertX509CertPayload(mServerEndCertificate));

        authRelatedPayloads.add(makeRespIdPayload(REMOTE_ID_IPV4));

        sendAuthFailRespAndVerifyCloseIke(makeIkeAuthRespWithChildPayloads(authRelatedPayloads));
    }

    @Test
    public void testCreateIkeLocalIkeAuthPskVerifyFail() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();
        resetMockIkeMessageHelper();

        // Build IKE AUTH response with invalid Auth-PSK Payload and ID-Responder Payload.
        List<IkePayload> authRelatedPayloads = new LinkedList<>();
        IkeAuthPskPayload spyAuthPayload = makeSpyRespPskPayload();
        doThrow(new AuthenticationFailedException("DummyAuthFailException"))
                .when(spyAuthPayload)
                .verifyInboundSignature(any(), any(), any(), any(), any(), any());
        authRelatedPayloads.add(spyAuthPayload);

        IkeIdPayload respIdPayload = makeRespIdPayload();
        authRelatedPayloads.add(respIdPayload);

        sendAuthFailRespAndVerifyCloseIke(makeIkeAuthRespWithChildPayloads(authRelatedPayloads));
    }

    private void sendAuthFailRespAndVerifyCloseIke(ReceivedIkePacket authFailResp)
            throws Exception {
        // Send response to IKE state machine
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, authFailResp);
        mLooper.dispatchAll();

        // Verify Delete request was sent
        List<IkePayload> payloads = verifyOutInfoMsgHeaderAndGetPayloads(false /*isResp*/);
        assertEquals(1, payloads.size());
        assertEquals(IkePayload.PAYLOAD_TYPE_DELETE, payloads.get(0).payloadType);

        // Verify IKE Session was closed properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback)
                .onClosedExceptionally(any(AuthenticationFailedException.class));
    }

    @Test
    public void testAuthHandlesIkeErrorNotify() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();
        resetMockIkeMessageHelper();

        // Mock rejecting IKE AUTH with Authenticatio Failure notification
        ReceivedIkePacket mockAuthFailPacket =
                makeIkeAuthRespWithoutChildPayloads(
                        Arrays.asList(new IkeNotifyPayload(ERROR_TYPE_AUTHENTICATION_FAILED)));
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, mockAuthFailPacket);
        mLooper.dispatchAll();

        // Verify IKE Session is closed properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback)
                .onClosedExceptionally(any(AuthenticationFailedException.class));
    }

    @Test
    public void testAuthHandlesCreateChildErrorNotify() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();
        resetMockIkeMessageHelper();

        // Mock rejecting IKE AUTH with a Create Child error notification
        ReceivedIkePacket mockAuthFailPacket =
                makeIkeAuthRespWithoutChildPayloads(
                        Arrays.asList(new IkeNotifyPayload(ERROR_TYPE_INTERNAL_ADDRESS_FAILURE)));
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, mockAuthFailPacket);
        mLooper.dispatchAll();

        // Verify IKE Session is closed properly
        assertNull(mIkeSessionStateMachine.getCurrentState());

        ArgumentCaptor<IkeProtocolException> captor =
                ArgumentCaptor.forClass(IkeProtocolException.class);
        verify(mMockIkeSessionCallback).onClosedExceptionally(captor.capture());
        IkeProtocolException exception = captor.getValue();
        assertEquals(ERROR_TYPE_INTERNAL_ADDRESS_FAILURE, exception.getErrorType());
    }

    @Test
    public void testAuthPskHandleRespWithParsingError() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();
        resetMockIkeMessageHelper();

        // Mock receiving packet with syntax error
        ReceivedIkePacket mockInvalidPacket =
                makeDummyReceivedIkePacketWithInvalidSyntax(
                        mSpyCurrentIkeSaRecord, true /*isResp*/, IkeHeader.EXCHANGE_TYPE_IKE_AUTH);
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, mockInvalidPacket);
        mLooper.dispatchAll();

        // Verify Delete request was sent
        List<IkePayload> payloads = verifyOutInfoMsgHeaderAndGetPayloads(false /*isResp*/);
        assertEquals(1, payloads.size());
        assertEquals(IkePayload.PAYLOAD_TYPE_DELETE, payloads.get(0).payloadType);

        // Verify IKE Session is closed properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(InvalidSyntaxException.class));
    }

    @Test
    public void testAuthWithOptionAcceptAnyRemoteId() throws Exception {
        mIkeSessionStateMachine.quitNow();
        reset(mMockChildSessionFactoryHelper);
        setupChildStateMachineFactory(mMockChildSessionStateMachine);

        IkeSessionParams ikeSessionParams =
                buildIkeSessionParamsCommon()
                        .setAuthPsk(mPsk)
                        .addIkeOption(IkeSessionParams.IKE_OPTION_ACCEPT_ANY_REMOTE_ID)
                        .build();
        mIkeSessionStateMachine = makeAndStartIkeSession(ikeSessionParams);

        // Mock IKE INIT
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();

        // Build IKE AUTH response with Auth-PSK Payload and ID-Responder Payload that is different
        // from configured ID-Responder.
        List<IkePayload> authRelatedPayloads = new LinkedList<>();
        IkeAuthPskPayload spyAuthPayload = makeSpyRespPskPayload();
        authRelatedPayloads.add(spyAuthPayload);

        IkeIdPayload respIdPayload = makeRespIdPayload(REMOTE_ID_IPV4);
        authRelatedPayloads.add(respIdPayload);

        // Send response to IKE state machine and verify authentication is done.
        verifySharedKeyAuthentication(
                spyAuthPayload,
                respIdPayload,
                authRelatedPayloads,
                true /*hasChildPayloads*/,
                false /*hasConfigPayloadInResp*/);
        verifyRetransmissionStopped();
    }

    @Test
    public void testAuthRejectOtherResponderId() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();

        // Build IKE AUTH response with Auth-PSK Payload and ID-Responder Payload that is different
        // from configured ID-Responder.
        List<IkePayload> authRelatedPayloads = new LinkedList<>();
        IkeAuthPskPayload spyAuthPayload = makeSpyRespPskPayload();
        authRelatedPayloads.add(spyAuthPayload);

        IkeIdPayload respIdPayload = makeRespIdPayload(REMOTE_ID_IPV4);
        authRelatedPayloads.add(respIdPayload);

        // Send response to IKE state machine
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeIkeAuthRespWithChildPayloads(authRelatedPayloads));
        mLooper.dispatchAll();

        // Verify IKE Session is closed properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback)
                .onClosedExceptionally(any(AuthenticationFailedException.class));
    }

    @Test
    public void testCreateIkeLocalIkeAuthPreEap() throws Exception {
        mIkeSessionStateMachine.quitNow();
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsEap());

        // Mock IKE INIT
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();

        // Build IKE AUTH response with EAP. Auth, ID-Resp and Cert payloads.
        List<IkePayload> authRelatedPayloads = new LinkedList<>();

        authRelatedPayloads.add(new IkeEapPayload(EAP_DUMMY_MSG));
        authRelatedPayloads.add(makeSpyDigitalSignAuthPayload());
        authRelatedPayloads.add(makeRespIdPayload());

        IkeCertX509CertPayload certPayload = new IkeCertX509CertPayload(mServerEndCertificate);
        authRelatedPayloads.add(certPayload);

        // Send IKE AUTH response to IKE state machine
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeIkeAuthRespWithoutChildPayloads(authRelatedPayloads));
        mLooper.dispatchAll();

        // Validate outbound IKE AUTH request
        IkeMessage ikeAuthReqMessage = verifyAuthReqWithChildPayloadsAndGetMsg();
        assertNull(
                ikeAuthReqMessage.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_AUTH, IkeAuthPayload.class));

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.CreateIkeLocalIkeAuthInEap);
        verifyRetransmissionStopped();
        assertNotNull(mIkeSessionStateMachine.mInitIdPayload);
        assertNotNull(mIkeSessionStateMachine.mRespIdPayload);
    }

    private IEapCallback verifyEapAuthenticatorCreatedAndGetCallback() {
        ArgumentCaptor<IEapCallback> captor = ArgumentCaptor.forClass(IEapCallback.class);

        verify(mMockEapAuthenticatorFactory)
                .newEapAuthenticator(
                        eq(mIkeSessionStateMachine.getHandler().getLooper()),
                        captor.capture(),
                        eq(mSpyContext),
                        eq(mEapSessionConfig),
                        any(RandomnessFactory.class));

        return captor.getValue();
    }

    @Test
    public void testCreateIkeLocalIkeAuthInEapStartsAuthenticatorAndProxiesMessage()
            throws Exception {
        mIkeSessionStateMachine.quitNow();
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsEap());

        // Setup state and go to IN_EAP state
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuthInEap);
        mLooper.dispatchAll();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EAP_START_EAP_AUTH, new IkeEapPayload(EAP_DUMMY_MSG));
        mLooper.dispatchAll();

        verifyEapAuthenticatorCreatedAndGetCallback();

        verify(mMockEapAuthenticator).processEapMessage(eq(EAP_DUMMY_MSG));
    }

    @Test
    public void testCreateIkeLocalIkeAuthInEapHandlesOutboundResponse() throws Exception {
        mIkeSessionStateMachine.quitNow();
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsEap());

        // Setup state and go to IN_EAP state
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuthInEap);
        mLooper.dispatchAll();

        IEapCallback callback = verifyEapAuthenticatorCreatedAndGetCallback();
        callback.onResponse(EAP_DUMMY_MSG);
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        // Verify EAP response
        IkeMessage resp = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);
        IkeHeader ikeHeader = resp.ikeHeader;
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, ikeHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_IKE_AUTH, ikeHeader.exchangeType);
        assertFalse(ikeHeader.isResponseMsg);
        assertEquals(mSpyCurrentIkeSaRecord.isLocalInit, ikeHeader.fromIkeInitiator);

        assertEquals(1, resp.ikePayloadList.size());
        assertArrayEquals(EAP_DUMMY_MSG, ((IkeEapPayload) resp.ikePayloadList.get(0)).eapMessage);
    }

    @Test
    public void testCreateIkeLocalIkeAuthInEapHandlesMissingEapPacket() throws Exception {
        mIkeSessionStateMachine.quitNow();
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsEap());

        // Setup state and go to IN_EAP state
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuthInEap);
        mLooper.dispatchAll();

        // Mock sending IKE_AUTH{EAP} request
        IEapCallback callback = verifyEapAuthenticatorCreatedAndGetCallback();
        callback.onResponse(EAP_DUMMY_MSG);
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        // Send IKE AUTH response with no EAP Payload to IKE state machine
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeIkeAuthRespWithoutChildPayloads(new LinkedList<>()));
        mLooper.dispatchAll();

        // Verify state machine quit properly
        verify(mMockIkeSessionCallback)
                .onClosedExceptionally(any(AuthenticationFailedException.class));
        assertNull(mIkeSessionStateMachine.getCurrentState());
    }

    @Test
    public void testCreateIkeLocalIkeAuthInEapHandlesSuccess() throws Exception {
        mIkeSessionStateMachine.quitNow();
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsEap());

        // Setup state and go to IN_EAP state
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuthInEap);
        mLooper.dispatchAll();

        IEapCallback callback = verifyEapAuthenticatorCreatedAndGetCallback();

        // Setup dummy initIdPayload for next state.
        mIkeSessionStateMachine.mInitIdPayload = mock(IkeIdPayload.class);
        doReturn(new byte[0]).when(mIkeSessionStateMachine.mInitIdPayload).getEncodedPayloadBody();

        callback.onSuccess(mPsk, new byte[0]); // use mPsk as MSK, eMSK does not matter
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.CreateIkeLocalIkeAuthPostEap);
    }

    @Test
    public void testCreateIkeLocalIkeAuthInEapHandlesError() throws Exception {
        mIkeSessionStateMachine.quitNow();
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsEap());

        // Setup state and go to IN_EAP state
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuthInEap);
        mLooper.dispatchAll();

        IEapCallback callback = verifyEapAuthenticatorCreatedAndGetCallback();

        Throwable error = new IllegalArgumentException();
        callback.onError(error);
        mLooper.dispatchAll();

        // Fires user error callbacks
        verify(mMockIkeSessionCallback)
                .onClosedExceptionally(argThat(err -> err.getCause() == error));

        // Verify state machine quit properly
        verify(mSpyCurrentIkeSaRecord).close();
        assertNull(mIkeSessionStateMachine.getCurrentState());
    }

    @Test
    public void testCreateIkeLocalIkeAuthInEapHandlesFailure() throws Exception {
        mIkeSessionStateMachine.quitNow();
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsEap());

        // Setup state and go to IN_EAP state
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuthInEap);
        mLooper.dispatchAll();

        IEapCallback callback = verifyEapAuthenticatorCreatedAndGetCallback();
        callback.onFail();
        mLooper.dispatchAll();

        // Fires user error callbacks
        verify(mMockIkeSessionCallback)
                .onClosedExceptionally(any(AuthenticationFailedException.class));

        // Verify state machine quit properly
        verify(mSpyCurrentIkeSaRecord).close();
        assertNull(mIkeSessionStateMachine.getCurrentState());
    }

    @Test
    public void testCreateIkeLocalIkeAuthPostEap() throws Exception {
        mIkeSessionStateMachine.quitNow();
        reset(mMockChildSessionFactoryHelper);
        setupChildStateMachineFactory(mMockChildSessionStateMachine);
        mIkeSessionStateMachine = makeAndStartIkeSession(buildIkeSessionParamsEap());

        // Setup dummy state from IkeAuthPreEap for next state.
        mIkeSessionStateMachine.mInitIdPayload = mock(IkeIdPayload.class);
        doReturn(new byte[0]).when(mIkeSessionStateMachine.mInitIdPayload).getEncodedPayloadBody();
        mIkeSessionStateMachine.mRespIdPayload = makeRespIdPayload();

        List<Integer> payloadTypeList = new LinkedList<>();
        List<String> payloadHexStringList = new LinkedList<>();

        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_SA);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_TS_INITIATOR);
        payloadTypeList.add(IkePayload.PAYLOAD_TYPE_TS_RESPONDER);

        payloadHexStringList.add(CHILD_SA_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(TS_INIT_PAYLOAD_HEX_STRING);
        payloadHexStringList.add(TS_RESP_PAYLOAD_HEX_STRING);

        mIkeSessionStateMachine.mFirstChildReqList =
                hexStrListToIkePayloadList(payloadTypeList, payloadHexStringList, false /*isResp*/);

        // Setup state and go to IN_EAP state
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuthPostEap);
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_EAP_FINISH_EAP_AUTH, mPsk);
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        // Build IKE AUTH response with Auth-PSK Payload and ID-Responder Payload.
        List<IkePayload> authRelatedPayloads = new LinkedList<>();
        IkeAuthPskPayload spyAuthPayload = makeSpyRespPskPayload();
        authRelatedPayloads.add(spyAuthPayload);

        IkeIdPayload respIdPayload = makeRespIdPayload();

        verifySharedKeyAuthentication(
                spyAuthPayload,
                respIdPayload,
                authRelatedPayloads,
                false /*hasChildPayloads*/,
                false /*hasConfigPayloadInResp*/);
        verifyRetransmissionStopped();
    }

    @Test
    public void testCreateIkeLocalIkeAuthHandlesFirstFrag() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();

        // Received IKE fragment
        byte[] unencryptedData = "testCreateIkeLocalIkeAuthHandlesFrag".getBytes();
        int fragNum = 1;
        int totalFragments = 2;
        IkeSkfPayload skfPayload =
                IkeTestUtils.makeDummySkfPayload(unencryptedData, fragNum, totalFragments);

        ReceivedIkePacket packet =
                makeDummyReceivedIkeFragmentPacket(
                        mSpyCurrentIkeSaRecord,
                        true /*isResp*/,
                        IkeHeader.EXCHANGE_TYPE_IKE_AUTH,
                        skfPayload,
                        PAYLOAD_TYPE_AUTH,
                        null /* collectedFrags*/);
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, packet);
        mLooper.dispatchAll();

        // Verify state doesn't change
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.CreateIkeLocalIkeAuth);

        // Verify the IkeSaRecord has stored the fragment.
        DecodeResultPartial resultPartial =
                mSpyCurrentIkeSaRecord.getCollectedFragments(true /*isResp*/);
        assertEquals(PAYLOAD_TYPE_AUTH, resultPartial.firstPayloadType);
        assertEquals(totalFragments, resultPartial.collectedFragsList.length);
        assertArrayEquals(unencryptedData, resultPartial.collectedFragsList[fragNum - 1]);
        assertFalse(resultPartial.isAllFragmentsReceived());

        assertNull(mSpyCurrentIkeSaRecord.getCollectedFragments(false /*isResp*/));
    }

    @Test
    public void testCreateIkeLocalIkeAuthHandlesLastFragOk() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();

        // Set previously collected IKE fragments
        DecodeResultPartial mockCollectedFrags = mock(DecodeResultPartial.class);
        mSpyCurrentIkeSaRecord.updateCollectedFragments(mockCollectedFrags, true /*isResp*/);

        // Build reassembled IKE AUTH response with Auth-PSK Payload and ID-Responder Payload.
        List<IkePayload> authRelatedPayloads = new LinkedList<>();
        IkeAuthPskPayload spyAuthPayload = makeSpyRespPskPayload();
        authRelatedPayloads.add(spyAuthPayload);

        IkeIdPayload respIdPayload = makeRespIdPayload();
        authRelatedPayloads.add(respIdPayload);

        List<IkePayload> authPayloadList =
                getIkeAuthPayloadListWithChildPayloads(authRelatedPayloads);

        // Receive last auth response and do IKE_AUTH
        ReceivedIkePacket packet =
                makeDummyReceivedLastIkeFragmentPacketOk(
                        mSpyCurrentIkeSaRecord,
                        true /*isResp*/,
                        IkeHeader.EXCHANGE_TYPE_IKE_AUTH,
                        mockCollectedFrags,
                        authPayloadList,
                        "FirstFrag".getBytes());
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, packet);
        mLooper.dispatchAll();

        // Verify IKE AUTH is done
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);

        // Verify collected response fragments are cleared.
        assertNull(mSpyCurrentIkeSaRecord.getCollectedFragments(true /*isResp*/));
        verify(mSpyCurrentIkeSaRecord).resetCollectedFragments(true /*isResp*/);
    }

    @Test
    public void testCreateIkeLocalIkeAuthHandlesLastFragError() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();
        resetMockIkeMessageHelper();

        // Set previously collected IKE fragments
        DecodeResultPartial mockCollectedFrags = mock(DecodeResultPartial.class);
        mSpyCurrentIkeSaRecord.updateCollectedFragments(mockCollectedFrags, true /*isResp*/);

        // Receive last auth response with syntax error
        ReceivedIkePacket packet =
                makeDummyReceivedLastIkeFragmentPacketError(
                        mSpyCurrentIkeSaRecord,
                        true /*isResp*/,
                        IkeHeader.EXCHANGE_TYPE_IKE_AUTH,
                        mockCollectedFrags,
                        new InvalidSyntaxException("IkeStateMachineTest"));
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, packet);
        mLooper.dispatchAll();

        // Verify Delete request is sent
        List<IkePayload> payloads = verifyOutInfoMsgHeaderAndGetPayloads(false /*isResp*/);
        assertEquals(1, payloads.size());
        assertEquals(IkePayload.PAYLOAD_TYPE_DELETE, payloads.get(0).payloadType);

        // Verify IKE Session is closed properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(InvalidSyntaxException.class));

        // Collected response fragments are cleared
        assertNull(mSpyCurrentIkeSaRecord.getCollectedFragments(true /*isResp*/));
        verify(mSpyCurrentIkeSaRecord).resetCollectedFragments(true /*isResp*/);
    }

    @Test
    public void testRekeyIkeLocalCreateSendsRequest() throws Exception {
        setupIdleStateMachine();

        // Send Rekey-Create request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeLocalCreate);
        verifyRetransmissionStarted();

        // Verify outbound message
        IkeMessage rekeyMsg = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);

        IkeHeader ikeHeader = rekeyMsg.ikeHeader;
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, ikeHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA, ikeHeader.exchangeType);
        assertEquals(mSpyCurrentIkeSaRecord.getLocalRequestMessageId(), ikeHeader.messageId);
        assertFalse(ikeHeader.isResponseMsg);
        assertTrue(ikeHeader.fromIkeInitiator);

        // Verify SA payload & proposals
        IkeSaPayload saPayload =
                rekeyMsg.getPayloadForType(IkePayload.PAYLOAD_TYPE_SA, IkeSaPayload.class);
        assertFalse(saPayload.isSaResponse);
        assertEquals(1, saPayload.proposalList.size());

        IkeSaPayload.IkeProposal proposal =
                (IkeSaPayload.IkeProposal) saPayload.proposalList.get(0);
        assertEquals(1, proposal.number); // Must be 1-indexed
        assertEquals(IkePayload.PROTOCOL_ID_IKE, proposal.protocolId);
        assertEquals(IkePayload.SPI_LEN_IKE, proposal.spiSize);
        assertEquals(mIkeSessionStateMachine.mSaProposal, proposal.saProposal);

        // Verify Nonce and KE payloads exist.
        assertNotNull(
                rekeyMsg.getPayloadForType(IkePayload.PAYLOAD_TYPE_NONCE, IkeNoncePayload.class));

        IkeKePayload kePayload =
                rekeyMsg.getPayloadForType(IkePayload.PAYLOAD_TYPE_KE, IkeKePayload.class);
        assertNotNull(kePayload);
        assertTrue(kePayload.isOutbound);
    }

    @Test
    public void testRekeyIkeLocalCreateHandlesResponse() throws Exception {
        setupIdleStateMachine();

        // Send Rekey-Create request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        // Prepare "rekeyed" SA
        setupRekeyedIkeSa(mSpyLocalInitIkeSaRecord);

        // Receive Rekey response
        ReceivedIkePacket dummyRekeyIkeRespReceivedPacket = makeRekeyIkeResponse();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyRekeyIkeRespReceivedPacket);
        mLooper.dispatchAll();
        verifyIncrementLocaReqMsgId();
        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyRekeyIkeRespReceivedPacket);

        // Verify in delete state, and new SA record was saved:
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeLocalDelete);
        verifyRetransmissionStarted();
        assertEquals(mSpyLocalInitIkeSaRecord, mIkeSessionStateMachine.mLocalInitNewIkeSaRecord);
        verify(mSpyCurrentIkeSocket)
                .registerIke(
                        eq(mSpyLocalInitIkeSaRecord.getLocalSpi()), eq(mIkeSessionStateMachine));
    }

    @Test
    public void testRekeyIkeLocalCreateHandleRespWithParsingError() throws Exception {
        setupIdleStateMachine();

        // Send Rekey-Create request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();
        verifyRetransmissionStarted();
        resetMockIkeMessageHelper();

        // Mock receiving packet with syntax error
        ReceivedIkePacket mockInvalidPacket =
                makeDummyReceivedIkePacketWithInvalidSyntax(
                        mSpyCurrentIkeSaRecord,
                        true /*isResp*/,
                        IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA);
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, mockInvalidPacket);
        mLooper.dispatchAll();

        // Verify Delete request was sent
        List<IkePayload> payloads = verifyOutInfoMsgHeaderAndGetPayloads(false /*isResp*/);
        assertEquals(1, payloads.size());
        assertEquals(IkePayload.PAYLOAD_TYPE_DELETE, payloads.get(0).payloadType);

        // Verify IKE Session is closed properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(InvalidSyntaxException.class));
    }

    @Test
    public void testRekeyIkeLocalCreateHandleRespWithNonFatalErrorNotify() throws Exception {
        setupIdleStateMachine();

        // Send Rekey-Create request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();

        // Mock receiving packet with NO_PROPOSAL_CHOSEN
        ReceivedIkePacket resp =
                makeResponseWithErrorNotify(new IkeNotifyPayload(ERROR_TYPE_NO_PROPOSAL_CHOSEN));
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, resp);
        mLooper.dispatchAll();

        // Verify IKE Session goes back to Idle and retry is scheduled
        verify(mSpyCurrentIkeSaRecord).rescheduleRekey(eq(RETRY_INTERVAL_MS));
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Test
    public void testRekeyIkeLocalCreateHandleRespWithFatalErrorNotify() throws Exception {
        setupIdleStateMachine();

        // Send Rekey-Create request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();
        resetMockIkeMessageHelper();

        // Mock receiving packet with NO_PROPOSAL_CHOSEN
        ReceivedIkePacket resp =
                makeResponseWithErrorNotify(new IkeNotifyPayload(ERROR_TYPE_INVALID_SYNTAX));
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, resp);
        mLooper.dispatchAll();

        // Verify no message was sent because a fatal error notification was received
        verifyEncryptAndEncodeNeverCalled(mSpyCurrentIkeSaRecord);

        // Verify IKE Session is closed properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(InvalidSyntaxException.class));
    }

    @Test
    public void testRekeyIkeLocalCreateSaCreationFail() throws Exception {
        // Throw error when building new IKE SA
        throwExceptionWhenMakeRekeyIkeSa(
                new GeneralSecurityException("testRekeyIkeLocalCreateSaCreationFail"));

        setupIdleStateMachine();

        // Send Rekey-Create request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();
        resetMockIkeMessageHelper();

        // Receive Rekey response
        ReceivedIkePacket dummyRekeyIkeRespReceivedPacket = makeRekeyIkeResponse();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyRekeyIkeRespReceivedPacket);
        mLooper.dispatchAll();

        // Verify Delete request was sent
        List<IkePayload> payloads = verifyOutInfoMsgHeaderAndGetPayloads(false /*isResp*/);
        assertEquals(1, payloads.size());
        assertEquals(IkePayload.PAYLOAD_TYPE_DELETE, payloads.get(0).payloadType);

        // Verify IKE Session is closed properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(IkeInternalException.class));
    }

    @Test
    public void testRekeyIkeLocalCreateHandleReqWithNonFatalError() throws Exception {
        setupIdleStateMachine();

        // Send Rekey-Create request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();
        verifyRetransmissionStarted();
        resetMockIkeMessageHelper();

        // Build protocol exception
        List<Integer> unsupportedPayloads = new LinkedList<>();
        unsupportedPayloads.add(PAYLOAD_TYPE_UNSUPPORTED);
        UnsupportedCriticalPayloadException exception =
                new UnsupportedCriticalPayloadException(unsupportedPayloads);

        // Mock receiving packet with unsupported critical payload
        ReceivedIkePacket mockInvalidPacket =
                makeDummyReceivedIkePacketWithDecodingError(
                        mSpyCurrentIkeSaRecord,
                        false /*isResp*/,
                        IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA,
                        exception);
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, mockInvalidPacket);
        mLooper.dispatchAll();

        // Verify error notification was sent
        List<IkePayload> payloads = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(1, payloads.size());

        IkePayload payload = payloads.get(0);
        assertEquals(IkePayload.PAYLOAD_TYPE_NOTIFY, payload.payloadType);
        assertEquals(
                ERROR_TYPE_UNSUPPORTED_CRITICAL_PAYLOAD, ((IkeNotifyPayload) payload).notifyType);

        // Verify IKE Session stays in the same state
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeLocalCreate);
    }

    private void mockRescheduleRekey(IkeSaRecord spySaRecord) {
        IkeLocalRequest rekeyReq =
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE);
        doAnswer(
                (invocation) -> {
                        mIkeSessionStateMachine.sendMessageDelayed(
                                IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE,
                                rekeyReq,
                                RETRY_INTERVAL_MS);
                        return null;
                })
                .when(spySaRecord)
                .rescheduleRekey(eq(RETRY_INTERVAL_MS));
    }

    @Test
    public void testRekeyIkeLocalCreateHandleRespWithTempFailure() throws Exception {
        setupIdleStateMachine();

        // Send Rekey-Create request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();

        // Mock sending TEMPORARY_FAILURE response
        mockRcvTempFail();
        mLooper.dispatchAll();

        verify(mSpyCurrentIkeSaRecord).rescheduleRekey(eq(RETRY_INTERVAL_MS));
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    private void mockCreateAndTransitionToRekeyDeleteLocal() {
        // Seed fake rekey data and force transition to RekeyIkeLocalDelete
        mIkeSessionStateMachine.mLocalInitNewIkeSaRecord = mSpyLocalInitIkeSaRecord;
        mIkeSessionStateMachine.addIkeSaRecord(mSpyLocalInitIkeSaRecord);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION,
                mIkeSessionStateMachine.mRekeyIkeLocalDelete);
        mLooper.dispatchAll();
        verifyRetransmissionStarted();
    }

    @Test
    public void testRekeyIkeLocalDeleteSendsRequest() throws Exception {
        setupIdleStateMachine();
        mockCreateAndTransitionToRekeyDeleteLocal();

        // Verify Rekey-Delete request
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeLocalDelete);
        verifyRetransmissionStarted();

        // Verify outbound message
        IkeMessage delMsg = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);

        IkeHeader ikeHeader = delMsg.ikeHeader;
        assertEquals(mSpyCurrentIkeSaRecord.getInitiatorSpi(), ikeHeader.ikeInitiatorSpi);
        assertEquals(mSpyCurrentIkeSaRecord.getResponderSpi(), ikeHeader.ikeResponderSpi);
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, ikeHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_INFORMATIONAL, ikeHeader.exchangeType);
        assertEquals(mSpyCurrentIkeSaRecord.isLocalInit, ikeHeader.fromIkeInitiator);
        assertFalse(ikeHeader.isResponseMsg);

        List<IkeDeletePayload> deletePayloadList =
                delMsg.getPayloadListForType(
                        IkePayload.PAYLOAD_TYPE_DELETE, IkeDeletePayload.class);
        assertEquals(1, deletePayloadList.size());

        IkeDeletePayload deletePayload = deletePayloadList.get(0);
        assertEquals(IkePayload.PROTOCOL_ID_IKE, deletePayload.protocolId);
        assertEquals(0, deletePayload.numSpi);
        assertEquals(0, deletePayload.spiSize);
        assertArrayEquals(new int[0], deletePayload.spisToDelete);
    }

    private void verifyRekeyReplaceSa(IkeSaRecord newSaRecord) {
        verify(mSpyCurrentIkeSaRecord).close();
        verify(mSpyCurrentIkeSocket).unregisterIke(eq(mSpyCurrentIkeSaRecord.getLocalSpi()));
        verify(mSpyCurrentIkeSocket, never()).unregisterIke(eq(newSaRecord.getLocalSpi()));

        assertEquals(mIkeSessionStateMachine.mCurrentIkeSaRecord, newSaRecord);

        verify(mMockChildSessionStateMachine).setSkD(newSaRecord.getSkD());
    }

    @Test
    public void testRekeyIkeLocalDeleteHandlesResponse() throws Exception {
        setupIdleStateMachine();
        mockCreateAndTransitionToRekeyDeleteLocal();

        // Receive Delete response
        ReceivedIkePacket dummyDeleteIkeRespReceivedPacket =
                makeDeleteIkeResponse(mSpyCurrentIkeSaRecord);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyDeleteIkeRespReceivedPacket);
        mLooper.dispatchAll();
        verifyIncrementLocaReqMsgId();
        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyDeleteIkeRespReceivedPacket);

        // Verify final state - Idle, with new SA, and old SA closed.
        verifyRekeyReplaceSa(mSpyLocalInitIkeSaRecord);
        verify(mMockIkeSessionCallback, never()).onClosed();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
        verifyRetransmissionStopped();
    }

    @Ignore
    public void disableTestRekeyIkeLocalDeleteHandlesRespWithParsingError() throws Exception {
        setupIdleStateMachine();
        mockCreateAndTransitionToRekeyDeleteLocal();
        resetMockIkeMessageHelper();

        // Mock receiving packet with syntax error
        ReceivedIkePacket mockInvalidPacket =
                makeDummyReceivedIkePacketWithInvalidSyntax(
                        mSpyCurrentIkeSaRecord,
                        true /*isResp*/,
                        IkeHeader.EXCHANGE_TYPE_INFORMATIONAL);
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, mockInvalidPacket);
        mLooper.dispatchAll();

        // Verify no more request out
        verifyEncryptAndEncodeNeverCalled(mSpyCurrentIkeSaRecord);

        // Verify final state - Idle, with new SA, and old SA closed.
        verifyRekeyReplaceSa(mSpyLocalInitIkeSaRecord);
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
        verifyRetransmissionStopped();
    }

    @Test
    public void testRekeyIkeLocalDeleteWithRequestOnNewSa() throws Exception {
        setupIdleStateMachine();
        mockCreateAndTransitionToRekeyDeleteLocal();

        // Receive an empty (DPD) request on the new IKE SA
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeDpdIkeRequest(mSpyLocalInitIkeSaRecord));
        mLooper.dispatchAll();

        // Verify final state - Idle, with new SA, and old SA closed.
        verifyRekeyReplaceSa(mSpyLocalInitIkeSaRecord);
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
        verifyRetransmissionStopped();
    }

    @Test
    public void testRekeyIkeLocalDeleteWithRequestFragOnNewSa() throws Exception {
        setupIdleStateMachine();
        mockCreateAndTransitionToRekeyDeleteLocal();

        // Received IKE fragment
        byte[] unencryptedData = "testRekeyIkeLocalDeleteWithRequestFragOnNewSa".getBytes();
        int fragNum = 1;
        int totalFragments = 2;
        IkeSkfPayload skfPayload =
                IkeTestUtils.makeDummySkfPayload(unencryptedData, fragNum, totalFragments);

        ReceivedIkePacket packet =
                makeDummyReceivedIkeFragmentPacket(
                        mSpyLocalInitIkeSaRecord,
                        false /*isResp*/,
                        IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA,
                        skfPayload,
                        PAYLOAD_TYPE_SA,
                        null /* collectedFrags*/);
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, packet);
        mLooper.dispatchAll();

        // Verify rekey is done.
        verifyRekeyReplaceSa(mSpyLocalInitIkeSaRecord);
        verifyRetransmissionStopped();

        // Verify the IkeSaRecord has stored the new fragment.
        DecodeResultPartial resultPartial =
                mSpyLocalInitIkeSaRecord.getCollectedFragments(false /*isResp*/);
        assertEquals(PAYLOAD_TYPE_SA, resultPartial.firstPayloadType);
        assertEquals(totalFragments, resultPartial.collectedFragsList.length);
        assertArrayEquals(unencryptedData, resultPartial.collectedFragsList[fragNum - 1]);
        assertFalse(resultPartial.isAllFragmentsReceived());
    }

    @Test
    public void testRekeyIkeRemoteDeleteWithRequestOnNewSa() throws Exception {
        setupIdleStateMachine();

        // Seed fake rekey data and force transition to RekeyIkeRemoteDelete
        mIkeSessionStateMachine.mRemoteInitNewIkeSaRecord = mSpyRemoteInitIkeSaRecord;
        mIkeSessionStateMachine.addIkeSaRecord(mSpyRemoteInitIkeSaRecord);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION,
                mIkeSessionStateMachine.mRekeyIkeRemoteDelete);
        mLooper.dispatchAll();

        // Receive an empty (DPD) request on the new IKE SA
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeDpdIkeRequest(mSpyRemoteInitIkeSaRecord));
        mLooper.dispatchAll();

        // Verify final state - Idle, with new SA, and old SA closed.
        verifyRekeyReplaceSa(mSpyRemoteInitIkeSaRecord);
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Test
    public void testRekeyIkeRemoteCreate() throws Exception {
        setupIdleStateMachine();

        setupRekeyedIkeSa(mSpyRemoteInitIkeSaRecord);

        // Receive Rekey request
        ReceivedIkePacket dummyRekeyIkeRequestReceivedPacket = makeRekeyIkeRequest();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyRekeyIkeRequestReceivedPacket);
        mLooper.dispatchAll();
        verifyIncrementRemoteReqMsgId();
        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyRekeyIkeRequestReceivedPacket);

        // Verify SA created with correct parameters
        ArgumentCaptor<SaRecord.IkeSaRecordConfig> recordConfigCaptor =
                ArgumentCaptor.forClass(SaRecord.IkeSaRecordConfig.class);
        verify(mMockSaRecordHelper)
                .makeRekeyedIkeSaRecord(any(), any(), any(), any(), recordConfigCaptor.capture());
        assertEquals(IKE_REKEY_SA_INITIATOR_SPI, recordConfigCaptor.getValue().initSpi.getSpi());

        // Verify outbound CREATE_CHILD_SA message
        IkeMessage rekeyCreateResp = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);
        IkeHeader rekeyCreateRespHeader = rekeyCreateResp.ikeHeader;
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, rekeyCreateRespHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA, rekeyCreateRespHeader.exchangeType);
        assertTrue(rekeyCreateRespHeader.isResponseMsg);
        assertTrue(rekeyCreateRespHeader.fromIkeInitiator);
        assertNotNull(
                rekeyCreateResp.getPayloadForType(IkePayload.PAYLOAD_TYPE_SA, IkeSaPayload.class));
        assertNotNull(
                rekeyCreateResp.getPayloadForType(IkePayload.PAYLOAD_TYPE_KE, IkeKePayload.class));
        assertNotNull(
                rekeyCreateResp.getPayloadForType(
                        IkePayload.PAYLOAD_TYPE_NONCE, IkeNoncePayload.class));

        // Verify SA, StateMachine state
        assertEquals(mSpyCurrentIkeSaRecord, mIkeSessionStateMachine.mIkeSaRecordAwaitingRemoteDel);
        assertEquals(mSpyRemoteInitIkeSaRecord, mIkeSessionStateMachine.mIkeSaRecordSurviving);
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeRemoteDelete);
        verify(mSpyCurrentIkeSocket)
                .registerIke(
                        eq(mSpyRemoteInitIkeSaRecord.getLocalSpi()), eq(mIkeSessionStateMachine));
    }

    @Ignore
    public void disableTestRekeyIkeRemoteCreateHandlesInvalidReq() throws Exception {
        setupIdleStateMachine();

        // Receive Rekey request
        ReceivedIkePacket request = makeRekeyIkeRequestWithUnacceptableProposal();
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, request);
        mLooper.dispatchAll();

        verifyProcessRekeyReqFailure(ERROR_TYPE_NO_PROPOSAL_CHOSEN);
    }

    @Ignore
    public void disableTestRekeyIkeRemoteCreateSaCreationFailure() throws Exception {
        // Throw error when building new IKE SA
        throwExceptionWhenMakeRekeyIkeSa(
                new GeneralSecurityException("testRekeyIkeRemoteCreateSaCreationFailure"));
        setupIdleStateMachine();

        // Receive Rekey request
        ReceivedIkePacket request = makeRekeyIkeRequest();
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, request);
        mLooper.dispatchAll();

        verifyProcessRekeyReqFailure(ERROR_TYPE_NO_PROPOSAL_CHOSEN);
    }

    private void verifyProcessRekeyReqFailure(int expectedErrorCode) {
        // Verify IKE Session is back to Idle
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);

        // Verify error notification was sent
        List<IkePayload> payloads = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(1, payloads.size());
        IkeNotifyPayload notify = (IkeNotifyPayload) payloads.get(0);
        assertEquals(expectedErrorCode, notify.notifyType);
    }

    @Test
    public void testRekeyIkeRemoteDelete() throws Exception {
        setupIdleStateMachine();

        // Seed fake rekey data and force transition to RekeyIkeLocalDelete
        mIkeSessionStateMachine.mRemoteInitNewIkeSaRecord = mSpyRemoteInitIkeSaRecord;
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION,
                mIkeSessionStateMachine.mRekeyIkeRemoteDelete);
        mLooper.dispatchAll();

        // Rekey Delete request
        ReceivedIkePacket dummyDeleteIkeRequestReceivedPacket =
                makeDeleteIkeRequest(mSpyCurrentIkeSaRecord);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyDeleteIkeRequestReceivedPacket);
        mLooper.dispatchAll();
        verifyIncrementRemoteReqMsgId();
        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyDeleteIkeRequestReceivedPacket);

        // Verify outbound DELETE_IKE_SA message
        IkeMessage rekeyDeleteResp = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);
        IkeHeader rekeyDeleteRespHeader = rekeyDeleteResp.ikeHeader;
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, rekeyDeleteRespHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_INFORMATIONAL, rekeyDeleteRespHeader.exchangeType);
        assertTrue(rekeyDeleteRespHeader.isResponseMsg);
        assertTrue(rekeyDeleteRespHeader.fromIkeInitiator);
        assertTrue(rekeyDeleteResp.ikePayloadList.isEmpty());

        // Verify final state - Idle, with new SA, and old SA closed.
        verifyRekeyReplaceSa(mSpyRemoteInitIkeSaRecord);

        verify(mMockIkeSessionCallback, never()).onClosed();

        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyDeleteIkeRequestReceivedPacket);
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Test
    public void testRekeyIkeRemoteDeleteExitAndRenter() throws Exception {
        setupIdleStateMachine();

        // Seed fake rekey data and force transition to RekeyIkeLocalDelete
        mIkeSessionStateMachine.mRemoteInitNewIkeSaRecord = mSpyRemoteInitIkeSaRecord;
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION,
                mIkeSessionStateMachine.mRekeyIkeRemoteDelete);
        mLooper.dispatchAll();

        // Trigger a timeout, and immediately re-enter remote-delete
        mLooper.moveTimeForward(IkeSessionStateMachine.REKEY_DELETE_TIMEOUT_MS / 2 + 1);
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.TIMEOUT_REKEY_REMOTE_DELETE);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION,
                mIkeSessionStateMachine.mRekeyIkeRemoteDelete);
        mLooper.dispatchAll();

        // Shift time forward, and assert the previous timeout was NOT fired.
        mLooper.moveTimeForward(IkeSessionStateMachine.REKEY_DELETE_TIMEOUT_MS / 2 + 1);
        mLooper.dispatchAll();

        // Verify no request received, or response sent.
        verify(mMockIkeMessageHelper, never()).decode(anyInt(), anyObject(), anyObject());
        verifyEncryptAndEncodeNeverCalled(mSpyCurrentIkeSaRecord);

        // Verify final state has not changed - signal was not sent.
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeRemoteDelete);
    }

    @Test
    public void testRekeyIkeRemoteDeleteTimedOut() throws Exception {
        setupIdleStateMachine();

        // Seed fake rekey data and force transition to RekeyIkeLocalDelete
        mIkeSessionStateMachine.mRemoteInitNewIkeSaRecord = mSpyRemoteInitIkeSaRecord;
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION,
                mIkeSessionStateMachine.mRekeyIkeRemoteDelete);
        mLooper.dispatchAll();

        mLooper.moveTimeForward(IkeSessionStateMachine.REKEY_DELETE_TIMEOUT_MS);
        mLooper.dispatchAll();

        // Verify no request received, or response sent.
        verify(mMockIkeMessageHelper, never()).decode(anyInt(), anyObject(), anyObject());
        verifyEncryptAndEncodeNeverCalled(mSpyCurrentIkeSaRecord);

        // Verify final state - Idle, with new SA, and old SA closed.
        verifyRekeyReplaceSa(mSpyRemoteInitIkeSaRecord);

        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Ignore
    public void disableTestSimulRekey() throws Exception {
        setupIdleStateMachine();

        // Prepare "rekeyed" SA
        setupRekeyedIkeSa(mSpyLocalInitIkeSaRecord);
        doReturn(1).when(mSpyLocalInitIkeSaRecord).compareTo(mSpyRemoteInitIkeSaRecord);

        // Send Rekey request on mSpyCurrentIkeSaRecord
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));

        // Receive Rekey request on mSpyCurrentIkeSaRecord
        ReceivedIkePacket dummyRekeyIkeRequestReceivedPacket = makeRekeyIkeRequest();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyRekeyIkeRequestReceivedPacket);
        mLooper.dispatchAll();
        verifyIncrementRemoteReqMsgId();

        // Receive Rekey response on mSpyCurrentIkeSaRecord
        ReceivedIkePacket dummyRekeyIkeRespReceivedPacket = makeRekeyIkeResponse();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyRekeyIkeRespReceivedPacket);
        mLooper.dispatchAll();
        verifyIncrementLocaReqMsgId();
        verify(mSpyCurrentIkeSocket)
                .registerIke(
                        eq(mSpyLocalInitIkeSaRecord.getLocalSpi()), eq(mIkeSessionStateMachine));

        // Receive Delete response on mSpyCurrentIkeSaRecord
        ReceivedIkePacket dummyDeleteIkeRespReceivedPacket =
                makeDeleteIkeResponse(mSpyCurrentIkeSaRecord);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyDeleteIkeRespReceivedPacket);
        mLooper.dispatchAll();
        verifyIncrementLocaReqMsgId();

        // Verify
        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyRekeyIkeRequestReceivedPacket);
        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyRekeyIkeRespReceivedPacket);
        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyDeleteIkeRespReceivedPacket);
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);

        verifyRekeyReplaceSa(mSpyLocalInitIkeSaRecord);
        verify(mMockIkeSessionCallback, never()).onClosed();
    }

    @Test
    public void testOpenIkeSession() throws Exception {
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.Initial);

        mIkeSessionStateMachine.openSession();
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.CreateIkeLocalIkeInit);
    }

    @Test
    public void testIkeInitSchedulesRekey() throws Exception {
        setupFirstIkeSa();

        // Send IKE INIT request
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_LOCAL_REQUEST_CREATE_IKE);

        // Receive IKE INIT response
        ReceivedIkePacket dummyReceivedIkePacket = makeIkeInitResponse();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyReceivedIkePacket);

        // Mock IKE AUTH and transition to Idle
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION, mIkeSessionStateMachine.mIdle);
        mLooper.dispatchAll();
        mIkeSessionStateMachine.mSaProposal = buildSaProposal();

        // Move time forward to trigger rekey
        mLooper.moveTimeForward(
                mIkeSessionStateMachine.mIkeSessionParams.getSoftLifetimeMsInternal());
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeLocalCreate);
    }

    @Test
    public void testRekeyCreateIkeSchedulesRekey() throws Exception {
        setupIdleStateMachine();

        // Send Rekey-Create request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();

        // Prepare "rekeyed" SA
        setupRekeyedIkeSa(mSpyLocalInitIkeSaRecord);

        // Receive Rekey response
        ReceivedIkePacket dummyRekeyIkeRespReceivedPacket = makeRekeyIkeResponse();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyRekeyIkeRespReceivedPacket);
        mLooper.dispatchAll();

        // Mock rekey delete and transition to Idle
        mIkeSessionStateMachine.mCurrentIkeSaRecord = mSpyLocalInitIkeSaRecord;
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION, mIkeSessionStateMachine.mIdle);
        mLooper.dispatchAll();

        // Move time forward to trigger rekey
        mLooper.moveTimeForward(
                mIkeSessionStateMachine.mIkeSessionParams.getSoftLifetimeMsInternal());
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeLocalCreate);
    }

    @Test
    public void testBuildEncryptedInformationalMessage() throws Exception {
        IkeNotifyPayload payload = new IkeNotifyPayload(ERROR_TYPE_INVALID_SYNTAX, new byte[0]);

        boolean isResp = false;
        IkeMessage generated =
                mIkeSessionStateMachine.buildEncryptedInformationalMessage(
                        mSpyCurrentIkeSaRecord, new IkeInformationalPayload[] {payload}, isResp, 0);

        assertEquals(mSpyCurrentIkeSaRecord.getInitiatorSpi(), generated.ikeHeader.ikeInitiatorSpi);
        assertEquals(mSpyCurrentIkeSaRecord.getResponderSpi(), generated.ikeHeader.ikeResponderSpi);
        assertEquals(
                mSpyCurrentIkeSaRecord.getLocalRequestMessageId(), generated.ikeHeader.messageId);
        assertEquals(isResp, generated.ikeHeader.isResponseMsg);
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, generated.ikeHeader.nextPayloadType);

        List<IkeNotifyPayload> generatedPayloads =
                generated.getPayloadListForType(
                        IkePayload.PAYLOAD_TYPE_NOTIFY, IkeNotifyPayload.class);
        assertEquals(1, generatedPayloads.size());

        IkeNotifyPayload generatedPayload = generatedPayloads.get(0);
        assertArrayEquals(new byte[0], generatedPayload.notifyData);
        assertEquals(ERROR_TYPE_INVALID_SYNTAX, generatedPayload.notifyType);
    }

    private void verifyLastSentRespAllPackets(byte[][] expectedPackets, IkeSaRecord saRecord) {
        if (expectedPackets == null) {
            assertNull(saRecord.getLastSentRespAllPackets());
            return;
        }

        assertEquals(expectedPackets.length, saRecord.getLastSentRespAllPackets().size());
        for (int i = 0; i < expectedPackets.length; i++) {
            assertArrayEquals(expectedPackets[i], saRecord.getLastSentRespAllPackets().get(i));
        }
    }

    @Test
    public void testEncryptedRetransmitterImmediatelySendsRequest() throws Exception {
        setupIdleStateMachine();
        byte[][] dummyLastRespBytes =
                new byte[][] {"testRetransmitterSendsRequestLastResp".getBytes()};
        mSpyCurrentIkeSaRecord.updateLastSentRespAllPackets(Arrays.asList(dummyLastRespBytes));

        IkeMessage spyIkeReqMessage =
                spy(
                        new IkeMessage(
                                new IkeHeader(
                                        mSpyCurrentIkeSaRecord.getInitiatorSpi(),
                                        mSpyCurrentIkeSaRecord.getResponderSpi(),
                                        IkePayload.PAYLOAD_TYPE_SK,
                                        EXCHANGE_TYPE_INFORMATIONAL,
                                        false /*isResp*/,
                                        mSpyCurrentIkeSaRecord.isLocalInit,
                                        mSpyCurrentIkeSaRecord.getLocalRequestMessageId()),
                                new LinkedList<>()));

        // Use something unique as a sentinel value
        byte[][] dummyReqBytesList =
                new byte[][] {
                    "testRetransmitterSendsReqFrag1".getBytes(),
                    "testRetransmitterSendsReqFrag2".getBytes()
                };

        doReturn(dummyReqBytesList)
                .when(spyIkeReqMessage)
                .encryptAndEncode(any(), any(), eq(mSpyCurrentIkeSaRecord), anyBoolean(), anyInt());

        IkeSessionStateMachine.EncryptedRetransmitter retransmitter =
                mIkeSessionStateMachine.new EncryptedRetransmitter(spyIkeReqMessage);

        // Verify message is sent out, and that request does not change cached retransmit-response
        // mLastSentIkeResp.
        verify(mSpyCurrentIkeSocket).sendIkePacket(eq(dummyReqBytesList[0]), eq(REMOTE_ADDRESS));
        verify(mSpyCurrentIkeSocket).sendIkePacket(eq(dummyReqBytesList[1]), eq(REMOTE_ADDRESS));
        verifyLastSentRespAllPackets(dummyLastRespBytes, mSpyCurrentIkeSaRecord);
    }

    // TODO: b/141275871 Test retransmisstions are fired for correct times within certain time.

    @Test
    public void testCacheLastRequestAndResponse() throws Exception {
        setupIdleStateMachine();
        mSpyCurrentIkeSaRecord.updateLastReceivedReqFirstPacket(null /*reqPacket*/);
        mSpyCurrentIkeSaRecord.updateLastSentRespAllPackets(null /*respPacketList*/);

        byte[] dummyIkeReqFirstPacket = "testLastSentRequest".getBytes();
        byte[][] dummyIkeResp =
                new byte[][] {
                    "testLastSentRespFrag1".getBytes(), "testLastSentRespFrag2".getBytes()
                };

        doReturn(dummyIkeResp)
                .when(mMockIkeMessageHelper)
                .encryptAndEncode(
                        any(),
                        any(),
                        eq(mSpyCurrentIkeSaRecord),
                        any(IkeMessage.class),
                        anyBoolean(),
                        anyInt());

        // Receive a DPD request, expect to send dummyIkeResp
        ReceivedIkePacket dummyDpdRequest =
                makeDpdIkeRequest(
                        mSpyCurrentIkeSaRecord.getRemoteRequestMessageId(), dummyIkeReqFirstPacket);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyDpdRequest);
        mLooper.dispatchAll();

        verify(mSpyCurrentIkeSocket).sendIkePacket(eq(dummyIkeResp[0]), eq(REMOTE_ADDRESS));
        verify(mSpyCurrentIkeSocket).sendIkePacket(eq(dummyIkeResp[1]), eq(REMOTE_ADDRESS));

        verifyLastSentRespAllPackets(dummyIkeResp, mSpyCurrentIkeSaRecord);
        assertTrue(mSpyCurrentIkeSaRecord.isRetransmittedRequest(dummyIkeReqFirstPacket));

        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Test
    public void testReplyRetransmittedRequest() throws Exception {
        setupIdleStateMachine();

        // Mock last sent request and response
        byte[] dummyIkeReqFirstPacket = "testRcvRetransmittedRequestReq".getBytes();
        byte[][] dummyIkeResp = new byte[][] {"testRcvRetransmittedRequestResp".getBytes()};

        mSpyCurrentIkeSaRecord.updateLastReceivedReqFirstPacket(dummyIkeReqFirstPacket);
        mSpyCurrentIkeSaRecord.updateLastSentRespAllPackets(Arrays.asList(dummyIkeResp));
        mSpyCurrentIkeSaRecord.incrementRemoteRequestMessageId();

        // Build request with last validated message ID
        ReceivedIkePacket request =
                makeDpdIkeRequest(
                        mSpyCurrentIkeSaRecord.getRemoteRequestMessageId() - 1,
                        dummyIkeReqFirstPacket);

        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, request);

        mLooper.dispatchAll();

        verifyLastSentRespAllPackets(dummyIkeResp, mSpyCurrentIkeSaRecord);
        verify(mSpyCurrentIkeSocket).sendIkePacket(eq(dummyIkeResp[0]), eq(REMOTE_ADDRESS));

        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Test
    public void testDiscardFakeRetransmittedRequest() throws Exception {
        setupIdleStateMachine();

        // Mock last sent request and response
        byte[] dummyIkeReqFirstPacket = "testDiscardFakeRetransmittedRequestReq".getBytes();
        byte[][] dummyIkeResp = new byte[][] {"testDiscardFakeRetransmittedRequestResp".getBytes()};
        mSpyCurrentIkeSaRecord.updateLastReceivedReqFirstPacket(dummyIkeReqFirstPacket);
        mSpyCurrentIkeSaRecord.updateLastSentRespAllPackets(Arrays.asList(dummyIkeResp));
        mSpyCurrentIkeSaRecord.incrementRemoteRequestMessageId();

        // Build request with last validated message ID but different bytes
        ReceivedIkePacket request =
                makeDpdIkeRequest(
                        mSpyCurrentIkeSaRecord.getRemoteRequestMessageId() - 1, new byte[0]);

        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, request);

        mLooper.dispatchAll();

        verifyLastSentRespAllPackets(dummyIkeResp, mSpyCurrentIkeSaRecord);
        verify(mSpyCurrentIkeSocket, never()).sendIkePacket(any(), any());

        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Test
    public void testDiscardRetransmittedResponse() throws Exception {
        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);
        verifyRetransmissionStarted();

        // Build and send fake response with last validated message ID to IKE state machine
        ReceivedIkePacket resp =
                makeDummyEncryptedReceivedIkePacketWithPayloadList(
                        mSpyCurrentIkeSaRecord,
                        IkeHeader.EXCHANGE_TYPE_IKE_SA_INIT,
                        true /*isResp*/,
                        mSpyCurrentIkeSaRecord.getLocalRequestMessageId() - 1,
                        new LinkedList<>(),
                        new byte[0]);

        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, resp);
        mLooper.dispatchAll();

        // Verify current state does not change
        verifyRetransmissionStarted();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.CreateIkeLocalIkeAuth);
    }

    @Test
    public void testDeleteIkeLocalDeleteRequest() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_DELETE_IKE));
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        // Verify outbound message
        IkeMessage delMsg = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);

        IkeHeader ikeHeader = delMsg.ikeHeader;
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, ikeHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_INFORMATIONAL, ikeHeader.exchangeType);
        assertFalse(ikeHeader.isResponseMsg);
        assertTrue(ikeHeader.fromIkeInitiator);

        List<IkeDeletePayload> deletePayloadList =
                delMsg.getPayloadListForType(
                        IkePayload.PAYLOAD_TYPE_DELETE, IkeDeletePayload.class);
        assertEquals(1, deletePayloadList.size());

        IkeDeletePayload deletePayload = deletePayloadList.get(0);
        assertEquals(IkePayload.PROTOCOL_ID_IKE, deletePayload.protocolId);
        assertEquals(0, deletePayload.numSpi);
        assertEquals(0, deletePayload.spiSize);
        assertArrayEquals(new int[0], deletePayload.spisToDelete);
    }

    @Test
    public void testDeleteIkeLocalDeleteResponse() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_DELETE_IKE));
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        ReceivedIkePacket received = makeDeleteIkeResponse(mSpyCurrentIkeSaRecord);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, received);
        mLooper.dispatchAll();
        verifyIncrementLocaReqMsgId();

        verifyNotifyUserCloseSession();

        // Verify state machine quit properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
    }

    @Test
    public void testDeleteIkeLocalDeleteResponseWithParsingError() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_DELETE_IKE));
        mLooper.dispatchAll();
        verifyRetransmissionStarted();
        resetMockIkeMessageHelper();

        // Mock receiving response with syntax error
        ReceivedIkePacket mockInvalidPacket =
                makeDummyReceivedIkePacketWithInvalidSyntax(
                        mSpyCurrentIkeSaRecord,
                        true /*isResp*/,
                        IkeHeader.EXCHANGE_TYPE_INFORMATIONAL);
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, mockInvalidPacket);
        mLooper.dispatchAll();

        // Verify no more request out
        verifyEncryptAndEncodeNeverCalled(mSpyCurrentIkeSaRecord);

        // Verify state machine quit properly
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(InvalidSyntaxException.class));
        assertNull(mIkeSessionStateMachine.getCurrentState());
    }

    @Test
    public void testDeleteIkeLocalDeleteHandlesInvalidResp() throws Exception {
        setupIdleStateMachine();

        // Send delete request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_DELETE_IKE));
        mLooper.dispatchAll();

        // Receive response with wrong exchange type
        ReceivedIkePacket resp =
                makeDummyReceivedIkePacketWithInvalidSyntax(
                        mSpyCurrentIkeSaRecord,
                        true /*isResp*/,
                        IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA);
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, resp);
        mLooper.dispatchAll();

        // Verify state machine quit properly
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(InvalidSyntaxException.class));
        assertNull(mIkeSessionStateMachine.getCurrentState());
    }

    @Test
    public void testDeleteIkeLocalDeleteReceivedNonDeleteRequest() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_DELETE_IKE));
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        // Verify delete sent out.
        verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);

        resetMockIkeMessageHelper(); // Discard value.

        ReceivedIkePacket received = makeRekeyIkeRequest();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, received);

        mLooper.dispatchAll();
        verifyRetransmissionStarted();
        verifyIncrementRemoteReqMsgId();

        verifySendTempFailResponse();
    }

    private void verifySendTempFailResponse() {
        IkeMessage resp = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);

        IkeHeader ikeHeader = resp.ikeHeader;
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, ikeHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_INFORMATIONAL, ikeHeader.exchangeType);
        assertTrue(ikeHeader.isResponseMsg);
        assertEquals(mSpyCurrentIkeSaRecord.isLocalInit, ikeHeader.fromIkeInitiator);

        List<IkeNotifyPayload> notificationPayloadList =
                resp.getPayloadListForType(IkePayload.PAYLOAD_TYPE_NOTIFY, IkeNotifyPayload.class);
        assertEquals(1, notificationPayloadList.size());

        IkeNotifyPayload notifyPayload = notificationPayloadList.get(0);
        assertEquals(IkeProtocolException.ERROR_TYPE_TEMPORARY_FAILURE, notifyPayload.notifyType);
    }

    @Test
    public void testDeleteIkeRemoteDelete() throws Exception {
        setupIdleStateMachine();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET,
                makeDeleteIkeRequest(mSpyCurrentIkeSaRecord));

        mLooper.dispatchAll();
        verifyIncrementRemoteReqMsgId();

        // Verify outbound message
        IkeMessage delMsg = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);

        IkeHeader ikeHeader = delMsg.ikeHeader;
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, ikeHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_INFORMATIONAL, ikeHeader.exchangeType);
        assertTrue(ikeHeader.isResponseMsg);
        assertEquals(mSpyCurrentIkeSaRecord.isLocalInit, ikeHeader.fromIkeInitiator);

        assertTrue(delMsg.ikePayloadList.isEmpty());

        verifyNotifyUserCloseSession();

        // Verify state machine quit properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
    }

    @Test
    public void testKillSession() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.killSession();
        mLooper.dispatchAll();

        verify(mSpyCurrentIkeSaRecord).close();
        verify(mSpyCurrentIkeSocket).unregisterIke(mSpyCurrentIkeSaRecord.getInitiatorSpi());
        verifyNotifyUserCloseSession();

        // Verify state machine quit properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockBusyWakelock).release();
    }

    @Test
    public void testReceiveDpd() throws Exception {
        setupIdleStateMachine();

        // Receive a DPD request, expect to stay in IDLE state
        ReceivedIkePacket dummyDpdRequest = makeDpdIkeRequest(mSpyCurrentIkeSaRecord);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyDpdRequest);
        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);

        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyDpdRequest);

        // Verify outbound response
        IkeMessage resp = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);
        IkeHeader ikeHeader = resp.ikeHeader;
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, ikeHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_INFORMATIONAL, ikeHeader.exchangeType);
        assertTrue(ikeHeader.isResponseMsg);
        assertEquals(mSpyCurrentIkeSaRecord.isLocalInit, ikeHeader.fromIkeInitiator);
        assertTrue(resp.ikePayloadList.isEmpty());
    }

    @Test
    public void testReceiveDpdNonIdle() throws Exception {
        setupIdleStateMachine();

        // Move to a non-idle state. Use RekeyIkeRemoteDelete, as it doesn't send out any requests.
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION,
                mIkeSessionStateMachine.mRekeyIkeRemoteDelete);
        mLooper.dispatchAll();

        // In a rekey state, receiving (and handling) a DPD should not result in a change of states
        ReceivedIkePacket dummyDpdRequest = makeDpdIkeRequest(mSpyCurrentIkeSaRecord);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyDpdRequest);
        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeRemoteDelete);

        verifyDecodeEncryptedMessage(mSpyCurrentIkeSaRecord, dummyDpdRequest);

        // Verify outbound response
        IkeMessage resp = verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);
        IkeHeader ikeHeader = resp.ikeHeader;
        assertEquals(IkePayload.PAYLOAD_TYPE_SK, ikeHeader.nextPayloadType);
        assertEquals(IkeHeader.EXCHANGE_TYPE_INFORMATIONAL, ikeHeader.exchangeType);
        assertTrue(ikeHeader.isResponseMsg);
        assertEquals(mSpyCurrentIkeSaRecord.isLocalInit, ikeHeader.fromIkeInitiator);
        assertTrue(resp.ikePayloadList.isEmpty());
    }

    private void executeAndVerifySendLocalDPD() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                CMD_FORCE_TRANSITION, mIkeSessionStateMachine.mDpdIkeLocalInfo);
        mLooper.dispatchAll();

        verifyEmptyInformationalSent(1, false /* expectedResp*/);
        resetMockIkeMessageHelper();
    }

    @Test
    public void testDpdIkeLocalInfoRcvDpdReq() throws Exception {
        executeAndVerifySendLocalDPD();
        mIkeSessionStateMachine.sendMessage(
                CMD_RECEIVE_IKE_PACKET, makeDpdIkeRequest(mSpyCurrentIkeSaRecord));
        mLooper.dispatchAll();

        verifyEmptyInformationalSent(1, true /* expectedResp*/);
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.DpdIkeLocalInfo);
    }

    @Test
    public void testDpdIkeLocalInfoRcvDeleteIkeReq() throws Exception {
        executeAndVerifySendLocalDPD();
        mIkeSessionStateMachine.sendMessage(
                CMD_RECEIVE_IKE_PACKET, makeDeleteIkeRequest(mSpyCurrentIkeSaRecord));
        mLooper.dispatchAll();

        verifyEmptyInformationalSent(1, true /* expectedResp*/);
        assertNull(mIkeSessionStateMachine.getCurrentState());
    }

    @Test
    public void testDpdIkeLocalInfoRcvRekeyIkeReq() throws Exception {
        executeAndVerifySendLocalDPD();
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, makeRekeyIkeRequest());
        mLooper.dispatchAll();

        verifySendTempFailResponse();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.DpdIkeLocalInfo);
    }

    @Test
    public void testIdleTriggersNewRequests() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();

        // Verify that the command is executed, and the state machine transitions to the right state
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeLocalCreate);
        verifyRetransmissionStarted();
    }

    @Test
    public void testNonIdleStateDoesNotTriggerNewRequests() throws Exception {
        setupIdleStateMachine();

        // Force ourselves into a non-idle state
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION, mIkeSessionStateMachine.mReceiving);
        mLooper.dispatchAll();
        verifyEncryptAndEncodeNeverCalled();

        // Queue a local request, and expect that it is not run (yet)
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();

        // Verify that the state machine is still in the Receiving state
        verifyEncryptAndEncodeNeverCalled();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.Receiving);

        // Go back to Idle, and expect to immediately transition to RekeyIkeLocalCreate from the
        // queued request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION, mIkeSessionStateMachine.mIdle);
        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeLocalCreate);
        verifyEncryptAndEncodeAndGetMessage(mSpyCurrentIkeSaRecord);
    }

    @Test
    public void testOpenChildSessionValidatesArgs() throws Exception {
        setupIdleStateMachine();

        // Expect failure - no callbacks provided
        try {
            mIkeSessionStateMachine.openChildSession(mChildSessionParams, null);
        } catch (IllegalArgumentException expected) {
        }

        // Expect failure - callbacks already registered
        try {
            mIkeSessionStateMachine.openChildSession(
                    mChildSessionParams, mMockChildSessionCallback);
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testOpenChildSession() throws Exception {
        setupIdleStateMachine();

        ChildSessionCallback cb = mock(ChildSessionCallback.class);
        mIkeSessionStateMachine.openChildSession(mChildSessionParams, cb);

        // Test that inserting the same cb returns an error, even before the state
        // machine has a chance to process it.
        try {
            mIkeSessionStateMachine.openChildSession(mChildSessionParams, cb);
        } catch (IllegalArgumentException expected) {
        }

        verify(mMockChildSessionFactoryHelper)
                .makeChildSessionStateMachine(
                        eq(mLooper.getLooper()),
                        eq(mSpyContext),
                        anyInt(),
                        any(AlarmManager.class),
                        any(RandomnessFactory.class),
                        any(IpSecSpiGenerator.class),
                        eq(mChildSessionParams),
                        eq(mSpyUserCbExecutor),
                        eq(cb),
                        any());

        // Verify state in IkeSessionStateMachine
        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);

        synchronized (mIkeSessionStateMachine.mChildCbToSessions) {
            assertTrue(mIkeSessionStateMachine.mChildCbToSessions.containsKey(cb));
        }
    }

    @Test
    public void testCloseChildSessionValidatesArgs() throws Exception {
        setupIdleStateMachine();

        // Expect failure - callbacks not registered
        try {
            mIkeSessionStateMachine.closeChildSession(mock(ChildSessionCallback.class));
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testCloseChildSession() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.closeChildSession(mMockChildSessionCallback);
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);
    }

    @Test
    public void testCloseImmediatelyAfterOpenChildSession() throws Exception {
        setupIdleStateMachine();

        ChildSessionCallback cb = mock(ChildSessionCallback.class);
        mIkeSessionStateMachine.openChildSession(mChildSessionParams, cb);

        // Verify that closing the session immediately still picks up the child callback
        // even before the looper has a chance to run.
        mIkeSessionStateMachine.closeChildSession(mMockChildSessionCallback);
    }

    @Test
    public void testOnChildSessionClosed() throws Exception {
        setupIdleStateMachine();

        mDummyChildSmCallback.onChildSessionClosed(mMockChildSessionCallback);

        synchronized (mIkeSessionStateMachine.mChildCbToSessions) {
            assertFalse(
                    mIkeSessionStateMachine.mChildCbToSessions.containsKey(
                            mMockChildSessionCallback));
        }
    }

    @Test
    public void testHandleUnexpectedExceptionInEnterState() throws Exception {
        Log spyIkeLog = TestUtils.makeSpyLogDoLogErrorForWtf(TAG);
        IkeManager.setIkeLog(spyIkeLog);

        IkeSessionParams mockSessionParams = mock(IkeSessionParams.class);
        when(mockSessionParams.getSaProposalsInternal()).thenThrow(mock(RuntimeException.class));

        DhGroupTransform dhGroupTransform = new DhGroupTransform(SaProposal.DH_GROUP_2048_BIT_MODP);
        IkeSaProposal mockSaProposal = mock(IkeSaProposal.class);
        when(mockSaProposal.getDhGroupTransforms())
                .thenReturn(new DhGroupTransform[] {dhGroupTransform});
        when(mockSessionParams.getSaProposals()).thenReturn(Arrays.asList(mockSaProposal));
        IkeSessionStateMachine ikeSession =
                new IkeSessionStateMachine(
                        mLooper.getLooper(),
                        mSpyContext,
                        mIpSecManager,
                        mockSessionParams,
                        mChildSessionParams,
                        mSpyUserCbExecutor,
                        mMockIkeSessionCallback,
                        mMockChildSessionCallback,
                        mMockEapAuthenticatorFactory);
        // Send IKE INIT request
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_LOCAL_REQUEST_CREATE_IKE);
        mLooper.dispatchAll();

        assertNull(ikeSession.getCurrentState());
        verify(mSpyUserCbExecutor).execute(any(Runnable.class));
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(IkeInternalException.class));
        verify(spyIkeLog).wtf(anyString(), anyString(), any(RuntimeException.class));
    }

    @Test
    public void testHandleUnexpectedExceptionInProcessStateMsg() throws Exception {
        Log spyIkeLog = TestUtils.makeSpyLogDoLogErrorForWtf(TAG);
        IkeManager.setIkeLog(spyIkeLog);

        setupIdleStateMachine();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, null /*receivedIkePacket*/);
        mLooper.dispatchAll();

        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mSpyUserCbExecutor).execute(any(Runnable.class));
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(IkeInternalException.class));
        verify(spyIkeLog).wtf(anyString(), anyString(), any(RuntimeException.class));
    }

    @Test
    public void testCreateIkeLocalIkeInitRcvErrorNotify() throws Exception {
        // Send IKE INIT request
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_LOCAL_REQUEST_CREATE_IKE);
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        // Receive IKE INIT response with erro notification.
        List<IkePayload> payloads = new LinkedList<>();
        payloads.add(new IkeNotifyPayload(IkeProtocolException.ERROR_TYPE_NO_PROPOSAL_CHOSEN));
        ReceivedIkePacket resp =
                makeDummyUnencryptedReceivedIkePacket(
                        1L /*initiator SPI*/,
                        2L /*respodner SPI*/,
                        IkeHeader.EXCHANGE_TYPE_IKE_SA_INIT,
                        true /*isResp*/,
                        false /*fromIkeInit*/,
                        payloads);

        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, resp);
        mLooper.dispatchAll();

        // Fires user error callbacks
        verify(mMockIkeSessionCallback)
                .onClosedExceptionally(
                        argThat(err -> err instanceof NoValidProposalChosenException));
        // Verify state machine quit properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
    }

    private void mockSendRekeyChildReq() throws Exception {
        setupIdleStateMachine();

        ChildLocalRequest childLocalRequest =
                new ChildLocalRequest(
                        IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_CHILD,
                        mMockChildSessionCallback,
                        null /*childParams*/);

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ, childLocalRequest);
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);
        verify(mMockChildSessionStateMachine).rekeyChildSession();

        // Mocking sending request
        mDummyChildSmCallback.onOutboundPayloadsReady(
                IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA,
                false /*isResp*/,
                new LinkedList<>(),
                mMockChildSessionStateMachine);
        mLooper.dispatchAll();
    }

    private void mockRcvTempFail() throws Exception {
        ReceivedIkePacket resp =
                makeResponseWithErrorNotify(new IkeNotifyPayload(ERROR_TYPE_TEMPORARY_FAILURE));

        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, resp);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION, mIkeSessionStateMachine.mIdle);
        mLooper.dispatchAll();
    }

    @Ignore
    public void disableTestTempFailureHandlerTimeout() throws Exception {
        long currentTime = 0;
        int retryCnt = 0;

        mockSendRekeyChildReq();

        while (currentTime + RETRY_INTERVAL_MS < TEMP_FAILURE_RETRY_TIMEOUT_MS) {
            mockRcvTempFail();

            mLooper.moveTimeForward(RETRY_INTERVAL_MS);
            currentTime += RETRY_INTERVAL_MS;
            mLooper.dispatchAll();

            retryCnt++;
            verify(mMockChildSessionStateMachine, times(1 + retryCnt)).rekeyChildSession();
        }

        mLooper.moveTimeForward(RETRY_INTERVAL_MS);
        mLooper.dispatchAll();

        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(IkeInternalException.class));
    }

    @Test
    public void testTempFailureHandlerCancelTimer() throws Exception {
        mockRescheduleRekey(mSpyCurrentIkeSaRecord);
        setupIdleStateMachine();

        // Send Rekey-Create request
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_EXECUTE_LOCAL_REQ,
                new IkeLocalRequest(IkeSessionStateMachine.CMD_LOCAL_REQUEST_REKEY_IKE));
        mLooper.dispatchAll();
        verifyRetransmissionStarted();

        // Mock sending TEMPORARY_FAILURE response
        mockRcvTempFail();
        mLooper.dispatchAll();
        verify(mSpyCurrentIkeSaRecord).rescheduleRekey(eq(RETRY_INTERVAL_MS));
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);

        // Move time forward to trigger retry
        mLooper.moveTimeForward(IkeSessionStateMachine.RETRY_INTERVAL_MS);
        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.RekeyIkeLocalCreate);

        // Prepare "rekeyed" SA
        setupRekeyedIkeSa(mSpyLocalInitIkeSaRecord);

        // Receive valid Rekey-Create response
        ReceivedIkePacket dummyRekeyIkeRespReceivedPacket = makeRekeyIkeResponse();
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyRekeyIkeRespReceivedPacket);
        mLooper.dispatchAll();

        // Receive Delete response
        ReceivedIkePacket dummyDeleteIkeRespReceivedPacket =
                makeDeleteIkeResponse(mSpyCurrentIkeSaRecord);
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, dummyDeleteIkeRespReceivedPacket);
        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);

        // Move time forward
        mLooper.moveTimeForward(IkeSessionStateMachine.TEMP_FAILURE_RETRY_TIMEOUT_MS);
        mLooper.dispatchAll();

        // Validate IKE Session is not closed
        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Ignore
    public void disableTestIdleReceiveRequestWithFatalError() throws Exception {
        setupIdleStateMachine();

        // Mock receiving packet with syntax error
        ReceivedIkePacket mockInvalidPacket =
                makeDummyReceivedIkePacketWithInvalidSyntax(
                        mSpyCurrentIkeSaRecord,
                        false /*isResp*/,
                        IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA);
        mIkeSessionStateMachine.sendMessage(CMD_RECEIVE_IKE_PACKET, mockInvalidPacket);
        mLooper.dispatchAll();

        // Verify Delete request was sent
        List<IkePayload> payloads = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(1, payloads.size());

        IkePayload payload = payloads.get(0);
        assertEquals(IkePayload.PAYLOAD_TYPE_NOTIFY, payload.payloadType);
        assertEquals(ERROR_TYPE_INVALID_SYNTAX, ((IkeNotifyPayload) payload).notifyType);

        // Verify IKE Session is closed properly
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(InvalidSyntaxException.class));
    }

    @Test
    public void testHandlesInvalidRequest() throws Exception {
        setupIdleStateMachine();

        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION,
                mIkeSessionStateMachine.mChildProcedureOngoing);

        // Receive an IKE AUTH request
        ReceivedIkePacket request =
                makeDummyEncryptedReceivedIkePacketWithPayloadList(
                        mSpyCurrentIkeSaRecord,
                        IkeHeader.EXCHANGE_TYPE_IKE_AUTH,
                        false /*isResp*/,
                        new LinkedList<IkePayload>());
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, request);
        mLooper.dispatchAll();

        // Verify error notification was sent
        List<IkePayload> ikePayloadList = verifyOutInfoMsgHeaderAndGetPayloads(true /*isResp*/);
        assertEquals(1, ikePayloadList.size());
        assertEquals(
                ERROR_TYPE_INVALID_SYNTAX, ((IkeNotifyPayload) ikePayloadList.get(0)).notifyType);

        // Verify IKE Session has quit
        assertNull(mIkeSessionStateMachine.getCurrentState());
        verify(mMockIkeSessionCallback).onClosedExceptionally(any(InvalidSyntaxException.class));
    }

    @Test
    public void testIdleHandlesDecryptPacketFailed() throws Exception {
        setupIdleStateMachine();

        ReceivedIkePacket packet =
                makeDummyReceivedIkePacketWithUnprotectedError(
                        mSpyCurrentIkeSaRecord,
                        false /*isResp*/,
                        EXCHANGE_TYPE_INFORMATIONAL,
                        mock(IkeException.class));
        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, packet);
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Test
    public void testHandlesUnencryptedPacket() throws Exception {
        setupIdleStateMachine();
        IkeMessage.setIkeMessageHelper(new IkeMessageHelper());

        ReceivedIkePacket packet =
                makeDummyUnencryptedReceivedIkePacket(
                        mSpyCurrentIkeSaRecord.getLocalSpi(),
                        mSpyCurrentIkeSaRecord.getRemoteSpi(),
                        IkeHeader.EXCHANGE_TYPE_INFORMATIONAL,
                        false /*isResp*/,
                        false /*fromIkeInit*/,
                        new LinkedList<>());

        mIkeSessionStateMachine.sendMessage(IkeSessionStateMachine.CMD_RECEIVE_IKE_PACKET, packet);
        mLooper.dispatchAll();

        assertTrue(
                mIkeSessionStateMachine.getCurrentState() instanceof IkeSessionStateMachine.Idle);
    }

    @Test
    public void testEapOnlyOption() throws Exception {
        mIkeSessionStateMachine.quitNow();
        IkeSessionParams ikeSessionParams =
                buildIkeSessionParamsCommon()
                        .setAuthEap(mRootCertificate, mEapSessionConfig)
                        .addIkeOption(IKE_OPTION_EAP_ONLY_AUTH)
                        .build();
        mIkeSessionStateMachine = makeAndStartIkeSession(ikeSessionParams);

        mockIkeInitAndTransitionToIkeAuth(mIkeSessionStateMachine.mCreateIkeLocalIkeAuth);

        IkeMessage ikeAuthReqMessage = verifyAuthReqAndGetMsg();
        List<IkeNotifyPayload> notifyPayloads =
                ikeAuthReqMessage.getPayloadListForType(
                        IkePayload.PAYLOAD_TYPE_NOTIFY, IkeNotifyPayload.class);
        assertTrue(hasEapOnlyNotifyPayload(notifyPayloads));
    }

    private IkeConfigPayload makeConfigPayload() throws Exception {
        return (IkeConfigPayload)
                IkeTestUtils.hexStringToIkePayload(
                        IkePayload.PAYLOAD_TYPE_CP, true /*isResp*/, CP_PAYLOAD_HEX_STRING);
    }

    private boolean hasEapOnlyNotifyPayload(List<IkeNotifyPayload> notifyPayloads) {
        for (IkeNotifyPayload payload : notifyPayloads) {
            if (payload.notifyType == NOTIFY_TYPE_EAP_ONLY_AUTHENTICATION) {
                return true;
            }
        }

        return false;
    }

    @Test
    public void testAcquireAndReleaseLocalReqWakeLock() throws Exception {
        setupIdleStateMachine();

        ChildSessionCallback cb = mock(ChildSessionCallback.class);
        mIkeSessionStateMachine.openChildSession(mChildSessionParams, cb);

        mLooper.dispatchAll();
        assertTrue(
                mIkeSessionStateMachine.getCurrentState()
                        instanceof IkeSessionStateMachine.ChildProcedureOngoing);
        verify(mMockLocalRequestWakelock).acquire();
        verify(mMockLocalRequestWakelock).release();
    }

    @Test
    public void testQuitClearAllLocalReqWakeLocks() throws Exception {
        final int localReqCnt = 3;
        setupIdleStateMachine();

        // Leave a Idle state so that the LocalRequest won't be executed
        mIkeSessionStateMachine.sendMessage(
                IkeSessionStateMachine.CMD_FORCE_TRANSITION, mIkeSessionStateMachine.mReceiving);
        mLooper.dispatchAll();

        // Only in test that all local requests will get the same WakeLock instance but in function
        // code each local request will have a separate WakeLock.
        for (int i = 0; i < localReqCnt; i++) {
            mIkeSessionStateMachine.openChildSession(
                    mChildSessionParams, mock(ChildSessionCallback.class));
        }
        mLooper.dispatchAll();
        verify(mMockLocalRequestWakelock, times(localReqCnt)).acquire();

        mIkeSessionStateMachine.killSession();
        mLooper.dispatchAll();
        verify(mMockLocalRequestWakelock, times(localReqCnt)).release();
    }
}
