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

#include "hci/hci_layer.h"

#include "common/bind.h"
#include "common/callback.h"
#include "os/alarm.h"
#include "os/queue.h"
#include "packet/packet_builder.h"

namespace {
using bluetooth::common::Bind;
using bluetooth::common::BindOnce;
using bluetooth::common::Callback;
using bluetooth::common::Closure;
using bluetooth::common::OnceCallback;
using bluetooth::common::OnceClosure;
using bluetooth::hci::CommandCompleteView;
using bluetooth::hci::CommandPacketBuilder;
using bluetooth::hci::CommandStatusView;
using bluetooth::hci::EventPacketView;
using bluetooth::hci::LeMetaEventView;
using bluetooth::os::Handler;

class EventHandler {
 public:
  EventHandler() : event_handler(), handler(nullptr) {}
  EventHandler(Callback<void(EventPacketView)> on_event, Handler* on_event_handler)
      : event_handler(std::move(on_event)), handler(on_event_handler) {}
  Callback<void(EventPacketView)> event_handler;
  Handler* handler;
};

class SubeventHandler {
 public:
  SubeventHandler() : subevent_handler(), handler(nullptr) {}
  SubeventHandler(Callback<void(LeMetaEventView)> on_event, Handler* on_event_handler)
      : subevent_handler(std::move(on_event)), handler(on_event_handler) {}
  Callback<void(LeMetaEventView)> subevent_handler;
  Handler* handler;
};

class CommandQueueEntry {
 public:
  CommandQueueEntry(std::unique_ptr<CommandPacketBuilder> command_packet,
                    OnceCallback<void(CommandCompleteView)> on_complete_function, Handler* handler)
      : command(std::move(command_packet)), waiting_for_status_(false), on_complete(std::move(on_complete_function)),
        caller_handler(handler) {}

  CommandQueueEntry(std::unique_ptr<CommandPacketBuilder> command_packet,
                    OnceCallback<void(CommandStatusView)> on_status_function, Handler* handler)
      : command(std::move(command_packet)), waiting_for_status_(true), on_status(std::move(on_status_function)),
        caller_handler(handler) {}

  std::unique_ptr<CommandPacketBuilder> command;
  bool waiting_for_status_;
  OnceCallback<void(CommandStatusView)> on_status;
  OnceCallback<void(CommandCompleteView)> on_complete;
  Handler* caller_handler;
};
}  // namespace

namespace bluetooth {
namespace hci {

using common::BidiQueue;
using common::BidiQueueEnd;
using os::Alarm;
using os::Handler;

namespace {
using hci::OpCode;
using hci::ResetCompleteView;

void fail_if_reset_complete_not_success(CommandCompleteView complete) {
  auto reset_complete = ResetCompleteView::Create(complete);
  ASSERT(reset_complete.IsValid());
  ASSERT(reset_complete.GetStatus() == ErrorCode::SUCCESS);
}

void on_hci_timeout(OpCode op_code) {
  ASSERT_LOG(false, "Timed out waiting for 0x%02hx (%s)", op_code, OpCodeText(op_code).c_str());
}
}  // namespace

class SecurityInterfaceImpl : public SecurityInterface {
 public:
  SecurityInterfaceImpl(HciLayer& hci) : hci_(hci) {}
  virtual ~SecurityInterfaceImpl() = default;

  virtual void EnqueueCommand(std::unique_ptr<SecurityCommandBuilder> command,
                              common::OnceCallback<void(CommandCompleteView)> on_complete,
                              os::Handler* handler) override {
    hci_.EnqueueCommand(std::move(command), std::move(on_complete), handler);
  }

  virtual void EnqueueCommand(std::unique_ptr<SecurityCommandBuilder> command,
                              common::OnceCallback<void(CommandStatusView)> on_status, os::Handler* handler) override {
    hci_.EnqueueCommand(std::move(command), std::move(on_status), handler);
  }
  HciLayer& hci_;
};

class LeSecurityInterfaceImpl : public LeSecurityInterface {
 public:
  LeSecurityInterfaceImpl(HciLayer& hci) : hci_(hci) {}
  virtual ~LeSecurityInterfaceImpl() = default;

