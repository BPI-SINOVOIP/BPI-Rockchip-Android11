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

std::variant<PairingFailure, KeyExchangeResult> PairingHandlerLe::ExchangePublicKeys(const InitialInformations& i,
                                                                                     OobDataFlag remote_have_oob_data) {
  // Generate ECDH, or use one that was used for OOB data
  const auto [private_key, public_key] = (remote_have_oob_data == OobDataFlag::NOT_PRESENT || !i.my_oob_data)
                                             ? GenerateECDHKeyPair()
                                             : std::make_pair(i.my_oob_data->private_key, i.my_oob_data->public_key);

  LOG_INFO("Public key exchange start");
  std::unique_ptr<PairingPublicKeyBuilder> myPublicKey = PairingPublicKeyBuilder::Create(public_key.x, public_key.y);

  if (!ValidateECDHPoint(public_key)) {
    LOG_ERROR("Can't validate my own public key!!!");
    return PairingFailure("Can't validate my own public key");
  }

  if (IAmMaster(i)) {
    // Send pairing public key
    LOG_INFO("Master sends out public key");
    SendL2capPacket(i, std::move(myPublicKey));
  }

  LOG_INFO(" Waiting for Public key...");
  auto response = WaitPairingPublicKey();
  LOG_INFO(" Received public key");
  if (std::holds_alternative<PairingFailure>(response)) {
    return std::get<PairingFailure>(response);
  }

  EcdhPublicKey remote_public_key;
  auto ppkv = std::get<PairingPublicKeyView>(response);
  remote_public_key.x = ppkv.GetPublicKeyX();
  remote_public_key.y = ppkv.GetPublicKeyY();
  LOG_INFO("Received Public key from remote");

  // validate received public key
  if (!ValidateECDHPoint(remote_public_key)) {
    // TODO: Spec is unclear what should happend when the point is not on
    // the correct curve: A device that detects an invalid public key from
    // the peer at any point during the LE Secure Connections pairing
    // process shall not use the resulting LTK, if any.
    LOG_INFO("Can't validate remote public key");
    return PairingFailure("Can't validate remote public key");
  }

  if (!IAmMaster(i)) {
    LOG_INFO("Slave sends out public key");
    // Send pairing public key
    SendL2capPacket(i, std::move(myPublicKey));
  }

  LOG_INFO("Public key exchange finish");

  std::array<uint8_t, 32> dhkey = ComputeDHKey(private_key, remote_public_key);

  const EcdhPublicKey& PKa = IAmMaster(i) ? public_key : remote_public_key;
  const EcdhPublicKey& PKb = IAmMaster(i) ? remote_public_key : public_key;

  return KeyExchangeResult{PKa, PKb, dhkey};
}

Stage1ResultOrFailure PairingHandlerLe::DoSecureConnectionsStage1(const InitialInformations& i,
                                                                  const EcdhPublicKey& PKa, const EcdhPublicKey& PKb,
                                                                  const PairingRequestView& pairing_request,
                                                                  const PairingResponseView& pairing_response) {
  if ((pairing_request.GetAuthReq() & pairing_response.GetAuthReq() & AuthReqMaskMitm) == 0) {
    // If both devices have not set MITM option, Just Works shall be used
    return SecureConnectionsJustWorks(i, PKa, PKb);
  }

  if (pairing_request.GetOobDataFlag() == OobDataFlag::PRESENT ||
      pairing_response.GetOobDataFlag() == OobDataFlag::PRESENT) {
    OobDataFlag remote_oob_flag = IAmMaster(i) ? pairing_response.GetOobDataFlag() : pairing_request.GetOobDataFlag();
    OobDataFlag my_oob_flag = IAmMaster(i) ? pairing_request.GetOobDataFlag() : pairing_response.GetOobDataFlag();
    return SecureConnectionsOutOfBand(i, PKa, PKb, my_oob_flag, remote_oob_flag);
  }

  const auto& iom = pairing_request.GetIoCapability();
  const auto& ios = pairing_response.GetIoCapability();

  if ((iom == IoCapability::KEYBOARD_DISPLAY || iom == IoCapability::DISPLAY_YES_NO) &&
      (ios == IoCapability::KEYBOARD_DISPLAY || ios == IoCapability::DISPLAY_YES_NO)) {
    return SecureConnectionsNumericComparison(i, PKa, PKb);
  }

  if (iom == IoCapability::NO_INPUT_NO_OUTPUT || ios == IoCapability::NO_INPUT_NO_OUTPUT) {
    return SecureConnectionsJustWorks(i, PKa, PKb);
  }

  if ((iom == IoCapability::DISPLAY_ONLY || iom == IoCapability::DISPLAY_YES_NO) &&
      (ios == IoCapability::DISPLAY_ONLY || ios == IoCapability::DISPLAY_YES_NO)) {
    return SecureConnectionsJustWorks(i, PKa, PKb);
  }

  IoCapability my_iocaps = IAmMaster(i) ? iom : ios;
  IoCapability remote_iocaps = IAmMaster(i) ? ios : iom;
  return SecureConnectionsPasskeyEntry(i, PKa, PKb, my_iocaps, remote_iocaps);
}

