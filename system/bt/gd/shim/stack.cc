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

#define LOG_TAG "bt_gd_shim"

#include "shim/stack.h"
#include "att/att_module.h"
#include "hal/hci_hal.h"
#include "hci/acl_manager.h"
#include "hci/hci_layer.h"
#include "hci/le_advertising_manager.h"
#include "hci/le_scanning_manager.h"
#include "l2cap/classic/l2cap_classic_module.h"
#include "l2cap/le/l2cap_le_module.h"
#include "neighbor/connectability.h"
#include "neighbor/discoverability.h"
#include "neighbor/inquiry.h"
#include "neighbor/name.h"
#include "neighbor/name_db.h"
#include "neighbor/page.h"
#include "neighbor/scan.h"
#include "os/log.h"
#include "os/thread.h"
#include "security/security_module.h"
#include "shim/dumpsys.h"
#include "shim/l2cap.h"
#include "stack_manager.h"
#include "storage/legacy.h"

using ::bluetooth::os::Thread;

struct bluetooth::shim::Stack::impl {
  void Start() {
    if (is_running_) {
      LOG_ERROR("%s Gd stack already running", __func__);
      return;
    }

    LOG_INFO("%s Starting Gd stack", __func__);
    ModuleList modules;
    modules.add<::bluetooth::att::AttModule>();
    modules.add<::bluetooth::hal::HciHal>();
    modules.add<::bluetooth::hci::AclManager>();
    modules.add<::bluetooth::hci::HciLayer>();
    modules.add<::bluetooth::hci::LeAdvertisingManager>();
    modules.add<::bluetooth::hci::LeScanningManager>();
    modules.add<::bluetooth::l2cap::classic::L2capClassicModule>();
    modules.add<::bluetooth::l2cap::le::L2capLeModule>();
    modules.add<::bluetooth::neighbor::ConnectabilityModule>();
    modules.add<::bluetooth::neighbor::DiscoverabilityModule>();
    modules.add<::bluetooth::neighbor::InquiryModule>();
    modules.add<::bluetooth::neighbor::NameModule>();
    modules.add<::bluetooth::neighbor::NameDbModule>();
    modules.add<::bluetooth::neighbor::PageModule>();
    modules.add<::bluetooth::neighbor::ScanModule>();
    modules.add<::bluetooth::security::SecurityModule>();
    modules.add<::bluetooth::storage::LegacyModule>();
    modules.add<::bluetooth::shim::Dumpsys>();
    modules.add<::bluetooth::shim::L2cap>();

    stack_thread_ = new Thread("gd_stack_thread", Thread::Priority::NORMAL);
    stack_manager_.StartUp(&modules, stack_thread_);
    // TODO(cmanton) Gd stack has spun up another thread with no
    // ability to ascertain the completion
    is_running_ = true;
    LOG_INFO("%s Successfully toggled Gd stack", __func__);
  }

  void Stop() {
    if (!is_running_) {
      LOG_ERROR("%s Gd stack not running", __func__);
      return;
    }

    stack_manager_.ShutDown();
    delete stack_thread_;
    is_running_ = false;
    LOG_INFO("%s Successfully shut down Gd stack", __func__);
  }

  StackManager* GetStackManager() {
    ASSERT(is_running_);
    return &stack_manager_;
  }

 private:
  os::Thread* stack_thread_ = nullptr;
  bool is_running_ = false;
  StackManager stack_manager_;
};

bluetooth::shim::Stack::Stack() {
  pimpl_ = std::make_unique<impl>();
  LOG_INFO("%s Created gd stack", __func__);
}

void bluetooth::shim::Stack::Start() {
  pimpl_->Start();
}

void bluetooth::shim::Stack::Stop() {
  pimpl_->Stop();
}

bluetooth::StackManager* bluetooth::shim::Stack::GetStackManager() {
  return pimpl_->GetStackManager();
}

bluetooth::shim::Stack* bluetooth::shim::GetGabeldorscheStack() {
  static Stack* instance = new Stack();
  return instance;
}
