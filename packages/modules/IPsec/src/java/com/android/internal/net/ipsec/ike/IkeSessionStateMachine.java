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
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_CHILD_SA_NOT_FOUND;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_INVALID_SYNTAX;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_NO_ADDITIONAL_SAS;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ERROR_TYPE_TEMPORARY_FAILURE;
import static android.net.ipsec.ike.exceptions.IkeProtocolException.ErrorType;
import static android.os.PowerManager.PARTIAL_WAKE_LOCK;

import static com.android.internal.net.ipsec.ike.message.IkeConfigPayload.CONFIG_TYPE_REPLY;
import static com.android.internal.net.ipsec.ike.message.IkeHeader.EXCHANGE_TYPE_INFORMATIONAL;
import static com.android.internal.net.ipsec.ike.message.IkeMessage.DECODE_STATUS_OK;
import static com.android.internal.net.ipsec.ike.message.IkeMessage.DECODE_STATUS_PARTIAL;
import static com.android.internal.net.ipsec.ike.message.IkeMessage.DECODE_STATUS_PROTECTED_ERROR;
import static com.android.internal.net.ipsec.ike.message.IkeMessage.DECODE_STATUS_UNPROTECTED_ERROR;
import static com.android.internal.net.ipsec.ike.message.IkeNotifyPayload.NOTIFY_TYPE_EAP_ONLY_AUTHENTICATION;
import static com.android.internal.net.ipsec.ike.message.IkeNotifyPayload.NOTIFY_TYPE_IKEV2_FRAGMENTATION_SUPPORTED;
import static com.android.internal.net.ipsec.ike.message.IkeNotifyPayload.NOTIFY_TYPE_NAT_DETECTION_DESTINATION_IP;
import static com.android.internal.net.ipsec.ike.message.IkeNotifyPayload.NOTIFY_TYPE_NAT_DETECTION_SOURCE_IP;
import static com.android.internal.net.ipsec.ike.message.IkeNotifyPayload.NOTIFY_TYPE_REKEY_SA;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_AUTH;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_CP;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_DELETE;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_EAP;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_NOTIFY;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_SA;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_TS_INITIATOR;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_TS_RESPONDER;
import static com.android.internal.net.ipsec.ike.message.IkePayload.PAYLOAD_TYPE_VENDOR;
import static com.android.internal.net.ipsec.ike.utils.IkeAlarmReceiver.ACTION_DELETE_CHILD;
import static com.android.internal.net.ipsec.ike.utils.IkeAlarmReceiver.ACTION_DELETE_IKE;
import static com.android.internal.net.ipsec.ike.utils.IkeAlarmReceiver.ACTION_DPD;
import static com.android.internal.net.ipsec.ike.utils.IkeAlarmReceiver.ACTION_KEEPALIVE;
import static com.android.internal.net.ipsec.ike.utils.IkeAlarmReceiver.ACTION_REKEY_CHILD;
import static com.android.internal.net.ipsec.ike.utils.IkeAlarmReceiver.ACTION_REKEY_IKE;

import android.annotation.IntDef;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.IpSecManager;
import android.net.IpSecManager.ResourceUnavailableException;
import android.net.IpSecManager.SpiUnavailableException;
import android.net.IpSecManager.UdpEncapsulationSocket;
import android.net.Network;
import android.net.ipsec.ike.ChildSessionCallback;
import android.net.ipsec.ike.ChildSessionParams;
import android.net.ipsec.ike.IkeSaProposal;
import android.net.ipsec.ike.IkeSessionCallback;
import android.net.ipsec.ike.IkeSessionConfiguration;
import android.net.ipsec.ike.IkeSessionConnectionInfo;
import android.net.ipsec.ike.IkeSessionParams;
import android.net.ipsec.ike.IkeSessionParams.IkeAuthConfig;
import android.net.ipsec.ike.IkeSessionParams.IkeAuthDigitalSignLocalConfig;
import android.net.ipsec.ike.IkeSessionParams.IkeAuthDigitalSignRemoteConfig;
import android.net.ipsec.ike.IkeSessionParams.IkeAuthPskConfig;
import android.net.ipsec.ike.exceptions.IkeException;
import android.net.ipsec.ike.exceptions.IkeInternalException;
import android.net.ipsec.ike.exceptions.IkeProtocolException;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.PowerManager;
import android.os.SystemClock;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.util.LongSparseArray;
import android.util.Pair;
import android.util.SparseArray;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.net.eap.EapAuthenticator;
import com.android.internal.net.eap.IEapCallback;
import com.android.internal.net.ipsec.ike.ChildSessionStateMachine.CreateChildSaHelper;
import com.android.internal.net.ipsec.ike.IkeLocalRequestScheduler.ChildLocalRequest;
import com.android.internal.net.ipsec.ike.IkeLocalRequestScheduler.IkeLocalRequest;
import com.android.internal.net.ipsec.ike.IkeLocalRequestScheduler.LocalRequest;
import com.android.internal.net.ipsec.ike.SaRecord.IkeSaRecord;
import com.android.internal.net.ipsec.ike.SaRecord.SaLifetimeAlarmScheduler;
import com.android.internal.net.ipsec.ike.crypto.IkeCipher;
import com.android.internal.net.ipsec.ike.crypto.IkeMacIntegrity;
import com.android.internal.net.ipsec.ike.crypto.IkeMacPrf;
import com.android.internal.net.ipsec.ike.exceptions.AuthenticationFailedException;
import com.android.internal.net.ipsec.ike.exceptions.InvalidKeException;
import com.android.internal.net.ipsec.ike.exceptions.InvalidSyntaxException;
import com.android.internal.net.ipsec.ike.exceptions.NoValidProposalChosenException;
import com.android.internal.net.ipsec.ike.keepalive.IkeNattKeepalive;
import com.android.internal.net.ipsec.ike.message.IkeAuthDigitalSignPayload;
import com.android.internal.net.ipsec.ike.message.IkeAuthPayload;
import com.android.internal.net.ipsec.ike.message.IkeAuthPskPayload;
import com.android.internal.net.ipsec.ike.message.IkeCertPayload;
import com.android.internal.net.ipsec.ike.message.IkeCertX509CertPayload;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload;
import com.android.internal.net.ipsec.ike.message.IkeConfigPayload.ConfigAttribute;
import com.android.internal.net.ipsec.ike.message.IkeDeletePayload;
import com.android.internal.net.ipsec.ike.message.IkeEapPayload;
import com.android.internal.net.ipsec.ike.message.IkeHeader;
import com.android.internal.net.ipsec.ike.message.IkeHeader.ExchangeType;
import com.android.internal.net.ipsec.ike.message.IkeIdPayload;
import com.android.internal.net.ipsec.ike.message.IkeInformationalPayload;
import com.android.internal.net.ipsec.ike.message.IkeKePayload;
import com.android.internal.net.ipsec.ike.message.IkeMessage;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResult;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResultError;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResultOk;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResultPartial;
import com.android.internal.net.ipsec.ike.message.IkeMessage.DecodeResultProtectedError;
import com.android.internal.net.ipsec.ike.message.IkeNoncePayload;
import com.android.internal.net.ipsec.ike.message.IkeNotifyPayload;
import com.android.internal.net.ipsec.ike.message.IkePayload;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IkeProposal;
import com.android.internal.net.ipsec.ike.message.IkeVendorPayload;
import com.android.internal.net.ipsec.ike.utils.IkeAlarmReceiver;
import com.android.internal.net.ipsec.ike.utils.IkeSecurityParameterIndex;
import com.android.internal.net.ipsec.ike.utils.IkeSpiGenerator;
import com.android.internal.net.ipsec.ike.utils.IpSecSpiGenerator;
import com.android.internal.net.ipsec.ike.utils.RandomnessFactory;
import com.android.internal.net.ipsec.ike.utils.Retransmitter;
import com.android.internal.util.State;

import java.io.FileDescriptor;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.security.GeneralSecurityException;
import java.security.cert.TrustAnchor;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * IkeSessionStateMachine tracks states and manages exchanges of this IKE session.
 *
 * <p>IkeSessionStateMachine has two types of states. One type are states where there is no ongoing
 * procedure affecting IKE session (non-procedure state), including Initial, Idle and Receiving. All
 * other states are "procedure" states which are named as follows:
 *
 * <pre>
 * State Name = [Procedure Type] + [Exchange Initiator] + [Exchange Type].
 * - An IKE procedure consists of one or two IKE exchanges:
 *      Procedure Type = {CreateIke | DeleteIke | Info | RekeyIke | SimulRekeyIke}.
 * - Exchange Initiator indicates whether local or remote peer is the exchange initiator:
 *      Exchange Initiator = {Local | Remote}
 * - Exchange type defines the function of this exchange. To make it more descriptive, we separate
 *      Delete Exchange from generic Informational Exchange:
 *      Exchange Type = {IkeInit | IkeAuth | Create | Delete | Info}
 * </pre>
 */
public class IkeSessionStateMachine extends AbstractSessionStateMachine {
    // Package private
    static final String TAG = "IkeSessionStateMachine";

    @VisibleForTesting static final String BUSY_WAKE_LOCK_TAG = "mBusyWakeLock";

    // TODO: b/140579254 Allow users to configure fragment size.

    private static final Object IKE_SESSION_LOCK = new Object();

    @GuardedBy("IKE_SESSION_LOCK")
    private static final HashMap<Context, Set<IkeSessionStateMachine>> sContextToIkeSmMap =
            new HashMap<>();

    /** Alarm receiver that will be shared by all IkeSessionStateMachine */
    private static final IkeAlarmReceiver sIkeAlarmReceiver = new IkeAlarmReceiver();

    /** Intent filter for all Intents that should be received by sIkeAlarmReceiver */
    private static final IntentFilter sIntentFilter = new IntentFilter();

    static {
        sIntentFilter.addAction(ACTION_DELETE_CHILD);
        sIntentFilter.addAction(ACTION_DELETE_IKE);
        sIntentFilter.addAction(ACTION_DPD);
        sIntentFilter.addAction(ACTION_REKEY_CHILD);
        sIntentFilter.addAction(ACTION_REKEY_IKE);
        sIntentFilter.addAction(ACTION_KEEPALIVE);
    }

    private static final AtomicInteger sIkeSessionIdGenerator = new AtomicInteger();

    // Bundle key for remote IKE SPI. Package private
    @VisibleForTesting static final String BUNDLE_KEY_IKE_REMOTE_SPI = "BUNDLE_KEY_IKE_REMOTE_SPI";
    // Bundle key for remote Child SPI. Package private
    @VisibleForTesting
    static final String BUNDLE_KEY_CHILD_REMOTE_SPI = "BUNDLE_KEY_CHILD_REMOTE_SPI";

    // Default fragment size in bytes.
    @VisibleForTesting static final int DEFAULT_FRAGMENT_SIZE = 1280;

    // Close IKE Session when all responses during this time were TEMPORARY_FAILURE(s). This
    // indicates that something has gone wrong, and we are out of sync.
    @VisibleForTesting
    static final long TEMP_FAILURE_RETRY_TIMEOUT_MS = TimeUnit.MINUTES.toMillis(5L);

    @VisibleForTesting static final int NATT_KEEPALIVE_DELAY_SECONDS = 10;

