/******************************************************************************
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "security/pairing_handler_le.h"

namespace bluetooth {
namespace security {

void PairingHandlerLe::PairingMain(InitialInformations i) {
  LOG_INFO("Pairing Started");

  if (i.remotely_initiated) {
    LOG_INFO("Was remotely initiated, presenting user with the accept prompt");
    i.user_interface_handler->Post(common::BindOnce(&UI::DisplayPairingPrompt, common::Unretained(i.user_interface),
                                                    i.remote_connection_address, i.remote_name));

    // If pairing was initiated by remote device, wait for the user to accept
    // the request from the UI.
    LOG_INFO("Waiting for the prompt response");
    std::optional<PairingEvent> pairingAccepted = WaitUiPairingAccept();
    if (!pairingAccepted || pairingAccepted->ui_value == 0) {
      LOG_INFO("User either did not accept the remote pairing, or the prompt timed out");
      SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::UNSPECIFIED_REASON));
      i.OnPairingFinished(PairingFailure("User either did not accept the remote pairing, or the prompt timed out"));
      return;
    }

    LOG_INFO("Pairing prompt accepted");
  }

  /************************************************ PHASE 1 *********************************************************/
  Phase1ResultOrFailure phase_1_result = ExchangePairingFeature(i);
  if (std::holds_alternative<PairingFailure>(phase_1_result)) {
    LOG_WARN("Pairing failed in phase 1");
    // We already send pairing fialed in lower layer. Which one should do that ? how about disconneciton?
    // SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::UNSPECIFIED_REASON));
    // TODO: disconnect?
    i.OnPairingFinished(std::get<PairingFailure>(phase_1_result));
    return;
  }

  auto [pairing_request, pairing_response] = std::get<Phase1Result>(phase_1_result);

  /************************************************ PHASE 2 *********************************************************/
  bool isSecureConnections = pairing_request.GetAuthReq() & pairing_response.GetAuthReq() & AuthReqMaskSc;
  if (isSecureConnections) {
    // 2.3.5.6 LE Secure Connections pairing phase 2
    LOG_INFO("Pairing Phase 2 LE Secure connections Started");

    /*
    TODO: what to do with this piece of spec ?
    If Secure Connections pairing has been initiated over BR/EDR, the
    following fields of the SM Pairing Request PDU are reserved for future use:
     • the IO Capability field,
     • the OOB data flag field, and
     • all bits in the Auth Req field except the CT2 bit.
    */

    OobDataFlag remote_have_oob_data =
        IAmMaster(i) ? pairing_response.GetOobDataFlag() : pairing_request.GetOobDataFlag();

    auto key_exchange_result = ExchangePublicKeys(i, remote_have_oob_data);
    if (std::holds_alternative<PairingFailure>(key_exchange_result)) {
      LOG_ERROR("Public key exchange failed");
      i.OnPairingFinished(std::get<PairingFailure>(key_exchange_result));
      return;
    }
    auto [PKa, PKb, dhkey] = std::get<KeyExchangeResult>(key_exchange_result);

    // Public key exchange finished, Diffie-Hellman key computed.

    Stage1ResultOrFailure stage1result = DoSecureConnectionsStage1(i, PKa, PKb, pairing_request, pairing_response);
    if (std::holds_alternative<PairingFailure>(stage1result)) {
      i.OnPairingFinished(std::get<PairingFailure>(stage1result));
      return;
    }

    Stage2ResultOrFailure stage_2_result = DoSecureConnectionsStage2(i, PKa, PKb, pairing_request, pairing_response,
                                                                     std::get<Stage1Result>(stage1result), dhkey);
    if (std::holds_alternative<PairingFailure>(stage_2_result)) {
      i.OnPairingFinished(std::get<PairingFailure>(stage_2_result));
      return;
    }

    Octet16 ltk = std::get<Octet16>(stage_2_result);
    if (IAmMaster(i)) {
      LOG_INFO("Sending start encryption request");
      SendHciLeStartEncryption(i, i.connection_handle, {0}, {0}, ltk);
    }

  } else {
    // 2.3.5.5 LE legacy pairing phase 2
    LOG_INFO("Pairing Phase 2 LE legacy pairing Started");

    LegacyStage1ResultOrFailure stage1result = DoLegacyStage1(i, pairing_request, pairing_response);
    if (std::holds_alternative<PairingFailure>(stage1result)) {
      LOG_ERROR("Phase 1 failed");
      i.OnPairingFinished(std::get<PairingFailure>(stage1result));
      return;
    }

    Octet16 tk = std::get<Octet16>(stage1result);
    StkOrFailure stage2result = DoLegacyStage2(i, pairing_request, pairing_response, tk);
    if (std::holds_alternative<PairingFailure>(stage2result)) {
      LOG_ERROR("stage 2 failed");
      i.OnPairingFinished(std::get<PairingFailure>(stage2result));
      return;
    }

    Octet16 stk = std::get<Octet16>(stage2result);
    if (IAmMaster(i)) {
      SendHciLeStartEncryption(i, i.connection_handle, {0}, {0}, stk);
    }
  }

  /************************************************ PHASE 3 *********************************************************/
  LOG_INFO("Waiting for encryption changed");
  auto encryption_change_result = WaitEncryptionChanged();
  if (std::holds_alternative<PairingFailure>(encryption_change_result)) {
    i.OnPairingFinished(std::get<PairingFailure>(encryption_change_result));
    return;
  } else if (std::holds_alternative<EncryptionChangeView>(encryption_change_result)) {
    EncryptionChangeView encryption_changed = std::get<EncryptionChangeView>(encryption_change_result);
    if (encryption_changed.GetStatus() != hci::ErrorCode::SUCCESS ||
        encryption_changed.GetEncryptionEnabled() != hci::EncryptionEnabled::ON) {
      i.OnPairingFinished(PairingFailure("Encryption change failed"));
      return;
    }
  } else if (std::holds_alternative<EncryptionKeyRefreshCompleteView>(encryption_change_result)) {
    EncryptionKeyRefreshCompleteView encryption_changed =
        std::get<EncryptionKeyRefreshCompleteView>(encryption_change_result);
    if (encryption_changed.GetStatus() != hci::ErrorCode::SUCCESS) {
      i.OnPairingFinished(PairingFailure("Encryption key refresh failed"));
      return;
    }
  } else {
    i.OnPairingFinished(PairingFailure("Unknown case of encryption change result"));
    return;
  }
  LOG_INFO("Encryption change finished successfully");

  DistributedKeysOrFailure keyExchangeStatus = DistributeKeys(i, pairing_response, isSecureConnections);
  if (std::holds_alternative<PairingFailure>(keyExchangeStatus)) {
    i.OnPairingFinished(std::get<PairingFailure>(keyExchangeStatus));
    LOG_ERROR("Key exchange failed");
    return;
  }

  // bool bonding = pairing_request.GetAuthReq() & pairing_response.GetAuthReq() & AuthReqMaskBondingFlag;

  i.OnPairingFinished(PairingResult{
      .connection_address = i.remote_connection_address,
      .distributed_keys = std::get<DistributedKeys>(keyExchangeStatus),
  });

  LOG_INFO("Pairing finished successfully.");
}