  virtual void EnqueueCommand(std::unique_ptr<LeSecurityCommandBuilder> command,
                              common::OnceCallback<void(CommandCompleteView)> on_complete,
                              os::Handler* handler) override {
    hci_.EnqueueCommand(std::move(command), std::move(on_complete), handler);
  }

  virtual void EnqueueCommand(std::unique_ptr<LeSecurityCommandBuilder> command,
                              common::OnceCallback<void(CommandStatusView)> on_status, os::Handler* handler) override {
    hci_.EnqueueCommand(std::move(command), std::move(on_status), handler);
  }
  HciLayer& hci_;
};

class LeAdvertisingInterfaceImpl : public LeAdvertisingInterface {
 public:
  LeAdvertisingInterfaceImpl(HciLayer& hci) : hci_(hci) {}
  virtual ~LeAdvertisingInterfaceImpl() = default;

  virtual void EnqueueCommand(std::unique_ptr<LeAdvertisingCommandBuilder> command,
                              common::OnceCallback<void(CommandCompleteView)> on_complete,
                              os::Handler* handler) override {
    hci_.EnqueueCommand(std::move(command), std::move(on_complete), handler);
  }

  virtual void EnqueueCommand(std::unique_ptr<LeAdvertisingCommandBuilder> command,
                              common::OnceCallback<void(CommandStatusView)> on_status, os::Handler* handler) override {
    hci_.EnqueueCommand(std::move(command), std::move(on_status), handler);
  }
  HciLayer& hci_;
};

class LeScanningInterfaceImpl : public LeScanningInterface {
 public:
  LeScanningInterfaceImpl(HciLayer& hci) : hci_(hci) {}
  virtual ~LeScanningInterfaceImpl() = default;

  virtual void EnqueueCommand(std::unique_ptr<LeScanningCommandBuilder> command,
                              common::OnceCallback<void(CommandCompleteView)> on_complete,
                              os::Handler* handler) override {
    hci_.EnqueueCommand(std::move(command), std::move(on_complete), handler);
  }

  virtual void EnqueueCommand(std::unique_ptr<LeScanningCommandBuilder> command,
                              common::OnceCallback<void(CommandStatusView)> on_status, os::Handler* handler) override {
    hci_.EnqueueCommand(std::move(command), std::move(on_status), handler);
  }
  HciLayer& hci_;
};

struct HciLayer::impl : public hal::HciHalCallbacks {
  impl(HciLayer& module) : hal_(nullptr), module_(module) {}

  ~impl() {}

  void Start(hal::HciHal* hal) {
    hal_ = hal;
    hci_timeout_alarm_ = new Alarm(module_.GetHandler());

    auto queue_end = acl_queue_.GetDownEnd();
    Handler* handler = module_.GetHandler();
    queue_end->RegisterDequeue(handler, Bind(&impl::dequeue_and_send_acl, common::Unretained(this)));
    RegisterEventHandler(EventCode::COMMAND_COMPLETE, Bind(&impl::command_complete_callback, common::Unretained(this)),
                         handler);
    RegisterEventHandler(EventCode::COMMAND_STATUS, Bind(&impl::command_status_callback, common::Unretained(this)),
                         handler);
    RegisterEventHandler(EventCode::LE_META_EVENT, Bind(&impl::le_meta_event_callback, common::Unretained(this)),
                         handler);
    // TODO find the right place
    RegisterEventHandler(EventCode::PAGE_SCAN_REPETITION_MODE_CHANGE, Bind(&impl::drop, common::Unretained(this)),
                         handler);
    RegisterEventHandler(EventCode::MAX_SLOTS_CHANGE, Bind(&impl::drop, common::Unretained(this)), handler);
    RegisterEventHandler(EventCode::VENDOR_SPECIFIC, Bind(&impl::drop, common::Unretained(this)), handler);

    EnqueueCommand(ResetBuilder::Create(), BindOnce(&fail_if_reset_complete_not_success), handler);
    hal_->registerIncomingPacketCallback(this);
  }

