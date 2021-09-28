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
#define LOG_TAG "bt_gd_neigh"

#include "neighbor/name.h"

#include <memory>
#include <unordered_map>
#include <utility>

#include "common/bind.h"
#include "hci/hci_layer.h"
#include "hci/hci_packets.h"
#include "module.h"
#include "os/handler.h"
#include "os/log.h"

namespace bluetooth {
namespace neighbor {

struct ReadCallbackHandler {
  ReadRemoteNameCallback callback;
  os::Handler* handler;
};

struct CancelCallbackHandler {
  CancelRemoteNameCallback callback;
  os::Handler* handler;
};

constexpr RemoteName kEmptyName{};

struct NameModule::impl {
  void ReadRemoteNameRequest(hci::Address address, hci::PageScanRepetitionMode page_scan_repetition_mode,
                             uint16_t clock_offset, hci::ClockOffsetValid clock_offset_valid,
                             ReadRemoteNameCallback callback, os::Handler* handler);
  void CancelRemoteNameRequest(hci::Address address, CancelRemoteNameCallback, os::Handler* handler);

  void Start();
  void Stop();

  impl(const NameModule& name_module);

 private:
  const NameModule& module_;

  void EnqueueCommandComplete(std::unique_ptr<hci::CommandPacketBuilder> command);
  void EnqueueCommandStatus(std::unique_ptr<hci::CommandPacketBuilder> command);

  void OnCommandComplete(hci::CommandCompleteView view);
  void OnCommandStatus(hci::CommandStatusView status);
  void OnEvent(hci::EventPacketView view);

  std::unordered_map<hci::Address, std::unique_ptr<ReadCallbackHandler>> read_callback_handler_map_;
  std::unordered_map<hci::Address, std::unique_ptr<CancelCallbackHandler>> cancel_callback_handler_map_;

