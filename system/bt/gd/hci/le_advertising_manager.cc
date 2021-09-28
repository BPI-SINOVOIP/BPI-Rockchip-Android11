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
#include <memory>
#include <mutex>

#include "hci/controller.h"
#include "hci/hci_layer.h"
#include "hci/hci_packets.h"
#include "hci/le_advertising_interface.h"
#include "hci/le_advertising_manager.h"
#include "module.h"
#include "os/handler.h"
#include "os/log.h"

namespace bluetooth {
namespace hci {

const ModuleFactory LeAdvertisingManager::Factory = ModuleFactory([]() { return new LeAdvertisingManager(); });

enum class AdvertisingApiType {
  LE_4_0 = 1,
  ANDROID_HCI = 2,
  LE_5_0 = 3,
};

struct Advertiser {
  os::Handler* handler;
  common::Callback<void(Address, AddressType)> scan_callback;
  common::Callback<void(ErrorCode, uint8_t, uint8_t)> set_terminated_callback;
};

ExtendedAdvertisingConfig::ExtendedAdvertisingConfig(const AdvertisingConfig& config) : AdvertisingConfig(config) {
  switch (config.event_type) {
    case AdvertisingEventType::ADV_IND:
      connectable = true;
      scannable = true;
      break;
    case AdvertisingEventType::ADV_DIRECT_IND:
      connectable = true;
      directed = true;
      high_duty_directed_connectable = true;
      break;
    case AdvertisingEventType::ADV_SCAN_IND:
      scannable = true;
      break;
    case AdvertisingEventType::ADV_NONCONN_IND:
      break;
    case AdvertisingEventType::ADV_DIRECT_IND_LOW:
      connectable = true;
      directed = true;
      break;
    default:
      LOG_WARN("Unknown event type");
      break;
  }
  if (config.address_type == AddressType::PUBLIC_DEVICE_ADDRESS) {
    own_address_type = OwnAddressType::PUBLIC_DEVICE_ADDRESS;
  } else if (config.address_type == AddressType::RANDOM_DEVICE_ADDRESS) {
    own_address_type = OwnAddressType::RANDOM_DEVICE_ADDRESS;
  }
  // TODO(b/149221472): Support fragmentation
  operation = Operation::COMPLETE_ADVERTISEMENT;
}

struct LeAdvertisingManager::impl {
  impl(Module* module) : module_(module), le_advertising_interface_(nullptr), num_instances_(0) {}

  void start(os::Handler* handler, hci::HciLayer* hci_layer, hci::Controller* controller) {
    module_handler_ = handler;
    hci_layer_ = hci_layer;
    controller_ = controller;
    le_advertising_interface_ = hci_layer_->GetLeAdvertisingInterface(
        common::Bind(&LeAdvertisingManager::impl::handle_event, common::Unretained(this)), module_handler_);
    num_instances_ = controller_->GetControllerLeNumberOfSupportedAdverisingSets();
    enabled_sets_ = std::vector<EnabledSet>(num_instances_);
    if (controller_->IsSupported(hci::OpCode::LE_SET_EXTENDED_ADVERTISING_PARAMETERS)) {
      advertising_api_type_ = AdvertisingApiType::LE_5_0;
    } else if (controller_->IsSupported(hci::OpCode::LE_MULTI_ADVT)) {
      advertising_api_type_ = AdvertisingApiType::ANDROID_HCI;
    } else {
      advertising_api_type_ = AdvertisingApiType::LE_4_0;
    }
  }

  size_t GetNumberOfAdvertisingInstances() const {
    return num_instances_;
  }

  void handle_event(LeMetaEventView event) {
    switch (event.GetSubeventCode()) {
      case hci::SubeventCode::SCAN_REQUEST_RECEIVED:
        handle_scan_request(LeScanRequestReceivedView::Create(event));
        break;
      case hci::SubeventCode::ADVERTISING_SET_TERMINATED:
        handle_set_terminated(LeAdvertisingSetTerminatedView::Create(event));
        break;
      default:
        LOG_INFO("Unknown subevent in scanner %s", hci::SubeventCodeText(event.GetSubeventCode()).c_str());
    }
  }

