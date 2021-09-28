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

#define LOG_TAG "bt_shim_l2cap"

#include "main/shim/l2c_api.h"
#include "main/shim/l2cap.h"
#include "main/shim/shim.h"
#include "osi/include/log.h"

static bluetooth::shim::legacy::L2cap shim_l2cap;

/**
 * Classic Service Registration APIs
 */
uint16_t bluetooth::shim::L2CA_Register(uint16_t client_psm,
                                        tL2CAP_APPL_INFO* callbacks,
                                        bool enable_snoop,
                                        tL2CAP_ERTM_INFO* p_ertm_info) {
  CHECK(callbacks != nullptr);

  if (L2C_INVALID_PSM(client_psm)) {
    LOG_ERROR(LOG_TAG, "%s Invalid classic psm:%hd", __func__, client_psm);
    return 0;
  }

  if ((callbacks->pL2CA_ConfigCfm_Cb == nullptr) ||
      (callbacks->pL2CA_ConfigInd_Cb == nullptr) ||
      (callbacks->pL2CA_DataInd_Cb == nullptr) ||
      (callbacks->pL2CA_DisconnectInd_Cb == nullptr)) {
    LOG_ERROR(LOG_TAG, "%s Invalid classic callbacks psm:%hd", __func__,
              client_psm);
    return 0;
  }

  /**
   * Check if this is a registration for an outgoing-only connection.
   */
  bool is_outgoing_connection_only = callbacks->pL2CA_ConnectInd_Cb == nullptr;
  uint16_t psm = shim_l2cap.ConvertClientToRealPsm(client_psm,
                                                   is_outgoing_connection_only);

  if (shim_l2cap.Classic().IsPsmRegistered(psm)) {
    LOG_ERROR(LOG_TAG, "%s Already registered classic client_psm:%hd psm:%hd",
              __func__, client_psm, psm);
    return 0;
  }
  LOG_INFO(LOG_TAG, "%s classic client_psm:%hd psm:%hd", __func__, client_psm,
           psm);

  return shim_l2cap.RegisterService(psm, callbacks, enable_snoop, p_ertm_info);
}

void bluetooth::shim::L2CA_Deregister(uint16_t client_psm) {
  if (L2C_INVALID_PSM(client_psm)) {
    LOG_ERROR(LOG_TAG, "%s Invalid classic client_psm:%hd", __func__,
              client_psm);
    return;
  }
  uint16_t psm = shim_l2cap.ConvertClientToRealPsm(client_psm);

  shim_l2cap.UnregisterService(psm);
  shim_l2cap.RemoveClientPsm(psm);
}

uint16_t bluetooth::shim::L2CA_AllocatePSM(void) {
  return shim_l2cap.GetNextDynamicClassicPsm();
}

uint16_t bluetooth::shim::L2CA_AllocateLePSM(void) {
  return shim_l2cap.GetNextDynamicLePsm();
}

void bluetooth::shim::L2CA_FreeLePSM(uint16_t psm) {
  if (!shim_l2cap.Le().IsPsmRegistered(psm)) {
    LOG_ERROR(LOG_TAG, "%s Not previously registered le psm:%hd", __func__,
              psm);
    return;
  }
  shim_l2cap.Le().UnregisterPsm(psm);
}

/**
 * Classic Connection Oriented Channel APIS
 */
uint16_t bluetooth::shim::L2CA_ErtmConnectReq(uint16_t psm,
                                              const RawAddress& raw_address,
                                              tL2CAP_ERTM_INFO* p_ertm_info) {
  return shim_l2cap.CreateConnection(psm, raw_address);
}

uint16_t bluetooth::shim::L2CA_ConnectReq(uint16_t psm,
                                          const RawAddress& raw_address) {
  return shim_l2cap.CreateConnection(psm, raw_address);
}

bool bluetooth::shim::L2CA_ErtmConnectRsp(const RawAddress& p_bd_addr,
                                          uint8_t id, uint16_t lcid,
                                          uint16_t result, uint16_t status,
                                          tL2CAP_ERTM_INFO* p_ertm_info) {
  return shim_l2cap.ConnectResponse(p_bd_addr, id, lcid, result, status,
                                    p_ertm_info);
}

bool bluetooth::shim::L2CA_ConnectRsp(const RawAddress& p_bd_addr, uint8_t id,
                                      uint16_t lcid, uint16_t result,
                                      uint16_t status) {
  return bluetooth::shim::L2CA_ErtmConnectRsp(p_bd_addr, id, lcid, result,
                                              status, nullptr);
}

bool bluetooth::shim::L2CA_ConfigReq(uint16_t cid, tL2CAP_CFG_INFO* cfg_info) {
  return shim_l2cap.ConfigRequest(cid, cfg_info);
}

bool bluetooth::shim::L2CA_ConfigRsp(uint16_t cid, tL2CAP_CFG_INFO* cfg_info) {
  return shim_l2cap.ConfigResponse(cid, cfg_info);
}

bool bluetooth::shim::L2CA_DisconnectReq(uint16_t cid) {
  return shim_l2cap.DisconnectRequest(cid);
}

bool bluetooth::shim::L2CA_DisconnectRsp(uint16_t cid) {
  return shim_l2cap.DisconnectResponse(cid);
}