  hci::HciLayer* hci_layer_;
  os::Handler* handler_;
};

const ModuleFactory neighbor::NameModule::Factory = ModuleFactory([]() { return new neighbor::NameModule(); });

neighbor::NameModule::impl::impl(const neighbor::NameModule& module) : module_(module) {}

void neighbor::NameModule::impl::EnqueueCommandComplete(std::unique_ptr<hci::CommandPacketBuilder> command) {
  hci_layer_->EnqueueCommand(std::move(command), common::BindOnce(&impl::OnCommandComplete, common::Unretained(this)),
                             handler_);
}

void neighbor::NameModule::impl::EnqueueCommandStatus(std::unique_ptr<hci::CommandPacketBuilder> command) {
  hci_layer_->EnqueueCommand(std::move(command), common::BindOnce(&impl::OnCommandStatus, common::Unretained(this)),
                             handler_);
}

void neighbor::NameModule::impl::OnCommandComplete(hci::CommandCompleteView view) {
  switch (view.GetCommandOpCode()) {
    case hci::OpCode::REMOTE_NAME_REQUEST_CANCEL: {
      auto packet = hci::RemoteNameRequestCancelCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
      hci::Address address = packet.GetBdAddr();
      ASSERT(cancel_callback_handler_map_.find(address) != cancel_callback_handler_map_.end());
      cancel_callback_handler_map_.erase(address);
    } break;
    default:
      LOG_WARN("Unhandled command:%s", hci::OpCodeText(view.GetCommandOpCode()).c_str());
      break;
  }
}

void neighbor::NameModule::impl::OnCommandStatus(hci::CommandStatusView status) {
  ASSERT(status.GetStatus() == hci::ErrorCode::SUCCESS);

  switch (status.GetCommandOpCode()) {
    case hci::OpCode::REMOTE_NAME_REQUEST: {
      auto packet = hci::RemoteNameRequestStatusView::Create(status);
      ASSERT(packet.IsValid());
    } break;

    default:
      LOG_WARN("Unhandled command:%s", hci::OpCodeText(status.GetCommandOpCode()).c_str());
      break;
  }
}

void neighbor::NameModule::impl::OnEvent(hci::EventPacketView view) {
  switch (view.GetEventCode()) {
    case hci::EventCode::REMOTE_NAME_REQUEST_COMPLETE: {
      auto packet = hci::RemoteNameRequestCompleteView::Create(view);
      ASSERT(packet.IsValid());
      hci::Address address = packet.GetBdAddr();
      ASSERT(read_callback_handler_map_.find(address) != read_callback_handler_map_.end());
      auto read_callback_handler = std::move(read_callback_handler_map_[address]);
      read_callback_handler->handler->Post(common::BindOnce(std::move(read_callback_handler->callback),
                                                            packet.GetStatus(), address, packet.GetRemoteName()));
      read_callback_handler_map_.erase(address);
    } break;
    default:
      LOG_ERROR("Unhandled event:%s", hci::EventCodeText(view.GetEventCode()).c_str());
      break;
  }
}

void neighbor::NameModule::impl::Start() {
  hci_layer_ = module_.GetDependency<hci::HciLayer>();
  handler_ = module_.GetHandler();

  hci_layer_->RegisterEventHandler(hci::EventCode::REMOTE_NAME_REQUEST_COMPLETE,
                                   common::Bind(&NameModule::impl::OnEvent, common::Unretained(this)), handler_);
}

void neighbor::NameModule::impl::Stop() {
  hci_layer_->UnregisterEventHandler(hci::EventCode::REMOTE_NAME_REQUEST_COMPLETE);
}

void neighbor::NameModule::impl::ReadRemoteNameRequest(hci::Address address,
                                                       hci::PageScanRepetitionMode page_scan_repetition_mode,
                                                       uint16_t clock_offset, hci::ClockOffsetValid clock_offset_valid,
                                                       ReadRemoteNameCallback callback, os::Handler* handler) {
  LOG_DEBUG("%s Start read remote name request for %s", __func__, address.ToString().c_str());

  if (read_callback_handler_map_.find(address) != read_callback_handler_map_.end()) {
    LOG_WARN("Ignoring duplicate read remote name request to:%s", address.ToString().c_str());
    handler->Post(common::BindOnce(std::move(callback), hci::ErrorCode::UNSPECIFIED_ERROR, address, kEmptyName));
    return;
  }
  read_callback_handler_map_[address] = std::unique_ptr<ReadCallbackHandler>(new ReadCallbackHandler{
      .callback = std::move(callback),
      .handler = handler,
  });

  EnqueueCommandStatus(
      hci::RemoteNameRequestBuilder::Create(address, page_scan_repetition_mode, clock_offset, clock_offset_valid));
}

void neighbor::NameModule::impl::CancelRemoteNameRequest(hci::Address address, CancelRemoteNameCallback callback,
                                                         os::Handler* handler) {
  LOG_DEBUG("%s Cancel remote name request for %s", __func__, address.ToString().c_str());

  if (cancel_callback_handler_map_.find(address) != cancel_callback_handler_map_.end()) {
    LOG_WARN("Ignoring duplicate cancel remote name request to:%s", address.ToString().c_str());
    handler->Post(common::BindOnce(std::move(callback), hci::ErrorCode::UNSPECIFIED_ERROR, address));
    return;
  }
  cancel_callback_handler_map_[address] = std::unique_ptr<CancelCallbackHandler>(new CancelCallbackHandler{
      .callback = std::move(callback),
      .handler = handler,
  });
  EnqueueCommandComplete(hci::RemoteNameRequestCancelBuilder::Create(address));
}

/**
 * General API here
 */
neighbor::NameModule::NameModule() : pimpl_(std::make_unique<impl>(*this)) {}

neighbor::NameModule::~NameModule() {
  pimpl_.reset();
}

void neighbor::NameModule::ReadRemoteNameRequest(hci::Address address,
                                                 hci::PageScanRepetitionMode page_scan_repetition_mode,
                                                 uint16_t clock_offset, hci::ClockOffsetValid clock_offset_valid,
                                                 ReadRemoteNameCallback callback, os::Handler* handler) {
  ASSERT(callback);
  ASSERT(handler != nullptr);
  GetHandler()->Post(common::BindOnce(&NameModule::impl::ReadRemoteNameRequest, common::Unretained(pimpl_.get()),
                                      address, page_scan_repetition_mode, clock_offset, clock_offset_valid,
                                      std::move(callback), handler));
}

void neighbor::NameModule::CancelRemoteNameRequest(hci::Address address, CancelRemoteNameCallback callback,
                                                   os::Handler* handler) {
  ASSERT(callback);
  ASSERT(handler != nullptr);
  GetHandler()->Post(common::BindOnce(&NameModule::impl::CancelRemoteNameRequest, common::Unretained(pimpl_.get()),
                                      address, std::move(callback), handler));
}

/**
 * Module methods here
 */
void neighbor::NameModule::ListDependencies(ModuleList* list) {
  list->add<hci::HciLayer>();
}

void neighbor::NameModule::Start() {
  pimpl_->Start();
}

void neighbor::NameModule::Stop() {
  pimpl_->Stop();
}

}  // namespace neighbor
}  // namespace bluetooth
