/*
 * Copyright 2019 The Android Open Source Project
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

#pragma once

#include <set>
#include <unordered_map>

#include "stack/include/l2c_api.h"
#include "stack/l2cap/l2c_int.h"

namespace bluetooth {
namespace shim {

/*******************************************************************************
 *
 * Function         L2CA_Register
 *
 * Description      Other layers call this function to register for L2CAP
 *                  services.
 *
 * Returns          PSM to use or zero if error. Typically, the PSM returned
 *                  is the same as was passed in, but for an outgoing-only
 *                  connection to a dynamic PSM, a "virtual" PSM is returned
 *                  and should be used in the calls to L2CA_ConnectReq() and
 *                  BTM_SetSecurityLevel().
 *
 ******************************************************************************/
uint16_t L2CA_Register(uint16_t psm, tL2CAP_APPL_INFO* p_cb_info,
                       bool enable_snoop, tL2CAP_ERTM_INFO* p_ertm_info);

/*******************************************************************************
 *
 * Function         L2CA_Deregister
 *
 * Description      Other layers call this function to deregister for L2CAP
 *                  services.
 *
 * Returns          void
 *
 ******************************************************************************/
void L2CA_Deregister(uint16_t psm);

/*******************************************************************************
 *
 * Function         L2CA_AllocatePSM
 *
 * Description      Other layers call this function to find an unused PSM for
 *                  L2CAP services.
 *
 * Returns          PSM to use.
 *
 ******************************************************************************/
uint16_t L2CA_AllocatePSM(void);

/*******************************************************************************
 *
 * Function         L2CA_AllocateLePSM
 *
 * Description      Other layers call this function to find an unused LE PSM for
 *                  L2CAP services.
 *
 * Returns          LE_PSM to use if success. Otherwise returns 0.
 *
 ******************************************************************************/
uint16_t L2CA_AllocateLePSM(void);

/*******************************************************************************
 *
 * Function         L2CA_FreeLePSM
 *
 * Description      Free an assigned LE PSM.
 *
 * Returns          void
 *
 ******************************************************************************/
void L2CA_FreeLePSM(uint16_t psm);

/*******************************************************************************
 *
 * Function         L2CA_ConnectReq
 *
 * Description      Higher layers call this function to create an L2CAP
 *                  connection.
 *                  Note that the connection is not established at this time,
 *                  but connection establishment gets started. The callback
 *                  will be invoked when connection establishes or fails.
 *
 * Returns          the CID of the connection, or 0 if it failed to start
 *
 ******************************************************************************/
uint16_t L2CA_ConnectReq(uint16_t psm, const RawAddress& p_bd_addr);

/*******************************************************************************
 *
 * Function         L2CA_ConnectRsp
 *
 * Description      Higher layers call this function to accept an incoming
 *                  L2CAP connection, for which they had gotten an connect
 *                  indication callback.
 *
 * Returns          true for success, false for failure
 *
 ******************************************************************************/
bool L2CA_ConnectRsp(const RawAddress& p_bd_addr, uint8_t id, uint16_t lcid,
                     uint16_t result, uint16_t status);

/*******************************************************************************
 *
 * Function         L2CA_ErtmConnectReq
 *
 * Description      Higher layers call this function to create an L2CAP
 *                  connection that needs to use Enhanced Retransmission Mode.
 *                  Note that the connection is not established at this time,
 *                  but connection establishment gets started. The callback
 *                  will be invoked when connection establishes or fails.
 *
 * Returns          the CID of the connection, or 0 if it failed to start
 *
 ******************************************************************************/
uint16_t L2CA_ErtmConnectReq(uint16_t psm, const RawAddress& p_bd_addr,
                             tL2CAP_ERTM_INFO* p_ertm_info);

/*******************************************************************************
 *
 * Function         L2CA_RegisterLECoc
 *
 * Description      Other layers call this function to register for L2CAP
 *                  Connection Oriented Channel.
 *
 * Returns          PSM to use or zero if error. Typically, the PSM returned
 *                  is the same as was passed in, but for an outgoing-only
 *                  connection to a dynamic PSM, a "virtual" PSM is returned
 *                  and should be used in the calls to L2CA_ConnectLECocReq()
 *                  and BTM_SetSecurityLevel().
 *
 ******************************************************************************/
