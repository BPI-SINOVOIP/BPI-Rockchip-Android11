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

#include <cstdint>
#include <unordered_map>
#include <utility>

#include "common/bidi_queue.h"
#include "common/bind.h"
#include "hci/address.h"
#include "hci/address_with_type.h"
#include "hci/facade/le_advertising_manager_facade.grpc.pb.h"
#include "hci/facade/le_advertising_manager_facade.h"
#include "hci/facade/le_advertising_manager_facade.pb.h"
#include "hci/le_advertising_manager.h"
#include "os/log.h"

namespace bluetooth {
namespace hci {
namespace facade {

using ::grpc::ServerAsyncResponseWriter;
using ::grpc::ServerAsyncWriter;
using ::grpc::ServerContext;
using ::grpc::ServerWriter;
using ::grpc::Status;

using ::bluetooth::facade::BluetoothAddress;
using ::bluetooth::facade::BluetoothAddressTypeEnum;

hci::GapData GapDataFromProto(const GapDataMsg& gap_data_proto) {
  hci::GapData gap_data;
  auto data_copy = std::make_shared<std::vector<uint8_t>>(gap_data_proto.data().begin(), gap_data_proto.data().end());
  packet::PacketView<packet::kLittleEndian> packet(data_copy);
  auto after = hci::GapData::Parse(&gap_data, packet.begin());
  ASSERT(after != packet.begin());
  return gap_data;
}

bool AdvertisingConfigFromProto(const AdvertisingConfig& config_proto, hci::AdvertisingConfig* config) {
  for (const auto& elem : config_proto.advertisement()) {
    config->advertisement.push_back(GapDataFromProto(elem));
  }

  for (const auto& elem : config_proto.scan_response()) {
    config->scan_response.push_back(GapDataFromProto(elem));
  }

  hci::Address::FromString(config_proto.random_address().address(), config->random_address);

  if (config_proto.interval_min() > UINT16_MAX || config_proto.interval_min() < 0) {
    LOG_WARN("Bad interval_min: %d", config_proto.interval_min());
    return false;
  }
  config->interval_min = static_cast<uint16_t>(config_proto.interval_min());

  if (config_proto.interval_max() > UINT16_MAX || config_proto.interval_max() < 0) {
    LOG_WARN("Bad interval_max: %d", config_proto.interval_max());
    return false;
  }
  config->interval_max = static_cast<uint16_t>(config_proto.interval_max());

  config->event_type = static_cast<hci::AdvertisingEventType>(config_proto.event_type());

  config->address_type = static_cast<::bluetooth::hci::AddressType>(config_proto.address_type());

  config->peer_address_type = static_cast<::bluetooth::hci::PeerAddressType>(config_proto.peer_address_type());

  hci::Address::FromString(config_proto.peer_address().address(), config->peer_address);

  if (config_proto.channel_map() > UINT8_MAX || config_proto.channel_map() < 0) {
    LOG_WARN("Bad channel_map: %d", config_proto.channel_map());
    return false;
  }
  config->channel_map = static_cast<uint8_t>(config_proto.channel_map());

  if (config_proto.tx_power() > UINT8_MAX || config_proto.tx_power() < 0) {
    LOG_WARN("Bad tx_power: %d", config_proto.tx_power());
    return false;
  }

  config->filter_policy = static_cast<hci::AdvertisingFilterPolicy>(config_proto.filter_policy());

  config->tx_power = static_cast<uint8_t>(config_proto.tx_power());
  return true;
}

class LeAdvertiser {
 public:
  LeAdvertiser(hci::AdvertisingConfig config) : config_(std::move(config)) {}

  void ScanCallback(Address address, AddressType address_type) {}

  void TerminatedCallback(ErrorCode error_code, uint8_t, uint8_t) {}

  hci::AdvertiserId GetAdvertiserId() {
    return id_;
  }

  void SetAdvertiserId(hci::AdvertiserId id) {
    id_ = id;
  }

 private:
  hci::AdvertiserId id_ = LeAdvertisingManager::kInvalidId;
  hci::AdvertisingConfig config_;
};

class LeAdvertisingManagerFacadeService : public LeAdvertisingManagerFacade::Service {
 public:
  LeAdvertisingManagerFacadeService(LeAdvertisingManager* le_advertising_manager, os::Handler* facade_handler)
      : le_advertising_manager_(le_advertising_manager), facade_handler_(facade_handler) {
    ASSERT(le_advertising_manager_ != nullptr);
    ASSERT(facade_handler_ != nullptr);
  }

