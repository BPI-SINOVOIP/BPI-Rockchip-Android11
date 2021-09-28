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

LegacyStage1ResultOrFailure PairingHandlerLe::DoLegacyStage1(const InitialInformations& i,
                                                             const PairingRequestView& pairing_request,
                                                             const PairingResponseView& pairing_response) {
  if (((pairing_request.GetAuthReq() | pairing_response.GetAuthReq()) & AuthReqMaskMitm) == 0) {
    // If both devices have not set MITM option, Just Works shall be used
    return LegacyJustWorks();
  }

  if (pairing_request.GetOobDataFlag() == OobDataFlag::PRESENT &&
      pairing_response.GetOobDataFlag() == OobDataFlag::PRESENT) {
    // OobDataFlag remote_oob_flag = IAmMaster(i) ? pairing_response.GetOobDataFlag() :
    // pairing_request.GetOobDataFlag(); OobDataFlag my_oob_flag = IAmMaster(i) ? pairing_request.GetOobDataFlag() :
    // pairing_response.GetOobDataFlag();
    return LegacyOutOfBand(i);
  }

  const auto& iom = pairing_request.GetIoCapability();
  const auto& ios = pairing_response.GetIoCapability();

  if (iom == IoCapability::NO_INPUT_NO_OUTPUT || ios == IoCapability::NO_INPUT_NO_OUTPUT) {
    return LegacyJustWorks();
  }

  if ((iom == IoCapability::DISPLAY_ONLY || iom == IoCapability::DISPLAY_YES_NO) &&
      (ios == IoCapability::DISPLAY_ONLY || ios == IoCapability::DISPLAY_YES_NO)) {
    return LegacyJustWorks();
  }

  // This if() should not be needed, these are only combinations left.
  if (iom == IoCapability::KEYBOARD_DISPLAY || iom == IoCapability::KEYBOARD_ONLY ||
      ios == IoCapability::KEYBOARD_DISPLAY || ios == IoCapability::KEYBOARD_ONLY) {
    IoCapability my_iocaps = IAmMaster(i) ? iom : ios;
    IoCapability remote_iocaps = IAmMaster(i) ? ios : iom;
    return LegacyPasskeyEntry(i, my_iocaps, remote_iocaps);
  }

  // We went through all possble combinations.
  LOG_ALWAYS_FATAL("This should never happen");
}

LegacyStage1ResultOrFailure PairingHandlerLe::LegacyJustWorks() {
  LOG_INFO("Legacy Just Works start");
  return Octet16{0};
}

LegacyStage1ResultOrFailure PairingHandlerLe::LegacyPasskeyEntry(const InitialInformations& i,
                                                                 const IoCapability& my_iocaps,
                                                                 const IoCapability& remote_iocaps) {
  bool i_am_displaying = false;
  if (my_iocaps == IoCapability::DISPLAY_ONLY || my_iocaps == IoCapability::DISPLAY_YES_NO) {
    i_am_displaying = true;
  } else if (IAmMaster(i) && remote_iocaps == IoCapability::KEYBOARD_DISPLAY &&
             my_iocaps == IoCapability::KEYBOARD_DISPLAY) {
    i_am_displaying = true;
  } else if (my_iocaps == IoCapability::KEYBOARD_DISPLAY && remote_iocaps == IoCapability::KEYBOARD_ONLY) {
    i_am_displaying = true;
  }

  LOG_INFO("Passkey Entry start %s", i_am_displaying ? "displaying" : "accepting");

  uint32_t passkey;
  if (i_am_displaying) {
    // generate passkey in a valid range
    passkey = GenerateRandom();
    passkey &= 0x0fffff; /* maximum 20 significant bits */
    constexpr uint32_t PASSKEY_MAX = 999999;
    if (passkey > PASSKEY_MAX) passkey >>= 1;

    i.user_interface_handler->Post(common::BindOnce(&UI::DisplayConfirmValue, common::Unretained(i.user_interface),
                                                    i.remote_connection_address, i.remote_name, passkey));
  } else {
    i.user_interface_handler->Post(common::BindOnce(&UI::DisplayEnterPasskeyDialog,
                                                    common::Unretained(i.user_interface), i.remote_connection_address,
                                                    i.remote_name));
    std::optional<PairingEvent> response = WaitUiPasskey();
    if (!response) return PairingFailure("Passkey did not arrive!");

    passkey = response->ui_value;
  }

  Octet16 tk{0};
  tk[0] = (uint8_t)(passkey);
  tk[1] = (uint8_t)(passkey >> 8);
  tk[2] = (uint8_t)(passkey >> 16);
  tk[3] = (uint8_t)(passkey >> 24);

  LOG_INFO("Passkey Entry finish");
  return tk;
}

LegacyStage1ResultOrFailure PairingHandlerLe::LegacyOutOfBand(const InitialInformations& i) {
  return i.remote_oob_data->security_manager_tk_value;
}