  void handle_scan_request(LeScanRequestReceivedView event_view) {
    if (!event_view.IsValid()) {
      LOG_INFO("Dropping invalid scan request event");
      return;
    }
    registered_handler_->Post(
        common::BindOnce(scan_callback_, event_view.GetScannerAddress(), event_view.GetScannerAddressType()));
  }

  void handle_set_terminated(LeAdvertisingSetTerminatedView event_view) {
    if (!event_view.IsValid()) {
      LOG_INFO("Dropping invalid advertising event");
      return;
    }
    registered_handler_->Post(common::BindOnce(set_terminated_callback_, event_view.GetStatus(),
                                               event_view.GetAdvertisingHandle(),
                                               event_view.GetNumCompletedExtendedAdvertisingEvents()));
  }

  AdvertiserId allocate_advertiser() {
    AdvertiserId id = 0;
    {
      std::unique_lock lock(id_mutex_);
      while (id < num_instances_ && advertising_sets_.count(id) != 0) {
        id++;
      }
    }
    if (id == num_instances_) {
      return kInvalidId;
    }
    return id;
  }

  void remove_advertiser(AdvertiserId id) {
    stop_advertising(id);
    std::unique_lock lock(id_mutex_);
    if (advertising_sets_.count(id) == 0) {
      return;
    }
    advertising_sets_.erase(id);
  }

  void create_advertiser(AdvertiserId id, const AdvertisingConfig& config,
                         const common::Callback<void(Address, AddressType)>& scan_callback,
                         const common::Callback<void(ErrorCode, uint8_t, uint8_t)>& set_terminated_callback,
                         os::Handler* handler) {
    advertising_sets_[id].scan_callback = scan_callback;
    advertising_sets_[id].set_terminated_callback = set_terminated_callback;
    advertising_sets_[id].handler = handler;
    switch (advertising_api_type_) {
      case (AdvertisingApiType::LE_4_0):
        le_advertising_interface_->EnqueueCommand(
            hci::LeSetAdvertisingParametersBuilder::Create(
                config.interval_min, config.interval_max, config.event_type, config.address_type,
                config.peer_address_type, config.peer_address, config.channel_map, config.filter_policy),
            common::BindOnce(impl::check_status<LeSetAdvertisingParametersCompleteView>), module_handler_);
        le_advertising_interface_->EnqueueCommand(hci::LeSetRandomAddressBuilder::Create(config.random_address),
                                                  common::BindOnce(impl::check_status<LeSetRandomAddressCompleteView>),
                                                  module_handler_);
        if (!config.scan_response.empty()) {
          le_advertising_interface_->EnqueueCommand(
              hci::LeSetScanResponseDataBuilder::Create(config.scan_response),
              common::BindOnce(impl::check_status<LeSetScanResponseDataCompleteView>), module_handler_);
        }
        le_advertising_interface_->EnqueueCommand(
            hci::LeSetAdvertisingDataBuilder::Create(config.advertisement),
            common::BindOnce(impl::check_status<LeSetAdvertisingDataCompleteView>), module_handler_);
        le_advertising_interface_->EnqueueCommand(
            hci::LeSetAdvertisingEnableBuilder::Create(Enable::ENABLED),
            common::BindOnce(impl::check_status<LeSetAdvertisingEnableCompleteView>), module_handler_);
        break;
      case (AdvertisingApiType::ANDROID_HCI):
        le_advertising_interface_->EnqueueCommand(
            hci::LeMultiAdvtParamBuilder::Create(config.interval_min, config.interval_max, config.event_type,
                                                 config.address_type, config.peer_address_type, config.peer_address,
                                                 config.channel_map, config.filter_policy, id, config.tx_power),
            common::BindOnce(impl::check_status<LeMultiAdvtCompleteView>), module_handler_);
        le_advertising_interface_->EnqueueCommand(hci::LeMultiAdvtSetDataBuilder::Create(config.advertisement, id),
                                                  common::BindOnce(impl::check_status<LeMultiAdvtCompleteView>),
                                                  module_handler_);
        if (!config.scan_response.empty()) {
          le_advertising_interface_->EnqueueCommand(
              hci::LeMultiAdvtSetScanRespBuilder::Create(config.scan_response, id),
              common::BindOnce(impl::check_status<LeMultiAdvtCompleteView>), module_handler_);
        }
        le_advertising_interface_->EnqueueCommand(
            hci::LeMultiAdvtSetRandomAddrBuilder::Create(config.random_address, id),
            common::BindOnce(impl::check_status<LeMultiAdvtCompleteView>), module_handler_);
        le_advertising_interface_->EnqueueCommand(hci::LeMultiAdvtSetEnableBuilder::Create(Enable::ENABLED, id),
                                                  common::BindOnce(impl::check_status<LeMultiAdvtCompleteView>),
                                                  module_handler_);
        break;
      case (AdvertisingApiType::LE_5_0): {
        ExtendedAdvertisingConfig new_config = config;
        new_config.legacy_pdus = true;
        create_extended_advertiser(id, new_config, scan_callback, set_terminated_callback, handler);
      } break;
    }
  }