uint16_t L2CA_RegisterLECoc(uint16_t psm, tL2CAP_APPL_INFO* p_cb_info);

/*******************************************************************************
 *
 * Function         L2CA_DeregisterLECoc
 *
 * Description      Other layers call this function to deregister for L2CAP
 *                  Connection Oriented Channel.
 *
 * Returns          void
 *
 ******************************************************************************/
void L2CA_DeregisterLECoc(uint16_t psm);

/*******************************************************************************
 *
 * Function         L2CA_ConnectLECocReq
 *
 * Description      Higher layers call this function to create an L2CAP LE COC.
 *                  Note that the connection is not established at this time,
 *                  but connection establishment gets started. The callback
 *                  will be invoked when connection establishes or fails.
 *
 * Returns          the CID of the connection, or 0 if it failed to start
 *
 ******************************************************************************/
uint16_t L2CA_ConnectLECocReq(uint16_t psm, const RawAddress& p_bd_addr,
                              tL2CAP_LE_CFG_INFO* p_cfg);

/*******************************************************************************
 *
 * Function         L2CA_ConnectLECocRsp
 *
 * Description      Higher layers call this function to accept an incoming
 *                  L2CAP LE COC connection, for which they had gotten a connect
 *                  indication callback.
 *
 * Returns          true for success, false for failure
 *
 ******************************************************************************/
bool L2CA_ConnectLECocRsp(const RawAddress& p_bd_addr, uint8_t id,
                          uint16_t lcid, uint16_t result, uint16_t status,
                          tL2CAP_LE_CFG_INFO* p_cfg);

/*******************************************************************************
 *
 *  Function         L2CA_GetPeerLECocConfig
 *
 *  Description      Get peers configuration for LE Connection Oriented Channel.
 *
 *  Return value:    true if peer is connected
 *
 ******************************************************************************/
bool L2CA_GetPeerLECocConfig(uint16_t lcid, tL2CAP_LE_CFG_INFO* peer_cfg);

// This function sets the callback routines for the L2CAP connection referred to
// by |local_cid|. The callback routines can only be modified for outgoing
// connections established by |L2CA_ConnectReq| or accepted incoming
// connections. |callbacks| must not be NULL. This function returns true if the
// callbacks could be updated, false if not (e.g. |local_cid| was not found).
bool L2CA_SetConnectionCallbacks(uint16_t local_cid,
                                 const tL2CAP_APPL_INFO* callbacks);

/*******************************************************************************
 *
 * Function         L2CA_ErtmConnectRsp
 *
 * Description      Higher layers call this function to accept an incoming
 *                  L2CAP connection, for which they had gotten an connect
 *                  indication callback, and for which the higher layer wants
 *                  to use Enhanced Retransmission Mode.
 *
 * Returns          true for success, false for failure
 *
 ******************************************************************************/
bool L2CA_ErtmConnectRsp(const RawAddress& p_bd_addr, uint8_t id, uint16_t lcid,
                         uint16_t result, uint16_t status,
                         tL2CAP_ERTM_INFO* p_ertm_info);

/*******************************************************************************
 *
 * Function         L2CA_ConfigReq
 *
 * Description      Higher layers call this function to send configuration.
 *
 * Returns          true if configuration sent, else false
 *
 ******************************************************************************/
bool L2CA_ConfigReq(uint16_t cid, tL2CAP_CFG_INFO* p_cfg);

/*******************************************************************************
 *
 * Function         L2CA_ConfigRsp
 *
 * Description      Higher layers call this function to send a configuration
 *                  response.
 *
 * Returns          true if configuration response sent, else false
 *
 ******************************************************************************/
bool L2CA_ConfigRsp(uint16_t cid, tL2CAP_CFG_INFO* p_cfg);

/*******************************************************************************
 *
 * Function         L2CA_DisconnectReq
 *
 * Description      Higher layers call this function to disconnect a channel.
 *
 * Returns          true if disconnect sent, else false
 *
 ******************************************************************************/
bool L2CA_DisconnectReq(uint16_t cid);

/*******************************************************************************
 *
 * Function         L2CA_DisconnectRsp
 *
 * Description      Higher layers call this function to acknowledge the
 *                  disconnection of a channel.
 *
 * Returns          void
 *
 ******************************************************************************/