Phase1ResultOrFailure PairingHandlerLe::ExchangePairingFeature(const InitialInformations& i) {
  LOG_INFO("Phase 1 start");

  if (IAmMaster(i)) {
    // Send Pairing Request
    const auto& x = i.myPairingCapabilities;
    auto pairing_request_builder =
        PairingRequestBuilder::Create(x.io_capability, x.oob_data_flag, x.auth_req, x.maximum_encryption_key_size,
                                      x.initiator_key_distribution, x.responder_key_distribution);
    // basically pairing_request = myPairingCapabilities;

    // Convert builder to view
    std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
    BitInserter it(*packet_bytes);
    pairing_request_builder->Serialize(it);
    PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
    auto temp_cmd_view = CommandView::Create(packet_bytes_view);
    auto pairing_request = PairingRequestView::Create(temp_cmd_view);
    ASSERT(pairing_request.IsValid());

    LOG_INFO("Sending Pairing Request");
    SendL2capPacket(i, std::move(pairing_request_builder));

    LOG_INFO("Waiting for Pairing Response");
    auto response = WaitPairingResponse();

    /* There is a potential collision where the slave initiates the pairing at the same time we initiate it, by sending
     * security request. */
    if (std::holds_alternative<PairingFailure>(response) &&
        std::get<PairingFailure>(response).received_code_ == Code::SECURITY_REQUEST) {
      LOG_INFO("Received security request, waiting for Pairing Response again...");
      response = WaitPairingResponse();
    }

    if (std::holds_alternative<PairingFailure>(response)) {
      // TODO: should the failure reason be different in some cases ? How about
      // when we lost connection ? Don't send anything at all, or have L2CAP
      // layer ignore it?
      SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::UNSPECIFIED_REASON));
      return std::get<PairingFailure>(response);
    }

    auto pairing_response = std::get<PairingResponseView>(response);

    LOG_INFO("Phase 1 finish");
    return Phase1Result{pairing_request, pairing_response};
  } else {
    std::optional<PairingRequestView> pairing_request;

    if (i.remotely_initiated) {
      if (!i.pairing_request.has_value()) {
        return PairingFailure("You must pass PairingRequest as a initial information to slave!");
      }

      pairing_request = i.pairing_request.value();

      if (!pairing_request->IsValid()) return PairingFailure("Malformed PairingRequest");
    } else {
      SendL2capPacket(i, SecurityRequestBuilder::Create(i.myPairingCapabilities.auth_req));

      LOG_INFO("Waiting for Pairing Request");
      auto request = WaitPairingRequest();
      if (std::holds_alternative<PairingFailure>(request)) {
        LOG_INFO("%s", std::get<PairingFailure>(request).message.c_str());
        SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::UNSPECIFIED_REASON));
        return std::get<PairingFailure>(request);
      }

      pairing_request = std::get<PairingRequestView>(request);
    }

    // Send Pairing Request
    const auto& x = i.myPairingCapabilities;
    // basically pairing_response_builder = my_first_packet;
    // We are not allowed to enable bits that the remote did not allow us to set in initiator_key_dist and
    // responder_key_distribution
    auto pairing_response_builder =
        PairingResponseBuilder::Create(x.io_capability, x.oob_data_flag, x.auth_req, x.maximum_encryption_key_size,
                                       x.initiator_key_distribution & pairing_request->GetInitiatorKeyDistribution(),
                                       x.responder_key_distribution & pairing_request->GetResponderKeyDistribution());

    // Convert builder to view
    std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
    BitInserter it(*packet_bytes);
    pairing_response_builder->Serialize(it);
    PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
    auto temp_cmd_view = CommandView::Create(packet_bytes_view);
    auto pairing_response = PairingResponseView::Create(temp_cmd_view);
    ASSERT(pairing_response.IsValid());

    LOG_INFO("Sending Pairing Response");
    SendL2capPacket(i, std::move(pairing_response_builder));

    LOG_INFO("Phase 1 finish");
    return Phase1Result{pairing_request.value(), pairing_response};
  }
}