Stage2ResultOrFailure PairingHandlerLe::DoSecureConnectionsStage2(const InitialInformations& i,
                                                                  const EcdhPublicKey& PKa, const EcdhPublicKey& PKb,
                                                                  const PairingRequestView& pairing_request,
                                                                  const PairingResponseView& pairing_response,
                                                                  const Stage1Result stage1result,
                                                                  const std::array<uint8_t, 32>& dhkey) {
  LOG_INFO("Authentication stage 2 started");

  auto [Na, Nb, ra, rb] = stage1result;

  // 2.3.5.6.5 Authentication stage 2 long term key calculation
  uint8_t a[7];
  uint8_t b[7];

  if (IAmMaster(i)) {
    memcpy(a, i.my_connection_address.GetAddress().address, 6);
    a[6] = (uint8_t)i.my_connection_address.GetAddressType();
    memcpy(b, i.remote_connection_address.GetAddress().address, 6);
    b[6] = (uint8_t)i.remote_connection_address.GetAddressType();
  } else {
    memcpy(a, i.remote_connection_address.GetAddress().address, 6);
    a[6] = (uint8_t)i.remote_connection_address.GetAddressType();
    memcpy(b, i.my_connection_address.GetAddress().address, 6);
    b[6] = (uint8_t)i.my_connection_address.GetAddressType();
  }

  Octet16 ltk, mac_key;
  crypto_toolbox::f5((uint8_t*)dhkey.data(), Na, Nb, a, b, &mac_key, &ltk);

  // DHKey exchange and check

  std::array<uint8_t, 3> iocapA{static_cast<uint8_t>(pairing_request.GetIoCapability()),
                                static_cast<uint8_t>(pairing_request.GetOobDataFlag()), pairing_request.GetAuthReq()};
  std::array<uint8_t, 3> iocapB{static_cast<uint8_t>(pairing_response.GetIoCapability()),
                                static_cast<uint8_t>(pairing_response.GetOobDataFlag()), pairing_response.GetAuthReq()};

  // LOG(INFO) << +(IAmMaster(i)) << " LTK = " << base::HexEncode(ltk.data(), ltk.size());
  // LOG(INFO) << +(IAmMaster(i)) << " MAC_KEY = " << base::HexEncode(mac_key.data(), mac_key.size());
  // LOG(INFO) << +(IAmMaster(i)) << " Na = " << base::HexEncode(Na.data(), Na.size());
  // LOG(INFO) << +(IAmMaster(i)) << " Nb = " << base::HexEncode(Nb.data(), Nb.size());
  // LOG(INFO) << +(IAmMaster(i)) << " ra = " << base::HexEncode(ra.data(), ra.size());
  // LOG(INFO) << +(IAmMaster(i)) << " rb = " << base::HexEncode(rb.data(), rb.size());
  // LOG(INFO) << +(IAmMaster(i)) << " iocapA = " << base::HexEncode(iocapA.data(), iocapA.size());
  // LOG(INFO) << +(IAmMaster(i)) << " iocapB = " << base::HexEncode(iocapB.data(), iocapB.size());
  // LOG(INFO) << +(IAmMaster(i)) << " a = " << base::HexEncode(a, 7);
  // LOG(INFO) << +(IAmMaster(i)) << " b = " << base::HexEncode(b, 7);

  Octet16 Ea = crypto_toolbox::f6(mac_key, Na, Nb, rb, iocapA.data(), a, b);

  Octet16 Eb = crypto_toolbox::f6(mac_key, Nb, Na, ra, iocapB.data(), b, a);

  if (IAmMaster(i)) {
    // send Pairing DHKey Check
    SendL2capPacket(i, PairingDhKeyCheckBuilder::Create(Ea));

    auto response = WaitPairingDHKeyCheck();
    if (std::holds_alternative<PairingFailure>(response)) {
      return std::get<PairingFailure>(response);
    }

    if (std::get<PairingDhKeyCheckView>(response).GetDhKeyCheck() != Eb) {
      LOG_INFO("Ea != Eb, aborting!");
      SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::DHKEY_CHECK_FAILED));
      return PairingFailure("Ea != Eb");
    }
  } else {
    auto response = WaitPairingDHKeyCheck();
    if (std::holds_alternative<PairingFailure>(response)) {
      return std::get<PairingFailure>(response);
    }

    if (std::get<PairingDhKeyCheckView>(response).GetDhKeyCheck() != Ea) {
      LOG_INFO("Ea != Eb, aborting!");
      SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::DHKEY_CHECK_FAILED));
      return PairingFailure("Ea != Eb");
    }

    // send Pairing DHKey Check
    SendL2capPacket(i, PairingDhKeyCheckBuilder::Create(Eb));
  }

  LOG_INFO("Authentication stage 2 (DHKey checks) finished");
  return ltk;
}