bool L2CA_DisconnectRsp(uint16_t cid);

/*******************************************************************************
 *
 * Function         L2CA_DataWrite
 *
 * Description      Higher layers call this function to write data.
 *
 * Returns          L2CAP_DW_SUCCESS, if data accepted, else false
 *                  L2CAP_DW_CONGESTED, if data accepted and the channel is
 *                                      congested
 *                  L2CAP_DW_FAILED, if error
 *
 ******************************************************************************/
uint8_t L2CA_DataWrite(uint16_t cid, BT_HDR* p_data);

// Given a local channel identifier, |lcid|, this function returns the bound
// remote channel identifier, |rcid|, and the ACL link handle, |handle|. If
// |lcid| is not known or is invalid, this function returns false and does not
// modify the values pointed at by |rcid| and |handle|. |rcid| and |handle| may
// be NULL.
bool L2CA_GetIdentifiers(uint16_t lcid, uint16_t* rcid, uint16_t* handle);

/*******************************************************************************
 *
 * Function         L2CA_SetIdleTimeout
 *
 * Description      Higher layers call this function to set the idle timeout for
 *                  a connection, or for all future connections. The "idle
 *                  timeout" is the amount of time that a connection can remain
 *                  up with no L2CAP channels on it. A timeout of zero means
 *                  that the connection will be torn down immediately when the
 *                  last channel is removed. A timeout of 0xFFFF means no
 *                  timeout. Values are in seconds.
 *
 * Returns          true if command succeeded, false if failed
 *
 ******************************************************************************/
bool L2CA_SetIdleTimeout(uint16_t cid, uint16_t timeout, bool is_global);

/*******************************************************************************
 *
 * Function         L2CA_SetIdleTimeoutByBdAddr
 *
 * Description      Higher layers call this function to set the idle timeout for
 *                  a connection. The "idle timeout" is the amount of time that
 *                  a connection can remain up with no L2CAP channels on it.
 *                  A timeout of zero means that the connection will be torn
 *                  down immediately when the last channel is removed.
 *                  A timeout of 0xFFFF means no timeout. Values are in seconds.
 *                  A bd_addr is the remote BD address. If bd_addr =
 *                  RawAddress::kAny, then the idle timeouts for all active
 *                  l2cap links will be changed.
 *
 * Returns          true if command succeeded, false if failed
 *
 * NOTE             This timeout applies to all logical channels active on the
 *                  ACL link.
 ******************************************************************************/
bool L2CA_SetIdleTimeoutByBdAddr(const RawAddress& bd_addr, uint16_t timeout,
                                 tBT_TRANSPORT transport);

/*******************************************************************************
 *
 * Function         L2CA_SetTraceLevel
 *
 * Description      This function sets the trace level for L2CAP. If called with
 *                  a value of 0xFF, it simply reads the current trace level.
 *
 * Returns          the new (current) trace level
 *
 ******************************************************************************/
uint8_t L2CA_SetTraceLevel(uint8_t trace_level);

/*******************************************************************************
 *
 * Function     L2CA_SetDesireRole
 *
 * Description  This function sets the desire role for L2CAP.
 *              If the new role is L2CAP_ROLE_ALLOW_SWITCH, allow switch on
 *              HciCreateConnection.
 *              If the new role is L2CAP_ROLE_DISALLOW_SWITCH, do not allow
 *              switch on HciCreateConnection.
 *
 *              If the new role is a valid role (HCI_ROLE_MASTER or
 *              HCI_ROLE_SLAVE), the desire role is set to the new value.
 *              Otherwise, it is not changed.
 *
 * Returns      the new (current) role
 *
 ******************************************************************************/
uint8_t L2CA_SetDesireRole(uint8_t new_role);

/*******************************************************************************
 *
 * Function     L2CA_FlushChannel
 *
 * Description  This function flushes none, some or all buffers queued up
 *              for xmission for a particular CID. If called with
 *              L2CAP_FLUSH_CHANS_GET (0), it simply returns the number
 *              of buffers queued for that CID L2CAP_FLUSH_CHANS_ALL (0xffff)
 *              flushes all buffers.  All other values specifies the maximum
 *              buffers to flush.
 *
 * Returns      Number of buffers left queued for that CID
 *
 ******************************************************************************/