DistributedKeysOrFailure PairingHandlerLe::DistributeKeys(const InitialInformations& i,
                                                          const PairingResponseView& pairing_response,
                                                          bool isSecureConnections) {
  uint8_t keys_i_receive =
      IAmMaster(i) ? pairing_response.GetResponderKeyDistribution() : pairing_response.GetInitiatorKeyDistribution();
  uint8_t keys_i_send =
      IAmMaster(i) ? pairing_response.GetInitiatorKeyDistribution() : pairing_response.GetResponderKeyDistribution();

  // In Secure Connections on the LE Transport, the EncKey field shall be ignored
  if (isSecureConnections) {
    keys_i_send = (~KeyMaskEnc) & keys_i_send;
    keys_i_receive = (~KeyMaskEnc) & keys_i_receive;
  }

  LOG_INFO("Key distribution start, keys_i_send=%02x, keys_i_receive=%02x", keys_i_send, keys_i_receive);

  // TODO: obtain actual values!
  Octet16 my_ltk = {0};
  uint16_t my_ediv{0};
  std::array<uint8_t, 8> my_rand = {0};

  Octet16 my_irk = {0x01};
  Address my_identity_address;
  AddrType my_identity_address_type = AddrType::PUBLIC;
  Octet16 my_signature_key{0};

  if (IAmMaster(i)) {
    // EncKey is unused for LE Secure Connections
    DistributedKeysOrFailure keys = ReceiveKeys(keys_i_receive);
    if (std::holds_alternative<PairingFailure>(keys)) {
      return keys;
    }

    SendKeys(i, keys_i_send, my_ltk, my_ediv, my_rand, my_irk, my_identity_address, my_identity_address_type,
             my_signature_key);
    LOG_INFO("Key distribution finish");
    return keys;
  } else {
    SendKeys(i, keys_i_send, my_ltk, my_ediv, my_rand, my_irk, my_identity_address, my_identity_address_type,
             my_signature_key);

    DistributedKeysOrFailure keys = ReceiveKeys(keys_i_receive);
    if (std::holds_alternative<PairingFailure>(keys)) {
      return keys;
    }
    LOG_INFO("Key distribution finish");
    return keys;
  }
}

