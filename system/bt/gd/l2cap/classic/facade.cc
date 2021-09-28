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

#include <condition_variable>
#include <cstdint>
#include <unordered_map>

#include "common/bidi_queue.h"
#include "common/bind.h"
#include "grpc/grpc_event_queue.h"
#include "hci/address.h"
#include "l2cap/classic/facade.grpc.pb.h"
#include "l2cap/classic/facade.h"
#include "l2cap/classic/l2cap_classic_module.h"
#include "l2cap/l2cap_packets.h"
#include "os/log.h"
#include "packet/raw_builder.h"

using ::grpc::ServerAsyncResponseWriter;
using ::grpc::ServerAsyncWriter;
using ::grpc::ServerContext;

using ::bluetooth::packet::RawBuilder;

namespace bluetooth {
namespace l2cap {
namespace classic {

class L2capClassicModuleFacadeService : public L2capClassicModuleFacade::Service {
 public:
  L2capClassicModuleFacadeService(L2capClassicModule* l2cap_layer, os::Handler* facade_handler)
      : l2cap_layer_(l2cap_layer), facade_handler_(facade_handler) {
    ASSERT(l2cap_layer_ != nullptr);
    ASSERT(facade_handler_ != nullptr);
  }

  ::grpc::Status FetchConnectionComplete(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                         ::grpc::ServerWriter<classic::ConnectionCompleteEvent>* writer) override {
    return pending_connection_complete_.RunLoop(context, writer);
  }

  ::grpc::Status FetchConnectionClose(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                      ::grpc::ServerWriter<classic::ConnectionCloseEvent>* writer) override {
    return pending_connection_close_.RunLoop(context, writer);
  }

  ::grpc::Status Connect(::grpc::ServerContext* context, const facade::BluetoothAddress* request,
                         ::google::protobuf::Empty* response) override {
    auto fixed_channel_manager = l2cap_layer_->GetFixedChannelManager();
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->address(), peer));
    fixed_channel_manager->ConnectServices(peer, common::BindOnce([](FixedChannelManager::ConnectionResult) {}),
                                           facade_handler_);
    return ::grpc::Status::OK;
  }