  void create_extended_advertiser(AdvertiserId id, const ExtendedAdvertisingConfig& config,
                                  const common::Callback<void(Address, AddressType)>& scan_callback,
                                  const common::Callback<void(ErrorCode, uint8_t, uint8_t)>& set_terminated_callback,
                                  os::Handler* handler) {
    if (advertising_api_type_ != AdvertisingApiType::LE_5_0) {
      create_advertiser(id, config, scan_callback, set_terminated_callback, handler);
      return;
    }

    if (config.legacy_pdus) {
      LegacyAdvertisingProperties legacy_properties = LegacyAdvertisingProperties::ADV_IND;
      if (config.connectable && config.directed) {
        if (config.high_duty_directed_connectable) {
          legacy_properties = LegacyAdvertisingProperties::ADV_DIRECT_IND_HIGH;
        } else {
          legacy_properties = LegacyAdvertisingProperties::ADV_DIRECT_IND_LOW;
        }
      }
      if (config.scannable && !config.connectable) {
        legacy_properties = LegacyAdvertisingProperties::ADV_SCAN_IND;
      }
      if (!config.scannable && !config.connectable) {
        legacy_properties = LegacyAdvertisingProperties::ADV_NONCONN_IND;
      }

      le_advertising_interface_->EnqueueCommand(
          LeSetExtendedAdvertisingLegacyParametersBuilder::Create(
              id, legacy_properties, config.interval_min, config.interval_max, config.channel_map,
              config.own_address_type, config.peer_address_type, config.peer_address, config.filter_policy,
              config.tx_power, config.sid, config.enable_scan_request_notifications),
          common::BindOnce(impl::check_status<LeSetExtendedAdvertisingParametersCompleteView>), module_handler_);
    } else {
      uint8_t legacy_properties = (config.connectable ? 0x1 : 0x00) | (config.scannable ? 0x2 : 0x00) |
                                  (config.directed ? 0x4 : 0x00) | (config.high_duty_directed_connectable ? 0x8 : 0x00);
      uint8_t extended_properties = (config.anonymous ? 0x20 : 0x00) | (config.include_tx_power ? 0x40 : 0x00);

      le_advertising_interface_->EnqueueCommand(
          hci::LeSetExtendedAdvertisingParametersBuilder::Create(
              id, legacy_properties, extended_properties, config.interval_min, config.interval_max, config.channel_map,
              config.own_address_type, config.peer_address_type, config.peer_address, config.filter_policy,
              config.tx_power, (config.use_le_coded_phy ? PrimaryPhyType::LE_CODED : PrimaryPhyType::LE_1M),
              config.secondary_max_skip, config.secondary_advertising_phy, config.sid,
              config.enable_scan_request_notifications),
          common::BindOnce(impl::check_status<LeSetExtendedAdvertisingParametersCompleteView>), module_handler_);
    }

    le_advertising_interface_->EnqueueCommand(
        hci::LeSetExtendedAdvertisingRandomAddressBuilder::Create(id, config.random_address),
        common::BindOnce(impl::check_status<LeSetExtendedAdvertisingRandomAddressCompleteView>), module_handler_);
    if (!config.scan_response.empty()) {
      le_advertising_interface_->EnqueueCommand(
          hci::LeSetExtendedAdvertisingScanResponseBuilder::Create(id, config.operation, config.fragment_preference,
                                                                   config.scan_response),
          common::BindOnce(impl::check_status<LeSetExtendedAdvertisingScanResponseCompleteView>), module_handler_);
    }
    le_advertising_interface_->EnqueueCommand(
        hci::LeSetExtendedAdvertisingDataBuilder::Create(id, config.operation, config.fragment_preference,
                                                         config.advertisement),
        common::BindOnce(impl::check_status<LeSetExtendedAdvertisingDataCompleteView>), module_handler_);

    EnabledSet curr_set;
    curr_set.advertising_handle_ = id;
    curr_set.duration_ = 0;                         // TODO: 0 means until the host disables it
    curr_set.max_extended_advertising_events_ = 0;  // TODO: 0 is no maximum
    std::vector<EnabledSet> enabled_sets = {curr_set};

    enabled_sets_[id] = curr_set;
    le_advertising_interface_->EnqueueCommand(
        hci::LeSetExtendedAdvertisingEnableBuilder::Create(Enable::ENABLED, enabled_sets),
        common::BindOnce(impl::check_status<LeSetExtendedAdvertisingEnableCompleteView>), module_handler_);

    advertising_sets_[id].scan_callback = scan_callback;
    advertising_sets_[id].set_terminated_callback = set_terminated_callback;
    advertising_sets_[id].handler = handler;
  }