Stage1ResultOrFailure PairingHandlerLe::SecureConnectionsOutOfBand(const InitialInformations& i,
                                                                   const EcdhPublicKey& Pka, const EcdhPublicKey& Pkb,
                                                                   OobDataFlag my_oob_flag,
                                                                   OobDataFlag remote_oob_flag) {
  LOG_INFO("Out Of Band start");

  Octet16 zeros{0};
  Octet16 localR = (remote_oob_flag == OobDataFlag::PRESENT && i.my_oob_data) ? i.my_oob_data->r : zeros;
  Octet16 remoteR;

  if (my_oob_flag == OobDataFlag::NOT_PRESENT || (my_oob_flag == OobDataFlag::PRESENT && !i.remote_oob_data)) {
    /* we have send the OOB data, but not received them. remote will check if
     * C value is correct */
    remoteR = zeros;
  } else {
    remoteR = i.remote_oob_data->le_sc_r;
    Octet16 remoteC = i.remote_oob_data->le_sc_c;

    Octet16 remoteC2;
    if (IAmMaster(i)) {
      remoteC2 = crypto_toolbox::f4((uint8_t*)Pkb.x.data(), (uint8_t*)Pkb.x.data(), remoteR, 0);
    } else {
      remoteC2 = crypto_toolbox::f4((uint8_t*)Pka.x.data(), (uint8_t*)Pka.x.data(), remoteR, 0);
    }

    if (remoteC2 != remoteC) {
      LOG_ERROR("C_computed != C_from_remote, aborting!");
      return PairingFailure("C_computed != C_from_remote, aborting");
    }
  }

  Octet16 Na, Nb, ra, rb;
  if (IAmMaster(i)) {
    ra = localR;
    rb = remoteR;
    Na = GenerateRandom<16>();
    // Send Pairing Random
    SendL2capPacket(i, PairingRandomBuilder::Create(Na));

    LOG_INFO("Master waits for Nb");
    auto random = WaitPairingRandom();
    if (std::holds_alternative<PairingFailure>(random)) {
      return std::get<PairingFailure>(random);
    }
    Nb = std::get<PairingRandomView>(random).GetRandomValue();
  } else {
    ra = remoteR;
    rb = localR;
    Nb = GenerateRandom<16>();

    LOG_INFO("Slave waits for random");
    auto random = WaitPairingRandom();
    if (std::holds_alternative<PairingFailure>(random)) {
      return std::get<PairingFailure>(random);
    }
    Na = std::get<PairingRandomView>(random).GetRandomValue();

    SendL2capPacket(i, PairingRandomBuilder::Create(Nb));
  }

  return Stage1Result{Na, Nb, ra, rb};
}