  void drop(EventPacketView) {}

  void dequeue_and_send_acl() {
    auto packet = acl_queue_.GetDownEnd()->TryDequeue();
    send_acl(std::move(packet));
  }

  void Stop() {
    hal_->unregisterIncomingPacketCallback();
    UnregisterEventHandler(EventCode::COMMAND_COMPLETE);
    UnregisterEventHandler(EventCode::COMMAND_STATUS);
    UnregisterEventHandler(EventCode::LE_META_EVENT);
    UnregisterEventHandler(EventCode::PAGE_SCAN_REPETITION_MODE_CHANGE);
    UnregisterEventHandler(EventCode::MAX_SLOTS_CHANGE);
    UnregisterEventHandler(EventCode::VENDOR_SPECIFIC);

    acl_queue_.GetDownEnd()->UnregisterDequeue();
    incoming_acl_packet_buffer_.Clear();
    delete hci_timeout_alarm_;
    command_queue_.clear();
    hal_ = nullptr;
  }

  void send_acl(std::unique_ptr<hci::BasePacketBuilder> packet) {
    std::vector<uint8_t> bytes;
    BitInserter bi(bytes);
    packet->Serialize(bi);
    hal_->sendAclData(bytes);
  }

  void send_sco(std::unique_ptr<hci::BasePacketBuilder> packet) {
    std::vector<uint8_t> bytes;
    BitInserter bi(bytes);
    packet->Serialize(bi);
    hal_->sendScoData(bytes);
  }

  void command_status_callback(EventPacketView event) {
    CommandStatusView status_view = CommandStatusView::Create(event);
    ASSERT(status_view.IsValid());
    command_credits_ = status_view.GetNumHciCommandPackets();
    OpCode op_code = status_view.GetCommandOpCode();
    if (op_code == OpCode::NONE) {
      send_next_command();
      return;
    }
    ASSERT_LOG(!command_queue_.empty(), "Unexpected status event with OpCode 0x%02hx (%s)", op_code,
               OpCodeText(op_code).c_str());
    ASSERT_LOG(waiting_command_ == op_code, "Waiting for 0x%02hx (%s), got 0x%02hx (%s)", waiting_command_,
               OpCodeText(waiting_command_).c_str(), op_code, OpCodeText(op_code).c_str());
    ASSERT_LOG(command_queue_.front().waiting_for_status_,
               "Waiting for command complete 0x%02hx (%s), got command status for 0x%02hx (%s)", waiting_command_,
               OpCodeText(waiting_command_).c_str(), op_code, OpCodeText(op_code).c_str());
    auto caller_handler = command_queue_.front().caller_handler;
    caller_handler->Post(BindOnce(std::move(command_queue_.front().on_status), std::move(status_view)));
    command_queue_.pop_front();
    waiting_command_ = OpCode::NONE;
    hci_timeout_alarm_->Cancel();
    send_next_command();
  }

  void command_complete_callback(EventPacketView event) {
    CommandCompleteView complete_view = CommandCompleteView::Create(event);
    ASSERT(complete_view.IsValid());
    command_credits_ = complete_view.GetNumHciCommandPackets();
    OpCode op_code = complete_view.GetCommandOpCode();
    if (op_code == OpCode::NONE) {
      send_next_command();
      return;
    }
    ASSERT_LOG(command_queue_.size() > 0, "Unexpected command complete with OpCode 0x%02hx (%s)", op_code,
               OpCodeText(op_code).c_str());
    ASSERT_LOG(waiting_command_ == op_code, "Waiting for 0x%02hx (%s), got 0x%02hx (%s)", waiting_command_,
               OpCodeText(waiting_command_).c_str(), op_code, OpCodeText(op_code).c_str());
    ASSERT_LOG(!command_queue_.front().waiting_for_status_,
               "Waiting for command status 0x%02hx (%s), got command complete for 0x%02hx (%s)", waiting_command_,
               OpCodeText(waiting_command_).c_str(), op_code, OpCodeText(op_code).c_str());
    auto caller_handler = command_queue_.front().caller_handler;
    caller_handler->Post(BindOnce(std::move(command_queue_.front().on_complete), complete_view));
    command_queue_.pop_front();
    waiting_command_ = OpCode::NONE;
    hci_timeout_alarm_->Cancel();
    send_next_command();
  }