  void stop_advertising(AdvertiserId advertising_set) {
    if (advertising_sets_.find(advertising_set) == advertising_sets_.end()) {
      LOG_INFO("Unknown advertising set %u", advertising_set);
      return;
    }
    switch (advertising_api_type_) {
      case (AdvertisingApiType::LE_4_0):
        le_advertising_interface_->EnqueueCommand(
            hci::LeSetAdvertisingEnableBuilder::Create(Enable::DISABLED),
            common::BindOnce(impl::check_status<LeSetAdvertisingEnableCompleteView>), module_handler_);
        break;
      case (AdvertisingApiType::ANDROID_HCI):
        le_advertising_interface_->EnqueueCommand(
            hci::LeMultiAdvtSetEnableBuilder::Create(Enable::DISABLED, advertising_set),
            common::BindOnce(impl::check_status<LeMultiAdvtCompleteView>), module_handler_);
        break;
      case (AdvertisingApiType::LE_5_0): {
        EnabledSet curr_set;
        curr_set.advertising_handle_ = advertising_set;
        std::vector<EnabledSet> enabled_vector{curr_set};
        le_advertising_interface_->EnqueueCommand(
            hci::LeSetExtendedAdvertisingEnableBuilder::Create(Enable::DISABLED, enabled_vector),
            common::BindOnce(impl::check_status<LeSetExtendedAdvertisingEnableCompleteView>), module_handler_);
      } break;
    }

    std::unique_lock lock(id_mutex_);
    enabled_sets_[advertising_set].advertising_handle_ = -1;
    advertising_sets_.erase(advertising_set);
  }

  common::Callback<void(Address, AddressType)> scan_callback_;
  common::Callback<void(ErrorCode, uint8_t, uint8_t)> set_terminated_callback_;
  os::Handler* registered_handler_{nullptr};
  Module* module_;
  os::Handler* module_handler_;
  hci::HciLayer* hci_layer_;
  hci::Controller* controller_;
  hci::LeAdvertisingInterface* le_advertising_interface_;
  std::map<AdvertiserId, Advertiser> advertising_sets_;

  std::mutex id_mutex_;
  size_t num_instances_;
  std::vector<hci::EnabledSet> enabled_sets_;

  AdvertisingApiType advertising_api_type_{0};