Stage1ResultOrFailure PairingHandlerLe::SecureConnectionsPasskeyEntry(const InitialInformations& i,
                                                                      const EcdhPublicKey& PKa,
                                                                      const EcdhPublicKey& PKb, IoCapability my_iocaps,
                                                                      IoCapability remote_iocaps) {
  LOG_INFO("Passkey Entry start");
  Octet16 Na, Nb, ra{0}, rb{0};

  uint32_t passkey;

  if (my_iocaps == IoCapability::DISPLAY_ONLY || remote_iocaps == IoCapability::KEYBOARD_ONLY) {
    // I display
    passkey = GenerateRandom();
    passkey &= 0x0fffff; /* maximum 20 significant bytes */
    constexpr uint32_t PASSKEY_MAX = 999999;
    while (passkey > PASSKEY_MAX) passkey >>= 1;

    i.user_interface_handler->Post(common::BindOnce(&UI::DisplayPasskey, common::Unretained(i.user_interface),
                                                    i.remote_connection_address, i.remote_name, passkey));

  } else if (my_iocaps == IoCapability::KEYBOARD_ONLY || remote_iocaps == IoCapability::DISPLAY_ONLY) {
    i.user_interface_handler->Post(common::BindOnce(&UI::DisplayEnterPasskeyDialog,
                                                    common::Unretained(i.user_interface), i.remote_connection_address,
                                                    i.remote_name));
    std::optional<PairingEvent> response = WaitUiPasskey();
    if (!response) return PairingFailure("Passkey did not arrive!");

    passkey = response->ui_value;

    /*TODO: shall we send "Keypress Notification" after each key ? This would
     * have impact on the SMP timeout*/

  } else {
    LOG(FATAL) << "THIS SHOULD NEVER HAPPEN";
    return PairingFailure("FATAL!");
  }

  uint32_t bitmask = 0x01;
  for (int loop = 0; loop < 20; loop++, bitmask <<= 1) {
    LOG_INFO("Iteration no %d", loop);
    bool bit_set = ((bitmask & passkey) != 0);
    uint8_t ri = bit_set ? 0x81 : 0x80;

    Octet16 Cai, Cbi, Nai, Nbi;
    if (IAmMaster(i)) {
      Nai = GenerateRandom<16>();

      Cai = crypto_toolbox::f4((uint8_t*)PKa.x.data(), (uint8_t*)PKb.x.data(), Nai, ri);

      // Send Pairing Confirm
      LOG_INFO("Master sends Cai");
      SendL2capPacket(i, PairingConfirmBuilder::Create(Cai));

      LOG_INFO("Master waits for the Cbi");
      auto confirm = WaitPairingConfirm();
      if (std::holds_alternative<PairingFailure>(confirm)) {
        return std::get<PairingFailure>(confirm);
      }
      Cbi = std::get<PairingConfirmView>(confirm).GetConfirmValue();

      // Send Pairing Random
      SendL2capPacket(i, PairingRandomBuilder::Create(Nai));

      LOG_INFO("Master waits for Nbi");
      auto random = WaitPairingRandom();
      if (std::holds_alternative<PairingFailure>(random)) {
        return std::get<PairingFailure>(random);
      }
      Nbi = std::get<PairingRandomView>(random).GetRandomValue();

      Octet16 Cbi2 = crypto_toolbox::f4((uint8_t*)PKb.x.data(), (uint8_t*)PKa.x.data(), Nbi, ri);
      if (Cbi != Cbi2) {
        LOG_INFO("Cai != Cbi, aborting!");
        SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::CONFIRM_VALUE_FAILED));
        return PairingFailure("Cai != Cbi");
      }
    } else {
      Nbi = GenerateRandom<16>();
      // Compute confirm
      Cbi = crypto_toolbox::f4((uint8_t*)PKb.x.data(), (uint8_t*)PKa.x.data(), Nbi, ri);

      LOG_INFO("Slave waits for the Cai");
      auto confirm = WaitPairingConfirm();
      if (std::holds_alternative<PairingFailure>(confirm)) {
        return std::get<PairingFailure>(confirm);
      }
      Cai = std::get<PairingConfirmView>(confirm).GetConfirmValue();

      // Send Pairing Confirm
      LOG_INFO("Slave sends confirmation");
      SendL2capPacket(i, PairingConfirmBuilder::Create(Cbi));

      LOG_INFO("Slave waits for random");
      auto random = WaitPairingRandom();
      if (std::holds_alternative<PairingFailure>(random)) {
        return std::get<PairingFailure>(random);
      }
      Nai = std::get<PairingRandomView>(random).GetRandomValue();

      Octet16 Cai2 = crypto_toolbox::f4((uint8_t*)PKa.x.data(), (uint8_t*)PKb.x.data(), Nai, ri);
      if (Cai != Cai2) {
        LOG_INFO("Cai != Cai2, aborting!");
        SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::CONFIRM_VALUE_FAILED));
        return PairingFailure("Cai != Cai2");
      }

      // Send Pairing Random
      SendL2capPacket(i, PairingRandomBuilder::Create(Nbi));
    }

    if (loop == 19) {
      Na = Nai;
      Nb = Nbi;
    }
  }

  ra[0] = (uint8_t)(passkey);
  ra[1] = (uint8_t)(passkey >> 8);
  ra[2] = (uint8_t)(passkey >> 16);
  ra[3] = (uint8_t)(passkey >> 24);
  rb = ra;

  return Stage1Result{Na, Nb, ra, rb};
}