  void le_meta_event_callback(EventPacketView event) {
    LeMetaEventView meta_event_view = LeMetaEventView::Create(event);
    ASSERT(meta_event_view.IsValid());
    SubeventCode subevent_code = meta_event_view.GetSubeventCode();
    ASSERT_LOG(subevent_handlers_.find(subevent_code) != subevent_handlers_.end(),
               "Unhandled le event of type 0x%02hhx (%s)", subevent_code, SubeventCodeText(subevent_code).c_str());
    auto& registered_handler = subevent_handlers_[subevent_code].subevent_handler;
    subevent_handlers_[subevent_code].handler->Post(BindOnce(registered_handler, meta_event_view));
  }

  void hciEventReceived(hal::HciPacket event_bytes) override {
    auto packet = packet::PacketView<packet::kLittleEndian>(std::make_shared<std::vector<uint8_t>>(event_bytes));
    EventPacketView event = EventPacketView::Create(packet);
    ASSERT(event.IsValid());
    module_.GetHandler()->Post(
        BindOnce(&HciLayer::impl::hci_event_received_handler, common::Unretained(this), std::move(event)));
  }

  void hci_event_received_handler(EventPacketView event) {
    EventCode event_code = event.GetEventCode();
    ASSERT_LOG(event_handlers_.find(event_code) != event_handlers_.end(), "Unhandled event of type 0x%02hhx (%s)",
               event_code, EventCodeText(event_code).c_str());
    auto& registered_handler = event_handlers_[event_code].event_handler;
    event_handlers_[event_code].handler->Post(BindOnce(registered_handler, std::move(event)));
  }

  void aclDataReceived(hal::HciPacket data_bytes) override {
    auto packet =
        packet::PacketView<packet::kLittleEndian>(std::make_shared<std::vector<uint8_t>>(std::move(data_bytes)));
    AclPacketView acl = AclPacketView::Create(packet);
    incoming_acl_packet_buffer_.Enqueue(std::make_unique<AclPacketView>(acl), module_.GetHandler());
  }

  void scoDataReceived(hal::HciPacket data_bytes) override {
    auto packet = packet::PacketView<packet::kLittleEndian>(std::make_shared<std::vector<uint8_t>>(data_bytes));
    ScoPacketView sco = ScoPacketView::Create(packet);
  }

  void EnqueueCommand(std::unique_ptr<CommandPacketBuilder> command,
                      OnceCallback<void(CommandCompleteView)> on_complete, os::Handler* handler) {
    module_.GetHandler()->Post(common::BindOnce(&impl::handle_enqueue_command_with_complete, common::Unretained(this),
                                                std::move(command), std::move(on_complete),
                                                common::Unretained(handler)));
  }

  void EnqueueCommand(std::unique_ptr<CommandPacketBuilder> command, OnceCallback<void(CommandStatusView)> on_status,
                      os::Handler* handler) {
    module_.GetHandler()->Post(common::BindOnce(&impl::handle_enqueue_command_with_status, common::Unretained(this),
                                                std::move(command), std::move(on_status), common::Unretained(handler)));
  }

  void handle_enqueue_command_with_complete(std::unique_ptr<CommandPacketBuilder> command,
                                            OnceCallback<void(CommandCompleteView)> on_complete, os::Handler* handler) {
    command_queue_.emplace_back(std::move(command), std::move(on_complete), handler);

    send_next_command();
  }

  void handle_enqueue_command_with_status(std::unique_ptr<CommandPacketBuilder> command,
                                          OnceCallback<void(CommandStatusView)> on_status, os::Handler* handler) {
    command_queue_.emplace_back(std::move(command), std::move(on_status), handler);

    send_next_command();
  }

