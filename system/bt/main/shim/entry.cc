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

#include "main/shim/entry.h"
#include "osi/include/future.h"
#include "osi/include/log.h"

#include "hci/controller.h"
#include "hci/hci_layer.h"
#include "hci/le_advertising_manager.h"
#include "hci/le_scanning_manager.h"
#include "main/shim/btm.h"
#include "neighbor/connectability.h"
#include "neighbor/discoverability.h"
#include "neighbor/inquiry.h"
#include "neighbor/name.h"
#include "neighbor/page.h"
#include "os/handler.h"
#include "security/security_module.h"
#include "shim/dumpsys.h"
#include "shim/l2cap.h"
#include "shim/stack.h"
#include "stack_manager.h"
#include "storage/legacy.h"

using bluetooth::shim::GetGabeldorscheStack;

extern bluetooth::shim::Btm shim_btm;

future_t* bluetooth::shim::StartGabeldorscheStack() {
  GetGabeldorscheStack()->Start();
  shim_btm.RegisterInquiryCallbacks();
  return (future_t*)nullptr;
}

future_t* bluetooth::shim::StopGabeldorscheStack() {
  GetGabeldorscheStack()->Stop();
  return (future_t*)nullptr;
}

bluetooth::os::Handler* bluetooth::shim::GetGdShimHandler() {
  return bluetooth::shim::GetDumpsys()->GetGdShimHandler();
}

bluetooth::hci::LeAdvertisingManager* bluetooth::shim::GetAdvertising() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::hci::LeAdvertisingManager>();
}

bluetooth::hci::Controller* bluetooth::shim::GetController() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::hci::Controller>();
}

bluetooth::neighbor::ConnectabilityModule*
bluetooth::shim::GetConnectability() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::neighbor::ConnectabilityModule>();
}

bluetooth::neighbor::DiscoverabilityModule*
bluetooth::shim::GetDiscoverability() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::neighbor::DiscoverabilityModule>();
}

bluetooth::shim::Dumpsys* bluetooth::shim::GetDumpsys() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::shim::Dumpsys>();
}

bluetooth::neighbor::InquiryModule* bluetooth::shim::GetInquiry() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::neighbor::InquiryModule>();
}

bluetooth::hci::HciLayer* bluetooth::shim::GetHciLayer() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::hci::HciLayer>();
}

bluetooth::shim::L2cap* bluetooth::shim::GetL2cap() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::shim::L2cap>();
}

bluetooth::neighbor::NameModule* bluetooth::shim::GetName() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::neighbor::NameModule>();
}

bluetooth::neighbor::PageModule* bluetooth::shim::GetPage() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::neighbor::PageModule>();
}

bluetooth::hci::LeScanningManager* bluetooth::shim::GetScanning() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::hci::LeScanningManager>();
}

bluetooth::security::SecurityModule* bluetooth::shim::GetSecurityModule() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::security::SecurityModule>();
}

bluetooth::storage::LegacyModule* bluetooth::shim::GetStorage() {
  return GetGabeldorscheStack()
      ->GetStackManager()
      ->GetInstance<bluetooth::storage::LegacyModule>();
}