Stage1ResultOrFailure PairingHandlerLe::SecureConnectionsNumericComparison(const InitialInformations& i,
                                                                           const EcdhPublicKey& PKa,
                                                                           const EcdhPublicKey& PKb) {
  LOG_INFO("Numeric Comparison start");
  Stage1ResultOrFailure result = SecureConnectionsJustWorks(i, PKa, PKb);
  if (std::holds_alternative<PairingFailure>(result)) {
    return std::get<PairingFailure>(result);
  }

  const auto [Na, Nb, ra, rb] = std::get<Stage1Result>(result);

  uint32_t number_to_display = crypto_toolbox::g2((uint8_t*)PKa.x.data(), (uint8_t*)PKb.x.data(), Na, Nb);

  i.user_interface_handler->Post(common::BindOnce(&UI::DisplayConfirmValue, common::Unretained(i.user_interface),
                                                  i.remote_connection_address, i.remote_name, number_to_display));

  std::optional<PairingEvent> confirmyesno = WaitUiConfirmYesNo();
  if (!confirmyesno || confirmyesno->ui_value == 0) {
    LOG_INFO("Was expecting the user value confirm");
    return PairingFailure("Was expecting the user value confirm");
  }

  return result;
}

Stage1ResultOrFailure PairingHandlerLe::SecureConnectionsJustWorks(const InitialInformations& i,
                                                                   const EcdhPublicKey& PKa, const EcdhPublicKey& PKb) {
  Octet16 Cb, Na, Nb, ra, rb;

  ra = rb = {0};

  if (IAmMaster(i)) {
    Na = GenerateRandom<16>();
    LOG_INFO("Master waits for confirmation");
    auto confirm = WaitPairingConfirm();
    if (std::holds_alternative<PairingFailure>(confirm)) {
      return std::get<PairingFailure>(confirm);
    }
    Cb = std::get<PairingConfirmView>(confirm).GetConfirmValue();

    // Send Pairing Random
    SendL2capPacket(i, PairingRandomBuilder::Create(Na));

    LOG_INFO("Master waits for Random");
    auto random = WaitPairingRandom();
    if (std::holds_alternative<PairingFailure>(random)) {
      return std::get<PairingFailure>(random);
    }
    Nb = std::get<PairingRandomView>(random).GetRandomValue();

    // Compute Cb locally
    Octet16 Cb_local = crypto_toolbox::f4((uint8_t*)PKb.x.data(), (uint8_t*)PKa.x.data(), Nb, 0);

    if (Cb_local != Cb) {
      LOG_INFO("Cb_local != Cb, aborting!");
      SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::CONFIRM_VALUE_FAILED));
      return PairingFailure("Cb_local != Cb");
    }
  } else {
    Nb = GenerateRandom<16>();
    // Compute confirm
    Cb = crypto_toolbox::f4((uint8_t*)PKb.x.data(), (uint8_t*)PKa.x.data(), Nb, 0);

    // Send Pairing Confirm
    LOG_INFO("Slave sends confirmation");
    SendL2capPacket(i, PairingConfirmBuilder::Create(Cb));

    LOG_INFO("Slave waits for random");
    auto random = WaitPairingRandom();
    if (std::holds_alternative<PairingFailure>(random)) {
      return std::get<PairingFailure>(random);
    }
    Na = std::get<PairingRandomView>(random).GetRandomValue();

    // Send Pairing Random
    SendL2capPacket(i, PairingRandomBuilder::Create(Nb));
  }

  return Stage1Result{Na, Nb, ra, rb};
}

}  // namespace security
}  // namespace bluetooth