  ::grpc::Status CreateAdvertiser(::grpc::ServerContext* context, const CreateAdvertiserRequest* request,
                                  CreateAdvertiserResponse* response) override {
    hci::AdvertisingConfig config = {};
    if (!AdvertisingConfigFromProto(request->config(), &config)) {
      LOG_WARN("Error parsing advertising config %s", request->SerializeAsString().c_str());
      response->set_advertiser_id(LeAdvertisingManager::kInvalidId);
      return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "Error while parsing advertising config");
    }
    LeAdvertiser le_advertiser(config);
    auto advertiser_id = le_advertising_manager_->CreateAdvertiser(
        config, common::Bind(&LeAdvertiser::ScanCallback, common::Unretained(&le_advertiser)),
        common::Bind(&LeAdvertiser::TerminatedCallback, common::Unretained(&le_advertiser)), facade_handler_);
    if (advertiser_id != LeAdvertisingManager::kInvalidId) {
      le_advertiser.SetAdvertiserId(advertiser_id);
      le_advertisers_.push_back(le_advertiser);
    } else {
      LOG_WARN("Failed to create advertiser");
    }
    response->set_advertiser_id(advertiser_id);
    return ::grpc::Status::OK;
  }

  ::grpc::Status ExtendedCreateAdvertiser(::grpc::ServerContext* context,
                                          const ExtendedCreateAdvertiserRequest* request,
                                          ExtendedCreateAdvertiserResponse* response) override {
    LOG_WARN("ExtendedCreateAdvertiser is not implemented");
    response->set_advertiser_id(LeAdvertisingManager::kInvalidId);
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "ExtendedCreateAdvertiser is not implemented");
  }

  ::grpc::Status GetNumberOfAdvertisingInstances(::grpc::ServerContext* context,
                                                 const ::google::protobuf::Empty* request,
                                                 GetNumberOfAdvertisingInstancesResponse* response) override {
    response->set_num_advertising_instances(le_advertising_manager_->GetNumberOfAdvertisingInstances());
    return ::grpc::Status::OK;
  }

  ::grpc::Status RemoveAdvertiser(::grpc::ServerContext* context, const RemoveAdvertiserRequest* request,
                                  ::google::protobuf::Empty* response) override {
    if (request->advertiser_id() == LeAdvertisingManager::kInvalidId) {
      LOG_WARN("Invalid advertiser ID %d", request->advertiser_id());
      return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "Invlid advertiser ID received");
    }
    le_advertising_manager_->RemoveAdvertiser(request->advertiser_id());
    for (auto iter = le_advertisers_.begin(); iter != le_advertisers_.end();) {
      if (iter->GetAdvertiserId() == request->advertiser_id()) {
        iter = le_advertisers_.erase(iter);
      } else {
        ++iter;
      }
    }
    return ::grpc::Status::OK;
  }

  std::vector<LeAdvertiser> le_advertisers_;
  LeAdvertisingManager* le_advertising_manager_;
  os::Handler* facade_handler_;
};

void LeAdvertisingManagerFacadeModule::ListDependencies(ModuleList* list) {
  ::bluetooth::grpc::GrpcFacadeModule::ListDependencies(list);
  list->add<hci::LeAdvertisingManager>();
}

void LeAdvertisingManagerFacadeModule::Start() {
  ::bluetooth::grpc::GrpcFacadeModule::Start();
  service_ = new LeAdvertisingManagerFacadeService(GetDependency<hci::LeAdvertisingManager>(), GetHandler());
}

void LeAdvertisingManagerFacadeModule::Stop() {
  delete service_;
  ::bluetooth::grpc::GrpcFacadeModule::Stop();
}

::grpc::Service* LeAdvertisingManagerFacadeModule::GetService() const {
  return service_;
}

const ModuleFactory LeAdvertisingManagerFacadeModule::Factory =
    ::bluetooth::ModuleFactory([]() { return new LeAdvertisingManagerFacadeModule(); });

}  // namespace facade
}  // namespace hci
}  // namespace bluetooth
