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

#include "hci/facade/facade.h"

#include <memory>

#include "common/bind.h"
#include "grpc/grpc_event_queue.h"
#include "hci/controller.h"
#include "hci/facade/facade.grpc.pb.h"
#include "hci/hci_layer.h"
#include "hci/hci_packets.h"

using ::grpc::ServerAsyncResponseWriter;
using ::grpc::ServerAsyncWriter;
using ::grpc::ServerContext;

namespace bluetooth {
namespace hci {
namespace facade {

class HciLayerFacadeService : public HciLayerFacade::Service {
 public:
  HciLayerFacadeService(HciLayer* hci_layer, Controller* controller, ::bluetooth::os::Handler* facade_handler)
      : hci_layer_(hci_layer), controller_(controller), facade_handler_(facade_handler) {}

  virtual ~HciLayerFacadeService() {
    if (unregister_acl_dequeue_) {
      hci_layer_->GetAclQueueEnd()->UnregisterDequeue();
    }
    if (waiting_acl_packet_ != nullptr) {
      hci_layer_->GetAclQueueEnd()->UnregisterEnqueue();
      if (waiting_acl_packet_ != nullptr) {
        waiting_acl_packet_.reset();
      }
    }
  }

  class TestCommandBuilder : public CommandPacketBuilder {
   public:
    explicit TestCommandBuilder(std::vector<uint8_t> bytes)
        : CommandPacketBuilder(OpCode::NONE), bytes_(std::move(bytes)) {}
    size_t size() const override {
      return bytes_.size();
    }
    void Serialize(BitInserter& bit_inserter) const override {
      for (auto&& b : bytes_) {
        bit_inserter.insert_byte(b);
      }
    }

   private:
    std::vector<uint8_t> bytes_;
  };

  ::grpc::Status EnqueueCommandWithComplete(::grpc::ServerContext* context, const ::bluetooth::hci::CommandMsg* command,
                                            ::google::protobuf::Empty* response) override {
    auto packet = std::make_unique<TestCommandBuilder>(
        std::vector<uint8_t>(command->command().begin(), command->command().end()));
    hci_layer_->EnqueueCommand(std::move(packet),
                               common::BindOnce(&HciLayerFacadeService::on_complete, common::Unretained(this)),
                               facade_handler_);
    return ::grpc::Status::OK;
  }

  ::grpc::Status EnqueueCommandWithStatus(::grpc::ServerContext* context, const ::bluetooth::hci::CommandMsg* command,
                                          ::google::protobuf::Empty* response) override {
    auto packet = std::make_unique<TestCommandBuilder>(
        std::vector<uint8_t>(command->command().begin(), command->command().end()));
    hci_layer_->EnqueueCommand(std::move(packet),
                               common::BindOnce(&HciLayerFacadeService::on_status, common::Unretained(this)),
                               facade_handler_);
    return ::grpc::Status::OK;
  }

  ::grpc::Status RegisterEventHandler(::grpc::ServerContext* context, const ::bluetooth::hci::EventCodeMsg* event,
                                      ::google::protobuf::Empty* response) override {
    hci_layer_->RegisterEventHandler(static_cast<EventCode>(event->code()),
                                     common::Bind(&HciLayerFacadeService::on_event, common::Unretained(this)),
                                     facade_handler_);
    return ::grpc::Status::OK;
  }

  ::grpc::Status RegisterLeEventHandler(::grpc::ServerContext* context,
                                        const ::bluetooth::hci::LeSubeventCodeMsg* event,
                                        ::google::protobuf::Empty* response) override {
    hci_layer_->RegisterLeEventHandler(static_cast<SubeventCode>(event->code()),
                                       common::Bind(&HciLayerFacadeService::on_le_subevent, common::Unretained(this)),
                                       facade_handler_);
    return ::grpc::Status::OK;
  }

  ::grpc::Status FetchEvents(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                             ::grpc::ServerWriter<EventMsg>* writer) override {
    return pending_events_.RunLoop(context, writer);
  };

  ::grpc::Status FetchLeSubevents(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                  ::grpc::ServerWriter<LeSubeventMsg>* writer) override {
    return pending_le_events_.RunLoop(context, writer);
  };

  class TestAclBuilder : public AclPacketBuilder {
   public:
    explicit TestAclBuilder(uint16_t handle, uint8_t packet_boundary_flag, uint8_t broadcast_flag,
                            std::vector<uint8_t> payload)
        : AclPacketBuilder(0xbad, PacketBoundaryFlag::CONTINUING_FRAGMENT, BroadcastFlag::ACTIVE_SLAVE_BROADCAST),
          handle_(handle), pb_flag_(packet_boundary_flag), b_flag_(broadcast_flag), bytes_(std::move(payload)) {}

    size_t size() const override {
      return bytes_.size();
    }
    void Serialize(BitInserter& bit_inserter) const override {
      LOG_INFO("handle 0x%hx boundary 0x%hhx broadcast 0x%hhx", handle_, pb_flag_, b_flag_);
      bit_inserter.insert_byte(handle_);
      bit_inserter.insert_bits((handle_ >> 8) & 0xf, 4);
      bit_inserter.insert_bits(pb_flag_, 2);
      bit_inserter.insert_bits(b_flag_, 2);
      bit_inserter.insert_byte(bytes_.size() & 0xff);
      bit_inserter.insert_byte((bytes_.size() & 0xff00) >> 8);
      for (auto&& b : bytes_) {
        bit_inserter.insert_byte(b);
      }
    }