StkOrFailure PairingHandlerLe::DoLegacyStage2(const InitialInformations& i, const PairingRequestView& pairing_request,
                                              const PairingResponseView& pairing_response, const Octet16& tk) {
  LOG_INFO("Legacy Step 2 start");
  std::vector<uint8_t> preq(pairing_request.begin(), pairing_request.end());
  std::vector<uint8_t> pres(pairing_response.begin(), pairing_response.end());

  Octet16 mrand, srand;
  if (IAmMaster(i)) {
    mrand = GenerateRandom<16>();

    // LOG(INFO) << +(IAmMaster(i)) << " tk = " << base::HexEncode(tk.data(), tk.size());
    // LOG(INFO) << +(IAmMaster(i)) << " mrand = " << base::HexEncode(mrand.data(), mrand.size());
    // LOG(INFO) << +(IAmMaster(i)) << " pres = " << base::HexEncode(pres.data(), pres.size());
    // LOG(INFO) << +(IAmMaster(i)) << " preq = " << base::HexEncode(preq.data(), preq.size());

    Octet16 mconfirm = crypto_toolbox::c1(
        tk, mrand, preq.data(), pres.data(), (uint8_t)i.my_connection_address.GetAddressType(),
        i.my_connection_address.GetAddress().address, (uint8_t)i.remote_connection_address.GetAddressType(),
        i.remote_connection_address.GetAddress().address);

    // LOG(INFO) << +(IAmMaster(i)) << " mconfirm = " << base::HexEncode(mconfirm.data(), mconfirm.size());

    LOG_INFO("Master sends Mconfirm");
    SendL2capPacket(i, PairingConfirmBuilder::Create(mconfirm));

    LOG_INFO("Master waits for the Sconfirm");
    auto sconfirm_pkt = WaitPairingConfirm();
    if (std::holds_alternative<PairingFailure>(sconfirm_pkt)) {
      return std::get<PairingFailure>(sconfirm_pkt);
    }
    Octet16 sconfirm = std::get<PairingConfirmView>(sconfirm_pkt).GetConfirmValue();

    LOG_INFO("Master sends Mrand");
    SendL2capPacket(i, PairingRandomBuilder::Create(mrand));

    LOG_INFO("Master waits for Srand");
    auto random_pkt = WaitPairingRandom();
    if (std::holds_alternative<PairingFailure>(random_pkt)) {
      return std::get<PairingFailure>(random_pkt);
    }
    srand = std::get<PairingRandomView>(random_pkt).GetRandomValue();

    // LOG(INFO) << +(IAmMaster(i)) << " srand = " << base::HexEncode(srand.data(), srand.size());

    Octet16 sconfirm_generated = crypto_toolbox::c1(
        tk, srand, preq.data(), pres.data(), (uint8_t)i.my_connection_address.GetAddressType(),
        i.my_connection_address.GetAddress().address, (uint8_t)i.remote_connection_address.GetAddressType(),
        i.remote_connection_address.GetAddress().address);

    if (sconfirm != sconfirm_generated) {
      LOG_INFO("sconfirm does not match generated value");

      SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::CONFIRM_VALUE_FAILED));
      return PairingFailure("sconfirm does not match generated value");
    }
  } else {
    srand = GenerateRandom<16>();

    std::vector<uint8_t> preq(pairing_request.begin(), pairing_request.end());
    std::vector<uint8_t> pres(pairing_response.begin(), pairing_response.end());

    Octet16 sconfirm = crypto_toolbox::c1(
        tk, srand, preq.data(), pres.data(), (uint8_t)i.remote_connection_address.GetAddressType(),
        i.remote_connection_address.GetAddress().address, (uint8_t)i.my_connection_address.GetAddressType(),
        i.my_connection_address.GetAddress().address);

    LOG_INFO("Slave waits for the Mconfirm");
    auto mconfirm_pkt = WaitPairingConfirm();
    if (std::holds_alternative<PairingFailure>(mconfirm_pkt)) {
      return std::get<PairingFailure>(mconfirm_pkt);
    }
    Octet16 mconfirm = std::get<PairingConfirmView>(mconfirm_pkt).GetConfirmValue();

    LOG_INFO("Slave sends Sconfirm");
    SendL2capPacket(i, PairingConfirmBuilder::Create(sconfirm));

    LOG_INFO("Slave waits for Mrand");
    auto random_pkt = WaitPairingRandom();
    if (std::holds_alternative<PairingFailure>(random_pkt)) {
      return std::get<PairingFailure>(random_pkt);
    }
    mrand = std::get<PairingRandomView>(random_pkt).GetRandomValue();

    Octet16 mconfirm_generated = crypto_toolbox::c1(
        tk, mrand, preq.data(), pres.data(), (uint8_t)i.remote_connection_address.GetAddressType(),
        i.remote_connection_address.GetAddress().address, (uint8_t)i.my_connection_address.GetAddressType(),
        i.my_connection_address.GetAddress().address);

    if (mconfirm != mconfirm_generated) {
      LOG_INFO("mconfirm does not match generated value");
      SendL2capPacket(i, PairingFailedBuilder::Create(PairingFailedReason::CONFIRM_VALUE_FAILED));
      return PairingFailure("mconfirm does not match generated value");
    }

    LOG_INFO("Slave sends Srand");
    SendL2capPacket(i, PairingRandomBuilder::Create(srand));
  }

  LOG_INFO("Legacy stage 2 finish");

  /* STK */
  return crypto_toolbox::s1(tk, mrand, srand);
}
}  // namespace security
}  // namespace bluetooth