  void send_next_command() {
    if (command_credits_ == 0) {
      return;
    }
    if (waiting_command_ != OpCode::NONE) {
      return;
    }
    if (command_queue_.size() == 0) {
      return;
    }
    std::shared_ptr<std::vector<uint8_t>> bytes = std::make_shared<std::vector<uint8_t>>();
    BitInserter bi(*bytes);
    command_queue_.front().command->Serialize(bi);
    hal_->sendHciCommand(*bytes);
    auto cmd_view = CommandPacketView::Create(bytes);
    ASSERT(cmd_view.IsValid());
    OpCode op_code = cmd_view.GetOpCode();
    waiting_command_ = op_code;
    command_credits_ = 0;  // Only allow one outstanding command
    hci_timeout_alarm_->Schedule(BindOnce(&on_hci_timeout, op_code), kHciTimeoutMs);
  }

  BidiQueueEnd<AclPacketBuilder, AclPacketView>* GetAclQueueEnd() {
    return acl_queue_.GetUpEnd();
  }

  void RegisterEventHandler(EventCode event_code, Callback<void(EventPacketView)> event_handler, os::Handler* handler) {
    module_.GetHandler()->Post(common::BindOnce(&impl::handle_register_event_handler, common::Unretained(this),
                                                event_code, event_handler, common::Unretained(handler)));
  }

  void handle_register_event_handler(EventCode event_code, Callback<void(EventPacketView)> event_handler,
                                     os::Handler* handler) {
    ASSERT_LOG(event_handlers_.count(event_code) == 0, "Can not register a second handler for event_code %02hhx (%s)",
               event_code, EventCodeText(event_code).c_str());
    EventHandler to_save(event_handler, handler);
    event_handlers_[event_code] = to_save;
  }

  void UnregisterEventHandler(EventCode event_code) {
    module_.GetHandler()->Post(
        common::BindOnce(&impl::handle_unregister_event_handler, common::Unretained(this), event_code));
  }

  void handle_unregister_event_handler(EventCode event_code) {
    event_handlers_.erase(event_code);
  }

  void RegisterLeEventHandler(SubeventCode subevent_code, Callback<void(LeMetaEventView)> event_handler,
                              os::Handler* handler) {
    module_.GetHandler()->Post(common::BindOnce(&impl::handle_register_le_event_handler, common::Unretained(this),
                                                subevent_code, event_handler, common::Unretained(handler)));
  }

  void handle_register_le_event_handler(SubeventCode subevent_code, Callback<void(LeMetaEventView)> subevent_handler,
                                        os::Handler* handler) {
    ASSERT_LOG(subevent_handlers_.count(subevent_code) == 0,
               "Can not register a second handler for subevent_code %02hhx (%s)", subevent_code,
               SubeventCodeText(subevent_code).c_str());
    SubeventHandler to_save(subevent_handler, handler);
    subevent_handlers_[subevent_code] = to_save;
  }

  void UnregisterLeEventHandler(SubeventCode subevent_code) {
    module_.GetHandler()->Post(
        common::BindOnce(&impl::handle_unregister_le_event_handler, common::Unretained(this), subevent_code));
  }

  void handle_unregister_le_event_handler(SubeventCode subevent_code) {
    subevent_handlers_.erase(subevent_code);
  }

  // The HAL
  hal::HciHal* hal_;

  // A reference to the HciLayer module
  HciLayer& module_;

  // Interfaces
  SecurityInterfaceImpl security_interface{module_};
  LeSecurityInterfaceImpl le_security_interface{module_};
  LeAdvertisingInterfaceImpl le_advertising_interface{module_};
  LeScanningInterfaceImpl le_scanning_interface{module_};

  // Command Handling
  std::list<CommandQueueEntry> command_queue_;

  std::map<EventCode, EventHandler> event_handlers_;
  std::map<SubeventCode, SubeventHandler> subevent_handlers_;
  OpCode waiting_command_{OpCode::NONE};
  uint8_t command_credits_{1};  // Send reset first
  Alarm* hci_timeout_alarm_{nullptr};