uint16_t L2CA_FlushChannel(uint16_t lcid, uint16_t num_to_flush);

/*******************************************************************************
 *
 * Function         L2CA_SetAclPriority
 *
 * Description      Sets the transmission priority for an ACL channel.
 *                  (For initial implementation only two values are valid.
 *                  L2CAP_PRIORITY_NORMAL and L2CAP_PRIORITY_HIGH).
 *
 * Returns          true if a valid channel, else false
 *
 ******************************************************************************/
bool L2CA_SetAclPriority(const RawAddress& bd_addr, uint8_t priority);

/*******************************************************************************
 *
 * Function         L2CA_SetTxPriority
 *
 * Description      Sets the transmission priority for a channel. (FCR Mode)
 *
 * Returns          true if a valid channel, else false
 *
 ******************************************************************************/
bool L2CA_SetTxPriority(uint16_t cid, tL2CAP_CHNL_PRIORITY priority);

/*******************************************************************************
 *
 * Function         L2CA_SetFlushTimeout
 *
 * Description      This function set the automatic flush time out in Baseband
 *                  for ACL-U packets.
 *                  BdAddr : the remote BD address of ACL link. If it is
 *                           BT_DB_ANY then the flush time out will be applied
 *                           to all ACL link.
 *                  FlushTimeout: flush time out in ms
 *                           0x0000 : No automatic flush
 *                           L2CAP_NO_RETRANSMISSION : No retransmission
 *                           0x0002 - 0xFFFE : flush time out, if
 *                                             (flush_tout * 8) + 3 / 5) <=
 *                                             HCI_MAX_AUTOMATIC_FLUSH_TIMEOUT
 *                                             (in 625us slot).
 *                                    Otherwise, return false.
 *                           L2CAP_NO_AUTOMATIC_FLUSH : No automatic flush
 *
 * Returns          true if command succeeded, false if failed
 *
 * NOTE             This flush timeout applies to all logical channels active on
 *                  the ACL link.
 ******************************************************************************/
bool L2CA_SetFlushTimeout(const RawAddress& bd_addr, uint16_t flush_tout);

/*******************************************************************************
 *
 * Function         L2CA_SetChnlFlushability
 *
 * Description      Higher layers call this function to set a channels
 *                  flushability flags
 *
 * Returns          true if CID found, else false
 *
 ******************************************************************************/
bool L2CA_SetChnlFlushability(uint16_t cid, bool is_flushable);

/*******************************************************************************
 *
 *  Function         L2CA_GetPeerFeatures
 *
 *  Description      Get a peers features and fixed channel map
 *
 *  Parameters:      BD address of the peer
 *                   Pointers to features and channel mask storage area
 *
 *  Return value:    true if peer is connected
 *
 ******************************************************************************/
bool L2CA_GetPeerFeatures(const RawAddress& bd_addr, uint32_t* p_ext_feat,
                          uint8_t* p_chnl_mask);

/*******************************************************************************
 *
 *  Function        L2CA_RegisterFixedChannel
 *
 *  Description     Register a fixed channel.
 *
 *  Parameters:     Fixed Channel #
 *                  Channel Callbacks and config
 *
 *  Return value:   true if registered OK
 *
 ******************************************************************************/
bool L2CA_RegisterFixedChannel(uint16_t fixed_cid,
                               tL2CAP_FIXED_CHNL_REG* p_freg);

/*******************************************************************************
 *
 *  Function        L2CA_ConnectFixedChnl
 *
 *  Description     Connect an fixed signalling channel to a remote device.
 *
 *  Parameters:     Fixed CID
 *                  BD Address of remote
 *
 *  Return value:   true if connection started
 *
 ******************************************************************************/
bool L2CA_ConnectFixedChnl(uint16_t fixed_cid, const RawAddress& bd_addr);
bool L2CA_ConnectFixedChnl(uint16_t fixed_cid, const RawAddress& bd_addr,
                           uint8_t initiating_phys);