  template <class View>
  static void check_status(CommandCompleteView view) {
    ASSERT(view.IsValid());
    auto status_view = View::Create(view);
    ASSERT(status_view.IsValid());
    if (status_view.GetStatus() != ErrorCode::SUCCESS) {
      LOG_INFO("SetEnable returned status %s", ErrorCodeText(status_view.GetStatus()).c_str());
    }
  }
};

LeAdvertisingManager::LeAdvertisingManager() {
  pimpl_ = std::make_unique<impl>(this);
}

void LeAdvertisingManager::ListDependencies(ModuleList* list) {
  list->add<hci::HciLayer>();
  list->add<hci::Controller>();
}

void LeAdvertisingManager::Start() {
  pimpl_->start(GetHandler(), GetDependency<hci::HciLayer>(), GetDependency<hci::Controller>());
}

void LeAdvertisingManager::Stop() {
  pimpl_.reset();
}

std::string LeAdvertisingManager::ToString() const {
  return "Le Advertising Manager";
}

size_t LeAdvertisingManager::GetNumberOfAdvertisingInstances() const {
  return pimpl_->GetNumberOfAdvertisingInstances();
}

AdvertiserId LeAdvertisingManager::CreateAdvertiser(
    const AdvertisingConfig& config, const common::Callback<void(Address, AddressType)>& scan_callback,
    const common::Callback<void(ErrorCode, uint8_t, uint8_t)>& set_terminated_callback, os::Handler* handler) {
  if (config.peer_address == Address::kEmpty) {
    if (config.address_type == hci::AddressType::PUBLIC_IDENTITY_ADDRESS ||
        config.address_type == hci::AddressType::RANDOM_IDENTITY_ADDRESS) {
      LOG_WARN("Peer address can not be empty");
      return kInvalidId;
    }
    if (config.event_type == hci::AdvertisingEventType::ADV_DIRECT_IND ||
        config.event_type == hci::AdvertisingEventType::ADV_DIRECT_IND_LOW) {
      LOG_WARN("Peer address can not be empty for directed advertising");
      return kInvalidId;
    }
  }
  AdvertiserId id = pimpl_->allocate_advertiser();
  if (id == kInvalidId) {
    return id;
  }
  GetHandler()->Post(common::BindOnce(&impl::create_advertiser, common::Unretained(pimpl_.get()), id, config,
                                      scan_callback, set_terminated_callback, handler));
  return id;
}

AdvertiserId LeAdvertisingManager::ExtendedCreateAdvertiser(
    const ExtendedAdvertisingConfig& config, const common::Callback<void(Address, AddressType)>& scan_callback,
    const common::Callback<void(ErrorCode, uint8_t, uint8_t)>& set_terminated_callback, os::Handler* handler) {
  if (config.directed) {
    if (config.peer_address == Address::kEmpty) {
      LOG_INFO("Peer address can not be empty for directed advertising");
      return kInvalidId;
    }
  }
  if (config.channel_map == 0) {
    LOG_INFO("At least one channel must be set in the map");
    return kInvalidId;
  }
  if (!config.legacy_pdus) {
    if (config.connectable && config.scannable) {
      LOG_INFO("Extended advertising PDUs can not be connectable and scannable");
      return kInvalidId;
    }
    if (config.high_duty_directed_connectable) {
      LOG_INFO("Extended advertising PDUs can not be high duty cycle");
      return kInvalidId;
    }
  }
  if (config.interval_min > config.interval_max) {
    LOG_INFO("Advertising interval: min (%hu) > max (%hu)", config.interval_min, config.interval_max);
    return kInvalidId;
  }
  AdvertiserId id = pimpl_->allocate_advertiser();
  if (id == kInvalidId) {
    return id;
  }
  GetHandler()->Post(common::BindOnce(&impl::create_extended_advertiser, common::Unretained(pimpl_.get()), id, config,
                                      scan_callback, set_terminated_callback, handler));
  return id;
}

void LeAdvertisingManager::RemoveAdvertiser(AdvertiserId id) {
  GetHandler()->Post(common::BindOnce(&impl::remove_advertiser, common::Unretained(pimpl_.get()), id));
}

}  // namespace hci
}  // namespace bluetooth