    // Package private IKE exchange subtypes describe the specific function of a IKE
    // request/response exchange. It helps IkeSessionStateMachine to do message validation according
    // to the subtype specific rules.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        IKE_EXCHANGE_SUBTYPE_INVALID,
        IKE_EXCHANGE_SUBTYPE_IKE_INIT,
        IKE_EXCHANGE_SUBTYPE_IKE_AUTH,
        IKE_EXCHANGE_SUBTYPE_DELETE_IKE,
        IKE_EXCHANGE_SUBTYPE_DELETE_CHILD,
        IKE_EXCHANGE_SUBTYPE_REKEY_IKE,
        IKE_EXCHANGE_SUBTYPE_REKEY_CHILD,
        IKE_EXCHANGE_SUBTYPE_GENERIC_INFO
    })
    @interface IkeExchangeSubType {}

    static final int IKE_EXCHANGE_SUBTYPE_INVALID = 0;
    static final int IKE_EXCHANGE_SUBTYPE_IKE_INIT = 1;
    static final int IKE_EXCHANGE_SUBTYPE_IKE_AUTH = 2;
    static final int IKE_EXCHANGE_SUBTYPE_CREATE_CHILD = 3;
    static final int IKE_EXCHANGE_SUBTYPE_DELETE_IKE = 4;
    static final int IKE_EXCHANGE_SUBTYPE_DELETE_CHILD = 5;
    static final int IKE_EXCHANGE_SUBTYPE_REKEY_IKE = 6;
    static final int IKE_EXCHANGE_SUBTYPE_REKEY_CHILD = 7;
    static final int IKE_EXCHANGE_SUBTYPE_GENERIC_INFO = 8;

    private static final SparseArray<String> EXCHANGE_SUBTYPE_TO_STRING;

    static {
        EXCHANGE_SUBTYPE_TO_STRING = new SparseArray<>();
        EXCHANGE_SUBTYPE_TO_STRING.put(IKE_EXCHANGE_SUBTYPE_INVALID, "Invalid");
        EXCHANGE_SUBTYPE_TO_STRING.put(IKE_EXCHANGE_SUBTYPE_IKE_INIT, "IKE INIT");
        EXCHANGE_SUBTYPE_TO_STRING.put(IKE_EXCHANGE_SUBTYPE_IKE_AUTH, "IKE AUTH");
        EXCHANGE_SUBTYPE_TO_STRING.put(IKE_EXCHANGE_SUBTYPE_CREATE_CHILD, "Create Child");
        EXCHANGE_SUBTYPE_TO_STRING.put(IKE_EXCHANGE_SUBTYPE_DELETE_IKE, "Delete IKE");
        EXCHANGE_SUBTYPE_TO_STRING.put(IKE_EXCHANGE_SUBTYPE_DELETE_CHILD, "Delete Child");
        EXCHANGE_SUBTYPE_TO_STRING.put(IKE_EXCHANGE_SUBTYPE_REKEY_IKE, "Rekey IKE");
        EXCHANGE_SUBTYPE_TO_STRING.put(IKE_EXCHANGE_SUBTYPE_REKEY_CHILD, "Rekey Child");
        EXCHANGE_SUBTYPE_TO_STRING.put(IKE_EXCHANGE_SUBTYPE_GENERIC_INFO, "Generic Info");
    }

    /** Package private signals accessible for testing code. */
    private static final int CMD_GENERAL_BASE = CMD_PRIVATE_BASE;

    /** Receive encoded IKE packet on IkeSessionStateMachine. */
    static final int CMD_RECEIVE_IKE_PACKET = CMD_GENERAL_BASE + 1;
    /** Receive encoded IKE packet with unrecognized IKE SPI on IkeSessionStateMachine. */
    static final int CMD_RECEIVE_PACKET_INVALID_IKE_SPI = CMD_GENERAL_BASE + 2;
    /** Receive an remote request for a Child procedure. */
    static final int CMD_RECEIVE_REQUEST_FOR_CHILD = CMD_GENERAL_BASE + 3;
    /** Receive payloads from Child Session for building an outbound IKE message. */
    static final int CMD_OUTBOUND_CHILD_PAYLOADS_READY = CMD_GENERAL_BASE + 4;
    /** A Child Session has finished its procedure. */
    static final int CMD_CHILD_PROCEDURE_FINISHED = CMD_GENERAL_BASE + 5;
    /** Send request/response payloads to ChildSessionStateMachine for further processing. */
    static final int CMD_HANDLE_FIRST_CHILD_NEGOTIATION = CMD_GENERAL_BASE + 6;
    /** Receive a local request to execute from the scheduler */
    static final int CMD_EXECUTE_LOCAL_REQ = CMD_GENERAL_BASE + 7;
    /** Trigger a retransmission. */
    public static final int CMD_RETRANSMIT = CMD_GENERAL_BASE + 8;
    /** Send EAP request payloads to EapAuthenticator for further processing. */
    static final int CMD_EAP_START_EAP_AUTH = CMD_GENERAL_BASE + 9;
    /** Send the outbound IKE-wrapped EAP-Response message. */
    static final int CMD_EAP_OUTBOUND_MSG_READY = CMD_GENERAL_BASE + 10;
    /** Proxy to IkeSessionStateMachine handler to notify of errors */
    static final int CMD_EAP_ERRORED = CMD_GENERAL_BASE + 11;
    /** Proxy to IkeSessionStateMachine handler to notify of failures */
    static final int CMD_EAP_FAILED = CMD_GENERAL_BASE + 12;
    /** Proxy to IkeSessionStateMachine handler to notify of success, to continue to post-auth */
    static final int CMD_EAP_FINISH_EAP_AUTH = CMD_GENERAL_BASE + 14;
    /** Alarm goes off for a scheduled event, check {@link Message.arg2} for event type */
    static final int CMD_ALARM_FIRED = CMD_GENERAL_BASE + 15;
    /** Send keepalive packet */
    static final int CMD_SEND_KEEPALIVE = CMD_GENERAL_BASE + 16;
    /** Force state machine to a target state for testing purposes. */
    static final int CMD_FORCE_TRANSITION = CMD_GENERAL_BASE + 99;

    static final int CMD_IKE_LOCAL_REQUEST_BASE = CMD_GENERAL_BASE + CMD_CATEGORY_SIZE;
    static final int CMD_LOCAL_REQUEST_CREATE_IKE = CMD_IKE_LOCAL_REQUEST_BASE + 1;
    static final int CMD_LOCAL_REQUEST_DELETE_IKE = CMD_IKE_LOCAL_REQUEST_BASE + 2;
    static final int CMD_LOCAL_REQUEST_REKEY_IKE = CMD_IKE_LOCAL_REQUEST_BASE + 3;
    static final int CMD_LOCAL_REQUEST_INFO = CMD_IKE_LOCAL_REQUEST_BASE + 4;
    static final int CMD_LOCAL_REQUEST_DPD = CMD_IKE_LOCAL_REQUEST_BASE + 5;

    private static final SparseArray<String> CMD_TO_STR;

    static {
        CMD_TO_STR = new SparseArray<>();
        CMD_TO_STR.put(CMD_RECEIVE_IKE_PACKET, "Rcv packet");
        CMD_TO_STR.put(CMD_RECEIVE_PACKET_INVALID_IKE_SPI, "Rcv invalid IKE SPI");
        CMD_TO_STR.put(CMD_RECEIVE_REQUEST_FOR_CHILD, "Rcv Child request");
        CMD_TO_STR.put(CMD_OUTBOUND_CHILD_PAYLOADS_READY, "Out child payloads ready");
        CMD_TO_STR.put(CMD_CHILD_PROCEDURE_FINISHED, "Child procedure finished");
        CMD_TO_STR.put(CMD_HANDLE_FIRST_CHILD_NEGOTIATION, "Negotiate first Child");
        CMD_TO_STR.put(CMD_EXECUTE_LOCAL_REQ, "Execute local request");
        CMD_TO_STR.put(CMD_RETRANSMIT, "Retransmit");
        CMD_TO_STR.put(CMD_EAP_START_EAP_AUTH, "Start EAP");
        CMD_TO_STR.put(CMD_EAP_OUTBOUND_MSG_READY, "EAP outbound msg ready");
        CMD_TO_STR.put(CMD_EAP_ERRORED, "EAP errored");
        CMD_TO_STR.put(CMD_EAP_FAILED, "EAP failed");
        CMD_TO_STR.put(CMD_EAP_FINISH_EAP_AUTH, "Finish EAP");
        CMD_TO_STR.put(CMD_ALARM_FIRED, "Alarm Fired");
        CMD_TO_STR.put(CMD_LOCAL_REQUEST_CREATE_IKE, "Create IKE");
        CMD_TO_STR.put(CMD_LOCAL_REQUEST_DELETE_IKE, "Delete IKE");
        CMD_TO_STR.put(CMD_LOCAL_REQUEST_REKEY_IKE, "Rekey IKE");
        CMD_TO_STR.put(CMD_LOCAL_REQUEST_INFO, "Info");
        CMD_TO_STR.put(CMD_LOCAL_REQUEST_DPD, "DPD");
    }

    /** Package */
    @VisibleForTesting final IkeSessionParams mIkeSessionParams;

    /** Map that stores all IkeSaRecords, keyed by locally generated IKE SPI. */
    private final LongSparseArray<IkeSaRecord> mLocalSpiToIkeSaRecordMap;
    /**
     * Map that stores all ChildSessionStateMachines, keyed by remotely generated Child SPI for
     * sending IPsec packet. Different SPIs may point to the same ChildSessionStateMachine if this
     * Child Session is doing Rekey.
     */
    private final SparseArray<ChildSessionStateMachine> mRemoteSpiToChildSessionMap;

    private final int mIkeSessionId;
    private final Context mContext;
    private final IpSecManager mIpSecManager;
    private final AlarmManager mAlarmManager;
    private final IkeLocalRequestScheduler mScheduler;
    private final IkeSessionCallback mIkeSessionCallback;
    private final IkeEapAuthenticatorFactory mEapAuthenticatorFactory;
    private final TempFailureHandler mTempFailHandler;

    /** Package private */
    @VisibleForTesting final RandomnessFactory mRandomFactory;

    /**
     * mIkeSpiGenerator will be used by all IKE SA creations in this IKE Session to avoid SPI
     * collision in test mode.
     */
    private final IkeSpiGenerator mIkeSpiGenerator;
    /**
     * mIpSecSpiGenerator will be shared by all Child Sessions under this IKE Session to avoid SPI
     * collision in test mode.
     */
    private final IpSecSpiGenerator mIpSecSpiGenerator;

    /** Ensures that the system does not go to sleep in the middle of an exchange. */
    private final PowerManager.WakeLock mBusyWakeLock;

    @VisibleForTesting
    @GuardedBy("mChildCbToSessions")
    final HashMap<ChildSessionCallback, ChildSessionStateMachine> mChildCbToSessions =
            new HashMap<>();

    /** Peer-selected DH group to use. Defaults to first proposed DH group in first SA proposal. */
    @VisibleForTesting int mPeerSelectedDhGroup;

    /**
     * Package private socket that sends and receives encoded IKE message. Initialized in Initial
     * State.
     */
    @VisibleForTesting IkeSocket mIkeSocket;

    /** Local address assigned on device. Initialized in Initial State. */
    @VisibleForTesting InetAddress mLocalAddress;
    /** Remote address configured by users. Initialized in Initial State. */
    @VisibleForTesting InetAddress mRemoteAddress;
    /** Local port assigned on device. Initialized in Initial State. */
    @VisibleForTesting int mLocalPort;

    /** Indicates if local node is behind a NAT. */
    @VisibleForTesting boolean mIsLocalBehindNat;
    /** Indicates if remote node is behind a NAT. */
    @VisibleForTesting boolean mIsRemoteBehindNat;
    /** NATT keepalive scheduler. Initialized when a NAT is detected */
    @VisibleForTesting IkeNattKeepalive mIkeNattKeepalive;

    /** Indicates if both sides support fragmentation. Set in IKE INIT */
    @VisibleForTesting boolean mSupportFragment;

    /** Package private IkeSaProposal that represents the negotiated IKE SA proposal. */
    @VisibleForTesting IkeSaProposal mSaProposal;

    @VisibleForTesting IkeCipher mIkeCipher;
    @VisibleForTesting IkeMacIntegrity mIkeIntegrity;
    @VisibleForTesting IkeMacPrf mIkePrf;

    // FIXME: b/131265898 Pass these parameters from CreateIkeLocalIkeInit to CreateIkeLocalIkeAuth
    // as entry data when Android StateMachine can support that.
    @VisibleForTesting byte[] mIkeInitRequestBytes;
    @VisibleForTesting byte[] mIkeInitResponseBytes;
    @VisibleForTesting IkeNoncePayload mIkeInitNoncePayload;
    @VisibleForTesting IkeNoncePayload mIkeRespNoncePayload;
    @VisibleForTesting List<byte[]> mRemoteVendorIds = new ArrayList<>();
    @VisibleForTesting List<Integer> mEnabledExtensions = new ArrayList<>();

    // FIXME: b/131265898 Pass these parameters from CreateIkeLocalIkeAuth through to
    // CreateIkeLocalIkeAuthPostEap as entry data when Android StateMachine can support that.
    @VisibleForTesting IkeIdPayload mInitIdPayload;
    @VisibleForTesting IkeIdPayload mRespIdPayload;
    @VisibleForTesting List<IkePayload> mFirstChildReqList;

    // FIXME: b/131265898 Move into CreateIkeLocalIkeAuth, and pass through to
    // CreateIkeLocalIkeAuthPostEap once passing entry data is supported
    private ChildSessionParams mFirstChildSessionParams;
    private ChildSessionCallback mFirstChildCallbacks;

    /** Package */
    @VisibleForTesting IkeSaRecord mCurrentIkeSaRecord;
    /** Package */
    @VisibleForTesting IkeSaRecord mLocalInitNewIkeSaRecord;
    /** Package */
    @VisibleForTesting IkeSaRecord mRemoteInitNewIkeSaRecord;

    /** Package */
    @VisibleForTesting IkeSaRecord mIkeSaRecordSurviving;
    /** Package */
    @VisibleForTesting IkeSaRecord mIkeSaRecordAwaitingLocalDel;
    /** Package */
    @VisibleForTesting IkeSaRecord mIkeSaRecordAwaitingRemoteDel;

    // States
    @VisibleForTesting final State mInitial = new Initial();
    @VisibleForTesting final State mIdle = new Idle();
    @VisibleForTesting final State mChildProcedureOngoing = new ChildProcedureOngoing();
    @VisibleForTesting final State mReceiving = new Receiving();
    @VisibleForTesting final State mCreateIkeLocalIkeInit = new CreateIkeLocalIkeInit();
    @VisibleForTesting final State mCreateIkeLocalIkeAuth = new CreateIkeLocalIkeAuth();
    @VisibleForTesting final State mCreateIkeLocalIkeAuthInEap = new CreateIkeLocalIkeAuthInEap();

    @VisibleForTesting
    final State mCreateIkeLocalIkeAuthPostEap = new CreateIkeLocalIkeAuthPostEap();

    @VisibleForTesting final State mRekeyIkeLocalCreate = new RekeyIkeLocalCreate();
    @VisibleForTesting final State mSimulRekeyIkeLocalCreate = new SimulRekeyIkeLocalCreate();

    @VisibleForTesting
    final State mSimulRekeyIkeLocalDeleteRemoteDelete = new SimulRekeyIkeLocalDeleteRemoteDelete();

    @VisibleForTesting final State mSimulRekeyIkeLocalDelete = new SimulRekeyIkeLocalDelete();
    @VisibleForTesting final State mSimulRekeyIkeRemoteDelete = new SimulRekeyIkeRemoteDelete();
    @VisibleForTesting final State mRekeyIkeLocalDelete = new RekeyIkeLocalDelete();
    @VisibleForTesting final State mRekeyIkeRemoteDelete = new RekeyIkeRemoteDelete();
    @VisibleForTesting final State mDeleteIkeLocalDelete = new DeleteIkeLocalDelete();
    @VisibleForTesting final State mDpdIkeLocalInfo = new DpdIkeLocalInfo();

    /** Constructor for testing. */
    @VisibleForTesting
    public IkeSessionStateMachine(
            Looper looper,
            Context context,
            IpSecManager ipSecManager,
            IkeSessionParams ikeParams,
            ChildSessionParams firstChildParams,
            Executor userCbExecutor,
            IkeSessionCallback ikeSessionCallback,
            ChildSessionCallback firstChildSessionCallback,
            IkeEapAuthenticatorFactory eapAuthenticatorFactory) {
        super(TAG, looper, userCbExecutor);

        synchronized (IKE_SESSION_LOCK) {
            if (!sContextToIkeSmMap.containsKey(context)) {
                // Pass in a Handler so #onReceive will run on the StateMachine thread
                context.registerReceiver(
                        sIkeAlarmReceiver,
                        sIntentFilter,
                        null /*broadcastPermission*/,
                        new Handler(looper));
                sContextToIkeSmMap.put(context, new HashSet<IkeSessionStateMachine>());
            }
            sContextToIkeSmMap.get(context).add(this);

            // TODO: Statically store the ikeSessionCallback to prevent user from providing the same
            // callback instance in the future
        }

        PowerManager pm = context.getSystemService(PowerManager.class);
        mBusyWakeLock = pm.newWakeLock(PARTIAL_WAKE_LOCK, TAG + BUSY_WAKE_LOCK_TAG);
        mBusyWakeLock.setReferenceCounted(false);

        mIkeSessionId = sIkeSessionIdGenerator.getAndIncrement();
        sIkeAlarmReceiver.registerIkeSession(mIkeSessionId, getHandler());

        mIkeSessionParams = ikeParams;
        mEapAuthenticatorFactory = eapAuthenticatorFactory;

        // SaProposals.Builder guarantees there is at least one SA proposal, and each SA proposal
        // has at least one DH group.
        mPeerSelectedDhGroup =
                mIkeSessionParams.getSaProposals().get(0).getDhGroupTransforms()[0].id;

        mTempFailHandler = new TempFailureHandler(looper);

        // There are at most three IkeSaRecords co-existing during simultaneous rekeying.
        mLocalSpiToIkeSaRecordMap = new LongSparseArray<>(3);
        mRemoteSpiToChildSessionMap = new SparseArray<>();

        mContext = context;
        mIpSecManager = ipSecManager;
        mAlarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);

        mRandomFactory = new RandomnessFactory(mContext, mIkeSessionParams.getNetwork());
        mIkeSpiGenerator = new IkeSpiGenerator(mRandomFactory);
        mIpSecSpiGenerator = new IpSecSpiGenerator(mIpSecManager, mRandomFactory);

        mIkeSessionCallback = ikeSessionCallback;

        mFirstChildSessionParams = firstChildParams;
        mFirstChildCallbacks = firstChildSessionCallback;
        registerChildSessionCallback(firstChildParams, firstChildSessionCallback, true);

        addState(mInitial);
        addState(mCreateIkeLocalIkeInit);
        addState(mCreateIkeLocalIkeAuth);
        addState(mCreateIkeLocalIkeAuthInEap);
        addState(mCreateIkeLocalIkeAuthPostEap);
        addState(mIdle);
        addState(mChildProcedureOngoing);
        addState(mReceiving);
        addState(mRekeyIkeLocalCreate);
        addState(mSimulRekeyIkeLocalCreate, mRekeyIkeLocalCreate);
        addState(mSimulRekeyIkeLocalDeleteRemoteDelete);
        addState(mSimulRekeyIkeLocalDelete, mSimulRekeyIkeLocalDeleteRemoteDelete);
        addState(mSimulRekeyIkeRemoteDelete, mSimulRekeyIkeLocalDeleteRemoteDelete);
        addState(mRekeyIkeLocalDelete);
        addState(mRekeyIkeRemoteDelete);
        addState(mDeleteIkeLocalDelete);
        addState(mDpdIkeLocalInfo);

        setInitialState(mInitial);

        // TODO: Find a way to make it safe to release WakeLock when #onNewProcedureReady is called
        mScheduler =
                new IkeLocalRequestScheduler(
                        localReq -> {
                            sendMessageAtFrontOfQueue(CMD_EXECUTE_LOCAL_REQ, localReq);
                        },
                        mContext);

        mBusyWakeLock.acquire();
        start();
    }

    /** Construct an instance of IkeSessionStateMachine. */
    public IkeSessionStateMachine(
            Looper looper,
            Context context,
            IpSecManager ipSecManager,
            IkeSessionParams ikeParams,
            ChildSessionParams firstChildParams,
            Executor userCbExecutor,
            IkeSessionCallback ikeSessionCallback,
            ChildSessionCallback firstChildSessionCallback) {
        this(
                looper,
                context,
                ipSecManager,
                ikeParams,
                firstChildParams,
                userCbExecutor,
                ikeSessionCallback,
                firstChildSessionCallback,
                new IkeEapAuthenticatorFactory());
    }

    private boolean hasChildSessionCallback(ChildSessionCallback callback) {
        synchronized (mChildCbToSessions) {
            return mChildCbToSessions.containsKey(callback);
        }
    }

    /**
     * Synchronously builds and registers a child session.
     *
     * <p>Setup of the child state machines MUST be done in two stages to ensure that if an external
     * caller calls openChildSession and then calls closeChildSession before the state machine has
     * gotten a chance to negotiate the sessions, a valid callback mapping exists (and does not
     * throw an exception that the callback was not found).
     *
     * <p>In the edge case where a child creation fails, and deletes itself, all pending requests
     * will no longer find the session in the map. Assume it has errored/failed, and skip/ignore.
     * This is safe, as closeChildSession() (previously) validated that the callback was registered.
     */
    @VisibleForTesting
    void registerChildSessionCallback(
            ChildSessionParams childParams, ChildSessionCallback callbacks, boolean isFirstChild) {
        synchronized (mChildCbToSessions) {
            if (!isFirstChild && getCurrentState() == null) {
                throw new IllegalStateException(
                        "Request rejected because IKE Session is being closed. ");
            }

            mChildCbToSessions.put(
                    callbacks,
                    ChildSessionStateMachineFactory.makeChildSessionStateMachine(
                            getHandler().getLooper(),
                            mContext,
                            mIkeSessionId,
                            mAlarmManager,
                            mRandomFactory,
                            mIpSecSpiGenerator,
                            childParams,
                            mUserCbExecutor,
                            callbacks,
                            new ChildSessionSmCallback()));
        }
    }

    /** Initiates IKE setup procedure. */
    public void openSession() {
        sendMessage(
                CMD_LOCAL_REQUEST_CREATE_IKE, new IkeLocalRequest(CMD_LOCAL_REQUEST_CREATE_IKE));
    }

    /** Schedules a Create Child procedure. */
    public void openChildSession(
            ChildSessionParams childSessionParams, ChildSessionCallback childSessionCallback) {
        if (childSessionCallback == null) {
            throw new IllegalArgumentException("Child Session Callback must be provided");
        }

        if (hasChildSessionCallback(childSessionCallback)) {
            throw new IllegalArgumentException("Child Session Callback handle already registered");
        }

        registerChildSessionCallback(
                childSessionParams, childSessionCallback, false /*isFirstChild*/);
        sendMessage(
                CMD_LOCAL_REQUEST_CREATE_CHILD,
                new ChildLocalRequest(
                        CMD_LOCAL_REQUEST_CREATE_CHILD, childSessionCallback, childSessionParams));
    }

    /** Schedules a Delete Child procedure. */
    public void closeChildSession(ChildSessionCallback childSessionCallback) {
        if (childSessionCallback == null) {
            throw new IllegalArgumentException("Child Session Callback must be provided");
        }

        if (!hasChildSessionCallback(childSessionCallback)) {
            throw new IllegalArgumentException("Child Session Callback handle not registered");
        }

        sendMessage(
                CMD_LOCAL_REQUEST_DELETE_CHILD,
                new ChildLocalRequest(CMD_LOCAL_REQUEST_DELETE_CHILD, childSessionCallback, null));
    }

    /** Initiates Delete IKE procedure. */
    public void closeSession() {
        sendMessage(
                CMD_LOCAL_REQUEST_DELETE_IKE, new IkeLocalRequest(CMD_LOCAL_REQUEST_DELETE_IKE));
    }

    /** Forcibly close IKE Session. */
    public void killSession() {
        // TODO(b/150327466): Notify remote serve when there is no outstanding request

        closeAllSaRecords(false /*expectSaClosed*/);
        executeUserCallback(
                () -> {
                    mIkeSessionCallback.onClosed();
                });

        quitNow();
    }

    private void scheduleRetry(LocalRequest localRequest) {
        sendMessageDelayed(localRequest.procedureType, localRequest, RETRY_INTERVAL_MS);
    }

    // TODO: Support initiating Delete IKE exchange when IKE SA expires

    // TODO: Add interfaces to initiate IKE exchanges.

    /**
     * This class is for handling temporary failure.
     *
     * <p>Receiving a TEMPORARY_FAILURE is caused by a temporary condition. IKE Session should be
     * closed if it continues to receive this error after several minutes.
     */
    @VisibleForTesting
    class TempFailureHandler extends Handler {
        private static final int TEMP_FAILURE_RETRY_TIMEOUT = 1;

        private boolean mTempFailureReceived = false;

        TempFailureHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            if (msg.what == TEMP_FAILURE_RETRY_TIMEOUT) {
                IOException error =
                        new IOException(
                                "Kept receiving TEMPORARY_FAILURE error. State information is out"
                                        + " of sync.");
                executeUserCallback(
                        () -> {
                            mIkeSessionCallback.onClosedExceptionally(
                                    new IkeInternalException(error));
                        });
                loge("Fatal error", error);

                closeAllSaRecords(false /*expectSaClosed*/);
                quitNow();
            } else {
                logWtf("Unknown message.what: " + msg.what);
            }
        }

        /**
         * Schedule temporary failure timeout.
         *
         * <p>Caller of this method is responsible for scheduling retry of the rejected request.
         */
        public void handleTempFailure() {
            logd("TempFailureHandler: Receive TEMPORARY FAILURE");

            if (!mTempFailureReceived) {
                sendEmptyMessageDelayed(TEMP_FAILURE_RETRY_TIMEOUT, TEMP_FAILURE_RETRY_TIMEOUT_MS);
                mTempFailureReceived = true;
            }
        }

        /** Stop tracking temporary condition when request was not rejected by TEMPORARY_FAILURE. */
        public void reset() {
            logd("TempFailureHandler: Reset Temporary failure retry timeout");
            removeMessages(TEMP_FAILURE_RETRY_TIMEOUT);
            mTempFailureReceived = false;
        }
    }

    // TODO: Add methods for building and validating general Informational packet.

    /** Switch to a new IKE socket due to NAT detection, or an underlying network change. */
    private void switchToIkeSocket(long localSpi, IkeSocket newSocket) {
        newSocket.registerIke(localSpi, this);
        mIkeSocket.unregisterIke(localSpi);
        mIkeSocket.releaseReference(this);
        mIkeSocket = newSocket;
    }

    @VisibleForTesting
    void addIkeSaRecord(IkeSaRecord record) {
        mLocalSpiToIkeSaRecordMap.put(record.getLocalSpi(), record);

        // In IKE_INIT exchange, local SPI was registered with this IkeSessionStateMachine before
        // IkeSaRecord is created. Calling this method at the end of exchange will double-register
        // the SPI but it is safe because the key and value are not changed.
        mIkeSocket.registerIke(record.getLocalSpi(), this);
    }

    @VisibleForTesting
    void removeIkeSaRecord(IkeSaRecord record) {
        mIkeSocket.unregisterIke(record.getLocalSpi());
        mLocalSpiToIkeSaRecordMap.remove(record.getLocalSpi());
    }

    /**
     * Receive IKE packet from remote server.
     *
     * <p>This method is called synchronously from IkeSocket. It proxies the synchronous call as an
     * asynchronous job to the IkeSessionStateMachine handler.
     *
     * @param ikeHeader the decoded IKE header.
     * @param ikePacketBytes the byte array of the entire received IKE packet.
     */
    public void receiveIkePacket(IkeHeader ikeHeader, byte[] ikePacketBytes) {
        sendMessage(CMD_RECEIVE_IKE_PACKET, new ReceivedIkePacket(ikeHeader, ikePacketBytes));
    }

    /**
     * ReceivedIkePacket is a package private data container consists of decoded IkeHeader and
     * encoded IKE packet in a byte array.
     */
    static class ReceivedIkePacket {
        /** Decoded IKE header */
        public final IkeHeader ikeHeader;
        /** Entire encoded IKE message including IKE header */
        public final byte[] ikePacketBytes;

        ReceivedIkePacket(IkeHeader ikeHeader, byte[] ikePacketBytes) {
            this.ikeHeader = ikeHeader;
            this.ikePacketBytes = ikePacketBytes;
        }
    }

    /** Class to group parameters for negotiating the first Child SA. */
    private static class FirstChildNegotiationData {
        public final ChildSessionParams childSessionParams;
        public final ChildSessionCallback childSessionCallback;
        public final List<IkePayload> reqPayloads;
        public final List<IkePayload> respPayloads;

        FirstChildNegotiationData(
                ChildSessionParams childSessionParams,
                ChildSessionCallback childSessionCallback,
                List<IkePayload> reqPayloads,
                List<IkePayload> respPayloads) {
            this.childSessionParams = childSessionParams;
            this.childSessionCallback = childSessionCallback;
            this.reqPayloads = reqPayloads;
            this.respPayloads = respPayloads;
        }
    }

    /** Class to group parameters for building an outbound message for ChildSessions. */
    private static class ChildOutboundData {
        @ExchangeType public final int exchangeType;
        public final boolean isResp;
        public final List<IkePayload> payloadList;
        public final ChildSessionStateMachine childSession;

        ChildOutboundData(
                @ExchangeType int exchangeType,
                boolean isResp,
                List<IkePayload> payloadList,
                ChildSessionStateMachine childSession) {
            this.exchangeType = exchangeType;
            this.isResp = isResp;
            this.payloadList = payloadList;
            this.childSession = childSession;
        }
    }

    /** Callback for ChildSessionStateMachine to notify IkeSessionStateMachine. */
    @VisibleForTesting
    class ChildSessionSmCallback implements ChildSessionStateMachine.IChildSessionSmCallback {
        @Override
        public void onChildSaCreated(int remoteSpi, ChildSessionStateMachine childSession) {
            mRemoteSpiToChildSessionMap.put(remoteSpi, childSession);
        }

        @Override
        public void onChildSaDeleted(int remoteSpi) {
            mRemoteSpiToChildSessionMap.remove(remoteSpi);
        }

        @Override
        public void scheduleRetryLocalRequest(ChildLocalRequest childRequest) {
            scheduleRetry(childRequest);
        }

        @Override
        public void onOutboundPayloadsReady(
                @ExchangeType int exchangeType,
                boolean isResp,
                List<IkePayload> payloadList,
                ChildSessionStateMachine childSession) {
            sendMessage(
                    CMD_OUTBOUND_CHILD_PAYLOADS_READY,
                    new ChildOutboundData(exchangeType, isResp, payloadList, childSession));
        }

        @Override
        public void onProcedureFinished(ChildSessionStateMachine childSession) {
            if (getHandler() == null) {
                // If the state machine has quit (because IKE Session is being closed), do not send
                // any message.
                return;
            }

            sendMessage(CMD_CHILD_PROCEDURE_FINISHED, childSession);
        }

        @Override
        public void onChildSessionClosed(ChildSessionCallback userCallbacks) {
            synchronized (mChildCbToSessions) {
                mChildCbToSessions.remove(userCallbacks);
            }
        }

        @Override
        public void onFatalIkeSessionError(boolean needsNotifyRemote) {
            // TODO: If needsNotifyRemote is true, send a Delete IKE request and then kill the IKE
            // Session. Otherwise, directly kill the IKE Session.
        }
    }

    /** Top level state for handling uncaught exceptions for all subclasses. */
    abstract class ExceptionHandler extends ExceptionHandlerBase {
        @Override
        protected void cleanUpAndQuit(RuntimeException e) {
            // Clean up all SaRecords.
            closeAllSaRecords(false /*expectSaClosed*/);

            executeUserCallback(
                    () -> {
                        mIkeSessionCallback.onClosedExceptionally(new IkeInternalException(e));
                    });
            logWtf("Unexpected exception in " + getCurrentState().getName(), e);
            quitNow();
        }

        @Override
        protected String getCmdString(int cmd) {
            return CMD_TO_STR.get(cmd);
        }
    }

    /** Called when this StateMachine quits. */
    @Override
    protected void onQuitting() {
        // Clean up all SaRecords.
        closeAllSaRecords(true /*expectSaClosed*/);

        synchronized (mChildCbToSessions) {
            for (ChildSessionStateMachine child : mChildCbToSessions.values()) {
                // Fire asynchronous call for Child Sessions to do cleanup and remove itself
                // from the map.
                child.killSession();
            }
        }

        // Release IPsec SPIs if IKE Session is terminated before receiving the IKE AUTH response
        // that contains the first child SA proposal
        CreateChildSaHelper.releaseSpiResources(mFirstChildReqList);

        if (mIkeNattKeepalive != null) {
            mIkeNattKeepalive.stop();
        }

        if (mIkeSocket != null) {
            mIkeSocket.releaseReference(this);
        }

        sIkeAlarmReceiver.unregisterIkeSession(mIkeSessionId);

        synchronized (IKE_SESSION_LOCK) {
            Set<IkeSessionStateMachine> ikeSet = sContextToIkeSmMap.get(mContext);
            ikeSet.remove(this);
            if (ikeSet.isEmpty()) {
                mContext.unregisterReceiver(sIkeAlarmReceiver);
                sContextToIkeSmMap.remove(mContext);
            }
            // TODO: Remove the stored ikeSessionCallback
        }

        mBusyWakeLock.release();
        mScheduler.releaseAllLocalRequestWakeLocks();
    }

    private void closeAllSaRecords(boolean expectSaClosed) {
        closeIkeSaRecord(mCurrentIkeSaRecord, expectSaClosed);
        closeIkeSaRecord(mLocalInitNewIkeSaRecord, expectSaClosed);
        closeIkeSaRecord(mRemoteInitNewIkeSaRecord, expectSaClosed);

        mCurrentIkeSaRecord = null;
        mLocalInitNewIkeSaRecord = null;
        mRemoteInitNewIkeSaRecord = null;
    }

    private void closeIkeSaRecord(IkeSaRecord ikeSaRecord, boolean expectSaClosed) {
        if (ikeSaRecord == null) return;

        removeIkeSaRecord(ikeSaRecord);
        ikeSaRecord.close();

        if (!expectSaClosed) return;

        logWtf(
                "IkeSaRecord with local SPI: "
                        + ikeSaRecord.getLocalSpi()
                        + " is not correctly closed.");
    }

    private void handleIkeFatalError(Exception error) {
        IkeException ikeException =
                error instanceof IkeException
                        ? (IkeException) error
                        : new IkeInternalException(error);

        // Clean up all SaRecords.
        closeAllSaRecords(false /*expectSaClosed*/);
        executeUserCallback(
                () -> {
                    mIkeSessionCallback.onClosedExceptionally(ikeException);
                });
        loge("IKE Session fatal error in " + getCurrentState().getName(), ikeException);

        quitNow();
    }

    /** Initial state of IkeSessionStateMachine. */
    class Initial extends ExceptionHandler {
        @Override
        public void enterState() {
            try {
                Network network = mIkeSessionParams.getNetwork();

                // TODO(b/149954916): Do DNS resolution asynchronously and support resolving
                // multiple addresses.
                mRemoteAddress = network.getByName(mIkeSessionParams.getServerHostname());

                boolean isIpv4 = mRemoteAddress instanceof Inet4Address;
                if (isIpv4) {
                    mIkeSocket = IkeUdp4Socket.getInstance(network, IkeSessionStateMachine.this);
                } else {
                    mIkeSocket = IkeUdp6Socket.getInstance(network, IkeSessionStateMachine.this);
                }

                FileDescriptor sock =
                        Os.socket(
                                isIpv4 ? OsConstants.AF_INET : OsConstants.AF_INET6,
                                OsConstants.SOCK_DGRAM,
                                OsConstants.IPPROTO_UDP);
                network.bindSocket(sock);
                Os.connect(sock, mRemoteAddress, mIkeSocket.getIkeServerPort());
                InetSocketAddress localAddr = (InetSocketAddress) Os.getsockname(sock);
                mLocalAddress = localAddr.getAddress();
                mLocalPort = mIkeSocket.getLocalPort();
                Os.close(sock);
            } catch (ErrnoException | IOException e) {
                handleIkeFatalError(e);
            }
        }

        @Override
        public boolean processStateMessage(Message message) {
            switch (message.what) {
                case CMD_LOCAL_REQUEST_CREATE_IKE:
                    transitionTo(mCreateIkeLocalIkeInit);
                    return HANDLED;
                case CMD_FORCE_TRANSITION:
                    transitionTo((State) message.obj);
                    return HANDLED;
                default:
                    return NOT_HANDLED;
            }
        }
    }

    /**
     * Idle represents a state when there is no ongoing IKE exchange affecting established IKE SA.
     */
    class Idle extends LocalRequestQueuer {
        private PendingIntent mDpdIntent;

        // TODO (b/152236790): Add wakelock for awaiting LocalRequests and ongoing procedures.

        @Override
        public void enterState() {
            if (!mScheduler.readyForNextProcedure()) {
                mBusyWakeLock.release();
            }

            if (mDpdIntent == null) {
                long remoteIkeSpi = mCurrentIkeSaRecord.getRemoteSpi();
                mDpdIntent =
                        buildIkeAlarmIntent(
                                mContext,
                                ACTION_DPD,
                                getIntentIdentifier(remoteIkeSpi),
                                getIntentIkeSmMsg(CMD_LOCAL_REQUEST_DPD, remoteIkeSpi));
            }
            long dpdDelayMs = TimeUnit.SECONDS.toMillis(mIkeSessionParams.getDpdDelaySeconds());

            // Initiating DPD is a way to detect the aliveness of the remote server and also a
            // way to assert the aliveness of IKE library. Considering this, the alarm to trigger
            // DPD needs to go off even when device is in doze mode to decrease the chance the
            // remote server thinks IKE library is dead. Also, since DPD initiation is
            // time-critical, we need to use "setExact" to avoid the batching alarm delay which can
            // be at most 75% for the alarm timeout (@see AlarmManagerService#maxTriggerTime).
            // Please check AlarmManager#setExactAndAllowWhileIdle for more details.
            mAlarmManager.setExactAndAllowWhileIdle(
                    AlarmManager.ELAPSED_REALTIME_WAKEUP,
                    SystemClock.elapsedRealtime() + dpdDelayMs,
                    mDpdIntent);
            logd("DPD Alarm scheduled with DPD delay: " + dpdDelayMs + "ms");
        }

        @Override
        protected void exitState() {
            // #exitState is guaranteed to be invoked when quit() or quitNow() is called
            mAlarmManager.cancel(mDpdIntent);
            logd("DPD Alarm canceled");

            mBusyWakeLock.acquire();
        }

        @Override
        public boolean processStateMessage(Message message) {
            switch (message.what) {
                case CMD_RECEIVE_IKE_PACKET:
                    deferMessage(message);
                    transitionTo(mReceiving);
                    return HANDLED;

                case CMD_ALARM_FIRED:
                    handleFiredAlarm(message);
                    return HANDLED;

                case CMD_FORCE_TRANSITION: // Testing command
                    transitionTo((State) message.obj);
                    return HANDLED;

                case CMD_EXECUTE_LOCAL_REQ:
                    executeLocalRequest((LocalRequest) message.obj, message);
                    return HANDLED;

                default:
                    // Queue local requests, and trigger next procedure
                    if (isLocalRequest(message.what)) {
                        handleLocalRequest(message.what, (LocalRequest) message.obj);

                        // Synchronously calls through to the scheduler callback, which will
                        // post the CMD_EXECUTE_LOCAL_REQ to the front of the queue, ensuring
                        // it is always the next request processed.
                        mScheduler.readyForNextProcedure();
                        return HANDLED;
                    }
                    return NOT_HANDLED;
            }
        }

        private void executeLocalRequest(LocalRequest req, Message message) {
            req.releaseWakeLock();

            if (!isRequestForCurrentSa(req)) {
                logd("Request is for a deleted SA. Ignore it.");
                mScheduler.readyForNextProcedure();
                return;
            }

            switch (req.procedureType) {
                case CMD_LOCAL_REQUEST_REKEY_IKE:
                    transitionTo(mRekeyIkeLocalCreate);
                    break;
                case CMD_LOCAL_REQUEST_DELETE_IKE:
                    transitionTo(mDeleteIkeLocalDelete);
                    break;
                case CMD_LOCAL_REQUEST_DPD:
                    transitionTo(mDpdIkeLocalInfo);
                    break;
                case CMD_LOCAL_REQUEST_CREATE_CHILD: // fallthrough
                case CMD_LOCAL_REQUEST_REKEY_CHILD: // fallthrough
                case CMD_LOCAL_REQUEST_DELETE_CHILD:
                    deferMessage(message);
                    transitionTo(mChildProcedureOngoing);
                    break;
                default:
                    cleanUpAndQuit(
                            new IllegalStateException(
                                    "Invalid local request procedure type: " + req.procedureType));
            }
        }

        // When in Idle state, this IkeSessionStateMachine and all its ChildSessionStateMachines
        // only have one alive IKE/Child SA respectively. Returns true if this local request is for
        // the current IKE/Child SA, or false if the request is for a deleted SA.
        private boolean isRequestForCurrentSa(LocalRequest localRequest) {
            if (localRequest.isChildRequest()) {
                ChildLocalRequest req = (ChildLocalRequest) localRequest;
                if (req.remoteSpi == IkeLocalRequestScheduler.SPI_NOT_INCLUDED
                        || mRemoteSpiToChildSessionMap.get(req.remoteSpi) != null) {
                    return true;
                }
            } else {
                IkeLocalRequest req = (IkeLocalRequest) localRequest;
                if (req.remoteSpi == IkeLocalRequestScheduler.SPI_NOT_INCLUDED
                        || req.remoteSpi == mCurrentIkeSaRecord.getRemoteSpi()) {
                    return true;
                }
            }
            return false;
        }
    }

    private String getIntentIdentifier() {
        return TAG + "_" + mIkeSessionId;
    }

    private String getIntentIdentifier(long remoteIkeSpi) {
        return TAG + "_" + mIkeSessionId + "_" + remoteIkeSpi;
    }

    private Message getIntentIkeSmMsg(int localRequestType, long remoteIkeSpi) {
        Bundle spiBundle = new Bundle();
        spiBundle.putLong(BUNDLE_KEY_IKE_REMOTE_SPI, remoteIkeSpi);

        return obtainMessage(CMD_ALARM_FIRED, mIkeSessionId, localRequestType, spiBundle);
    }

    @VisibleForTesting
    SaLifetimeAlarmScheduler buildSaLifetimeAlarmScheduler(long remoteSpi) {
        PendingIntent deleteSaIntent =
                buildIkeAlarmIntent(
                        mContext,
                        ACTION_DELETE_IKE,
                        getIntentIdentifier(remoteSpi),
                        getIntentIkeSmMsg(CMD_LOCAL_REQUEST_DELETE_IKE, remoteSpi));
        PendingIntent rekeySaIntent =
                buildIkeAlarmIntent(
                        mContext,
                        ACTION_REKEY_IKE,
                        getIntentIdentifier(remoteSpi),
                        getIntentIkeSmMsg(CMD_LOCAL_REQUEST_REKEY_IKE, remoteSpi));

        return new SaLifetimeAlarmScheduler(
                mIkeSessionParams.getHardLifetimeMsInternal(),
                mIkeSessionParams.getSoftLifetimeMsInternal(),
                deleteSaIntent,
                rekeySaIntent,
                mAlarmManager);
    }

    // Package private. Accessible to ChildSessionStateMachine
    static PendingIntent buildIkeAlarmIntent(
            Context context, String intentAction, String intentId, Message ikeSmMsg) {
        Intent intent = new Intent(intentAction);
        intent.setIdentifier(intentId);
        intent.setPackage(context.getPackageName());

        Bundle bundle = new Bundle();
        bundle.putParcelable(IkeAlarmReceiver.PARCELABLE_NAME_IKE_SESSION_MSG, ikeSmMsg);
        intent.putExtras(bundle);

        return PendingIntent.getBroadcast(
                context, 0 /*requestCode unused*/, intent, 0 /*default flags*/);
    }

    /**
     * Gets IKE exchange subtype of a inbound IKE request message.
     *
     * <p>Knowing IKE exchange subtype of a inbound IKE request message helps IkeSessionStateMachine
     * to validate this request using the specific rule.
     *
     * <p>It is not allowed to obtain exchange subtype from a inbound response message for two
     * reasons. Firstly, the exchange subtype of a response message is the same with its
     * corresponding request message. Secondly, trying to get the exchange subtype from a response
     * message will easily fail when the response message contains only error notification payloads.
     *
     * @param ikeMessage inbound request IKE message to check.
     * @return IKE exchange subtype.
     */
    @IkeExchangeSubType
    private static int getIkeExchangeSubType(IkeMessage ikeMessage) {
        IkeHeader ikeHeader = ikeMessage.ikeHeader;
        if (ikeHeader.isResponseMsg) {
            throw new IllegalStateException("IKE Exchange subtype invalid for response messages.");
        }

        switch (ikeHeader.exchangeType) {
                // DPD omitted - should never be handled via handleRequestIkeMessage()
            case IkeHeader.EXCHANGE_TYPE_IKE_SA_INIT:
                return IKE_EXCHANGE_SUBTYPE_IKE_INIT;
            case IkeHeader.EXCHANGE_TYPE_IKE_AUTH:
                return IKE_EXCHANGE_SUBTYPE_IKE_AUTH;
            case IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA:
                // It is guaranteed in the decoding process that SA Payload has at least one SA
                // Proposal. Since Rekey IKE and Create Child (both initial creation and rekey
                // creation) will cause a collision, although the RFC 7296 does not prohibit one SA
                // Payload to contain both IKE proposals and Child proposals, containing two types
                // does not make sense. IKE libary will reply according to the first SA Proposal
                // type and ignore the other type.
                IkeSaPayload saPayload =
                        ikeMessage.getPayloadForType(
                                IkePayload.PAYLOAD_TYPE_SA, IkeSaPayload.class);
                if (saPayload == null) {
                    return IKE_EXCHANGE_SUBTYPE_INVALID;
                }

                // If the received message has both SA(IKE) Payload and Notify-Rekey Payload, IKE
                // library will treat it as a Rekey IKE request and ignore the Notify-Rekey
                // Payload to provide better interoperability.
                if (saPayload.proposalList.get(0).protocolId == IkePayload.PROTOCOL_ID_IKE) {
                    return IKE_EXCHANGE_SUBTYPE_REKEY_IKE;
                }

                // If a Notify-Rekey Payload is found, this message is for rekeying a Child SA.
                List<IkeNotifyPayload> notifyPayloads =
                        ikeMessage.getPayloadListForType(
                                IkePayload.PAYLOAD_TYPE_NOTIFY, IkeNotifyPayload.class);

                // It is checked during decoding that there is at most one Rekey notification
                // payload.
                for (IkeNotifyPayload notifyPayload : notifyPayloads) {
                    if (notifyPayload.notifyType == IkeNotifyPayload.NOTIFY_TYPE_REKEY_SA) {
                        return IKE_EXCHANGE_SUBTYPE_REKEY_CHILD;
                    }
                }

                return IKE_EXCHANGE_SUBTYPE_CREATE_CHILD;
            case IkeHeader.EXCHANGE_TYPE_INFORMATIONAL:
                List<IkeDeletePayload> deletePayloads =
                        ikeMessage.getPayloadListForType(
                                IkePayload.PAYLOAD_TYPE_DELETE, IkeDeletePayload.class);

                // If no Delete payload was found, this request is a generic informational request.
                if (deletePayloads.isEmpty()) return IKE_EXCHANGE_SUBTYPE_GENERIC_INFO;

                // IKEv2 protocol does not clearly disallow to have both a Delete IKE payload and a
                // Delete Child payload in one IKE message. In this case, IKE library will only
                // respond to the Delete IKE payload.
                for (IkeDeletePayload deletePayload : deletePayloads) {
                    if (deletePayload.protocolId == IkePayload.PROTOCOL_ID_IKE) {
                        return IKE_EXCHANGE_SUBTYPE_DELETE_IKE;
                    }
                }
                return IKE_EXCHANGE_SUBTYPE_DELETE_CHILD;
            default:
                throw new IllegalStateException(
                        "Unrecognized exchange type in the validated IKE header: "
                                + ikeHeader.exchangeType);
        }
    }

    // Sends the provided IkeMessage using the current IKE SA record
    @VisibleForTesting
    void sendEncryptedIkeMessage(IkeMessage msg) {
        sendEncryptedIkeMessage(mCurrentIkeSaRecord, msg);
    }

    // Sends the provided IkeMessage using the provided IKE SA record
    @VisibleForTesting
    void sendEncryptedIkeMessage(IkeSaRecord ikeSaRecord, IkeMessage msg) {
        byte[][] packetList =
                msg.encryptAndEncode(
                        mIkeIntegrity,
                        mIkeCipher,
                        ikeSaRecord,
                        mSupportFragment,
                        DEFAULT_FRAGMENT_SIZE);
        for (byte[] packet : packetList) {
            mIkeSocket.sendIkePacket(packet, mRemoteAddress);
        }
        if (msg.ikeHeader.isResponseMsg) {
            ikeSaRecord.updateLastSentRespAllPackets(Arrays.asList(packetList));
        }
    }

    // Builds and sends IKE-level error notification response on the provided IKE SA record
    @VisibleForTesting
    void buildAndSendErrorNotificationResponse(
            IkeSaRecord ikeSaRecord, int messageId, @ErrorType int errorType) {
        IkeNotifyPayload error = new IkeNotifyPayload(errorType);
        buildAndSendNotificationResponse(ikeSaRecord, messageId, error);
    }

    // Builds and sends error notification response on the provided IKE SA record
    @VisibleForTesting
    void buildAndSendNotificationResponse(
            IkeSaRecord ikeSaRecord, int messageId, IkeNotifyPayload notifyPayload) {
        IkeMessage msg =
                buildEncryptedNotificationMessage(
                        ikeSaRecord,
                        new IkeInformationalPayload[] {notifyPayload},
                        EXCHANGE_TYPE_INFORMATIONAL,
                        true /*isResponse*/,
                        messageId);

        sendEncryptedIkeMessage(ikeSaRecord, msg);
    }

    // Builds an Encrypted IKE Informational Message for the given IkeInformationalPayload using the
    // current IKE SA record.
    @VisibleForTesting
    IkeMessage buildEncryptedInformationalMessage(
            IkeInformationalPayload[] payloads, boolean isResponse, int messageId) {
        return buildEncryptedInformationalMessage(
                mCurrentIkeSaRecord, payloads, isResponse, messageId);
    }

    // Builds an Encrypted IKE Informational Message for the given IkeInformationalPayload using the
    // provided IKE SA record.
    @VisibleForTesting
    IkeMessage buildEncryptedInformationalMessage(
            IkeSaRecord saRecord,
            IkeInformationalPayload[] payloads,
            boolean isResponse,
            int messageId) {
        return buildEncryptedNotificationMessage(
                saRecord, payloads, IkeHeader.EXCHANGE_TYPE_INFORMATIONAL, isResponse, messageId);
    }

    // Builds an Encrypted IKE Message for the given IkeInformationalPayload using the provided IKE
    // SA record and exchange type.
    @VisibleForTesting
    IkeMessage buildEncryptedNotificationMessage(
            IkeSaRecord saRecord,
            IkeInformationalPayload[] payloads,
            @ExchangeType int exchangeType,
            boolean isResponse,
            int messageId) {
        IkeHeader header =
                new IkeHeader(
                        saRecord.getInitiatorSpi(),
                        saRecord.getResponderSpi(),
                        IkePayload.PAYLOAD_TYPE_SK,
                        exchangeType,
                        isResponse /*isResponseMsg*/,
                        saRecord.isLocalInit /*fromIkeInitiator*/,
                        messageId);

        return new IkeMessage(header, Arrays.asList(payloads));
    }

    private abstract class LocalRequestQueuer extends ExceptionHandler {
        /**
         * Reroutes all local requests to the scheduler
         *
         * @param requestVal The command value of the request
         * @param req The instance of the LocalRequest to be queued.
         */
        protected void handleLocalRequest(int requestVal, LocalRequest req) {
            switch (requestVal) {
                case CMD_LOCAL_REQUEST_DELETE_IKE:
                    mScheduler.addRequestAtFront(req);
                    return;

                case CMD_LOCAL_REQUEST_REKEY_IKE: // Fallthrough
                case CMD_LOCAL_REQUEST_INFO: // Fallthrough
                case CMD_LOCAL_REQUEST_DPD:
                    mScheduler.addRequest(req);
                    return;

                case CMD_LOCAL_REQUEST_CREATE_CHILD: // Fallthrough
                case CMD_LOCAL_REQUEST_REKEY_CHILD: // Fallthrough
                case CMD_LOCAL_REQUEST_DELETE_CHILD:
                    ChildLocalRequest childReq = (ChildLocalRequest) req;
                    if (childReq.procedureType != requestVal) {
                        cleanUpAndQuit(
                                new IllegalArgumentException(
                                        "ChildLocalRequest procedure type was invalid"));
                    }
                    mScheduler.addRequest(childReq);
                    return;

                default:
                    cleanUpAndQuit(
                            new IllegalStateException(
                                    "Unknown local request passed to handleLocalRequest"));
            }
        }

        /** Check if received signal is a local request. */
        protected boolean isLocalRequest(int msgWhat) {
            if ((msgWhat >= CMD_IKE_LOCAL_REQUEST_BASE
                            && msgWhat < CMD_IKE_LOCAL_REQUEST_BASE + CMD_CATEGORY_SIZE)
                    || (msgWhat >= CMD_CHILD_LOCAL_REQUEST_BASE
                            && msgWhat < CMD_CHILD_LOCAL_REQUEST_BASE + CMD_CATEGORY_SIZE)) {
                return true;
            }
            return false;
        }

        protected void handleFiredAlarm(Message message) {
            switch (message.arg2) {
                case CMD_SEND_KEEPALIVE:
                    // Software keepalive alarm is fired
                    mIkeNattKeepalive.onAlarmFired();
                    return;
                case CMD_LOCAL_REQUEST_DELETE_CHILD: // Hits hard lifetime; fall through
                case CMD_LOCAL_REQUEST_REKEY_CHILD: // Hits soft lifetime
                    int remoteChildSpi = ((Bundle) message.obj).getInt(BUNDLE_KEY_CHILD_REMOTE_SPI);
                    enqueueLocalRequestSynchronously(
                            new ChildLocalRequest(message.arg2, remoteChildSpi));
                    return;
                case CMD_LOCAL_REQUEST_DELETE_IKE: // Hits hard lifetime; fall through
                case CMD_LOCAL_REQUEST_REKEY_IKE: // Hits soft lifetime; fall through
                case CMD_LOCAL_REQUEST_DPD:
                    // IKE Session has not received any protectd IKE packet for the whole DPD delay
                    long remoteIkeSpi = ((Bundle) message.obj).getLong(BUNDLE_KEY_IKE_REMOTE_SPI);
                    enqueueLocalRequestSynchronously(
                            new IkeLocalRequest(message.arg2, remoteIkeSpi));

                    // TODO(b/152442041): Cancel the scheduled DPD request if IKE Session starts any
                    // procedure before DPD get executed.
                    return;
                default:
                    logWtf("Invalid alarm action: " + message.arg2);
            }
        }

        private void enqueueLocalRequestSynchronously(LocalRequest request) {
            // Use dispatchMessage to synchronously handle this message so that the AlarmManager
            // WakeLock can keep protecting this message until it is enquequed in mScheduler. It is
            // safe because the alarmReceiver is called on the Ike HandlerThread, and the
            // IkeSessionStateMachine is not currently in a state transition.
            getHandler().dispatchMessage(obtainMessage(request.procedureType, request));
        }
    }

    /**
     * Base state defines common behaviours when receiving an IKE packet.
     *
     * <p>State that represents an ongoing IKE procedure MUST extend BusyState to handle received
     * IKE packet. Idle state will defer the received packet to a BusyState to process it.
     */
    private abstract class BusyState extends LocalRequestQueuer {
        @Override
        public boolean processStateMessage(Message message) {
            switch (message.what) {
                case CMD_RECEIVE_IKE_PACKET:
                    handleReceivedIkePacket(message);
                    return HANDLED;
                case CMD_ALARM_FIRED:
                    handleFiredAlarm(message);
                    return HANDLED;
                case CMD_FORCE_TRANSITION:
                    transitionTo((State) message.obj);
                    return HANDLED;

                case CMD_EXECUTE_LOCAL_REQ:
                    logWtf("Invalid execute local request command in non-idle state");
                    return NOT_HANDLED;

                case CMD_RETRANSMIT:
                    triggerRetransmit();
                    return HANDLED;

                default:
                    // Queue local requests, and trigger next procedure
                    if (isLocalRequest(message.what)) {
                        handleLocalRequest(message.what, (LocalRequest) message.obj);
                        return HANDLED;
                    }
                    return NOT_HANDLED;
            }
        }

        /**
         * Handler for retransmission timer firing
         *
         * <p>By default, the trigger is logged and dropped. States that have a retransmitter should
         * override this function, and proxy the call to Retransmitter.retransmit()
         */
        protected void triggerRetransmit() {
            logWtf("Retransmission trigger dropped in state: " + this.getClass().getSimpleName());
        }

        protected IkeSaRecord getIkeSaRecordForPacket(IkeHeader ikeHeader) {
            if (ikeHeader.fromIkeInitiator) {
                return mLocalSpiToIkeSaRecordMap.get(ikeHeader.ikeResponderSpi);
            } else {
                return mLocalSpiToIkeSaRecordMap.get(ikeHeader.ikeInitiatorSpi);
            }
        }

        protected void handleReceivedIkePacket(Message message) {
            // TODO: b/138411550 Notify subclasses when discarding a received packet. Receiving MUST
            // go back to Idle state in this case.

            String methodTag = "handleReceivedIkePacket: ";

            ReceivedIkePacket receivedIkePacket = (ReceivedIkePacket) message.obj;
            IkeHeader ikeHeader = receivedIkePacket.ikeHeader;
            byte[] ikePacketBytes = receivedIkePacket.ikePacketBytes;
            IkeSaRecord ikeSaRecord = getIkeSaRecordForPacket(ikeHeader);

            String msgDirection = ikeHeader.isResponseMsg ? "response" : "request";

            // Drop packets that we don't have an SA for:
            if (ikeSaRecord == null) {
                // TODO: Print a summary of the IKE message (perhaps the IKE header)
                cleanUpAndQuit(
                        new IllegalStateException(
                                "Received an IKE "
                                        + msgDirection
                                        + "but found no matching SA for it"));
                return;
            }

            logd(
                    methodTag
                            + "Received an "
                            + ikeHeader.getBasicInfoString()
                            + " on IKE SA with local SPI: "
                            + ikeSaRecord.getLocalSpi()
                            + ". Packet size: "
                            + ikePacketBytes.length);

            if (ikeHeader.isResponseMsg) {
                int expectedMsgId = ikeSaRecord.getLocalRequestMessageId();
                if (expectedMsgId - 1 == ikeHeader.messageId) {
                    logd(methodTag + "Received re-transmitted response. Discard it.");
                    return;
                }

                DecodeResult decodeResult =
                        IkeMessage.decode(
                                expectedMsgId,
                                mIkeIntegrity,
                                mIkeCipher,
                                ikeSaRecord,
                                ikeHeader,
                                ikePacketBytes,
                                ikeSaRecord.getCollectedFragments(true /*isResp*/));
                switch (decodeResult.status) {
                    case DECODE_STATUS_OK:
                        ikeSaRecord.incrementLocalRequestMessageId();
                        ikeSaRecord.resetCollectedFragments(true /*isResp*/);

                        DecodeResultOk resultOk = (DecodeResultOk) decodeResult;
                        if (isTempFailure(resultOk.ikeMessage)) {
                            handleTempFailure();
                        } else {
                            mTempFailHandler.reset();
                        }

                        handleResponseIkeMessage(resultOk.ikeMessage);
                        break;
                    case DECODE_STATUS_PARTIAL:
                        ikeSaRecord.updateCollectedFragments(
                                (DecodeResultPartial) decodeResult, true /*isResp*/);
                        break;
                    case DECODE_STATUS_PROTECTED_ERROR:
                        IkeException ikeException = ((DecodeResultError) decodeResult).ikeException;
                        logi(methodTag + "Protected error", ikeException);

                        ikeSaRecord.incrementLocalRequestMessageId();
                        ikeSaRecord.resetCollectedFragments(true /*isResp*/);

                        handleResponseGenericProcessError(
                                ikeSaRecord,
                                new InvalidSyntaxException(
                                        "Generic processing error in the received response",
                                        ikeException));
                        break;
                    case DECODE_STATUS_UNPROTECTED_ERROR:
                        logi(
                                methodTag
                                        + "Message authentication or decryption failed on received"
                                        + " response. Discard it",
                                ((DecodeResultError) decodeResult).ikeException);
                        break;
                    default:
                        cleanUpAndQuit(
                                new IllegalStateException(
                                        "Unrecognized decoding status: " + decodeResult.status));
                }

            } else {
                int expectedMsgId = ikeSaRecord.getRemoteRequestMessageId();
                if (expectedMsgId - 1 == ikeHeader.messageId) {

                    if (ikeSaRecord.isRetransmittedRequest(ikePacketBytes)) {
                        logd("Received re-transmitted request. Retransmitting response");

                        if (ikeSaRecord.getLastSentRespAllPackets() != null) {
                            for (byte[] packet : ikeSaRecord.getLastSentRespAllPackets()) {
                                mIkeSocket.sendIkePacket(packet, mRemoteAddress);
                            }
                        }

                        // TODO:Support resetting remote rekey delete timer.
                    } else {
                        logi(methodTag + "Received response with invalid message ID. Discard it.");
                    }
                } else {
                    DecodeResult decodeResult =
                            IkeMessage.decode(
                                    expectedMsgId,
                                    mIkeIntegrity,
                                    mIkeCipher,
                                    ikeSaRecord,
                                    ikeHeader,
                                    ikePacketBytes,
                                    ikeSaRecord.getCollectedFragments(false /*isResp*/));
                    switch (decodeResult.status) {
                        case DECODE_STATUS_OK:
                            ikeSaRecord.incrementRemoteRequestMessageId();
                            ikeSaRecord.resetCollectedFragments(false /*isResp*/);

                            DecodeResultOk resultOk = (DecodeResultOk) decodeResult;
                            IkeMessage ikeMessage = resultOk.ikeMessage;
                            ikeSaRecord.updateLastReceivedReqFirstPacket(resultOk.firstPacket);

                            // Handle DPD here.
                            if (ikeMessage.isDpdRequest()) {
                                logd(methodTag + "Received DPD request");
                                IkeMessage dpdResponse =
                                        buildEncryptedInformationalMessage(
                                                ikeSaRecord,
                                                new IkeInformationalPayload[] {},
                                                true,
                                                ikeHeader.messageId);
                                sendEncryptedIkeMessage(ikeSaRecord, dpdResponse);
                                break;
                            }

                            int ikeExchangeSubType = getIkeExchangeSubType(ikeMessage);
                            logd(
                                    methodTag
                                            + "Request exchange subtype: "
                                            + EXCHANGE_SUBTYPE_TO_STRING.get(ikeExchangeSubType));

                            if (ikeExchangeSubType == IKE_EXCHANGE_SUBTYPE_INVALID
                                    || ikeExchangeSubType == IKE_EXCHANGE_SUBTYPE_IKE_INIT
                                    || ikeExchangeSubType == IKE_EXCHANGE_SUBTYPE_IKE_AUTH) {

                                // Reply with INVALID_SYNTAX and close IKE Session.
                                buildAndSendErrorNotificationResponse(
                                        mCurrentIkeSaRecord,
                                        ikeHeader.messageId,
                                        ERROR_TYPE_INVALID_SYNTAX);
                                handleIkeFatalError(
                                        new InvalidSyntaxException(
                                                "Cannot handle message with invalid or unexpected"
                                                        + " IkeExchangeSubType: "
                                                        + ikeExchangeSubType));
                                return;
                            }
                            handleRequestIkeMessage(ikeMessage, ikeExchangeSubType, message);
                            break;
                        case DECODE_STATUS_PARTIAL:
                            ikeSaRecord.updateCollectedFragments(
                                    (DecodeResultPartial) decodeResult, false /*isResp*/);
                            break;
                        case DECODE_STATUS_PROTECTED_ERROR:
                            DecodeResultProtectedError resultError =
                                    (DecodeResultProtectedError) decodeResult;

                            IkeException ikeException = resultError.ikeException;
                            logi(methodTag + "Protected error", resultError.ikeException);

                            ikeSaRecord.incrementRemoteRequestMessageId();
                            ikeSaRecord.resetCollectedFragments(false /*isResp*/);

                            ikeSaRecord.updateLastReceivedReqFirstPacket(resultError.firstPacket);

                            // IkeException MUST be already wrapped into an IkeProtocolException
                            handleRequestGenericProcessError(
                                    ikeSaRecord,
                                    ikeHeader.messageId,
                                    (IkeProtocolException) ikeException);
                            break;
                        case DECODE_STATUS_UNPROTECTED_ERROR:
                            logi(
                                    methodTag
                                            + "Message authentication or decryption failed on"
                                            + " received request. Discard it",
                                    ((DecodeResultError) decodeResult).ikeException);
                            break;
                        default:
                            cleanUpAndQuit(
                                    new IllegalStateException(
                                            "Unrecognized decoding status: "
                                                    + decodeResult.status));
                    }
                }
            }
        }

        private boolean isTempFailure(IkeMessage message) {
            List<IkeNotifyPayload> notifyPayloads =
                    message.getPayloadListForType(PAYLOAD_TYPE_NOTIFY, IkeNotifyPayload.class);

            for (IkeNotifyPayload notify : notifyPayloads) {
                if (notify.notifyType == ERROR_TYPE_TEMPORARY_FAILURE) {
                    return true;
                }
            }
            return false;
        }

        protected void handleTempFailure() {
            // Log and close IKE Session due to unexpected TEMPORARY_FAILURE. This error should
            // only occur during CREATE_CHILD_SA exchange.
            handleIkeFatalError(
                    new InvalidSyntaxException("Received unexpected TEMPORARY_FAILURE"));

            // States that accept a TEMPORARY MUST override this method to schedule a retry.
        }

        protected void handleGenericInfoRequest(IkeMessage ikeMessage) {
            // TODO(b/150327849): Respond with vendor ID or config payload responses.

            IkeMessage emptyInfoResp =
                    buildEncryptedInformationalMessage(
                            new IkeInformationalPayload[0],
                            true /* isResponse */,
                            ikeMessage.ikeHeader.messageId);
            sendEncryptedIkeMessage(emptyInfoResp);
        }

        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            // Subclasses MUST override it if they care
            cleanUpAndQuit(
                    new IllegalStateException(
                            "Do not support handling an encrypted request: " + ikeExchangeSubType));
        }

        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            // Subclasses MUST override it if they care
            cleanUpAndQuit(
                    new IllegalStateException("Do not support handling an encrypted response"));
        }

        /**
         * Method for handling generic processing error of a request.
         *
         * <p>A generic processing error is usally syntax error, unsupported critical payload error
         * and major version error. IKE SA that should reply with corresponding error notifications
         */
        protected void handleRequestGenericProcessError(
                IkeSaRecord ikeSaRecord, int messageId, IkeProtocolException exception) {
            IkeNotifyPayload errNotify = exception.buildNotifyPayload();
            sendEncryptedIkeMessage(
                    ikeSaRecord,
                    buildEncryptedInformationalMessage(
                            ikeSaRecord,
                            new IkeInformationalPayload[] {errNotify},
                            true /*isResponse*/,
                            messageId));

            // Receiver of INVALID_SYNTAX error notification should delete the IKE SA
            if (exception.getErrorType() == ERROR_TYPE_INVALID_SYNTAX) {
                handleIkeFatalError(exception);
            }
        }

        /**
         * Method for handling generic processing error of a response.
         *
         * <p>Detailed error is wrapped in the InvalidSyntaxException, which is usally syntax error,
         * unsupported critical payload error and major version error. IKE SA that receives a
         * response with these errors should be closed.
         */
        protected void handleResponseGenericProcessError(
                IkeSaRecord ikeSaRecord, InvalidSyntaxException ikeException) {
            // Subclasses MUST override it if they care
            cleanUpAndQuit(
                    new IllegalStateException(
                            "Do not support handling generic processing error of encrypted"
                                    + " response"));
        }
    }

    /**
     * Retransmitter represents a RAII class to send the initial request, and retransmit as needed.
     *
     * <p>The Retransmitter class will automatically start transmission upon creation.
     */
    @VisibleForTesting
    class EncryptedRetransmitter extends Retransmitter {
        private final IkeSaRecord mIkeSaRecord;

        @VisibleForTesting
        EncryptedRetransmitter(IkeMessage msg) {
            this(mCurrentIkeSaRecord, msg);
        }

        private EncryptedRetransmitter(IkeSaRecord ikeSaRecord, IkeMessage msg) {
            super(getHandler(), msg, mIkeSessionParams.getRetransmissionTimeoutsMillis());

            mIkeSaRecord = ikeSaRecord;

            retransmit();
        }

        @Override
        public void send(IkeMessage msg) {
            sendEncryptedIkeMessage(mIkeSaRecord, msg);
        }

        @Override
        public void handleRetransmissionFailure() {
            handleIkeFatalError(new IOException("Retransmitting failure"));
        }
    }

    /**
     * DeleteResponderBase represents all states after IKE_INIT and IKE_AUTH.
     *
     * <p>All post-init states share common functionality of being able to respond to IKE_DELETE
     * requests.
     */
    private abstract class DeleteResponderBase extends BusyState {
        /** Builds a IKE Delete Response for the given IKE SA and request. */
        protected IkeMessage buildIkeDeleteResp(IkeMessage req, IkeSaRecord ikeSaRecord) {
            IkeInformationalPayload[] payloads = new IkeInformationalPayload[] {};
            return buildEncryptedInformationalMessage(
                    ikeSaRecord, payloads, true /* isResp */, req.ikeHeader.messageId);
        }

        /**
         * Validates that the delete request is acceptable.
         *
         * <p>The request message must be guaranteed by previous checks to be of SUBTYPE_DELETE_IKE,
         * and therefore contains an IkeDeletePayload. This is checked in getIkeExchangeSubType.
         */
        protected void validateIkeDeleteReq(IkeMessage req, IkeSaRecord expectedRecord)
                throws InvalidSyntaxException {
            if (expectedRecord != getIkeSaRecordForPacket(req.ikeHeader)) {
                throw new InvalidSyntaxException("Delete request received in wrong SA");
            }
        }

        /**
         * Helper method for responding to a session deletion request
         *
         * <p>Note that this method expects that the session is keyed on the current IKE SA session,
         * and closing the IKE SA indicates that the remote wishes to end the session as a whole. As
         * such, this should not be used in rekey cases where there is any ambiguity as to which IKE
         * SA the session is reliant upon.
         *
         * <p>Note that this method will also quit the state machine.
         *
         * @param ikeMessage The received session deletion request
         */
        protected void handleDeleteSessionRequest(IkeMessage ikeMessage) {
            try {
                validateIkeDeleteReq(ikeMessage, mCurrentIkeSaRecord);
                IkeMessage resp = buildIkeDeleteResp(ikeMessage, mCurrentIkeSaRecord);

                executeUserCallback(
                        () -> {
                            mIkeSessionCallback.onClosed();
                        });

                sendEncryptedIkeMessage(mCurrentIkeSaRecord, resp);

                removeIkeSaRecord(mCurrentIkeSaRecord);
                mCurrentIkeSaRecord.close();
                mCurrentIkeSaRecord = null;

                quitNow();
            } catch (InvalidSyntaxException e) {
                // Got deletion of a non-Current IKE SA. Program error.
                cleanUpAndQuit(new IllegalStateException(e));
            }
        }
    }

    /**
     * DeleteBase abstracts deletion handling for all states initiating a delete exchange
     *
     * <p>All subclasses of this state share common functionality that a deletion request is sent,
     * and the response is received.
     */
    private abstract class DeleteBase extends DeleteResponderBase {
        /** Builds a IKE Delete Request for the given IKE SA. */
        protected IkeMessage buildIkeDeleteReq(IkeSaRecord ikeSaRecord) {
            IkeInformationalPayload[] payloads =
                    new IkeInformationalPayload[] {new IkeDeletePayload()};
            return buildEncryptedInformationalMessage(
                    ikeSaRecord,
                    payloads,
                    false /* isResp */,
                    ikeSaRecord.getLocalRequestMessageId());
        }

        protected void validateIkeDeleteResp(IkeMessage resp, IkeSaRecord expectedSaRecord)
                throws InvalidSyntaxException {
            if (expectedSaRecord != getIkeSaRecordForPacket(resp.ikeHeader)) {
                throw new IllegalStateException("Response received on incorrect SA");
            }

            if (resp.ikeHeader.exchangeType != IkeHeader.EXCHANGE_TYPE_INFORMATIONAL) {
                throw new InvalidSyntaxException(
                        "Invalid exchange type; expected INFORMATIONAL, but got: "
                                + resp.ikeHeader.exchangeType);
            }

            if (!resp.ikePayloadList.isEmpty()) {
                throw new InvalidSyntaxException(
                        "Unexpected payloads - IKE Delete response should be empty.");
            }
        }
    }

    /**
     * Receiving represents a state when idle IkeSessionStateMachine receives an incoming packet.
     *
     * <p>If this incoming packet is fully handled by Receiving state and does not trigger any
     * further state transition or deletion of whole IKE Session, IkeSessionStateMachine MUST
     * transition back to Idle.
     */
    class Receiving extends RekeyIkeHandlerBase {
        private boolean mProcedureFinished = true;

        @Override
        public void enterState() {
            mProcedureFinished = true;
        }

        @Override
        protected void handleReceivedIkePacket(Message message) {
            super.handleReceivedIkePacket(message);

            // If the received packet does not trigger a state transition or the packet causes this
            // state machine to quit, transition back to Idle State. In the second case, state
            // machine will first go back to Idle and then quit.
            if (mProcedureFinished) transitionTo(mIdle);
        }

        @Override
        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            switch (ikeExchangeSubType) {
                case IKE_EXCHANGE_SUBTYPE_REKEY_IKE:
                    // Errors in this exchange with no specific protocol error code will all be
                    // classified to use NO_PROPOSAL_CHOSEN. The reason that we don't use
                    // NO_ADDITIONAL_SAS is because it indicates "responder is unwilling to accept
                    // any more Child SAs on this IKE SA.", according to RFC 7296. Sending this
                    // error may mislead the remote peer.
                    try {
                        validateIkeRekeyReq(ikeMessage);

                        // TODO: Add support for limited re-negotiation of parameters

                        // Build a rekey response payload with our previously selected proposal,
                        // against which we will validate the received proposals.
                        IkeSaPayload reqSaPayload =
                                ikeMessage.getPayloadForType(
                                        IkePayload.PAYLOAD_TYPE_SA, IkeSaPayload.class);
                        byte respProposalNumber =
                                reqSaPayload.getNegotiatedProposalNumber(mSaProposal);

                        List<IkePayload> payloadList =
                                CreateIkeSaHelper.getRekeyIkeSaResponsePayloads(
                                        respProposalNumber,
                                        mSaProposal,
                                        mIkeSpiGenerator,
                                        mLocalAddress,
                                        mRandomFactory);

                        // Build IKE header
                        IkeHeader ikeHeader =
                                new IkeHeader(
                                        mCurrentIkeSaRecord.getInitiatorSpi(),
                                        mCurrentIkeSaRecord.getResponderSpi(),
                                        IkePayload.PAYLOAD_TYPE_SK,
                                        IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA,
                                        true /*isResponseMsg*/,
                                        mCurrentIkeSaRecord.isLocalInit,
                                        ikeMessage.ikeHeader.messageId);

                        IkeMessage responseIkeMessage = new IkeMessage(ikeHeader, payloadList);

                        // Build new SA first to ensure that we can find a valid proposal.
                        mRemoteInitNewIkeSaRecord =
                                validateAndBuildIkeSa(
                                        ikeMessage, responseIkeMessage, false /*isLocalInit*/);

                        sendEncryptedIkeMessage(responseIkeMessage);

                        transitionTo(mRekeyIkeRemoteDelete);
                        mProcedureFinished = false;
                    } catch (IkeProtocolException e) {
                        handleRekeyCreationFailure(ikeMessage.ikeHeader.messageId, e);
                    } catch (GeneralSecurityException e) {
                        handleRekeyCreationFailure(
                                ikeMessage.ikeHeader.messageId,
                                new NoValidProposalChosenException(
                                        "Error in building new IKE SA", e));
                    } catch (IOException e) {
                        handleRekeyCreationFailure(
                                ikeMessage.ikeHeader.messageId,
                                new NoValidProposalChosenException(
                                        "IKE SPI allocation collided - they reused an SPI.", e));
                    }
                    return;
                case IKE_EXCHANGE_SUBTYPE_DELETE_IKE:
                    handleDeleteSessionRequest(ikeMessage);
                    return;
                case IKE_EXCHANGE_SUBTYPE_CREATE_CHILD: // Fall through
                case IKE_EXCHANGE_SUBTYPE_DELETE_CHILD: // Fall through
                case IKE_EXCHANGE_SUBTYPE_REKEY_CHILD:
                    deferMessage(
                            obtainMessage(
                                    CMD_RECEIVE_REQUEST_FOR_CHILD,
                                    ikeExchangeSubType,
                                    0 /*placeHolder*/,
                                    ikeMessage));
                    transitionTo(mChildProcedureOngoing);
                    mProcedureFinished = false;
                    return;
                case IKE_EXCHANGE_SUBTYPE_GENERIC_INFO:
                    handleGenericInfoRequest(ikeMessage);
                    return;
                default:
            }
        }

        private void handleRekeyCreationFailure(int messageId, IkeProtocolException e) {
            loge("Received invalid Rekey IKE request. Reject with error notification", e);

            buildAndSendNotificationResponse(
                    mCurrentIkeSaRecord, messageId, e.buildNotifyPayload());
        }
    }

    /**
     * This class represents a state when there is at least one ongoing Child procedure
     * (Create/Rekey/Delete Child)
     *
     * <p>For a locally initiated Child procedure, this state is responsible for notifying Child
     * Session to initiate the exchange, building outbound request IkeMessage with Child Session
     * provided payload list and redirecting the inbound response to Child Session for validation.
     *
     * <p>For a remotely initiated Child procedure, this state is responsible for redirecting the
     * inbound request to Child Session(s) and building outbound response IkeMessage with Child
     * Session provided payload list. Exchange collision on a Child Session will be resolved inside
     * the Child Session.
     *
     * <p>For a remotely initiated IKE procedure, this state will only accept a Delete IKE request
     * and reject other types with TEMPORARY_FAILURE, since it causes conflict with the ongoing
     * Child procedure.
     *
     * <p>For most inbound request/response, this state will first pick out and handle IKE related
     * payloads and then send the rest of the payloads to Child Session for further validation. It
     * is the Child Session's responsibility to check required payloads (and verify the exchange
     * type) according to its procedure type. Only when receiving an inbound delete Child request,
     * as the only case where multiple Child Sessions will be affected by one IkeMessage, this state
     * will only send Delete Payload(s) to Child Session.
     */
    class ChildProcedureOngoing extends DeleteBase {
        // It is possible that mChildInLocalProcedure is also in mChildInRemoteProcedures when both
        // sides initiated exchange for the same Child Session.
        private ChildSessionStateMachine mChildInLocalProcedure;
        private Set<ChildSessionStateMachine> mChildInRemoteProcedures;

        private ChildLocalRequest mLocalRequestOngoing;

        private int mLastInboundRequestMsgId;
        private List<IkePayload> mOutboundRespPayloads;
        private Set<ChildSessionStateMachine> mAwaitingChildResponse;

        private EncryptedRetransmitter mRetransmitter;

        @Override
        public void enterState() {
            mChildInLocalProcedure = null;
            mChildInRemoteProcedures = new HashSet<>();

            mLocalRequestOngoing = null;

            mLastInboundRequestMsgId = 0;
            mOutboundRespPayloads = new LinkedList<>();
            mAwaitingChildResponse = new HashSet<>();
        }

        @Override
        protected void triggerRetransmit() {
            mRetransmitter.retransmit();
        }

        @Override
        public boolean processStateMessage(Message message) {
            switch (message.what) {
                case CMD_RECEIVE_REQUEST_FOR_CHILD:
                    // Handle remote request (and do state transition)
                    handleRequestIkeMessage(
                            (IkeMessage) message.obj,
                            message.arg1 /*ikeExchangeSubType*/,
                            null /*ReceivedIkePacket*/);
                    return HANDLED;
                case CMD_OUTBOUND_CHILD_PAYLOADS_READY:
                    ChildOutboundData outboundData = (ChildOutboundData) message.obj;
                    int exchangeType = outboundData.exchangeType;
                    List<IkePayload> outboundPayloads = outboundData.payloadList;

                    if (outboundData.isResp) {
                        handleOutboundResponse(
                                exchangeType, outboundPayloads, outboundData.childSession);
                    } else {
                        handleOutboundRequest(exchangeType, outboundPayloads);
                    }

                    return HANDLED;
                case CMD_CHILD_PROCEDURE_FINISHED:
                    ChildSessionStateMachine childSession = (ChildSessionStateMachine) message.obj;

                    if (mChildInLocalProcedure == childSession) {
                        mChildInLocalProcedure = null;
                        mLocalRequestOngoing = null;
                    }
                    mChildInRemoteProcedures.remove(childSession);

                    transitionToIdleIfAllProceduresDone();
                    return HANDLED;
                case CMD_HANDLE_FIRST_CHILD_NEGOTIATION:
                    FirstChildNegotiationData childData = (FirstChildNegotiationData) message.obj;

                    mChildInLocalProcedure = getChildSession(childData.childSessionCallback);
                    if (mChildInLocalProcedure == null) {
                        cleanUpAndQuit(new IllegalStateException("First child not found."));
                        return HANDLED;
                    }

                    mChildInLocalProcedure.handleFirstChildExchange(
                            childData.reqPayloads,
                            childData.respPayloads,
                            mLocalAddress,
                            mRemoteAddress,
                            getEncapSocketIfNatDetected(),
                            mIkePrf,
                            mCurrentIkeSaRecord.getSkD());
                    return HANDLED;
                case CMD_EXECUTE_LOCAL_REQ:
                    executeLocalRequest((ChildLocalRequest) message.obj);
                    return HANDLED;
                default:
                    return super.processStateMessage(message);
            }
        }

        @Override
        protected void handleTempFailure() {
            // The ChildSessionStateMachine will be responsible for rescheduling the rejected
            // request.
            mTempFailHandler.handleTempFailure();
        }

        private void transitionToIdleIfAllProceduresDone() {
            if (mChildInLocalProcedure == null && mChildInRemoteProcedures.isEmpty()) {
                transitionTo(mIdle);
            }
        }

        private ChildSessionStateMachine getChildSession(ChildLocalRequest req) {
            if (req.childSessionCallback == null) {
                return mRemoteSpiToChildSessionMap.get(req.remoteSpi);
            }
            return getChildSession(req.childSessionCallback);
        }

        private ChildSessionStateMachine getChildSession(ChildSessionCallback callback) {
            synchronized (mChildCbToSessions) {
                return mChildCbToSessions.get(callback);
            }
        }

        // Returns the UDP-Encapsulation socket to the newly created ChildSessionStateMachine if
        // a NAT is detected. It allows the ChildSessionStateMachine to build IPsec transforms that
        // can send and receive IPsec traffic through a NAT.
        private UdpEncapsulationSocket getEncapSocketIfNatDetected() {
            boolean isNatDetected = mIsLocalBehindNat || mIsRemoteBehindNat;

            if (!isNatDetected) return null;

            if (!(mIkeSocket instanceof IkeUdpEncapSocket)) {
                throw new IllegalStateException(
                        "NAT is detected but IKE packet is not UDP-Encapsulated.");
            }
            return ((IkeUdpEncapSocket) mIkeSocket).getUdpEncapsulationSocket();
        }

        private void executeLocalRequest(ChildLocalRequest req) {
            req.releaseWakeLock();
            mChildInLocalProcedure = getChildSession(req);
            mLocalRequestOngoing = req;

            if (mChildInLocalProcedure == null) {
                // This request has been validated to have a recognized target Child Session when
                // it was sent to IKE Session at the begginnig. Failing to find this Child Session
                // now means the Child creation has failed.
                logd(
                        "Child state machine not found for local request: "
                                + req.procedureType
                                + " Creation of Child Session may have been failed.");

                transitionToIdleIfAllProceduresDone();
                return;
            }
            switch (req.procedureType) {
                case CMD_LOCAL_REQUEST_CREATE_CHILD:
                    mChildInLocalProcedure.createChildSession(
                            mLocalAddress,
                            mRemoteAddress,
                            getEncapSocketIfNatDetected(),
                            mIkePrf,
                            mCurrentIkeSaRecord.getSkD());
                    break;
                case CMD_LOCAL_REQUEST_REKEY_CHILD:
                    mChildInLocalProcedure.rekeyChildSession();
                    break;
                case CMD_LOCAL_REQUEST_DELETE_CHILD:
                    mChildInLocalProcedure.deleteChildSession();
                    break;
                default:
                    cleanUpAndQuit(
                            new IllegalStateException(
                                    "Invalid Child procedure type: " + req.procedureType));
                    break;
            }
        }

        /**
         * This method is called when this state receives an inbound request or when mReceiving
         * received an inbound Child request and deferred it to this state.
         */
        @Override
        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            // TODO: Grab a remote lock and hand payloads to the Child Session

            mLastInboundRequestMsgId = ikeMessage.ikeHeader.messageId;
            switch (ikeExchangeSubType) {
                case IKE_EXCHANGE_SUBTYPE_CREATE_CHILD:
                    buildAndSendErrorNotificationResponse(
                            mCurrentIkeSaRecord,
                            ikeMessage.ikeHeader.messageId,
                            ERROR_TYPE_NO_ADDITIONAL_SAS);
                    break;
                case IKE_EXCHANGE_SUBTYPE_DELETE_IKE:
                    // Send response and quit state machine
                    handleDeleteSessionRequest(ikeMessage);

                    // Return immediately to avoid transitioning to mIdle
                    return;
                case IKE_EXCHANGE_SUBTYPE_DELETE_CHILD:
                    handleInboundDeleteChildRequest(ikeMessage);
                    break;
                case IKE_EXCHANGE_SUBTYPE_REKEY_IKE:
                    buildAndSendErrorNotificationResponse(
                            mCurrentIkeSaRecord,
                            ikeMessage.ikeHeader.messageId,
                            ERROR_TYPE_TEMPORARY_FAILURE);
                    break;
                case IKE_EXCHANGE_SUBTYPE_REKEY_CHILD:
                    handleInboundRekeyChildRequest(ikeMessage);
                    break;
                case IKE_EXCHANGE_SUBTYPE_GENERIC_INFO:
                    handleGenericInfoRequest(ikeMessage);
                    break;
                default:
                    cleanUpAndQuit(
                            new IllegalStateException(
                                    "Invalid IKE exchange subtype: " + ikeExchangeSubType));
                    return;
            }
            transitionToIdleIfAllProceduresDone();
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            mRetransmitter.stopRetransmitting();

            List<IkePayload> handledPayloads = new LinkedList<>();

            for (IkePayload payload : ikeMessage.ikePayloadList) {
                switch (payload.payloadType) {
                    case PAYLOAD_TYPE_NOTIFY:
                        // TODO: Handle fatal IKE error notification and IKE status notification.
                        break;
                    case PAYLOAD_TYPE_VENDOR:
                        // TODO: Handle Vendor ID Payload
                        handledPayloads.add(payload);
                        break;
                    case PAYLOAD_TYPE_CP:
                        // TODO: Handle IKE related configuration attributes and pass the payload to
                        // Child to further handle internal IP address attributes.
                        break;
                    default:
                        break;
                }
            }

            List<IkePayload> payloads = new LinkedList<>();
            payloads.addAll(ikeMessage.ikePayloadList);
            payloads.removeAll(handledPayloads);

            mChildInLocalProcedure.receiveResponse(ikeMessage.ikeHeader.exchangeType, payloads);
        }

        @Override
        protected void handleResponseGenericProcessError(
                IkeSaRecord ikeSaRecord, InvalidSyntaxException ikeException) {
            mRetransmitter.stopRetransmitting();

            sendEncryptedIkeMessage(buildIkeDeleteReq(mCurrentIkeSaRecord));
            handleIkeFatalError(ikeException);
        }

        private void handleInboundDeleteChildRequest(IkeMessage ikeMessage) {
            // It is guaranteed in #getIkeExchangeSubType that at least one Delete Child Payload
            // exists.

            HashMap<ChildSessionStateMachine, List<IkePayload>> childToDelPayloadsMap =
                    new HashMap<>();
            Set<Integer> spiHandled = new HashSet<>();

            for (IkePayload payload : ikeMessage.ikePayloadList) {
                switch (payload.payloadType) {
                    case PAYLOAD_TYPE_VENDOR:
                        // TODO: Investigate if Vendor ID Payload can be in an INFORMATIONAL
                        // message.
                        break;
                    case PAYLOAD_TYPE_NOTIFY:
                        logw(
                                "Unexpected or unknown notification: "
                                        + ((IkeNotifyPayload) payload).notifyType);
                        break;
                    case PAYLOAD_TYPE_DELETE:
                        IkeDeletePayload delPayload = (IkeDeletePayload) payload;

                        for (int spi : delPayload.spisToDelete) {
                            ChildSessionStateMachine child = mRemoteSpiToChildSessionMap.get(spi);
                            if (child == null) {
                                // TODO: Investigate how other implementations handle that.
                                logw("Child SA not found with received SPI: " + spi);
                            } else if (!spiHandled.add(spi)) {
                                logw("Received repeated Child SPI: " + spi);
                            } else {
                                // Store Delete Payload with its target ChildSession
                                if (!childToDelPayloadsMap.containsKey(child)) {
                                    childToDelPayloadsMap.put(child, new LinkedList<>());
                                }
                                List<IkePayload> delPayloads = childToDelPayloadsMap.get(child);

                                // Avoid storing repeated Delete Payload
                                if (!delPayloads.contains(delPayload)) delPayloads.add(delPayload);
                            }
                        }

                        break;
                    case PAYLOAD_TYPE_CP:
                        // TODO: Handle it
                        break;
                    default:
                        logw("Unexpected payload types found: " + payload.payloadType);
                }
            }

            // If no Child SA is found, only reply with IKE related payloads or an empty
            // message
            if (childToDelPayloadsMap.isEmpty()) {
                logd("No Child SA is found for this request.");
                sendEncryptedIkeMessage(
                        buildEncryptedInformationalMessage(
                                new IkeInformationalPayload[0],
                                true /*isResp*/,
                                ikeMessage.ikeHeader.messageId));
                return;
            }

            // Send Delete Payloads to Child Sessions
            for (ChildSessionStateMachine child : childToDelPayloadsMap.keySet()) {
                child.receiveRequest(
                        IKE_EXCHANGE_SUBTYPE_DELETE_CHILD,
                        EXCHANGE_TYPE_INFORMATIONAL,
                        childToDelPayloadsMap.get(child));
                mAwaitingChildResponse.add(child);
                mChildInRemoteProcedures.add(child);
            }
        }

        private void handleInboundRekeyChildRequest(IkeMessage ikeMessage) {
            // It is guaranteed in #getIkeExchangeSubType that at least one Notify-Rekey Child
            // Payload exists.
            List<IkePayload> handledPayloads = new LinkedList<>();
            ChildSessionStateMachine targetChild = null;
            Set<Integer> unrecognizedSpis = new HashSet<>();

            for (IkePayload payload : ikeMessage.ikePayloadList) {
                switch (payload.payloadType) {
                    case PAYLOAD_TYPE_VENDOR:
                        // TODO: Handle it.
                        handledPayloads.add(payload);
                        break;
                    case PAYLOAD_TYPE_NOTIFY:
                        IkeNotifyPayload notifyPayload = (IkeNotifyPayload) payload;
                        if (NOTIFY_TYPE_REKEY_SA != notifyPayload.notifyType) break;

                        int childSpi = notifyPayload.spi;
                        ChildSessionStateMachine child = mRemoteSpiToChildSessionMap.get(childSpi);

                        if (child == null) {
                            // Remember unrecognized SPIs and reply error notification if no
                            // recognized SPI found.
                            unrecognizedSpis.add(childSpi);
                            logw("Child SA not found with received SPI: " + childSpi);
                        } else if (targetChild == null) {
                            // Each message should have only one Notify-Rekey Payload. If there are
                            // multiple of them, we only process the first valid one and ignore
                            // others.
                            targetChild = mRemoteSpiToChildSessionMap.get(childSpi);
                        } else {
                            logw("More than one Notify-Rekey Payload found with SPI: " + childSpi);
                            handledPayloads.add(notifyPayload);
                        }
                        break;
                    case PAYLOAD_TYPE_CP:
                        // TODO: Handle IKE related configuration attributes and pass the payload to
                        // Child to further handle internal IP address attributes.
                        break;
                    default:
                        break;
                }
            }

            // Reject request with error notification.
            if (targetChild == null) {
                IkeInformationalPayload[] errorPayloads =
                        new IkeInformationalPayload[unrecognizedSpis.size()];
                int i = 0;
                for (Integer spi : unrecognizedSpis) {
                    errorPayloads[i++] =
                            new IkeNotifyPayload(
                                    IkePayload.PROTOCOL_ID_ESP,
                                    spi,
                                    ERROR_TYPE_CHILD_SA_NOT_FOUND,
                                    new byte[0]);
                }

                IkeMessage msg =
                        buildEncryptedNotificationMessage(
                                mCurrentIkeSaRecord,
                                errorPayloads,
                                EXCHANGE_TYPE_INFORMATIONAL,
                                true /*isResponse*/,
                                ikeMessage.ikeHeader.messageId);

                sendEncryptedIkeMessage(mCurrentIkeSaRecord, msg);
                return;
            }

            // Normal path
            List<IkePayload> payloads = new LinkedList<>();
            payloads.addAll(ikeMessage.ikePayloadList);
            payloads.removeAll(handledPayloads);

            mAwaitingChildResponse.add(targetChild);
            mChildInRemoteProcedures.add(targetChild);

            targetChild.receiveRequest(
                    IKE_EXCHANGE_SUBTYPE_REKEY_CHILD, ikeMessage.ikeHeader.exchangeType, payloads);
        }

        private void handleOutboundRequest(int exchangeType, List<IkePayload> outboundPayloads) {
            IkeHeader ikeHeader =
                    new IkeHeader(
                            mCurrentIkeSaRecord.getInitiatorSpi(),
                            mCurrentIkeSaRecord.getResponderSpi(),
                            IkePayload.PAYLOAD_TYPE_SK,
                            exchangeType,
                            false /*isResp*/,
                            mCurrentIkeSaRecord.isLocalInit,
                            mCurrentIkeSaRecord.getLocalRequestMessageId());
            IkeMessage ikeMessage = new IkeMessage(ikeHeader, outboundPayloads);

            mRetransmitter = new EncryptedRetransmitter(ikeMessage);
        }

        private void handleOutboundResponse(
                int exchangeType,
                List<IkePayload> outboundPayloads,
                ChildSessionStateMachine childSession) {
            // For each request IKE passed to Child, Child will send back to IKE a response. Even
            // if the Child Sesison is under simultaneous deletion, it will send back an empty
            // payload list.
            mOutboundRespPayloads.addAll(outboundPayloads);
            mAwaitingChildResponse.remove(childSession);
            if (!mAwaitingChildResponse.isEmpty()) return;

            IkeHeader ikeHeader =
                    new IkeHeader(
                            mCurrentIkeSaRecord.getInitiatorSpi(),
                            mCurrentIkeSaRecord.getResponderSpi(),
                            IkePayload.PAYLOAD_TYPE_SK,
                            exchangeType,
                            true /*isResp*/,
                            mCurrentIkeSaRecord.isLocalInit,
                            mLastInboundRequestMsgId);
            IkeMessage ikeMessage = new IkeMessage(ikeHeader, mOutboundRespPayloads);
            sendEncryptedIkeMessage(ikeMessage);
        }
    }

    /** CreateIkeLocalIkeInit represents state when IKE library initiates IKE_INIT exchange. */
    @VisibleForTesting
    public class CreateIkeLocalIkeInit extends BusyState {
        private IkeSecurityParameterIndex mLocalIkeSpiResource;
        private IkeSecurityParameterIndex mRemoteIkeSpiResource;
        private Retransmitter mRetransmitter;

        // TODO: Support negotiating IKE fragmentation

        @Override
        public void enterState() {
            try {
                IkeMessage request = buildIkeInitReq();

                // Register local SPI to receive the IKE INIT response.
                mIkeSocket.registerIke(
                        request.ikeHeader.ikeInitiatorSpi, IkeSessionStateMachine.this);

                mIkeInitRequestBytes = request.encode();
                mIkeInitNoncePayload =
                        request.getPayloadForType(
                                IkePayload.PAYLOAD_TYPE_NONCE, IkeNoncePayload.class);
                mRetransmitter = new UnencryptedRetransmitter(request);
            } catch (IOException e) {
                // Fail to assign IKE SPI
                handleIkeFatalError(e);
            }
        }

        @Override
        protected void triggerRetransmit() {
            mRetransmitter.retransmit();
        }

        @Override
        public boolean processStateMessage(Message message) {
            switch (message.what) {
                case CMD_RECEIVE_IKE_PACKET:
                    handleReceivedIkePacket(message);
                    return HANDLED;

                default:
                    return super.processStateMessage(message);
            }
        }

        protected void handleReceivedIkePacket(Message message) {
            String methodTag = "handleReceivedIkePacket: ";

            ReceivedIkePacket receivedIkePacket = (ReceivedIkePacket) message.obj;
            IkeHeader ikeHeader = receivedIkePacket.ikeHeader;
            byte[] ikePacketBytes = receivedIkePacket.ikePacketBytes;

            logd(
                    methodTag
                            + "Received an "
                            + ikeHeader.getBasicInfoString()
                            + ". Packet size: "
                            + ikePacketBytes.length);

            if (ikeHeader.isResponseMsg) {
                DecodeResult decodeResult = IkeMessage.decode(0, ikeHeader, ikePacketBytes);

                switch (decodeResult.status) {
                    case DECODE_STATUS_OK:
                        handleResponseIkeMessage(((DecodeResultOk) decodeResult).ikeMessage);
                        mIkeInitResponseBytes = ikePacketBytes;

                        // SA negotiation failed
                        if (mCurrentIkeSaRecord == null) break;

                        mCurrentIkeSaRecord.incrementLocalRequestMessageId();
                        break;
                    case DECODE_STATUS_PARTIAL:
                        // Fall through. We don't support IKE fragmentation here. We should never
                        // get this status.
                    case DECODE_STATUS_PROTECTED_ERROR:
                        // IKE INIT response is not protected. So we should never get this status
                        cleanUpAndQuit(
                                new IllegalStateException(
                                        "Unexpected decoding status: " + decodeResult.status));
                        break;
                    case DECODE_STATUS_UNPROTECTED_ERROR:
                        logi(
                                "Discard unencrypted response with syntax error",
                                ((DecodeResultError) decodeResult).ikeException);
                        break;
                    default:
                        cleanUpAndQuit(
                                new IllegalStateException(
                                        "Invalid decoding status: " + decodeResult.status));
                }

            } else {
                // TODO: Also prettyprint IKE header in the log.
                logi("Received a request while waiting for IKE_INIT response. Discard it.");
            }
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            boolean ikeInitSuccess = false;
            try {
                validateIkeInitResp(mRetransmitter.getMessage(), ikeMessage);

                mCurrentIkeSaRecord =
                        IkeSaRecord.makeFirstIkeSaRecord(
                                mRetransmitter.getMessage(),
                                ikeMessage,
                                mLocalIkeSpiResource,
                                mRemoteIkeSpiResource,
                                mIkePrf,
                                mIkeIntegrity == null ? 0 : mIkeIntegrity.getKeyLength(),
                                mIkeCipher.getKeyLength(),
                                buildSaLifetimeAlarmScheduler(mRemoteIkeSpiResource.getSpi()));

                addIkeSaRecord(mCurrentIkeSaRecord);
                ikeInitSuccess = true;

                transitionTo(mCreateIkeLocalIkeAuth);
            } catch (IkeProtocolException | GeneralSecurityException | IOException e) {
                if (e instanceof InvalidKeException) {
                    InvalidKeException keException = (InvalidKeException) e;

                    int requestedDhGroup = keException.getDhGroup();
                    boolean doAllProposalsHaveDhGroup = true;
                    for (IkeSaProposal proposal : mIkeSessionParams.getSaProposalsInternal()) {
                        doAllProposalsHaveDhGroup &=
                                proposal.getDhGroups().contains(requestedDhGroup);
                    }

                    // If DH group is not acceptable for all proposals, fail. The caller explicitly
                    // did not want that combination, and the IKE library must honor it.
                    if (doAllProposalsHaveDhGroup) {
                        mPeerSelectedDhGroup = requestedDhGroup;

                        // Remove state set during request creation
                        mIkeSocket.unregisterIke(
                                mRetransmitter.getMessage().ikeHeader.ikeInitiatorSpi);
                        mIkeInitRequestBytes = null;
                        mIkeInitNoncePayload = null;

                        transitionTo(mInitial);
                        openSession();

                        return;
                    }
                }

                handleIkeFatalError(e);
            } finally {
                if (!ikeInitSuccess) {
                    if (mLocalIkeSpiResource != null) {
                        mLocalIkeSpiResource.close();
                        mLocalIkeSpiResource = null;
                    }
                    if (mRemoteIkeSpiResource != null) {
                        mRemoteIkeSpiResource.close();
                        mRemoteIkeSpiResource = null;
                    }
                }
            }
        }

        private IkeMessage buildIkeInitReq() throws IOException {
            // Generate IKE SPI
            mLocalIkeSpiResource = mIkeSpiGenerator.allocateSpi(mLocalAddress);

            long initSpi = mLocalIkeSpiResource.getSpi();
            long respSpi = 0;

            // It is validated in IkeSessionParams.Builder to ensure IkeSessionParams has at least
            // one IkeSaProposal and all SaProposals are valid for IKE SA negotiation.
            IkeSaProposal[] saProposals = mIkeSessionParams.getSaProposalsInternal();
            List<IkePayload> payloadList =
                    CreateIkeSaHelper.getIkeInitSaRequestPayloads(
                            saProposals,
                            mPeerSelectedDhGroup,
                            initSpi,
                            respSpi,
                            mLocalAddress,
                            mRemoteAddress,
                            mLocalPort,
                            mIkeSocket.getIkeServerPort(),
                            mRandomFactory);
            payloadList.add(
                    new IkeNotifyPayload(
                            IkeNotifyPayload.NOTIFY_TYPE_IKEV2_FRAGMENTATION_SUPPORTED));

            ByteBuffer signatureHashAlgoTypes =
                    ByteBuffer.allocate(
                            IkeAuthDigitalSignPayload.ALL_SIGNATURE_ALGO_TYPES.length * 2);
            for (short type : IkeAuthDigitalSignPayload.ALL_SIGNATURE_ALGO_TYPES) {
                signatureHashAlgoTypes.putShort(type);
            }
            payloadList.add(
                    new IkeNotifyPayload(
                            IkeNotifyPayload.NOTIFY_TYPE_SIGNATURE_HASH_ALGORITHMS,
                            signatureHashAlgoTypes.array()));

            // TODO: Add Notification Payloads according to user configurations.

            // Build IKE header
            IkeHeader ikeHeader =
                    new IkeHeader(
                            initSpi,
                            respSpi,
                            IkePayload.PAYLOAD_TYPE_SA,
                            IkeHeader.EXCHANGE_TYPE_IKE_SA_INIT,
                            false /*isResponseMsg*/,
                            true /*fromIkeInitiator*/,
                            0 /*messageId*/);

            return new IkeMessage(ikeHeader, payloadList);
        }

        private void validateIkeInitResp(IkeMessage reqMsg, IkeMessage respMsg)
                throws IkeProtocolException, IOException {
            IkeHeader respIkeHeader = respMsg.ikeHeader;
            mRemoteIkeSpiResource =
                    mIkeSpiGenerator.allocateSpi(mRemoteAddress, respIkeHeader.ikeResponderSpi);

            int exchangeType = respIkeHeader.exchangeType;
            if (exchangeType != IkeHeader.EXCHANGE_TYPE_IKE_SA_INIT) {
                throw new InvalidSyntaxException(
                        "Expected EXCHANGE_TYPE_IKE_SA_INIT but received: " + exchangeType);
            }

            IkeSaPayload respSaPayload = null;
            IkeKePayload respKePayload = null;

            /**
             * There MAY be multiple NAT_DETECTION_SOURCE_IP payloads in a message if the sender
             * does not know which of several network attachments will be used to send the packet.
             */
            List<IkeNotifyPayload> natSourcePayloads = new LinkedList<>();
            IkeNotifyPayload natDestPayload = null;

            boolean hasNoncePayload = false;

            for (IkePayload payload : respMsg.ikePayloadList) {
                switch (payload.payloadType) {
                    case IkePayload.PAYLOAD_TYPE_SA:
                        respSaPayload = (IkeSaPayload) payload;
                        break;
                    case IkePayload.PAYLOAD_TYPE_KE:
                        respKePayload = (IkeKePayload) payload;
                        break;
                    case IkePayload.PAYLOAD_TYPE_CERT_REQUEST:
                        // Certificates unconditionally sent (only) for Digital Signature Auth
                        break;
                    case IkePayload.PAYLOAD_TYPE_NONCE:
                        hasNoncePayload = true;
                        mIkeRespNoncePayload = (IkeNoncePayload) payload;
                        break;
                    case IkePayload.PAYLOAD_TYPE_VENDOR:
                        mRemoteVendorIds.add(((IkeVendorPayload) payload).vendorId);
                        break;
                    case IkePayload.PAYLOAD_TYPE_NOTIFY:
                        IkeNotifyPayload notifyPayload = (IkeNotifyPayload) payload;

                        if (notifyPayload.isErrorNotify()) {
                            throw notifyPayload.validateAndBuildIkeException();
                        }

                        switch (notifyPayload.notifyType) {
                            case NOTIFY_TYPE_NAT_DETECTION_SOURCE_IP:
                                natSourcePayloads.add(notifyPayload);
                                break;
                            case NOTIFY_TYPE_NAT_DETECTION_DESTINATION_IP:
                                if (natDestPayload != null) {
                                    throw new InvalidSyntaxException(
                                            "More than one"
                                                    + " NOTIFY_TYPE_NAT_DETECTION_DESTINATION_IP"
                                                    + " found");
                                }
                                natDestPayload = notifyPayload;
                                break;
                            case NOTIFY_TYPE_IKEV2_FRAGMENTATION_SUPPORTED:
                                mSupportFragment = true;
                                mEnabledExtensions.add(EXTENSION_TYPE_FRAGMENTATION);
                                break;
                            default:
                                // Unknown and unexpected status notifications are ignored as per
                                // RFC7296.
                                logw(
                                        "Received unknown or unexpected status notifications with"
                                                + " notify type: "
                                                + notifyPayload.notifyType);
                        }

                        break;
                    default:
                        logw(
                                "Received unexpected payload in IKE INIT response. Payload type: "
                                        + payload.payloadType);
                }
            }

            if (respSaPayload == null
                    || respKePayload == null
                    || !hasNoncePayload) {
                throw new InvalidSyntaxException("SA, KE, or Nonce payload missing.");
            }

            IkeSaPayload reqSaPayload =
                    reqMsg.getPayloadForType(IkePayload.PAYLOAD_TYPE_SA, IkeSaPayload.class);
            mSaProposal =
                    IkeSaPayload.getVerifiedNegotiatedIkeProposalPair(
                                    reqSaPayload, respSaPayload, mIkeSpiGenerator, mRemoteAddress)
                            .second
                            .saProposal;

            // Build IKE crypto tools using mSaProposal. It is ensured that mSaProposal is valid and
            // has exactly one Transform for each Transform type. Only exception is when
            // combined-mode cipher is used, there will be either no integrity algorithm or an
            // INTEGRITY_ALGORITHM_NONE type algorithm.
            mIkeCipher = IkeCipher.create(mSaProposal.getEncryptionTransforms()[0]);
            if (!mIkeCipher.isAead()) {
                mIkeIntegrity = IkeMacIntegrity.create(mSaProposal.getIntegrityTransforms()[0]);
            }
            mIkePrf = IkeMacPrf.create(mSaProposal.getPrfTransforms()[0]);

            IkeKePayload reqKePayload =
                    reqMsg.getPayloadForType(IkePayload.PAYLOAD_TYPE_KE, IkeKePayload.class);
            if (reqKePayload.dhGroup != respKePayload.dhGroup
                    && respKePayload.dhGroup != mPeerSelectedDhGroup) {
                throw new InvalidSyntaxException("Received KE payload with mismatched DH group.");
            }

            if (mRemoteAddress instanceof Inet4Address) {
                // UDP encapsulation not (currently) supported on IPv6. Even if there is a NAT on
                // IPv6, the best we can currently do is try non-encap'd anyways
                handleNatDetection(respMsg, natSourcePayloads, natDestPayload);
            }
        }

        private void handleNatDetection(
                IkeMessage respMsg,
                List<IkeNotifyPayload> natSourcePayloads,
                IkeNotifyPayload natDestPayload)
                throws InvalidSyntaxException, IOException {
            if (natSourcePayloads.isEmpty() || natDestPayload == null) {
                throw new InvalidSyntaxException("NAT detection notifications missing.");
            }

            // NAT detection
            long initIkeSpi = respMsg.ikeHeader.ikeInitiatorSpi;
            long respIkeSpi = respMsg.ikeHeader.ikeResponderSpi;
            mIsLocalBehindNat = true;
            mIsRemoteBehindNat = true;

            // Check if local node is behind NAT
            byte[] expectedLocalNatData =
                    IkeNotifyPayload.generateNatDetectionData(
                            initIkeSpi, respIkeSpi, mLocalAddress, mLocalPort);
            mIsLocalBehindNat = !Arrays.equals(expectedLocalNatData, natDestPayload.notifyData);

            // Check if the remote node is behind NAT
            byte[] expectedRemoteNatData =
                    IkeNotifyPayload.generateNatDetectionData(
                            initIkeSpi, respIkeSpi, mRemoteAddress, mIkeSocket.getIkeServerPort());
            for (IkeNotifyPayload natPayload : natSourcePayloads) {
                // If none of the received hash matches the expected value, the remote node is
                // behind NAT.
                if (Arrays.equals(expectedRemoteNatData, natPayload.notifyData)) {
                    mIsRemoteBehindNat = false;
                }
            }

            if (mIsLocalBehindNat || mIsRemoteBehindNat) {
                if (!(mRemoteAddress instanceof Inet4Address)) {
                    handleIkeFatalError(
                            new IllegalStateException("Remote IPv6 server was behind a NAT"));
                }

                logd("Switching to UDP encap socket");

                try {
                    IkeSocket newSocket =
                            IkeUdpEncapSocket.getIkeUdpEncapSocket(
                                    mIkeSessionParams.getNetwork(),
                                    mIpSecManager,
                                    IkeSessionStateMachine.this,
                                    getHandler().getLooper());
                    switchToIkeSocket(initIkeSpi, newSocket);
                } catch (ErrnoException | IOException | ResourceUnavailableException e) {
                    handleIkeFatalError(e);
                }

                mIkeNattKeepalive =
                        new IkeNattKeepalive(
                                mContext,
                                NATT_KEEPALIVE_DELAY_SECONDS,
                                (Inet4Address) mLocalAddress,
                                (Inet4Address) mRemoteAddress,
                                ((IkeUdpEncapSocket) mIkeSocket).getUdpEncapsulationSocket(),
                                mIkeSocket.getNetwork(),
                                buildKeepaliveIntent());
                mIkeNattKeepalive.start();
            }
        }

        private PendingIntent buildKeepaliveIntent() {
            return buildIkeAlarmIntent(
                    mContext,
                    ACTION_KEEPALIVE,
                    getIntentIdentifier(),
                    obtainMessage(CMD_ALARM_FIRED, mIkeSessionId, CMD_SEND_KEEPALIVE));
        }

        @Override
        public void exitState() {
            super.exitState();
            mRetransmitter.stopRetransmitting();
        }

        private class UnencryptedRetransmitter extends Retransmitter {
            private UnencryptedRetransmitter(IkeMessage msg) {
                super(getHandler(), msg, mIkeSessionParams.getRetransmissionTimeoutsMillis());

                retransmit();
            }

            @Override
            public void send(IkeMessage msg) {
                // Sends unencrypted
                mIkeSocket.sendIkePacket(msg.encode(), mRemoteAddress);
            }

            @Override
            public void handleRetransmissionFailure() {
                handleIkeFatalError(new IOException("Retransmitting IKE INIT request failure"));
            }
        }
    }

    /**
     * CreateIkeLocalIkeAuthBase represents the common state and functionality required to perform
     * IKE AUTH exchanges in both the EAP and non-EAP flows.
     */
    abstract class CreateIkeLocalIkeAuthBase extends DeleteBase {
        protected Retransmitter mRetransmitter;

        @Override
        protected void triggerRetransmit() {
            mRetransmitter.retransmit();
        }

        // TODO: b/139482382 If receiving a remote request while waiting for the last IKE AUTH
        // response, defer it to next state.

        @Override
        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            IkeSaRecord ikeSaRecord = getIkeSaRecordForPacket(ikeMessage.ikeHeader);

            // Null out last received packet, so the next state (that handles the actual request)
            // does not treat the message as a retransmission.
            ikeSaRecord.updateLastReceivedReqFirstPacket(null);

            // Send to next state; we can't handle this yet.
            deferMessage(message);
        }

        protected IkeMessage buildIkeAuthReqMessage(List<IkePayload> payloadList) {
            // Build IKE header
            IkeHeader ikeHeader =
                    new IkeHeader(
                            mCurrentIkeSaRecord.getInitiatorSpi(),
                            mCurrentIkeSaRecord.getResponderSpi(),
                            IkePayload.PAYLOAD_TYPE_SK,
                            IkeHeader.EXCHANGE_TYPE_IKE_AUTH,
                            false /*isResponseMsg*/,
                            true /*fromIkeInitiator*/,
                            mCurrentIkeSaRecord.getLocalRequestMessageId());

            return new IkeMessage(ikeHeader, payloadList);
        }

        protected void authenticatePsk(
                byte[] psk, IkeAuthPayload authPayload, IkeIdPayload respIdPayload)
                throws AuthenticationFailedException {
            if (authPayload.authMethod != IkeAuthPayload.AUTH_METHOD_PRE_SHARED_KEY) {
                throw new AuthenticationFailedException(
                        "Expected the remote/server to use PSK-based authentication but"
                                + " they used: "
                                + authPayload.authMethod);
            }

            IkeAuthPskPayload pskPayload = (IkeAuthPskPayload) authPayload;
            pskPayload.verifyInboundSignature(
                    psk,
                    mIkeInitResponseBytes,
                    mCurrentIkeSaRecord.nonceInitiator,
                    respIdPayload.getEncodedPayloadBody(),
                    mIkePrf,
                    mCurrentIkeSaRecord.getSkPr());
        }

        protected List<IkePayload> extractChildPayloadsFromMessage(IkeMessage ikeMessage)
                throws InvalidSyntaxException {
            List<IkePayload> list = new LinkedList<>();
            for (IkePayload payload : ikeMessage.ikePayloadList) {
                switch (payload.payloadType) {
                    case PAYLOAD_TYPE_SA: // fall through
                    case PAYLOAD_TYPE_TS_INITIATOR: // fall through
                    case PAYLOAD_TYPE_TS_RESPONDER: // fall through
                    case PAYLOAD_TYPE_CP:
                        list.add(payload);
                        break;
                    case PAYLOAD_TYPE_NOTIFY:
                        if (((IkeNotifyPayload) payload).isNewChildSaNotify()) {
                            list.add(payload);
                        }
                        break;
                    default:
                        // Ignore payloads unrelated with Child negotiation
                }
            }

            // Payload validation is done in ChildSessionStateMachine
            return list;
        }

        protected void performFirstChildNegotiation(
                List<IkePayload> childReqList, List<IkePayload> childRespList) {
            childReqList.add(mIkeInitNoncePayload);
            childRespList.add(mIkeRespNoncePayload);

            deferMessage(
                    obtainMessage(
                            CMD_HANDLE_FIRST_CHILD_NEGOTIATION,
                            new FirstChildNegotiationData(
                                    mFirstChildSessionParams,
                                    mFirstChildCallbacks,
                                    childReqList,
                                    childRespList)));

            transitionTo(mChildProcedureOngoing);
        }

        protected IkeSessionConfiguration buildIkeSessionConfiguration(IkeMessage ikeMessage) {
            IkeConfigPayload configPayload =
                    ikeMessage.getPayloadForType(
                            IkePayload.PAYLOAD_TYPE_CP, IkeConfigPayload.class);
            if (configPayload == null) {
                logi("No config payload in ikeMessage.");
            } else if (configPayload.configType != CONFIG_TYPE_REPLY) {
                logi("Unexpected config payload. Config Type: " + configPayload.configType);
                configPayload = null;
            }

            IkeSessionConnectionInfo ikeConnInfo =
                    new IkeSessionConnectionInfo(
                            mLocalAddress, mRemoteAddress, mIkeSessionParams.getNetwork());

            return new IkeSessionConfiguration(
                    ikeConnInfo, configPayload, mRemoteVendorIds, mEnabledExtensions);
        }

        protected void notifyIkeSessionSetup(IkeMessage msg) {
            IkeSessionConfiguration ikeSessionConfig = buildIkeSessionConfiguration(msg);
            executeUserCallback(
                    () -> {
                        mIkeSessionCallback.onOpened(ikeSessionConfig);
                    });
        }
    }

    /**
     * CreateIkeLocalIkeAuth represents state when IKE library initiates IKE_AUTH exchange.
     *
     * <p>If using EAP, CreateIkeLocalIkeAuth will transition to CreateIkeLocalIkeAuthInEap state
     * after validating the IKE AUTH response.
     */
    class CreateIkeLocalIkeAuth extends CreateIkeLocalIkeAuthBase {
        private boolean mUseEap;

        @Override
        public void enterState() {
            try {
                super.enterState();
                mRetransmitter = new EncryptedRetransmitter(buildIkeAuthReq());
                mUseEap =
                        (IkeSessionParams.IKE_AUTH_METHOD_EAP
                                == mIkeSessionParams.getLocalAuthConfig().mAuthMethod);
            } catch (SpiUnavailableException | ResourceUnavailableException e) {
                // Handle IPsec SPI assigning failure.
                handleIkeFatalError(e);
            }
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            try {
                int exchangeType = ikeMessage.ikeHeader.exchangeType;
                if (exchangeType != IkeHeader.EXCHANGE_TYPE_IKE_AUTH) {
                    throw new InvalidSyntaxException(
                            "Expected EXCHANGE_TYPE_IKE_AUTH but received: " + exchangeType);
                }

                validateIkeAuthResp(ikeMessage);

                List<IkePayload> childReqList =
                        extractChildPayloadsFromMessage(mRetransmitter.getMessage());
                if (mUseEap) {
                    // childReqList needed after EAP completed, so persist to IkeSessionStateMachine
                    // state.
                    mFirstChildReqList = childReqList;

                    IkeEapPayload ikeEapPayload =
                            ikeMessage.getPayloadForType(
                                    IkePayload.PAYLOAD_TYPE_EAP, IkeEapPayload.class);
                    if (ikeEapPayload == null) {
                        throw new AuthenticationFailedException("Missing EAP payload");
                    }
                    deferMessage(obtainMessage(CMD_EAP_START_EAP_AUTH, ikeEapPayload));
                    transitionTo(mCreateIkeLocalIkeAuthInEap);
                } else {
                    notifyIkeSessionSetup(ikeMessage);

                    performFirstChildNegotiation(
                            childReqList, extractChildPayloadsFromMessage(ikeMessage));
                }
            } catch (IkeProtocolException e) {
                if (!mUseEap) {
                    // Notify the remote because they may have set up the IKE SA.
                    sendEncryptedIkeMessage(buildIkeDeleteReq(mCurrentIkeSaRecord));
                }
                handleIkeFatalError(e);
            }
        }

        @Override
        protected void handleResponseGenericProcessError(
                IkeSaRecord ikeSaRecord, InvalidSyntaxException ikeException) {
            mRetransmitter.stopRetransmitting();

            if (!mUseEap) {
                // Notify the remote because they may have set up the IKE SA.
                sendEncryptedIkeMessage(buildIkeDeleteReq(mCurrentIkeSaRecord));
            }
            handleIkeFatalError(ikeException);
        }

        private IkeMessage buildIkeAuthReq()
                throws SpiUnavailableException, ResourceUnavailableException {
            List<IkePayload> payloadList = new LinkedList<>();

            // Build Identification payloads
            mInitIdPayload =
                    new IkeIdPayload(
                            true /*isInitiator*/, mIkeSessionParams.getLocalIdentification());
            IkeIdPayload respIdPayload =
                    new IkeIdPayload(
                            false /*isInitiator*/, mIkeSessionParams.getRemoteIdentification());
            payloadList.add(mInitIdPayload);
            payloadList.add(respIdPayload);

            if (mIkeSessionParams.hasIkeOption(IKE_OPTION_EAP_ONLY_AUTH)) {
                payloadList.add(new IkeNotifyPayload(NOTIFY_TYPE_EAP_ONLY_AUTHENTICATION));
            }

            // Build Authentication payload
            IkeAuthConfig authConfig = mIkeSessionParams.getLocalAuthConfig();
            switch (authConfig.mAuthMethod) {
                case IkeSessionParams.IKE_AUTH_METHOD_PSK:
                    IkeAuthPskPayload pskPayload =
                            new IkeAuthPskPayload(
                                    ((IkeAuthPskConfig) authConfig).mPsk,
                                    mIkeInitRequestBytes,
                                    mCurrentIkeSaRecord.nonceResponder,
                                    mInitIdPayload.getEncodedPayloadBody(),
                                    mIkePrf,
                                    mCurrentIkeSaRecord.getSkPi());
                    payloadList.add(pskPayload);
                    break;
                case IkeSessionParams.IKE_AUTH_METHOD_PUB_KEY_SIGNATURE:
                    IkeAuthDigitalSignLocalConfig localAuthConfig =
                            (IkeAuthDigitalSignLocalConfig) mIkeSessionParams.getLocalAuthConfig();

                    // Add certificates to list
                    payloadList.add(
                            new IkeCertX509CertPayload(localAuthConfig.getClientEndCertificate()));
                    for (X509Certificate intermediateCert : localAuthConfig.mIntermediateCerts) {
                        payloadList.add(new IkeCertX509CertPayload(intermediateCert));
                    }

                    IkeAuthDigitalSignPayload digitalSignaturePayload =
                            new IkeAuthDigitalSignPayload(
                                    IkeAuthDigitalSignPayload.SIGNATURE_ALGO_RSA_SHA2_512,
                                    localAuthConfig.mPrivateKey,
                                    mIkeInitRequestBytes,
                                    mCurrentIkeSaRecord.nonceResponder,
                                    mInitIdPayload.getEncodedPayloadBody(),
                                    mIkePrf,
                                    mCurrentIkeSaRecord.getSkPi());
                    payloadList.add(digitalSignaturePayload);

                    break;
                case IkeSessionParams.IKE_AUTH_METHOD_EAP:
                    // Do not include AUTH payload when using EAP.
                    break;
                default:
                    cleanUpAndQuit(
                            new IllegalArgumentException(
                                    "Unrecognized authentication method: "
                                            + authConfig.mAuthMethod));
            }

            payloadList.addAll(
                    CreateChildSaHelper.getInitChildCreateReqPayloads(
                            mRandomFactory,
                            mIpSecSpiGenerator,
                            mLocalAddress,
                            mFirstChildSessionParams,
                            true /*isFirstChildSa*/));

            final List<ConfigAttribute> configAttributes = new ArrayList<>();
            configAttributes.addAll(
                    Arrays.asList(
                            CreateChildSaHelper.getConfigAttributes(mFirstChildSessionParams)));
            configAttributes.addAll(
                    Arrays.asList(mIkeSessionParams.getConfigurationAttributesInternal()));
            // Always request app version
            configAttributes.add(new IkeConfigPayload.ConfigAttributeAppVersion());
            payloadList.add(new IkeConfigPayload(false /*isReply*/, configAttributes));

            return buildIkeAuthReqMessage(payloadList);
        }

        private void validateIkeAuthResp(IkeMessage authResp) throws IkeProtocolException {
            // Validate IKE Authentication
            IkeAuthPayload authPayload = null;
            List<IkeCertPayload> certPayloads = new LinkedList<>();

            for (IkePayload payload : authResp.ikePayloadList) {
                switch (payload.payloadType) {
                    case IkePayload.PAYLOAD_TYPE_ID_RESPONDER:
                        mRespIdPayload = (IkeIdPayload) payload;
                        if (!mIkeSessionParams.hasIkeOption(
                                        IkeSessionParams.IKE_OPTION_ACCEPT_ANY_REMOTE_ID)
                                && !mIkeSessionParams
                                        .getRemoteIdentification()
                                        .equals(mRespIdPayload.ikeId)) {
                            throw new AuthenticationFailedException(
                                    "Unrecognized Responder Identification.");
                        }
                        break;
                    case IkePayload.PAYLOAD_TYPE_AUTH:
                        authPayload = (IkeAuthPayload) payload;
                        break;
                    case IkePayload.PAYLOAD_TYPE_CERT:
                        certPayloads.add((IkeCertPayload) payload);
                        break;
                    case IkePayload.PAYLOAD_TYPE_NOTIFY:
                        IkeNotifyPayload notifyPayload = (IkeNotifyPayload) payload;
                        if (notifyPayload.isErrorNotify()) {
                            if (notifyPayload.isNewChildSaNotify()
                                    && authResp.getPayloadForType(
                                                    PAYLOAD_TYPE_AUTH, IkeAuthPayload.class)
                                            != null) {
                                // If error is for creating Child and Auth payload is included, try
                                // to do authentication first and let ChildSessionStateMachine
                                // handle the error later.
                                continue;
                            } else {
                                throw notifyPayload.validateAndBuildIkeException();
                            }

                        } else {
                            // Unknown and unexpected status notifications are ignored as per
                            // RFC7296.
                            logw(
                                    "Received unknown or unexpected status notifications with"
                                            + " notify type: "
                                            + notifyPayload.notifyType);
                        }
                        break;
                    case PAYLOAD_TYPE_SA: // Will be handled separately; fall through
                    case PAYLOAD_TYPE_CP: // Will be handled separately; fall through
                    case PAYLOAD_TYPE_TS_INITIATOR: // Will be handled separately; fall through
                    case PAYLOAD_TYPE_TS_RESPONDER: // Will be handled separately; fall through
                    case PAYLOAD_TYPE_EAP: // Will be handled separately
                        break;
                    default:
                        logw(
                                "Received unexpected payload in IKE AUTH response. Payload"
                                        + " type: "
                                        + payload.payloadType);
                }
            }

            // Verify existence of payloads

            if (authPayload == null && mIkeSessionParams.hasIkeOption(IKE_OPTION_EAP_ONLY_AUTH)) {
                // If EAP-only option is selected, the responder will not send auth payload if it
                // accepts EAP-only authentication. Currently only EAP-only safe methods are
                // proposed to the responder if IKE_OPTION_EAP_ONLY_AUTH option is set. So there is
                // no need to check if the responder selected an EAP-only safe method
                return;
            }

            // Authenticate the remote peer.
            if (authPayload != null && mRespIdPayload != null) {
                authenticate(authPayload, mRespIdPayload, certPayloads);
                return;
            }

            throw new AuthenticationFailedException("ID-Responder or Auth payload is missing.");
        }

        private void authenticate(
                IkeAuthPayload authPayload,
                IkeIdPayload respIdPayload,
                List<IkeCertPayload> certPayloads)
                throws AuthenticationFailedException {
            switch (mIkeSessionParams.getRemoteAuthConfig().mAuthMethod) {
                case IkeSessionParams.IKE_AUTH_METHOD_PSK:
                    authenticatePsk(
                            ((IkeAuthPskConfig) mIkeSessionParams.getRemoteAuthConfig()).mPsk,
                            authPayload,
                            respIdPayload);
                    break;
                case IkeSessionParams.IKE_AUTH_METHOD_PUB_KEY_SIGNATURE:
                    authenticateDigitalSignature(
                            certPayloads,
                            ((IkeAuthDigitalSignRemoteConfig)
                                            mIkeSessionParams.getRemoteAuthConfig())
                                    .mTrustAnchor,
                            authPayload,
                            respIdPayload);
                    break;
                default:
                    cleanUpAndQuit(
                            new IllegalArgumentException(
                                    "Unrecognized auth method: " + authPayload.authMethod));
            }
        }

        private void authenticateDigitalSignature(
                List<IkeCertPayload> certPayloads,
                TrustAnchor trustAnchor,
                IkeAuthPayload authPayload,
                IkeIdPayload respIdPayload)
                throws AuthenticationFailedException {
            if (authPayload.authMethod != IkeAuthPayload.AUTH_METHOD_RSA_DIGITAL_SIGN
                    && authPayload.authMethod != IkeAuthPayload.AUTH_METHOD_GENERIC_DIGITAL_SIGN) {
                throw new AuthenticationFailedException(
                        "Expected the remote/server to use digital-signature-based authentication"
                                + " but they used: "
                                + authPayload.authMethod);
            }

            X509Certificate endCert = null;
            List<X509Certificate> certList = new LinkedList<>();

            // TODO: b/122676944 Extract CRL from IkeCrlPayload when we support IkeCrlPayload
            for (IkeCertPayload certPayload : certPayloads) {
                X509Certificate cert = ((IkeCertX509CertPayload) certPayload).certificate;

                // The first certificate MUST be the end entity certificate.
                if (endCert == null) endCert = cert;
                certList.add(cert);
            }

            if (endCert == null) {
                throw new AuthenticationFailedException(
                        "The remote/server failed to provide a end certificate");
            }

            respIdPayload.validateEndCertIdOrThrow(endCert);

            Set<TrustAnchor> trustAnchorSet =
                    trustAnchor == null ? null : Collections.singleton(trustAnchor);

            IkeCertPayload.validateCertificates(
                    endCert, certList, null /*crlList*/, trustAnchorSet);

            IkeAuthDigitalSignPayload signPayload = (IkeAuthDigitalSignPayload) authPayload;
            signPayload.verifyInboundSignature(
                    endCert,
                    mIkeInitResponseBytes,
                    mCurrentIkeSaRecord.nonceInitiator,
                    respIdPayload.getEncodedPayloadBody(),
                    mIkePrf,
                    mCurrentIkeSaRecord.getSkPr());
        }

        @Override
        public void exitState() {
            mRetransmitter.stopRetransmitting();
        }
    }

    /**
     * CreateIkeLocalIkeAuthInEap represents the state when the IKE library authenticates the client
     * with an EAP session.
     */
    class CreateIkeLocalIkeAuthInEap extends CreateIkeLocalIkeAuthBase {
        private EapAuthenticator mEapAuthenticator;

        @Override
        public void enterState() {
            IkeSessionParams.IkeAuthEapConfig ikeAuthEapConfig =
                    (IkeSessionParams.IkeAuthEapConfig) mIkeSessionParams.getLocalAuthConfig();

            // TODO(b/148689509): Pass in deterministic random when test mode is enabled
            mEapAuthenticator =
                    mEapAuthenticatorFactory.newEapAuthenticator(
                            getHandler().getLooper(),
                            new IkeEapCallback(),
                            mContext,
                            ikeAuthEapConfig.mEapConfig,
                            mRandomFactory);
        }

        @Override
        public boolean processStateMessage(Message msg) {
            switch (msg.what) {
                case CMD_EAP_START_EAP_AUTH:
                    IkeEapPayload ikeEapPayload = (IkeEapPayload) msg.obj;
                    mEapAuthenticator.processEapMessage(ikeEapPayload.eapMessage);

                    return HANDLED;
                case CMD_EAP_OUTBOUND_MSG_READY:
                    byte[] eapMsgBytes = (byte[]) msg.obj;
                    IkeEapPayload eapPayload = new IkeEapPayload(eapMsgBytes);

                    // Setup new retransmitter with EAP response
                    mRetransmitter =
                            new EncryptedRetransmitter(
                                    buildIkeAuthReqMessage(Arrays.asList(eapPayload)));

                    return HANDLED;
                case CMD_EAP_ERRORED:
                    handleIkeFatalError(new AuthenticationFailedException((Throwable) msg.obj));
                    return HANDLED;
                case CMD_EAP_FAILED:
                    AuthenticationFailedException exception =
                            new AuthenticationFailedException("EAP Authentication Failed");

                    handleIkeFatalError(exception);
                    return HANDLED;
                case CMD_EAP_FINISH_EAP_AUTH:
                    deferMessage(msg);
                    transitionTo(mCreateIkeLocalIkeAuthPostEap);

                    return HANDLED;
                default:
                    return super.processStateMessage(msg);
            }
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            try {
                mRetransmitter.stopRetransmitting();

                int exchangeType = ikeMessage.ikeHeader.exchangeType;
                if (exchangeType != IkeHeader.EXCHANGE_TYPE_IKE_AUTH) {
                    throw new InvalidSyntaxException(
                            "Expected EXCHANGE_TYPE_IKE_AUTH but received: " + exchangeType);
                }

                IkeEapPayload eapPayload = null;
                for (IkePayload payload : ikeMessage.ikePayloadList) {
                    switch (payload.payloadType) {
                        case IkePayload.PAYLOAD_TYPE_EAP:
                            eapPayload = (IkeEapPayload) payload;
                            break;
                        case IkePayload.PAYLOAD_TYPE_NOTIFY:
                            IkeNotifyPayload notifyPayload = (IkeNotifyPayload) payload;
                            if (notifyPayload.isErrorNotify()) {
                                throw notifyPayload.validateAndBuildIkeException();
                            } else {
                                // Unknown and unexpected status notifications are ignored as per
                                // RFC7296.
                                logw(
                                        "Received unknown or unexpected status notifications with"
                                                + " notify type: "
                                                + notifyPayload.notifyType);
                            }
                            break;
                        default:
                            logw(
                                    "Received unexpected payload in IKE AUTH response. Payload"
                                            + " type: "
                                            + payload.payloadType);
                    }
                }

                if (eapPayload == null) {
                    throw new AuthenticationFailedException("EAP Payload is missing.");
                }

                mEapAuthenticator.processEapMessage(eapPayload.eapMessage);
            } catch (IkeProtocolException exception) {
                handleIkeFatalError(exception);
            }
        }

        @Override
        protected void handleResponseGenericProcessError(
                IkeSaRecord ikeSaRecord, InvalidSyntaxException ikeException) {
            mRetransmitter.stopRetransmitting();
            handleIkeFatalError(ikeException);
        }

        private class IkeEapCallback implements IEapCallback {
            @Override
            public void onSuccess(byte[] msk, byte[] emsk) {
                // Extended MSK not used in IKEv2, drop.
                sendMessage(CMD_EAP_FINISH_EAP_AUTH, msk);
            }

            @Override
            public void onFail() {
                sendMessage(CMD_EAP_FAILED);
            }

            @Override
            public void onResponse(byte[] eapMsg) {
                sendMessage(CMD_EAP_OUTBOUND_MSG_READY, eapMsg);
            }

            @Override
            public void onError(Throwable cause) {
                sendMessage(CMD_EAP_ERRORED, cause);
            }
        }
    }

    /**
     * CreateIkeLocalIkeAuthPostEap represents the state when the IKE library is performing the
     * post-EAP PSK-base authentication run.
     */
    class CreateIkeLocalIkeAuthPostEap extends CreateIkeLocalIkeAuthBase {
        private byte[] mEapMsk = new byte[0];

        @Override
        public boolean processStateMessage(Message msg) {
            switch (msg.what) {
                case CMD_EAP_FINISH_EAP_AUTH:
                    mEapMsk = (byte[]) msg.obj;

                    IkeAuthPskPayload pskPayload =
                            new IkeAuthPskPayload(
                                    mEapMsk,
                                    mIkeInitRequestBytes,
                                    mCurrentIkeSaRecord.nonceResponder,
                                    mInitIdPayload.getEncodedPayloadBody(),
                                    mIkePrf,
                                    mCurrentIkeSaRecord.getSkPi());
                    IkeMessage postEapAuthMsg = buildIkeAuthReqMessage(Arrays.asList(pskPayload));
                    mRetransmitter = new EncryptedRetransmitter(postEapAuthMsg);

                    return HANDLED;
                default:
                    return super.processStateMessage(msg);
            }
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            try {
                int exchangeType = ikeMessage.ikeHeader.exchangeType;
                if (exchangeType != IkeHeader.EXCHANGE_TYPE_IKE_AUTH) {
                    throw new InvalidSyntaxException(
                            "Expected EXCHANGE_TYPE_IKE_AUTH but received: " + exchangeType);
                }

                validateIkeAuthRespPostEap(ikeMessage);
                notifyIkeSessionSetup(ikeMessage);

                performFirstChildNegotiation(
                        mFirstChildReqList, extractChildPayloadsFromMessage(ikeMessage));
            } catch (IkeProtocolException e) {
                // Notify the remote because they may have set up the IKE SA.
                sendEncryptedIkeMessage(buildIkeDeleteReq(mCurrentIkeSaRecord));
                handleIkeFatalError(e);
            }
        }

        @Override
        protected void handleResponseGenericProcessError(
                IkeSaRecord ikeSaRecord, InvalidSyntaxException ikeException) {
            mRetransmitter.stopRetransmitting();
            // Notify the remote because they may have set up the IKE SA.
            sendEncryptedIkeMessage(buildIkeDeleteReq(mCurrentIkeSaRecord));
            handleIkeFatalError(ikeException);
        }

        private void validateIkeAuthRespPostEap(IkeMessage authResp) throws IkeProtocolException {
            IkeAuthPayload authPayload = null;

            for (IkePayload payload : authResp.ikePayloadList) {
                switch (payload.payloadType) {
                    case IkePayload.PAYLOAD_TYPE_AUTH:
                        authPayload = (IkeAuthPayload) payload;
                        break;
                    case IkePayload.PAYLOAD_TYPE_NOTIFY:
                        IkeNotifyPayload notifyPayload = (IkeNotifyPayload) payload;
                        if (notifyPayload.isErrorNotify()) {
                            if (notifyPayload.isNewChildSaNotify()
                                    && authResp.getPayloadForType(
                                                    PAYLOAD_TYPE_AUTH, IkeAuthPayload.class)
                                            != null) {
                                // If error is for creating Child and Auth payload is included, try
                                // to do authentication first and let ChildSessionStateMachine
                                // handle the error later.
                                continue;
                            } else {
                                throw notifyPayload.validateAndBuildIkeException();
                            }

                        } else {
                            // Unknown and unexpected status notifications are ignored as per
                            // RFC7296.
                            logw(
                                    "Received unknown or unexpected status notifications with"
                                            + " notify type: "
                                            + notifyPayload.notifyType);
                        }
                        break;
                    case PAYLOAD_TYPE_SA: // Will be handled separately; fall through
                    case PAYLOAD_TYPE_CP: // Will be handled separately; fall through
                    case PAYLOAD_TYPE_TS_INITIATOR: // Will be handled separately; fall through
                    case PAYLOAD_TYPE_TS_RESPONDER: // Will be handled separately; fall through
                        break;
                    default:
                        logw(
                                "Received unexpected payload in IKE AUTH response. Payload"
                                        + " type: "
                                        + payload.payloadType);
                }
            }

            // Verify existence of payloads
            if (authPayload == null) {
                throw new AuthenticationFailedException("Post-EAP Auth payload missing.");
            }

            authenticatePsk(mEapMsk, authPayload, mRespIdPayload);
        }

        @Override
        public void exitState() {
            mRetransmitter.stopRetransmitting();
        }
    }

    private abstract class RekeyIkeHandlerBase extends DeleteBase {
        private void validateIkeRekeyCommon(IkeMessage ikeMessage) throws InvalidSyntaxException {
            boolean hasSaPayload = false;
            boolean hasKePayload = false;
            boolean hasNoncePayload = false;
            for (IkePayload payload : ikeMessage.ikePayloadList) {
                switch (payload.payloadType) {
                    case IkePayload.PAYLOAD_TYPE_SA:
                        hasSaPayload = true;
                        break;
                    case IkePayload.PAYLOAD_TYPE_KE:
                        hasKePayload = true;
                        break;
                    case IkePayload.PAYLOAD_TYPE_NONCE:
                        hasNoncePayload = true;
                        break;
                    case IkePayload.PAYLOAD_TYPE_VENDOR:
                        // Vendor payloads allowed, but not verified
                        break;
                    case IkePayload.PAYLOAD_TYPE_NOTIFY:
                        // Notification payloads allowed, but left to handler methods to process.
                        break;
                    default:
                        logw(
                                "Received unexpected payload in IKE REKEY request. Payload type: "
                                        + payload.payloadType);
                }
            }

            if (!hasSaPayload || !hasKePayload || !hasNoncePayload) {
                throw new InvalidSyntaxException("SA, KE or Nonce payload missing.");
            }
        }

        @VisibleForTesting
        void validateIkeRekeyReq(IkeMessage ikeMessage) throws InvalidSyntaxException {
            // Skip validation of exchange type since it has been done during decoding request.

            List<IkeNotifyPayload> notificationPayloads =
                    ikeMessage.getPayloadListForType(
                            IkePayload.PAYLOAD_TYPE_NOTIFY, IkeNotifyPayload.class);
            for (IkeNotifyPayload notifyPayload : notificationPayloads) {
                if (notifyPayload.isErrorNotify()) {
                    logw("Error notifications invalid in request: " + notifyPayload.notifyType);
                }
            }

            validateIkeRekeyCommon(ikeMessage);
        }

        @VisibleForTesting
        void validateIkeRekeyResp(IkeMessage reqMsg, IkeMessage respMsg)
                throws InvalidSyntaxException {
            int exchangeType = respMsg.ikeHeader.exchangeType;
            if (exchangeType != IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA
                    && exchangeType != IkeHeader.EXCHANGE_TYPE_INFORMATIONAL) {
                throw new InvalidSyntaxException(
                        "Expected Rekey response (CREATE_CHILD_SA or INFORMATIONAL) but received: "
                                + exchangeType);
            }

            List<IkeNotifyPayload> notificationPayloads =
                    respMsg.getPayloadListForType(
                            IkePayload.PAYLOAD_TYPE_NOTIFY, IkeNotifyPayload.class);
            for (IkeNotifyPayload notifyPayload : notificationPayloads) {
                if (notifyPayload.isErrorNotify()) {
                    // Error notifications found. Stop validation for SA negotiation.
                    return;
                }
            }

            validateIkeRekeyCommon(respMsg);

            // Verify DH groups matching
            IkeKePayload reqKePayload =
                    reqMsg.getPayloadForType(IkePayload.PAYLOAD_TYPE_KE, IkeKePayload.class);
            IkeKePayload respKePayload =
                    respMsg.getPayloadForType(IkePayload.PAYLOAD_TYPE_KE, IkeKePayload.class);
            if (reqKePayload.dhGroup != respKePayload.dhGroup) {
                throw new InvalidSyntaxException("Received KE payload with mismatched DH group.");
            }
        }

        // It doesn't make sense to include multiple error notify payloads in one response. If it
        // happens, IKE Session will only handle the most severe one.
        protected boolean handleErrorNotifyIfExists(IkeMessage respMsg, boolean isSimulRekey) {
            IkeNotifyPayload invalidSyntaxNotifyPayload = null;
            IkeNotifyPayload tempFailureNotifyPayload = null;
            IkeNotifyPayload firstErrorNotifyPayload = null;

            List<IkeNotifyPayload> notificationPayloads =
                    respMsg.getPayloadListForType(
                            IkePayload.PAYLOAD_TYPE_NOTIFY, IkeNotifyPayload.class);
            for (IkeNotifyPayload notifyPayload : notificationPayloads) {
                if (!notifyPayload.isErrorNotify()) continue;

                if (firstErrorNotifyPayload == null) firstErrorNotifyPayload = notifyPayload;

                if (ERROR_TYPE_INVALID_SYNTAX == notifyPayload.notifyType) {
                    invalidSyntaxNotifyPayload = notifyPayload;
                } else if (ERROR_TYPE_TEMPORARY_FAILURE == notifyPayload.notifyType) {
                    tempFailureNotifyPayload = notifyPayload;
                }
            }

            // No error Notify Payload included in this response.
            if (firstErrorNotifyPayload == null) return NOT_HANDLED;

            // Handle Invalid Syntax if it exists
            if (invalidSyntaxNotifyPayload != null) {
                try {
                    IkeProtocolException exception =
                            invalidSyntaxNotifyPayload.validateAndBuildIkeException();
                    handleIkeFatalError(exception);
                } catch (InvalidSyntaxException e) {
                    // Error notify payload has invalid syntax
                    handleIkeFatalError(e);
                }
                return HANDLED;
            }

            if (tempFailureNotifyPayload != null) {
                // Handle Temporary Failure if exists
                loge("Received TEMPORARY_FAILURE for rekey IKE. Already handled during decoding.");
            } else {
                // Handle other errors
                loge(
                        "Received error notification: "
                                + firstErrorNotifyPayload.notifyType
                                + " for rekey IKE. Schedule a retry");
                if (!isSimulRekey) {
                    mCurrentIkeSaRecord.rescheduleRekey(RETRY_INTERVAL_MS);
                }
            }

            if (isSimulRekey) {
                transitionTo(mRekeyIkeRemoteDelete);
            } else {
                transitionTo(mIdle);
            }
            return HANDLED;
        }

        protected IkeSaRecord validateAndBuildIkeSa(
                IkeMessage reqMsg, IkeMessage respMessage, boolean isLocalInit)
                throws IkeProtocolException, GeneralSecurityException, IOException {
            InetAddress initAddr = isLocalInit ? mLocalAddress : mRemoteAddress;
            InetAddress respAddr = isLocalInit ? mRemoteAddress : mLocalAddress;

            Pair<IkeProposal, IkeProposal> negotiatedProposals = null;
            try {
                IkeSaPayload reqSaPayload =
                        reqMsg.getPayloadForType(IkePayload.PAYLOAD_TYPE_SA, IkeSaPayload.class);
                IkeSaPayload respSaPayload =
                        respMessage.getPayloadForType(
                                IkePayload.PAYLOAD_TYPE_SA, IkeSaPayload.class);

                // Throw exception or return valid negotiated proposal with allocated SPIs
                negotiatedProposals =
                        IkeSaPayload.getVerifiedNegotiatedIkeProposalPair(
                                reqSaPayload, respSaPayload, mIkeSpiGenerator, mRemoteAddress);
                IkeProposal reqProposal = negotiatedProposals.first;
                IkeProposal respProposal = negotiatedProposals.second;

                IkeMacPrf newPrf;
                IkeCipher newCipher;
                IkeMacIntegrity newIntegrity = null;

                newCipher = IkeCipher.create(respProposal.saProposal.getEncryptionTransforms()[0]);
                if (!newCipher.isAead()) {
                    newIntegrity =
                            IkeMacIntegrity.create(
                                    respProposal.saProposal.getIntegrityTransforms()[0]);
                }
                newPrf = IkeMacPrf.create(respProposal.saProposal.getPrfTransforms()[0]);

                // Build new SaRecord
                long remoteSpi =
                        isLocalInit
                                ? respProposal.getIkeSpiResource().getSpi()
                                : reqProposal.getIkeSpiResource().getSpi();
                IkeSaRecord newSaRecord =
                        IkeSaRecord.makeRekeyedIkeSaRecord(
                                mCurrentIkeSaRecord,
                                mIkePrf,
                                reqMsg,
                                respMessage,
                                reqProposal.getIkeSpiResource(),
                                respProposal.getIkeSpiResource(),
                                newPrf,
                                newIntegrity == null ? 0 : newIntegrity.getKeyLength(),
                                newCipher.getKeyLength(),
                                isLocalInit,
                                buildSaLifetimeAlarmScheduler(remoteSpi));
                addIkeSaRecord(newSaRecord);

                mIkeCipher = newCipher;
                mIkePrf = newPrf;
                mIkeIntegrity = newIntegrity;

                return newSaRecord;
            } catch (IkeProtocolException | GeneralSecurityException | IOException e) {
                if (negotiatedProposals != null) {
                    negotiatedProposals.first.getIkeSpiResource().close();
                    negotiatedProposals.second.getIkeSpiResource().close();
                }
                throw e;
            }
        }
    }

    /** RekeyIkeLocalCreate represents state when IKE library initiates Rekey IKE exchange. */
    class RekeyIkeLocalCreate extends RekeyIkeHandlerBase {
        protected Retransmitter mRetransmitter;

        @Override
        public void enterState() {
            try {
                mRetransmitter = new EncryptedRetransmitter(buildIkeRekeyReq());
            } catch (IOException e) {
                loge("Fail to assign IKE SPI for rekey. Schedule a retry.", e);
                mCurrentIkeSaRecord.rescheduleRekey(RETRY_INTERVAL_MS);
                transitionTo(mIdle);
            }
        }

        @Override
        protected void triggerRetransmit() {
            mRetransmitter.retransmit();
        }

        @Override
        protected void handleTempFailure() {
            mTempFailHandler.handleTempFailure();
            mCurrentIkeSaRecord.rescheduleRekey(RETRY_INTERVAL_MS);
        }

        /**
         * Builds a IKE Rekey request, reusing the current proposal
         *
         * <p>As per RFC 7296, rekey messages are of format: { HDR { SK { SA, Ni, KEi } } }
         *
         * <p>This method currently reuses agreed upon proposal.
         */
        private IkeMessage buildIkeRekeyReq() throws IOException {
            // TODO: Evaluate if we need to support different proposals for rekeys
            IkeSaProposal[] saProposals = new IkeSaProposal[] {mSaProposal};

            // No need to allocate SPIs; they will be allocated as part of the
            // getRekeyIkeSaRequestPayloads
            List<IkePayload> payloadList =
                    CreateIkeSaHelper.getRekeyIkeSaRequestPayloads(
                            saProposals, mIkeSpiGenerator, mLocalAddress, mRandomFactory);

            // Build IKE header
            IkeHeader ikeHeader =
                    new IkeHeader(
                            mCurrentIkeSaRecord.getInitiatorSpi(),
                            mCurrentIkeSaRecord.getResponderSpi(),
                            IkePayload.PAYLOAD_TYPE_SK,
                            IkeHeader.EXCHANGE_TYPE_CREATE_CHILD_SA,
                            false /*isResponseMsg*/,
                            mCurrentIkeSaRecord.isLocalInit,
                            mCurrentIkeSaRecord.getLocalRequestMessageId());

            return new IkeMessage(ikeHeader, payloadList);
        }

        @Override
        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            switch (ikeExchangeSubType) {
                case IKE_EXCHANGE_SUBTYPE_DELETE_IKE:
                    handleDeleteSessionRequest(ikeMessage);
                    break;
                default:
                    // TODO: Implement simultaneous rekey
                    buildAndSendErrorNotificationResponse(
                            mCurrentIkeSaRecord,
                            ikeMessage.ikeHeader.messageId,
                            ERROR_TYPE_TEMPORARY_FAILURE);
            }
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            try {
                // Validate syntax
                validateIkeRekeyResp(mRetransmitter.getMessage(), ikeMessage);

                // Handle error notifications if they exist
                if (handleErrorNotifyIfExists(ikeMessage, false /*isSimulRekey*/) == NOT_HANDLED) {
                    // No error notifications included. Negotiate new SA
                    mLocalInitNewIkeSaRecord =
                            validateAndBuildIkeSa(
                                    mRetransmitter.getMessage(), ikeMessage, true /*isLocalInit*/);
                    transitionTo(mRekeyIkeLocalDelete);
                }

                // Stop retransmissions
                mRetransmitter.stopRetransmitting();
            } catch (IkeProtocolException e) {
                if (e instanceof InvalidSyntaxException) {
                    handleProcessRespOrSaCreationFailureAndQuit(e);
                } else {
                    handleProcessRespOrSaCreationFailureAndQuit(
                            new InvalidSyntaxException(
                                    "Error in processing IKE Rekey-Create response", e));
                }

            } catch (GeneralSecurityException | IOException e) {
                handleProcessRespOrSaCreationFailureAndQuit(
                        new IkeInternalException("Error in creating a new IKE SA during rekey", e));
            }
        }

        @Override
        protected void handleResponseGenericProcessError(
                IkeSaRecord ikeSaRecord, InvalidSyntaxException ikeException) {
            handleProcessRespOrSaCreationFailureAndQuit(ikeException);
        }

        private void handleProcessRespOrSaCreationFailureAndQuit(IkeException exception) {
            // We don't retry rekey if failure was caused by invalid response or SA creation error.
            // Reason is there is no way to notify the remote side the old SA is still alive but the
            // new one has failed.

            mRetransmitter.stopRetransmitting();

            sendEncryptedIkeMessage(buildIkeDeleteReq(mCurrentIkeSaRecord));
            handleIkeFatalError(exception);
        }
    }

    /**
     * SimulRekeyIkeLocalCreate represents the state where IKE library has replied to rekey request
     * sent from the remote and is waiting for a rekey response for a locally initiated rekey
     * request.
     *
     * <p>SimulRekeyIkeLocalCreate extends RekeyIkeLocalCreate so that it can call super class to
     * validate incoming rekey response against locally initiated rekey request.
     */
    class SimulRekeyIkeLocalCreate extends RekeyIkeLocalCreate {
        @Override
        public void enterState() {
            mRetransmitter = new EncryptedRetransmitter(null);
            // TODO: Populate super.mRetransmitter from state initialization data
            // Do not send request.
        }

        public IkeMessage buildRequest() {
            throw new UnsupportedOperationException(
                    "Do not support sending request in " + getCurrentState().getName());
        }

        @Override
        public void exitState() {
            // Do nothing.
        }

        @Override
        public boolean processStateMessage(Message message) {
            switch (message.what) {
                case CMD_RECEIVE_IKE_PACKET:
                    ReceivedIkePacket receivedIkePacket = (ReceivedIkePacket) message.obj;
                    IkeHeader ikeHeader = receivedIkePacket.ikeHeader;

                    if (mRemoteInitNewIkeSaRecord == getIkeSaRecordForPacket(ikeHeader)) {
                        deferMessage(message);
                    } else {
                        handleReceivedIkePacket(message);
                    }
                    return HANDLED;

                default:
                    return super.processStateMessage(message);
            }
        }

        @Override
        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            switch (ikeExchangeSubType) {
                case IKE_EXCHANGE_SUBTYPE_DELETE_IKE:
                    deferMessage(message);
                    return;
                default:
                    // TODO: Add more cases for other types of request.
            }
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            try {
                validateIkeRekeyResp(mRetransmitter.getMessage(), ikeMessage);

                // TODO: Check and handle error notifications before SA negotiation

                mLocalInitNewIkeSaRecord =
                        validateAndBuildIkeSa(
                                mRetransmitter.getMessage(), ikeMessage, true /*isLocalInit*/);
                transitionTo(mSimulRekeyIkeLocalDeleteRemoteDelete);
            } catch (IkeProtocolException e) {
                // TODO: Handle processing errors.
            } catch (GeneralSecurityException e) {
                // TODO: Fatal - kill session.
            } catch (IOException e) {
                // TODO: SPI allocation collided - delete new IKE SA, retry rekey.
            }
        }
    }

    /** RekeyIkeDeleteBase represents common behaviours of deleting stage during rekeying IKE SA. */
    private abstract class RekeyIkeDeleteBase extends DeleteBase {
        @Override
        public boolean processStateMessage(Message message) {
            switch (message.what) {
                case CMD_RECEIVE_IKE_PACKET:
                    ReceivedIkePacket receivedIkePacket = (ReceivedIkePacket) message.obj;
                    IkeHeader ikeHeader = receivedIkePacket.ikeHeader;

                    // Verify that this message is correctly authenticated and encrypted:
                    IkeSaRecord ikeSaRecord = getIkeSaRecordForPacket(ikeHeader);
                    boolean isMessageOnNewSa = false;
                    if (ikeSaRecord != null && mIkeSaRecordSurviving == ikeSaRecord) {
                        DecodeResult decodeResult =
                                IkeMessage.decode(
                                        ikeHeader.isResponseMsg
                                                ? ikeSaRecord.getLocalRequestMessageId()
                                                : ikeSaRecord.getRemoteRequestMessageId(),
                                        mIkeIntegrity,
                                        mIkeCipher,
                                        ikeSaRecord,
                                        ikeHeader,
                                        receivedIkePacket.ikePacketBytes,
                                        ikeSaRecord.getCollectedFragments(ikeHeader.isResponseMsg));
                        isMessageOnNewSa =
                                (decodeResult.status == DECODE_STATUS_PROTECTED_ERROR)
                                        || (decodeResult.status == DECODE_STATUS_OK)
                                        || (decodeResult.status == DECODE_STATUS_PARTIAL);
                    }

                    // Authenticated request received on the new/surviving SA; treat it as
                    // an acknowledgement that the remote has successfully rekeyed.
                    if (isMessageOnNewSa) {
                        State nextState = mIdle;

                        // This is the first IkeMessage seen on the new SA. It cannot be a response.
                        // Likewise, if it a request, it must not be a retransmission. Verify msgId.
                        // If either condition happens, consider rekey a success, but immediately
                        // kill the session.
                        if (ikeHeader.isResponseMsg
                                || ikeSaRecord.getRemoteRequestMessageId() - ikeHeader.messageId
                                        != 0) {
                            nextState = mDeleteIkeLocalDelete;
                        } else {
                            deferMessage(message);
                        }

                        // Locally close old (and losing) IKE SAs. As a result of not waiting for
                        // delete responses, the old SA can be left in a state where the stored ID
                        // is no longer correct. However, this finishRekey() call will remove that
                        // SA, so it doesn't matter.
                        finishRekey();
                        transitionTo(nextState);
                    } else {
                        handleReceivedIkePacket(message);
                    }

                    return HANDLED;
                default:
                    return super.processStateMessage(message);
                    // TODO: Add more cases for other packet types.
            }
        }

        // Rekey timer for old (and losing) SAs will be cancelled as part of the closing of the SA.
        protected void finishRekey() {
            mCurrentIkeSaRecord = mIkeSaRecordSurviving;
            mLocalInitNewIkeSaRecord = null;
            mRemoteInitNewIkeSaRecord = null;

            mIkeSaRecordSurviving = null;

            if (mIkeSaRecordAwaitingLocalDel != null) {
                removeIkeSaRecord(mIkeSaRecordAwaitingLocalDel);
                mIkeSaRecordAwaitingLocalDel.close();
                mIkeSaRecordAwaitingLocalDel = null;
            }

            if (mIkeSaRecordAwaitingRemoteDel != null) {
                removeIkeSaRecord(mIkeSaRecordAwaitingRemoteDel);
                mIkeSaRecordAwaitingRemoteDel.close();
                mIkeSaRecordAwaitingRemoteDel = null;
            }

            synchronized (mChildCbToSessions) {
                for (ChildSessionStateMachine child : mChildCbToSessions.values()) {
                    child.setSkD(mCurrentIkeSaRecord.getSkD());
                }
            }

            // TODO: Update prf of all child sessions
        }
    }

    /**
     * SimulRekeyIkeLocalDeleteRemoteDelete represents the deleting stage during simultaneous
     * rekeying when IKE library is waiting for both a Delete request and a Delete response.
     */
    class SimulRekeyIkeLocalDeleteRemoteDelete extends RekeyIkeDeleteBase {
        private Retransmitter mRetransmitter;

        @Override
        public void enterState() {
            // Detemine surviving IKE SA. According to RFC 7296: "The new IKE SA containing the
            // lowest nonce SHOULD be deleted by the node that created it, and the other surviving
            // new IKE SA MUST inherit all the Child SAs."
            if (mLocalInitNewIkeSaRecord.compareTo(mRemoteInitNewIkeSaRecord) > 0) {
                mIkeSaRecordSurviving = mLocalInitNewIkeSaRecord;
                mIkeSaRecordAwaitingLocalDel = mCurrentIkeSaRecord;
                mIkeSaRecordAwaitingRemoteDel = mRemoteInitNewIkeSaRecord;
            } else {
                mIkeSaRecordSurviving = mRemoteInitNewIkeSaRecord;
                mIkeSaRecordAwaitingLocalDel = mLocalInitNewIkeSaRecord;
                mIkeSaRecordAwaitingRemoteDel = mCurrentIkeSaRecord;
            }
            mRetransmitter =
                    new EncryptedRetransmitter(
                            mIkeSaRecordAwaitingLocalDel,
                            buildIkeDeleteReq(mIkeSaRecordAwaitingLocalDel));
            // TODO: Set timer awaiting for delete request.
        }

        @Override
        protected void triggerRetransmit() {
            mRetransmitter.retransmit();
        }

        @Override
        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            IkeSaRecord ikeSaRecordForPacket = getIkeSaRecordForPacket(ikeMessage.ikeHeader);
            switch (ikeExchangeSubType) {
                case IKE_EXCHANGE_SUBTYPE_DELETE_IKE:
                    try {
                        validateIkeDeleteReq(ikeMessage, mIkeSaRecordAwaitingRemoteDel);
                        IkeMessage respMsg =
                                buildIkeDeleteResp(ikeMessage, mIkeSaRecordAwaitingRemoteDel);
                        removeIkeSaRecord(mIkeSaRecordAwaitingRemoteDel);
                        // TODO: Encode and send response and close
                        // mIkeSaRecordAwaitingRemoteDel.
                        // TODO: Stop timer awating delete request.
                        transitionTo(mSimulRekeyIkeLocalDelete);
                    } catch (InvalidSyntaxException e) {
                        logd("Validation failed for delete request", e);
                        // TODO: Shutdown - fatal error
                    }
                    return;
                default:
                    // TODO: Reply with TEMPORARY_FAILURE
            }
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            try {
                validateIkeDeleteResp(ikeMessage, mIkeSaRecordAwaitingLocalDel);
                finishDeleteIkeSaAwaitingLocalDel();
            } catch (InvalidSyntaxException e) {
                loge("Invalid syntax on IKE Delete response. Shutting down anyways", e);
                finishDeleteIkeSaAwaitingLocalDel();
            } catch (IllegalStateException e) {
                // Response received on incorrect SA
                cleanUpAndQuit(e);
            }
        }

        @Override
        protected void handleResponseGenericProcessError(
                IkeSaRecord ikeSaRecord, InvalidSyntaxException exception) {
            if (mIkeSaRecordAwaitingLocalDel == ikeSaRecord) {
                loge("Invalid syntax on IKE Delete response. Shutting down anyways", exception);
                finishDeleteIkeSaAwaitingLocalDel();
            } else {
                cleanUpAndQuit(
                        new IllegalStateException("Delete response received on incorrect SA"));
            }
        }

        private void finishDeleteIkeSaAwaitingLocalDel() {
            mRetransmitter.stopRetransmitting();

            removeIkeSaRecord(mIkeSaRecordAwaitingLocalDel);
            mIkeSaRecordAwaitingLocalDel.close();
            mIkeSaRecordAwaitingLocalDel = null;

            transitionTo(mSimulRekeyIkeRemoteDelete);
        }

        @Override
        public void exitState() {
            finishRekey();
            mRetransmitter.stopRetransmitting();
            // TODO: Stop awaiting delete request timer.
        }
    }

    /**
     * SimulRekeyIkeLocalDelete represents the state when IKE library is waiting for a Delete
     * response during simultaneous rekeying.
     */
    class SimulRekeyIkeLocalDelete extends RekeyIkeDeleteBase {
        private Retransmitter mRetransmitter;

        @Override
        public void enterState() {
            mRetransmitter = new EncryptedRetransmitter(mIkeSaRecordAwaitingLocalDel, null);
            // TODO: Populate mRetransmitter from state initialization data.
        }

        @Override
        protected void triggerRetransmit() {
            mRetransmitter.retransmit();
        }

        @Override
        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            // Always return a TEMPORARY_FAILURE. In no case should we accept a message on an SA
            // that is going away. All messages on the new SA is caught in RekeyIkeDeleteBase
            buildAndSendErrorNotificationResponse(
                    mIkeSaRecordAwaitingLocalDel,
                    ikeMessage.ikeHeader.messageId,
                    ERROR_TYPE_TEMPORARY_FAILURE);
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            try {
                validateIkeDeleteResp(ikeMessage, mIkeSaRecordAwaitingLocalDel);
                finishRekey();
                transitionTo(mIdle);
            } catch (InvalidSyntaxException e) {
                loge(
                        "Invalid syntax on IKE Delete response. Shutting down old IKE SA and"
                                + " finishing rekey",
                        e);
                finishRekey();
                transitionTo(mIdle);
            } catch (IllegalStateException e) {
                // Response received on incorrect SA
                cleanUpAndQuit(e);
            }
        }

        @Override
        protected void handleResponseGenericProcessError(
                IkeSaRecord ikeSaRecord, InvalidSyntaxException exception) {
            if (mIkeSaRecordAwaitingLocalDel == ikeSaRecord) {
                loge(
                        "Invalid syntax on IKE Delete response. Shutting down old IKE SA and"
                                + " finishing rekey",
                        exception);
                finishRekey();
                transitionTo(mIdle);
            } else {
                cleanUpAndQuit(
                        new IllegalStateException("Delete response received on incorrect SA"));
            }
        }
    }

    /**
     * SimulRekeyIkeRemoteDelete represents the state that waiting for a Delete request during
     * simultaneous rekeying.
     */
    class SimulRekeyIkeRemoteDelete extends RekeyIkeDeleteBase {
        @Override
        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            // At this point, the incoming request can ONLY be on mIkeSaRecordAwaitingRemoteDel - if
            // it was on the surviving SA, it is deferred and the rekey is finished. It is likewise
            // impossible to have this on the local-deleted SA, since the delete has already been
            // acknowledged in the SimulRekeyIkeLocalDeleteRemoteDelete state.
            switch (ikeExchangeSubType) {
                case IKE_EXCHANGE_SUBTYPE_DELETE_IKE:
                    try {
                        validateIkeDeleteReq(ikeMessage, mIkeSaRecordAwaitingRemoteDel);

                        IkeMessage respMsg =
                                buildIkeDeleteResp(ikeMessage, mIkeSaRecordAwaitingRemoteDel);
                        sendEncryptedIkeMessage(mIkeSaRecordAwaitingRemoteDel, respMsg);

                        finishRekey();
                        transitionTo(mIdle);
                    } catch (InvalidSyntaxException e) {
                        // Program error.
                        cleanUpAndQuit(new IllegalStateException(e));
                    }
                    return;
                default:
                    buildAndSendErrorNotificationResponse(
                            mIkeSaRecordAwaitingRemoteDel,
                            ikeMessage.ikeHeader.messageId,
                            ERROR_TYPE_TEMPORARY_FAILURE);
            }
        }
    }

    /**
     * RekeyIkeLocalDelete represents the deleting stage when IKE library is initiating a Rekey
     * procedure.
     *
     * <p>RekeyIkeLocalDelete and SimulRekeyIkeLocalDelete have same behaviours in
     * processStateMessage(). While RekeyIkeLocalDelete overrides enterState() and exitState()
     * methods for initiating and finishing the deleting stage for IKE rekeying.
     */
    class RekeyIkeLocalDelete extends SimulRekeyIkeLocalDelete {
        private Retransmitter mRetransmitter;

        @Override
        public void enterState() {
            mIkeSaRecordSurviving = mLocalInitNewIkeSaRecord;
            mIkeSaRecordAwaitingLocalDel = mCurrentIkeSaRecord;
            mRetransmitter =
                    new EncryptedRetransmitter(
                            mIkeSaRecordAwaitingLocalDel,
                            buildIkeDeleteReq(mIkeSaRecordAwaitingLocalDel));
        }

        @Override
        protected void triggerRetransmit() {
            mRetransmitter.retransmit();
        }

        @Override
        public void exitState() {
            mRetransmitter.stopRetransmitting();
        }
    }

    /**
     * RekeyIkeRemoteDelete represents the deleting stage when responding to a Rekey procedure.
     *
     * <p>RekeyIkeRemoteDelete and SimulRekeyIkeRemoteDelete have same behaviours in
     * processStateMessage(). While RekeyIkeLocalDelete overrides enterState() and exitState()
     * methods for waiting incoming delete request and for finishing the deleting stage for IKE
     * rekeying.
     */
    class RekeyIkeRemoteDelete extends SimulRekeyIkeRemoteDelete {
        @Override
        public void enterState() {
            mIkeSaRecordSurviving = mRemoteInitNewIkeSaRecord;
            mIkeSaRecordAwaitingRemoteDel = mCurrentIkeSaRecord;

            sendMessageDelayed(TIMEOUT_REKEY_REMOTE_DELETE, REKEY_DELETE_TIMEOUT_MS);
        }

        @Override
        public boolean processStateMessage(Message message) {
            // Intercept rekey delete timeout. Assume rekey succeeded since no retransmissions
            // were received.
            if (message.what == TIMEOUT_REKEY_REMOTE_DELETE) {
                finishRekey();
                transitionTo(mIdle);

                return HANDLED;
            } else {
                return super.processStateMessage(message);
            }
        }

        @Override
        public void exitState() {
            removeMessages(TIMEOUT_REKEY_REMOTE_DELETE);
        }
    }

    /** DeleteIkeLocalDelete initiates a deletion request of the current IKE Session. */
    class DeleteIkeLocalDelete extends DeleteBase {
        private Retransmitter mRetransmitter;

        @Override
        public void enterState() {
            mRetransmitter = new EncryptedRetransmitter(buildIkeDeleteReq(mCurrentIkeSaRecord));
        }

        @Override
        protected void triggerRetransmit() {
            mRetransmitter.retransmit();
        }

        @Override
        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            switch (ikeExchangeSubType) {
                case IKE_EXCHANGE_SUBTYPE_DELETE_IKE:
                    handleDeleteSessionRequest(ikeMessage);
                    return;
                default:
                    buildAndSendErrorNotificationResponse(
                            mCurrentIkeSaRecord,
                            ikeMessage.ikeHeader.messageId,
                            ERROR_TYPE_TEMPORARY_FAILURE);
            }
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            try {
                validateIkeDeleteResp(ikeMessage, mCurrentIkeSaRecord);
                executeUserCallback(
                        () -> {
                            mIkeSessionCallback.onClosed();
                        });

                removeIkeSaRecord(mCurrentIkeSaRecord);
                mCurrentIkeSaRecord.close();
                mCurrentIkeSaRecord = null;
                quitNow();
            } catch (InvalidSyntaxException e) {
                handleResponseGenericProcessError(mCurrentIkeSaRecord, e);
            }
        }

        @Override
        protected void handleResponseGenericProcessError(
                IkeSaRecord ikeSaRecord, InvalidSyntaxException exception) {
            loge("Invalid syntax on IKE Delete response. Shutting down anyways", exception);
            handleIkeFatalError(exception);
            quitNow();
        }

        @Override
        public void exitState() {
            mRetransmitter.stopRetransmitting();
        }
    }

    /** DpdIkeLocalInfo initiates a dead peer detection for IKE Session. */
    class DpdIkeLocalInfo extends DeleteBase {
        private Retransmitter mRetransmitter;

        @Override
        public void enterState() {
            mRetransmitter =
                    new EncryptedRetransmitter(
                            buildEncryptedInformationalMessage(
                                    new IkeInformationalPayload[0],
                                    false /*isResp*/,
                                    mCurrentIkeSaRecord.getLocalRequestMessageId()));
        }

        @Override
        protected void triggerRetransmit() {
            mRetransmitter.retransmit();
        }

        @Override
        protected void handleRequestIkeMessage(
                IkeMessage ikeMessage, int ikeExchangeSubType, Message message) {
            switch (ikeExchangeSubType) {
                case IKE_EXCHANGE_SUBTYPE_GENERIC_INFO:
                    handleGenericInfoRequest(ikeMessage);
                    return;
                case IKE_EXCHANGE_SUBTYPE_DELETE_IKE:
                    // Reply and close IKE
                    handleDeleteSessionRequest(ikeMessage);
                    return;
                default:
                    // Reply and stay in current state
                    buildAndSendErrorNotificationResponse(
                            mCurrentIkeSaRecord,
                            ikeMessage.ikeHeader.messageId,
                            ERROR_TYPE_TEMPORARY_FAILURE);
                    return;
            }
        }

        @Override
        protected void handleResponseIkeMessage(IkeMessage ikeMessage) {
            // DPD response usually contains no payload. But since there is not any requirement of
            // it, payload validation will be skipped.
            if (ikeMessage.ikeHeader.exchangeType == IkeHeader.EXCHANGE_TYPE_INFORMATIONAL) {
                transitionTo(mIdle);
                return;
            }

            handleResponseGenericProcessError(
                    mCurrentIkeSaRecord,
                    new InvalidSyntaxException(
                            "Invalid exchange type; expected INFORMATIONAL, but got: "
                                    + ikeMessage.ikeHeader.exchangeType));
        }

        @Override
        protected void handleResponseGenericProcessError(
                IkeSaRecord ikeSaRecord, InvalidSyntaxException exception) {
            loge("Invalid syntax on IKE DPD response.", exception);
            handleIkeFatalError(exception);

            // #exitState will be called when StateMachine quits
            quitNow();
        }

        @Override
        public void exitState() {
            mRetransmitter.stopRetransmitting();
        }
    }

    /**
     * Helper class to generate IKE SA creation payloads, in both request and response directions.
     */
    private static class CreateIkeSaHelper {
        public static List<IkePayload> getIkeInitSaRequestPayloads(
                IkeSaProposal[] saProposals,
                int selectedDhGroup,
                long initIkeSpi,
                long respIkeSpi,
                InetAddress localAddr,
                InetAddress remoteAddr,
                int localPort,
                int remotePort,
                RandomnessFactory randomFactory)
                throws IOException {
            List<IkePayload> payloadList =
                    getCreateIkeSaPayloads(
                            selectedDhGroup,
                            IkeSaPayload.createInitialIkeSaPayload(saProposals),
                            randomFactory);
            if (localAddr instanceof Inet4Address) {
                // Though RFC says Notify-NAT payload is "just after the Ni and Nr payloads (before
                // the optional CERTREQ payload)", it also says recipient MUST NOT reject " messages
                // in which the payloads were not in the "right" order" due to the lack of clarity
                // of the payload order.
                payloadList.add(
                        new IkeNotifyPayload(
                                NOTIFY_TYPE_NAT_DETECTION_SOURCE_IP,
                                IkeNotifyPayload.generateNatDetectionData(
                                        initIkeSpi, respIkeSpi, localAddr, localPort)));
                payloadList.add(
                        new IkeNotifyPayload(
                                NOTIFY_TYPE_NAT_DETECTION_DESTINATION_IP,
                                IkeNotifyPayload.generateNatDetectionData(
                                        initIkeSpi, respIkeSpi, remoteAddr, remotePort)));
            }

            return payloadList;
        }

        public static List<IkePayload> getRekeyIkeSaRequestPayloads(
                IkeSaProposal[] saProposals,
                IkeSpiGenerator ikeSpiGenerator,
                InetAddress localAddr,
                RandomnessFactory randomFactory)
                throws IOException {
            if (localAddr == null) {
                throw new IllegalArgumentException("Local address was null for rekey");
            }

            // Guaranteed to have at least one SA Proposal, since the IKE session was set up
            // properly.
            int selectedDhGroup = saProposals[0].getDhGroupTransforms()[0].id;

            return getCreateIkeSaPayloads(
                    selectedDhGroup,
                    IkeSaPayload.createRekeyIkeSaRequestPayload(
                            saProposals, ikeSpiGenerator, localAddr),
                    randomFactory);
        }

        public static List<IkePayload> getRekeyIkeSaResponsePayloads(
                byte respProposalNumber,
                IkeSaProposal saProposal,
                IkeSpiGenerator ikeSpiGenerator,
                InetAddress localAddr,
                RandomnessFactory randomFactory)
                throws IOException {
            if (localAddr == null) {
                throw new IllegalArgumentException("Local address was null for rekey");
            }

            int selectedDhGroup = saProposal.getDhGroupTransforms()[0].id;

            return getCreateIkeSaPayloads(
                    selectedDhGroup,
                    IkeSaPayload.createRekeyIkeSaResponsePayload(
                            respProposalNumber, saProposal, ikeSpiGenerator, localAddr),
                    randomFactory);
        }

        /**
         * Builds the initial or rekey IKE creation payloads.
         *
         * <p>Will return a non-empty list of IkePayloads, the first of which WILL be the SA payload
         */
        private static List<IkePayload> getCreateIkeSaPayloads(
                int selectedDhGroup, IkeSaPayload saPayload, RandomnessFactory randomFactory)
                throws IOException {
            if (saPayload.proposalList.size() == 0) {
                throw new IllegalArgumentException("Invalid SA proposal list - was empty");
            }

            List<IkePayload> payloadList = new ArrayList<>(3);

            payloadList.add(saPayload);
            payloadList.add(new IkeNoncePayload(randomFactory));

            // SaPropoals.Builder guarantees that each SA proposal has at least one DH group.
            payloadList.add(new IkeKePayload(selectedDhGroup, randomFactory));

            return payloadList;
        }
    }
}