/*******************************************************************************
 *
 *  Function        L2CA_SendFixedChnlData
 *
 *  Description     Write data on a fixed signalling channel.
 *
 *  Parameters:     Fixed CID
 *                  BD Address of remote
 *                  Pointer to buffer of type BT_HDR
 *
 * Return value     L2CAP_DW_SUCCESS, if data accepted
 *                  L2CAP_DW_FAILED,  if error
 *
 ******************************************************************************/
uint16_t L2CA_SendFixedChnlData(uint16_t fixed_cid, const RawAddress& rem_bda,
                                BT_HDR* p_buf);

/*******************************************************************************
 *
 *  Function        L2CA_RemoveFixedChnl
 *
 *  Description     Remove a fixed channel to a remote device.
 *
 *  Parameters:     Fixed CID
 *                  BD Address of remote
 *                  Idle timeout to use (or 0xFFFF if don't care)
 *
 *  Return value:   true if channel removed
 *
 ******************************************************************************/
bool L2CA_RemoveFixedChnl(uint16_t fixed_cid, const RawAddress& rem_bda);

/*******************************************************************************
 *
 * Function         L2CA_SetFixedChannelTout
 *
 * Description      Higher layers call this function to set the idle timeout for
 *                  a fixed channel. The "idle timeout" is the amount of time
 *                  that a connection can remain up with no L2CAP channels on
 *                  it. A timeout of zero means that the connection will be torn
 *                  down immediately when the last channel is removed.
 *                  A timeout of 0xFFFF means no timeout. Values are in seconds.
 *                  A bd_addr is the remote BD address. If bd_addr =
 *                  RawAddress::kAny, then the idle timeouts for all active
 *                  l2cap links will be changed.
 *
 * Returns          true if command succeeded, false if failed
 *
 ******************************************************************************/
bool L2CA_SetFixedChannelTout(const RawAddress& rem_bda, uint16_t fixed_cid,
                              uint16_t idle_tout);

/*******************************************************************************
 *
 *  Function        L2CA_CancelBleConnectReq
 *
 *  Description     Cancel a pending connection attempt to a BLE device.
 *
 *  Parameters:     BD Address of remote
 *
 *  Return value:   true if connection was cancelled
 *
 ******************************************************************************/
bool L2CA_CancelBleConnectReq(const RawAddress& rem_bda);

/*******************************************************************************
 *
 *  Function        L2CA_UpdateBleConnParams
 *
 *  Description     Update BLE connection parameters.
 *
 *  Parameters:     BD Address of remote
 *
 *  Return value:   true if update started
 *
 ******************************************************************************/
bool L2CA_UpdateBleConnParams(const RawAddress& rem_bdRa, uint16_t min_int,
                              uint16_t max_int, uint16_t latency,
                              uint16_t timeout);
bool L2CA_UpdateBleConnParams(const RawAddress& rem_bda, uint16_t min_int,
                              uint16_t max_int, uint16_t latency,
                              uint16_t timeout, uint16_t min_ce_len,
                              uint16_t max_ce_len);

/*******************************************************************************
 *
 *  Function        L2CA_EnableUpdateBleConnParams
 *
 *  Description     Update BLE connection parameters.
 *
 *  Parameters:     BD Address of remote
 *                  enable flag
 *
 *  Return value:   true if update started
 *
 ******************************************************************************/
bool L2CA_EnableUpdateBleConnParams(const RawAddress& rem_bda, bool enable);

/*******************************************************************************
 *
 * Function         L2CA_GetBleConnRole
 *
 * Description      This function returns the connection role.
 *
 * Returns          link role.
 *
 ******************************************************************************/
uint8_t L2CA_GetBleConnRole(const RawAddress& bd_addr);

/*******************************************************************************
 *
 * Function         L2CA_GetDisconnectReason
 *
 * Description      This function returns the disconnect reason code.
 *
 *  Parameters:     BD Address of remote
 *                  Physical transport for the L2CAP connection (BR/EDR or LE)
 *
 * Returns          disconnect reason
 *
 ******************************************************************************/
uint16_t L2CA_GetDisconnectReason(const RawAddress& remote_bda,
                                  tBT_TRANSPORT transport);

void L2CA_AdjustConnectionIntervals(uint16_t* min_interval,
                                    uint16_t* max_interval,
                                    uint16_t floor_interval);

}  // namespace shim
}  // namespace bluetooth