DistributedKeysOrFailure PairingHandlerLe::ReceiveKeys(const uint8_t& keys_i_receive) {
  std::optional<Octet16> ltk;                 /* Legacy only */
  std::optional<uint16_t> ediv;               /* Legacy only */
  std::optional<std::array<uint8_t, 8>> rand; /* Legacy only */
  std::optional<Address> identity_address;
  AddrType identity_address_type;
  std::optional<Octet16> irk;
  std::optional<Octet16> signature_key;

  if (keys_i_receive & KeyMaskEnc) {
    {
      auto packet = WaitEncryptionInformation();
      if (std::holds_alternative<PairingFailure>(packet)) {
        LOG_ERROR(" Was expecting Encryption Information but did not receive!");
        return std::get<PairingFailure>(packet);
      }
      ltk = std::get<EncryptionInformationView>(packet).GetLongTermKey();
    }

    {
      auto packet = WaitMasterIdentification();
      if (std::holds_alternative<PairingFailure>(packet)) {
        LOG_ERROR(" Was expecting Master Identification but did not receive!");
        return std::get<PairingFailure>(packet);
      }
      ediv = std::get<MasterIdentificationView>(packet).GetEdiv();
      rand = std::get<MasterIdentificationView>(packet).GetRand();
    }
  }

  if (keys_i_receive & KeyMaskId) {
    auto packet = WaitIdentityInformation();
    if (std::holds_alternative<PairingFailure>(packet)) {
      LOG_ERROR(" Was expecting Identity Information but did not receive!");
      return std::get<PairingFailure>(packet);
    }

    LOG_INFO("Received Identity Information");
    irk = std::get<IdentityInformationView>(packet).GetIdentityResolvingKey();

    auto iapacket = WaitIdentityAddressInformation();
    if (std::holds_alternative<PairingFailure>(iapacket)) {
      LOG_ERROR(
          "Was expecting Identity Address Information but did "
          "not receive!");
      return std::get<PairingFailure>(iapacket);
    }
    LOG_INFO("Received Identity Address Information");
    identity_address = std::get<IdentityAddressInformationView>(iapacket).GetBdAddr();
    identity_address_type = std::get<IdentityAddressInformationView>(iapacket).GetAddrType();
  }

  if (keys_i_receive & KeyMaskSign) {
    auto packet = WaitSigningInformation();
    if (std::holds_alternative<PairingFailure>(packet)) {
      LOG_ERROR(" Was expecting Signing Information but did not receive!");
      return std::get<PairingFailure>(packet);
    }

    LOG_INFO("Received Signing Information");
    signature_key = std::get<SigningInformationView>(packet).GetSignatureKey();
  }

  return DistributedKeys{ltk, ediv, rand, identity_address, identity_address_type, irk, signature_key};
}

void PairingHandlerLe::SendKeys(const InitialInformations& i, const uint8_t& keys_i_send, Octet16 ltk, uint16_t ediv,
                                std::array<uint8_t, 8> rand, Octet16 irk, Address identity_address,
                                AddrType identity_addres_type, Octet16 signature_key) {
  if (keys_i_send & KeyMaskEnc) {
    LOG_INFO("Sending Encryption Information");
    SendL2capPacket(i, EncryptionInformationBuilder::Create(ltk));
    LOG_INFO("Sending Master Identification");
    SendL2capPacket(i, MasterIdentificationBuilder::Create(ediv, rand));
  }

  if (keys_i_send & KeyMaskId) {
    LOG_INFO("Sending Identity Information");
    SendL2capPacket(i, IdentityInformationBuilder::Create(irk));
    LOG_INFO("Sending Identity Address Information");
    SendL2capPacket(i, IdentityAddressInformationBuilder::Create(identity_addres_type, identity_address));
  }

  if (keys_i_send & KeyMaskSign) {
    LOG_INFO("Sending Signing Information");
    SendL2capPacket(i, SigningInformationBuilder::Create(signature_key));
  }
}

}  // namespace security
}  // namespace bluetooth