/**
 * Le Connection Oriented Channel APIs
 */
uint16_t bluetooth::shim::L2CA_RegisterLECoc(uint16_t psm,
                                             tL2CAP_APPL_INFO* callbacks) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s psm:%hd callbacks:%p", __func__, psm,
           callbacks);
  return 0;
}

void bluetooth::shim::L2CA_DeregisterLECoc(uint16_t psm) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s psm:%hd", __func__, psm);
}

uint16_t bluetooth::shim::L2CA_ConnectLECocReq(uint16_t psm,
                                               const RawAddress& p_bd_addr,
                                               tL2CAP_LE_CFG_INFO* p_cfg) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s psm:%hd addr:%s p_cfg:%p", __func__, psm,
           p_bd_addr.ToString().c_str(), p_cfg);
  return 0;
}

bool bluetooth::shim::L2CA_ConnectLECocRsp(const RawAddress& p_bd_addr,
                                           uint8_t id, uint16_t lcid,
                                           uint16_t result, uint16_t status,
                                           tL2CAP_LE_CFG_INFO* p_cfg) {
  LOG_INFO(LOG_TAG,
           "UNIMPLEMENTED %s addr:%s id:%hhd lcid:%hd result:%hd status:%hd "
           "p_cfg:%p",
           __func__, p_bd_addr.ToString().c_str(), id, lcid, result, status,
           p_cfg);
  return false;
}

bool bluetooth::shim::L2CA_GetPeerLECocConfig(uint16_t lcid,
                                              tL2CAP_LE_CFG_INFO* peer_cfg) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s lcid:%hd peer_cfg:%p", __func__, lcid,
           peer_cfg);
  return false;
}

/**
 * Channel Data Writes
 */
bool bluetooth::shim::L2CA_SetConnectionCallbacks(
    uint16_t cid, const tL2CAP_APPL_INFO* callbacks) {
  LOG_INFO(LOG_TAG, "Unsupported API %s", __func__);
  return false;
}

uint8_t bluetooth::shim::L2CA_DataWrite(uint16_t cid, BT_HDR* p_data) {
  bool write_success = shim_l2cap.Write(cid, p_data);
  return write_success ? L2CAP_DW_SUCCESS : L2CAP_DW_FAILED;
}

/**
 * L2cap Layer APIs
 */
uint8_t bluetooth::shim::L2CA_SetDesireRole(uint8_t new_role) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return 0;
}

/**
 * Link APIs
 */
bool bluetooth::shim::L2CA_SetIdleTimeoutByBdAddr(const RawAddress& bd_addr,
                                                  uint16_t timeout,
                                                  tBT_TRANSPORT transport) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

bool bluetooth::shim::L2CA_SetAclPriority(const RawAddress& bd_addr,
                                          uint8_t priority) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

bool bluetooth::shim::L2CA_SetFlushTimeout(const RawAddress& bd_addr,
                                           uint16_t flush_tout) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

bool bluetooth::shim::L2CA_GetPeerFeatures(const RawAddress& bd_addr,
                                           uint32_t* p_ext_feat,
                                           uint8_t* p_chnl_mask) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

/**
 * Fixed Channel APIs. Note: Classic fixed channel (connectionless and BR SMP)
 * is not supported
 */
bool bluetooth::shim::L2CA_RegisterFixedChannel(uint16_t fixed_cid,
                                                tL2CAP_FIXED_CHNL_REG* p_freg) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

bool bluetooth::shim::L2CA_ConnectFixedChnl(uint16_t fixed_cid,
                                            const RawAddress& rem_bda) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

bool bluetooth::shim::L2CA_ConnectFixedChnl(uint16_t fixed_cid,
                                            const RawAddress& rem_bda,
                                            uint8_t initiating_phys) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

uint16_t bluetooth::shim::L2CA_SendFixedChnlData(uint16_t fixed_cid,
                                                 const RawAddress& rem_bda,
                                                 BT_HDR* p_buf) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return 0;
}

bool bluetooth::shim::L2CA_RemoveFixedChnl(uint16_t fixed_cid,
                                           const RawAddress& rem_bda) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

/**
 * Channel hygiene APIs
 */
bool bluetooth::shim::L2CA_GetIdentifiers(uint16_t lcid, uint16_t* rcid,
                                          uint16_t* handle) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

bool bluetooth::shim::L2CA_SetIdleTimeout(uint16_t cid, uint16_t timeout,
                                          bool is_global) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

bool bluetooth::shim::L2CA_SetTxPriority(uint16_t cid,
                                         tL2CAP_CHNL_PRIORITY priority) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

bool bluetooth::shim::L2CA_SetFixedChannelTout(const RawAddress& rem_bda,
                                               uint16_t fixed_cid,
                                               uint16_t idle_tout) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

bool bluetooth::shim::L2CA_SetChnlFlushability(uint16_t cid,
                                               bool is_flushable) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return false;
}

uint16_t bluetooth::shim::L2CA_FlushChannel(uint16_t lcid,
                                            uint16_t num_to_flush) {
  LOG_INFO(LOG_TAG, "UNIMPLEMENTED %s", __func__);
  return 0;
}