   private:
    uint16_t handle_;
    uint8_t pb_flag_;
    uint8_t b_flag_;
    std::vector<uint8_t> bytes_;
  };

  ::grpc::Status SendAclData(::grpc::ServerContext* context, const ::bluetooth::hci::AclMsg* acl,
                             ::google::protobuf::Empty* response) override {
    waiting_acl_packet_ =
        std::make_unique<TestAclBuilder>(acl->handle(), acl->packet_boundary_flag(), acl->broadcast_flag(),
                                         std::vector<uint8_t>(acl->data().begin(), acl->data().end()));
    std::promise<void> enqueued;
    auto future = enqueued.get_future();
    if (!completed_packets_callback_registered_) {
      controller_->RegisterCompletedAclPacketsCallback(common::Bind([](uint16_t, uint16_t) { /* do nothing */ }),
                                                       facade_handler_);
      completed_packets_callback_registered_ = true;
    }
    hci_layer_->GetAclQueueEnd()->RegisterEnqueue(
        facade_handler_, common::Bind(&HciLayerFacadeService::handle_enqueue_acl, common::Unretained(this),
                                      common::Unretained(&enqueued)));
    auto result = future.wait_for(std::chrono::milliseconds(100));
    ASSERT(std::future_status::ready == result);
    return ::grpc::Status::OK;
  }

  ::grpc::Status FetchAclPackets(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                 ::grpc::ServerWriter<AclMsg>* writer) override {
    hci_layer_->GetAclQueueEnd()->RegisterDequeue(
        facade_handler_, common::Bind(&HciLayerFacadeService::on_acl_ready, common::Unretained(this)));
    unregister_acl_dequeue_ = true;
    return pending_acl_events_.RunLoop(context, writer);
  };

 private:
  std::unique_ptr<AclPacketBuilder> handle_enqueue_acl(std::promise<void>* promise) {
    promise->set_value();
    hci_layer_->GetAclQueueEnd()->UnregisterEnqueue();
    return std::move(waiting_acl_packet_);
  }

  void on_acl_ready() {
    auto acl_ptr = hci_layer_->GetAclQueueEnd()->TryDequeue();
    ASSERT(acl_ptr != nullptr);
    ASSERT(acl_ptr->IsValid());
    LOG_INFO("Got an Acl message for handle 0x%hx", acl_ptr->GetHandle());
    AclMsg incoming;
    incoming.set_data(std::string(acl_ptr->begin(), acl_ptr->end()));
    pending_acl_events_.OnIncomingEvent(std::move(incoming));
  }

  void on_event(hci::EventPacketView view) {
    ASSERT(view.IsValid());
    LOG_INFO("Got an Event %s", EventCodeText(view.GetEventCode()).c_str());
    EventMsg response;
    response.set_event(std::string(view.begin(), view.end()));
    pending_events_.OnIncomingEvent(std::move(response));
  }

  void on_le_subevent(hci::LeMetaEventView view) {
    ASSERT(view.IsValid());
    LOG_INFO("Got an LE Event %s", SubeventCodeText(view.GetSubeventCode()).c_str());
    LeSubeventMsg response;
    response.set_event(std::string(view.begin(), view.end()));
    pending_le_events_.OnIncomingEvent(std::move(response));
  }

  void on_complete(hci::CommandCompleteView view) {
    ASSERT(view.IsValid());
    LOG_INFO("Got a Command complete %s", OpCodeText(view.GetCommandOpCode()).c_str());
    EventMsg response;
    response.set_event(std::string(view.begin(), view.end()));
    pending_events_.OnIncomingEvent(std::move(response));
  }

  void on_status(hci::CommandStatusView view) {
    ASSERT(view.IsValid());
    LOG_INFO("Got a Command status %s", OpCodeText(view.GetCommandOpCode()).c_str());
    EventMsg response;
    response.set_event(std::string(view.begin(), view.end()));
    pending_events_.OnIncomingEvent(std::move(response));
  }

  HciLayer* hci_layer_;
  Controller* controller_;
  ::bluetooth::os::Handler* facade_handler_;
  ::bluetooth::grpc::GrpcEventQueue<EventMsg> pending_events_{"FetchHciEvent"};
  ::bluetooth::grpc::GrpcEventQueue<LeSubeventMsg> pending_le_events_{"FetchLeSubevent"};
  ::bluetooth::grpc::GrpcEventQueue<AclMsg> pending_acl_events_{"FetchAclData"};
  bool unregister_acl_dequeue_{false};
  std::unique_ptr<TestAclBuilder> waiting_acl_packet_;
  bool completed_packets_callback_registered_{false};
};

void HciLayerFacadeModule::ListDependencies(ModuleList* list) {
  ::bluetooth::grpc::GrpcFacadeModule::ListDependencies(list);
  list->add<HciLayer>();
  list->add<Controller>();
}

void HciLayerFacadeModule::Start() {
  ::bluetooth::grpc::GrpcFacadeModule::Start();
  service_ = new HciLayerFacadeService(GetDependency<HciLayer>(), GetDependency<Controller>(), GetHandler());
}

void HciLayerFacadeModule::Stop() {
  delete service_;
  ::bluetooth::grpc::GrpcFacadeModule::Stop();
}

::grpc::Service* HciLayerFacadeModule::GetService() const {
  return service_;
}

const ModuleFactory HciLayerFacadeModule::Factory =
    ::bluetooth::ModuleFactory([]() { return new HciLayerFacadeModule(); });

}  // namespace facade
}  // namespace hci
}  // namespace bluetooth