  ::grpc::Status SendL2capPacket(::grpc::ServerContext* context, const classic::L2capPacket* request,
                                 SendL2capPacketResult* response) override {
    std::unique_lock<std::mutex> lock(channel_map_mutex_);
    if (fixed_channel_helper_map_.find(request->channel()) == fixed_channel_helper_map_.end()) {
      return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "Channel not registered");
    }
    std::vector<uint8_t> packet(request->payload().begin(), request->payload().end());
    if (!fixed_channel_helper_map_[request->channel()]->SendPacket(packet)) {
      return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "Channel not open");
    }
    response->set_result_type(SendL2capPacketResultType::OK);
    return ::grpc::Status::OK;
  }

  ::grpc::Status SendDynamicChannelPacket(::grpc::ServerContext* context, const DynamicChannelPacket* request,
                                          ::google::protobuf::Empty* response) override {
    std::unique_lock<std::mutex> lock(channel_map_mutex_);
    if (dynamic_channel_helper_map_.find(request->psm()) == dynamic_channel_helper_map_.end()) {
      return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "Psm not registered");
    }
    std::vector<uint8_t> packet(request->payload().begin(), request->payload().end());
    if (!dynamic_channel_helper_map_[request->psm()]->SendPacket(packet)) {
      return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "Channel not open");
    }
    return ::grpc::Status::OK;
  }

  ::grpc::Status OpenChannel(::grpc::ServerContext* context,
                             const ::bluetooth::l2cap::classic::OpenChannelRequest* request,
                             ::google::protobuf::Empty* response) override {
    std::unique_lock<std::mutex> lock(channel_map_mutex_);
    auto psm = request->psm();
    auto mode = request->mode();
    dynamic_channel_helper_map_.emplace(
        psm, std::make_unique<L2capDynamicChannelHelper>(this, l2cap_layer_, facade_handler_, psm, mode));
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->remote().address(), peer));
    dynamic_channel_helper_map_[psm]->Connect(peer);
    return ::grpc::Status::OK;
  }

  ::grpc::Status CloseChannel(::grpc::ServerContext* context,
                              const ::bluetooth::l2cap::classic::CloseChannelRequest* request,
                              ::google::protobuf::Empty* response) override {
    auto psm = request->psm();
    if (dynamic_channel_helper_map_.find(request->psm()) == dynamic_channel_helper_map_.end()) {
      return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "Psm not registered");
    }
    dynamic_channel_helper_map_[psm]->disconnect();
    return ::grpc::Status::OK;
  }

  ::grpc::Status FetchL2capData(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                ::grpc::ServerWriter<classic::L2capPacket>* writer) override {
    {
      std::unique_lock<std::mutex> lock(channel_map_mutex_);

      for (auto& connection : fixed_channel_helper_map_) {
        if (connection.second->channel_ != nullptr) {
          connection.second->channel_->GetQueueUpEnd()->RegisterDequeue(
              facade_handler_,
              common::Bind(&L2capFixedChannelHelper::on_incoming_packet, common::Unretained(connection.second.get())));
        }
      }

      for (auto& connection : dynamic_channel_helper_map_) {
        if (connection.second->channel_ != nullptr) {
          connection.second->channel_->GetQueueUpEnd()->RegisterDequeue(
              facade_handler_, common::Bind(&L2capDynamicChannelHelper::on_incoming_packet,
                                            common::Unretained(connection.second.get())));
        }
      }

      fetch_l2cap_data_ = true;
    }

    auto status = pending_l2cap_data_.RunLoop(context, writer);

    {
      std::unique_lock<std::mutex> lock(channel_map_mutex_);

      fetch_l2cap_data_ = false;

      for (auto& connection : fixed_channel_helper_map_) {
        if (connection.second->channel_ != nullptr) {
          connection.second->channel_->GetQueueUpEnd()->RegisterDequeue(
              facade_handler_,
              common::Bind(&L2capFixedChannelHelper::on_incoming_packet, common::Unretained(connection.second.get())));
        }
      }

      for (auto& connection : dynamic_channel_helper_map_) {
        if (connection.second->channel_ != nullptr) {
          connection.second->channel_->GetQueueUpEnd()->RegisterDequeue(
              facade_handler_, common::Bind(&L2capDynamicChannelHelper::on_incoming_packet,
                                            common::Unretained(connection.second.get())));
        }
      }
    }

    return status;
  }

  ::grpc::Status RegisterChannel(::grpc::ServerContext* context, const classic::RegisterChannelRequest* request,
                                 ::google::protobuf::Empty* response) override {
    std::unique_lock<std::mutex> lock(channel_map_mutex_);
    if (fixed_channel_helper_map_.find(request->channel()) != fixed_channel_helper_map_.end()) {
      return ::grpc::Status(::grpc::StatusCode::FAILED_PRECONDITION, "Already registered");
    }
    fixed_channel_helper_map_.emplace(request->channel(), std::make_unique<L2capFixedChannelHelper>(
                                                              this, l2cap_layer_, facade_handler_, request->channel()));

    return ::grpc::Status::OK;
  }

  class L2capFixedChannelHelper {
   public:
    L2capFixedChannelHelper(L2capClassicModuleFacadeService* service, L2capClassicModule* l2cap_layer,
                            os::Handler* handler, Cid cid)
        : facade_service_(service), l2cap_layer_(l2cap_layer), handler_(handler), cid_(cid) {
      fixed_channel_manager_ = l2cap_layer_->GetFixedChannelManager();
      fixed_channel_manager_->RegisterService(
          cid, {},
          common::BindOnce(&L2capFixedChannelHelper::on_l2cap_service_registration_complete, common::Unretained(this)),
          common::Bind(&L2capFixedChannelHelper::on_connection_open, common::Unretained(this)), handler_);
    }

    void on_l2cap_service_registration_complete(FixedChannelManager::RegistrationResult registration_result,
                                                std::unique_ptr<FixedChannelService> service) {
      service_ = std::move(service);
    }

    void on_connection_open(std::unique_ptr<FixedChannel> channel) {
      ConnectionCompleteEvent event;
      event.mutable_remote()->set_address(channel->GetDevice().ToString());
      facade_service_->pending_connection_complete_.OnIncomingEvent(event);
      channel_ = std::move(channel);
      channel_->RegisterOnCloseCallback(
          facade_service_->facade_handler_,
          common::BindOnce(&L2capFixedChannelHelper::on_close_callback, common::Unretained(this)));
      {
        std::unique_lock<std::mutex> lock(facade_service_->channel_map_mutex_);
        if (facade_service_->fetch_l2cap_data_) {
          channel_->GetQueueUpEnd()->RegisterDequeue(
              facade_service_->facade_handler_,
              common::Bind(&L2capFixedChannelHelper::on_incoming_packet, common::Unretained(this)));
        }
      }
    }

    bool SendPacket(const std::vector<uint8_t>& packet) {
      if (channel_ == nullptr) {
        LOG_WARN("Channel is not open");
        return false;
      }
      channel_->GetQueueUpEnd()->RegisterEnqueue(
          handler_, common::Bind(&L2capFixedChannelHelper::enqueue_callback, common::Unretained(this), packet));
      return true;
    }

    void on_close_callback(hci::ErrorCode error_code) {
      {
        std::unique_lock<std::mutex> lock(facade_service_->channel_map_mutex_);
        if (facade_service_->fetch_l2cap_data_) {
          channel_->GetQueueUpEnd()->UnregisterDequeue();
        }
      }
      channel_ = nullptr;
      classic::ConnectionCloseEvent event;
      event.mutable_remote()->set_address(channel_->GetDevice().ToString());
      event.set_reason(static_cast<uint32_t>(error_code));
      facade_service_->pending_connection_close_.OnIncomingEvent(event);
    }

    void on_incoming_packet() {
      auto packet = channel_->GetQueueUpEnd()->TryDequeue();
      std::string data = std::string(packet->begin(), packet->end());
      L2capPacket l2cap_data;
      l2cap_data.set_channel(cid_);
      l2cap_data.set_payload(data);
      facade_service_->pending_l2cap_data_.OnIncomingEvent(l2cap_data);
    }

    std::unique_ptr<packet::BasePacketBuilder> enqueue_callback(const std::vector<uint8_t>& packet) {
      auto packet_one = std::make_unique<packet::RawBuilder>();
      packet_one->AddOctets(packet);
      channel_->GetQueueUpEnd()->UnregisterEnqueue();
      return packet_one;
    };

    L2capClassicModuleFacadeService* facade_service_;
    L2capClassicModule* l2cap_layer_;
    os::Handler* handler_;
    std::unique_ptr<FixedChannelManager> fixed_channel_manager_;
    std::unique_ptr<FixedChannelService> service_;
    std::unique_ptr<FixedChannel> channel_ = nullptr;
    Cid cid_;
  };

  ::grpc::Status SetDynamicChannel(::grpc::ServerContext* context, const SetEnableDynamicChannelRequest* request,
                                   google::protobuf::Empty* response) override {
    dynamic_channel_helper_map_.emplace(
        request->psm(), std::make_unique<L2capDynamicChannelHelper>(this, l2cap_layer_, facade_handler_, request->psm(),
                                                                    request->retransmission_mode()));
    return ::grpc::Status::OK;
  }

  class L2capDynamicChannelHelper {
   public:
    L2capDynamicChannelHelper(L2capClassicModuleFacadeService* service, L2capClassicModule* l2cap_layer,
                              os::Handler* handler, Psm psm, RetransmissionFlowControlMode mode)
        : facade_service_(service), l2cap_layer_(l2cap_layer), handler_(handler), psm_(psm) {
      dynamic_channel_manager_ = l2cap_layer_->GetDynamicChannelManager();
      DynamicChannelConfigurationOption configuration_option = {};
      if (mode == RetransmissionFlowControlMode::BASIC) {
        configuration_option.channel_mode =
            DynamicChannelConfigurationOption::RetransmissionAndFlowControlMode::L2CAP_BASIC;
      } else if (mode == RetransmissionFlowControlMode::ERTM) {
        configuration_option.channel_mode =
            DynamicChannelConfigurationOption::RetransmissionAndFlowControlMode::ENHANCED_RETRANSMISSION;
      }
      dynamic_channel_manager_->RegisterService(
          psm, configuration_option, {},
          common::BindOnce(&L2capDynamicChannelHelper::on_l2cap_service_registration_complete,
                           common::Unretained(this)),
          common::Bind(&L2capDynamicChannelHelper::on_connection_open, common::Unretained(this)), handler_);
    }

    void Connect(hci::Address address) {
      // TODO: specify channel mode
      dynamic_channel_manager_->ConnectChannel(
          address, {}, psm_, common::Bind(&L2capDynamicChannelHelper::on_connection_open, common::Unretained(this)),
          common::Bind(&L2capDynamicChannelHelper::on_connect_fail, common::Unretained(this)), handler_);
    }

    void disconnect() {
      channel_->Close();
    }

    void on_l2cap_service_registration_complete(DynamicChannelManager::RegistrationResult registration_result,
                                                std::unique_ptr<DynamicChannelService> service) {}

    void on_connection_open(std::unique_ptr<DynamicChannel> channel) {
      ConnectionCompleteEvent event;
      event.mutable_remote()->set_address(channel->GetDevice().ToString());
      facade_service_->pending_connection_complete_.OnIncomingEvent(event);
      {
        std::unique_lock<std::mutex> lock(channel_open_cv_mutex_);
        channel_ = std::move(channel);
      }
      channel_open_cv_.notify_all();
      channel_->RegisterOnCloseCallback(
          facade_service_->facade_handler_,
          common::BindOnce(&L2capDynamicChannelHelper::on_close_callback, common::Unretained(this)));
      {
        std::unique_lock<std::mutex> lock(facade_service_->channel_map_mutex_);
        if (facade_service_->fetch_l2cap_data_) {
          channel_->GetQueueUpEnd()->RegisterDequeue(
              facade_service_->facade_handler_,
              common::Bind(&L2capDynamicChannelHelper::on_incoming_packet, common::Unretained(this)));
        }
      }
    }

    void on_close_callback(hci::ErrorCode error_code) {
      {
        std::unique_lock<std::mutex> lock(channel_open_cv_mutex_);
        if (facade_service_->fetch_l2cap_data_) {
          channel_->GetQueueUpEnd()->UnregisterDequeue();
        }
      }
      classic::ConnectionCloseEvent event;
      event.mutable_remote()->set_address(channel_->GetDevice().ToString());
      channel_ = nullptr;
      event.set_reason(static_cast<uint32_t>(error_code));
      facade_service_->pending_connection_close_.OnIncomingEvent(event);
    }

    void on_connect_fail(DynamicChannelManager::ConnectionResult result) {}

    void on_incoming_packet() {
      auto packet = channel_->GetQueueUpEnd()->TryDequeue();
      std::string data = std::string(packet->begin(), packet->end());
      L2capPacket l2cap_data;
      //      l2cap_data.set_channel(cid_);
      l2cap_data.set_payload(data);
      facade_service_->pending_l2cap_data_.OnIncomingEvent(l2cap_data);
    }

    bool SendPacket(std::vector<uint8_t> packet) {
      if (channel_ == nullptr) {
        std::unique_lock<std::mutex> lock(channel_open_cv_mutex_);
        if (!channel_open_cv_.wait_for(lock, std::chrono::seconds(1), [this] { return channel_ != nullptr; })) {
          LOG_WARN("Channel is not open");
          return false;
        }
      }
      channel_->GetQueueUpEnd()->RegisterEnqueue(
          handler_, common::Bind(&L2capDynamicChannelHelper::enqueue_callback, common::Unretained(this), packet));
      return true;
    }

    std::unique_ptr<packet::BasePacketBuilder> enqueue_callback(std::vector<uint8_t> packet) {
      auto packet_one = std::make_unique<packet::RawBuilder>(2000);
      packet_one->AddOctets(packet);
      channel_->GetQueueUpEnd()->UnregisterEnqueue();
      return packet_one;
    };

    L2capClassicModuleFacadeService* facade_service_;
    L2capClassicModule* l2cap_layer_;
    os::Handler* handler_;
    std::unique_ptr<DynamicChannelManager> dynamic_channel_manager_;
    std::unique_ptr<DynamicChannelService> service_;
    std::unique_ptr<DynamicChannel> channel_ = nullptr;
    Psm psm_;
    std::condition_variable channel_open_cv_;
    std::mutex channel_open_cv_mutex_;
  };

  L2capClassicModule* l2cap_layer_;
  ::bluetooth::os::Handler* facade_handler_;
  std::mutex channel_map_mutex_;
  std::map<Cid, std::unique_ptr<L2capFixedChannelHelper>> fixed_channel_helper_map_;
  std::map<Psm, std::unique_ptr<L2capDynamicChannelHelper>> dynamic_channel_helper_map_;
  bool fetch_l2cap_data_ = false;
  ::bluetooth::grpc::GrpcEventQueue<classic::ConnectionCompleteEvent> pending_connection_complete_{
      "FetchConnectionComplete"};
  ::bluetooth::grpc::GrpcEventQueue<classic::ConnectionCloseEvent> pending_connection_close_{"FetchConnectionClose"};
  ::bluetooth::grpc::GrpcEventQueue<L2capPacket> pending_l2cap_data_{"FetchL2capData"};
};

void L2capClassicModuleFacadeModule::ListDependencies(ModuleList* list) {
  ::bluetooth::grpc::GrpcFacadeModule::ListDependencies(list);
  list->add<l2cap::classic::L2capClassicModule>();
}

void L2capClassicModuleFacadeModule::Start() {
  ::bluetooth::grpc::GrpcFacadeModule::Start();
  service_ = new L2capClassicModuleFacadeService(GetDependency<l2cap::classic::L2capClassicModule>(), GetHandler());
}

void L2capClassicModuleFacadeModule::Stop() {
  delete service_;
  ::bluetooth::grpc::GrpcFacadeModule::Stop();
}

::grpc::Service* L2capClassicModuleFacadeModule::GetService() const {
  return service_;
}

const ModuleFactory L2capClassicModuleFacadeModule::Factory =
    ::bluetooth::ModuleFactory([]() { return new L2capClassicModuleFacadeModule(); });

}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