  // Acl packets
  BidiQueue<AclPacketView, AclPacketBuilder> acl_queue_{3 /* TODO: Set queue depth */};
  os::EnqueueBuffer<AclPacketView> incoming_acl_packet_buffer_{acl_queue_.GetDownEnd()};
};

HciLayer::HciLayer() : impl_(std::make_unique<impl>(*this)) {}

HciLayer::~HciLayer() {
  impl_.reset();
}

void HciLayer::EnqueueCommand(std::unique_ptr<CommandPacketBuilder> command,
                              common::OnceCallback<void(CommandCompleteView)> on_complete, os::Handler* handler) {
  impl_->EnqueueCommand(std::move(command), std::move(on_complete), handler);
}

void HciLayer::EnqueueCommand(std::unique_ptr<CommandPacketBuilder> command,
                              common::OnceCallback<void(CommandStatusView)> on_status, os::Handler* handler) {
  impl_->EnqueueCommand(std::move(command), std::move(on_status), handler);
}

common::BidiQueueEnd<AclPacketBuilder, AclPacketView>* HciLayer::GetAclQueueEnd() {
  return impl_->GetAclQueueEnd();
}

void HciLayer::RegisterEventHandler(EventCode event_code, common::Callback<void(EventPacketView)> event_handler,
                                    os::Handler* handler) {
  impl_->RegisterEventHandler(event_code, std::move(event_handler), handler);
}

void HciLayer::UnregisterEventHandler(EventCode event_code) {
  impl_->UnregisterEventHandler(event_code);
}

void HciLayer::RegisterLeEventHandler(SubeventCode subevent_code, common::Callback<void(LeMetaEventView)> event_handler,
                                      os::Handler* handler) {
  impl_->RegisterLeEventHandler(subevent_code, std::move(event_handler), handler);
}

void HciLayer::UnregisterLeEventHandler(SubeventCode subevent_code) {
  impl_->UnregisterLeEventHandler(subevent_code);
}

SecurityInterface* HciLayer::GetSecurityInterface(common::Callback<void(EventPacketView)> event_handler,
                                                  os::Handler* handler) {
  for (const auto event : SecurityInterface::SecurityEvents) {
    RegisterEventHandler(event, event_handler, handler);
  }
  return &impl_->security_interface;
}

LeSecurityInterface* HciLayer::GetLeSecurityInterface(common::Callback<void(LeMetaEventView)> event_handler,
                                                      os::Handler* handler) {
  for (const auto subevent : LeSecurityInterface::LeSecurityEvents) {
    RegisterLeEventHandler(subevent, event_handler, handler);
  }
  return &impl_->le_security_interface;
}

LeAdvertisingInterface* HciLayer::GetLeAdvertisingInterface(common::Callback<void(LeMetaEventView)> event_handler,
                                                            os::Handler* handler) {
  for (const auto subevent : LeAdvertisingInterface::LeAdvertisingEvents) {
    RegisterLeEventHandler(subevent, event_handler, handler);
  }
  return &impl_->le_advertising_interface;
}

LeScanningInterface* HciLayer::GetLeScanningInterface(common::Callback<void(LeMetaEventView)> event_handler,
                                                      os::Handler* handler) {
  for (const auto subevent : LeScanningInterface::LeScanningEvents) {
    RegisterLeEventHandler(subevent, event_handler, handler);
  }
  return &impl_->le_scanning_interface;
}

const ModuleFactory HciLayer::Factory = ModuleFactory([]() { return new HciLayer(); });

void HciLayer::ListDependencies(ModuleList* list) {
  list->add<hal::HciHal>();
}

void HciLayer::Start() {
  impl_->Start(GetDependency<hal::HciHal>());
}

void HciLayer::Stop() {
  impl_->Stop();
}

std::string HciLayer::ToString() const {
  return "Hci Layer";
}
}  // namespace hci
}  // namespace bluetooth
