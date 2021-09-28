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

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/bind.h"
#include "hci/address.h"
#include "hci/hci_packets.h"
#include "l2cap/classic/dynamic_channel_manager.h"
#include "l2cap/classic/l2cap_classic_module.h"
#include "l2cap/psm.h"
#include "l2cap/security_policy.h"
#include "module.h"
#include "os/handler.h"
#include "os/log.h"
#include "packet/packet_view.h"
#include "packet/raw_builder.h"
#include "shim/dumpsys.h"
#include "shim/l2cap.h"

namespace bluetooth {
namespace shim {

namespace {

constexpr char kModuleName[] = "shim::L2cap";

constexpr bool kConnectionFailed = false;
constexpr bool kConnectionOpened = true;
constexpr bool kRegistrationFailed = false;
constexpr bool kRegistrationSuccess = true;

using ConnectionInterfaceDescriptor = uint16_t;
constexpr ConnectionInterfaceDescriptor kInvalidConnectionInterfaceDescriptor = 0;
constexpr ConnectionInterfaceDescriptor kStartConnectionInterfaceDescriptor = 64;
constexpr ConnectionInterfaceDescriptor kMaxConnections = UINT16_MAX - kStartConnectionInterfaceDescriptor - 1;

using PendingConnectionId = int;

using ConnectionClosed = std::function<void(ConnectionInterfaceDescriptor)>;
using PendingConnectionOpen = std::function<void(std::unique_ptr<l2cap::classic::DynamicChannel>)>;
using PendingConnectionFail = std::function<void(l2cap::classic::DynamicChannelManager::ConnectionResult)>;
using RegisterServiceComplete = std::function<void(l2cap::Psm, bool is_registered)>;
using UnregisterServiceDone = std::function<void()>;
using ServiceConnectionOpen =
    std::function<void(ConnectionCompleteCallback, std::unique_ptr<l2cap::classic::DynamicChannel>)>;

std::unique_ptr<packet::RawBuilder> MakeUniquePacket(const uint8_t* data, size_t len) {
  packet::RawBuilder builder;
  std::vector<uint8_t> bytes(data, data + len);
  auto payload = std::make_unique<packet::RawBuilder>();
  payload->AddOctets(bytes);
  return payload;
}

}  // namespace

class ConnectionInterface {
 public:
  ConnectionInterface(ConnectionInterfaceDescriptor cid, std::unique_ptr<l2cap::classic::DynamicChannel> channel,
                      os::Handler* handler, ConnectionClosed on_closed)
      : cid_(cid), channel_(std::move(channel)), handler_(handler), on_data_ready_callback_(nullptr),
        on_connection_closed_callback_(nullptr), address_(channel_->GetDevice()), on_closed_(on_closed) {
    channel_->RegisterOnCloseCallback(
        handler_, common::BindOnce(&ConnectionInterface::OnConnectionClosed, common::Unretained(this)));
    channel_->GetQueueUpEnd()->RegisterDequeue(
        handler_, common::Bind(&ConnectionInterface::OnReadReady, common::Unretained(this)));
    dequeue_registered_ = true;
  }

  ~ConnectionInterface() {
    ASSERT(!dequeue_registered_);
  }

  void OnReadReady() {
    std::unique_ptr<packet::PacketView<packet::kLittleEndian>> packet = channel_->GetQueueUpEnd()->TryDequeue();
    if (packet == nullptr) {
      LOG_WARN("Got read ready from gd l2cap but no packet is ready");
      return;
    }
    std::vector<const uint8_t> data(packet->begin(), packet->end());
    ASSERT(on_data_ready_callback_ != nullptr);
    on_data_ready_callback_(cid_, data);
  }

  void SetReadDataReadyCallback(ReadDataReadyCallback on_data_ready) {
    ASSERT(on_data_ready != nullptr);
    ASSERT(on_data_ready_callback_ == nullptr);
    on_data_ready_callback_ = on_data_ready;
  }

  std::unique_ptr<packet::BasePacketBuilder> WriteReady() {
    auto data = std::move(write_queue_.front());
    write_queue_.pop();
    if (write_queue_.empty()) {
      channel_->GetQueueUpEnd()->UnregisterEnqueue();
      enqueue_registered_ = false;
    }
    return data;
  }

  void Write(std::unique_ptr<packet::RawBuilder> packet) {
    LOG_DEBUG("Writing packet cid:%hd size:%zd", cid_, packet->size());
    write_queue_.push(std::move(packet));
    if (!enqueue_registered_) {
      enqueue_registered_ = true;
      channel_->GetQueueUpEnd()->RegisterEnqueue(
          handler_, common::Bind(&ConnectionInterface::WriteReady, common::Unretained(this)));
    }
  }

  void Close() {
    if (dequeue_registered_) {
      channel_->GetQueueUpEnd()->UnregisterDequeue();
      dequeue_registered_ = false;
    }
    ASSERT(write_queue_.empty());
    channel_->Close();
  }

  void OnConnectionClosed(hci::ErrorCode error_code) {
    LOG_DEBUG("Channel interface closed reason:%s cid:%hd device:%s", hci::ErrorCodeText(error_code).c_str(), cid_,
              address_.ToString().c_str());
    if (dequeue_registered_) {
      channel_->GetQueueUpEnd()->UnregisterDequeue();
      dequeue_registered_ = false;
    }
    ASSERT(on_connection_closed_callback_ != nullptr);
    on_connection_closed_callback_(cid_, static_cast<int>(error_code));
    on_closed_(cid_);
  }

  void SetConnectionClosedCallback(::bluetooth::shim::ConnectionClosedCallback on_connection_closed) {
    ASSERT(on_connection_closed != nullptr);
    ASSERT(on_connection_closed_callback_ == nullptr);
    on_connection_closed_callback_ = std::move(on_connection_closed);
  }

  hci::Address GetRemoteAddress() const {
    return address_;
  }

 private:
  const ConnectionInterfaceDescriptor cid_;
  const std::unique_ptr<l2cap::classic::DynamicChannel> channel_;
  os::Handler* handler_;

  ReadDataReadyCallback on_data_ready_callback_;
  ConnectionClosedCallback on_connection_closed_callback_;

  const hci::Address address_;

  ConnectionClosed on_closed_{};

  std::queue<std::unique_ptr<packet::PacketBuilder<hci::kLittleEndian>>> write_queue_;

  bool enqueue_registered_{false};
  bool dequeue_registered_{false};

  DISALLOW_COPY_AND_ASSIGN(ConnectionInterface);
};

class ConnectionInterfaceManager {
 public:
  void AddConnection(ConnectionInterfaceDescriptor cid, std::unique_ptr<l2cap::classic::DynamicChannel> channel);
  void RemoveConnection(ConnectionInterfaceDescriptor cid);

  void SetReadDataReadyCallback(ConnectionInterfaceDescriptor cid, ReadDataReadyCallback on_data_ready);
  void SetConnectionClosedCallback(ConnectionInterfaceDescriptor cid, ConnectionClosedCallback on_closed);

  bool Write(ConnectionInterfaceDescriptor cid, std::unique_ptr<packet::RawBuilder> packet);

  size_t NumberOfActiveConnections() const {
    return cid_to_interface_map_.size();
  }

  void ConnectionOpened(ConnectionCompleteCallback on_complete, l2cap::Psm psm, ConnectionInterfaceDescriptor cid) {
    hci::Address address = cid_to_interface_map_[cid]->GetRemoteAddress();
    LOG_DEBUG("Connection opened address:%s psm:%hd cid:%hd", address.ToString().c_str(), psm, cid);
    on_complete(address.ToString(), static_cast<uint16_t>(psm), static_cast<uint16_t>(cid), kConnectionOpened);
  }

  void ConnectionFailed(ConnectionCompleteCallback on_complete, hci::Address address, l2cap::Psm psm,
                        ConnectionInterfaceDescriptor cid) {
    LOG_DEBUG("Connection failed address:%s psm:%hd", address.ToString().c_str(), psm);
    on_complete(address.ToString(), static_cast<uint16_t>(psm), static_cast<uint16_t>(cid), kConnectionFailed);
  }

  ConnectionInterfaceManager(os::Handler* handler);

  ConnectionInterfaceDescriptor AllocateConnectionInterfaceDescriptor();
  void FreeConnectionInterfaceDescriptor(ConnectionInterfaceDescriptor cid);

 private:
  os::Handler* handler_;
  ConnectionInterfaceDescriptor current_connection_interface_descriptor_;

  bool HasResources() const;
  bool ConnectionExists(ConnectionInterfaceDescriptor id) const;
  bool CidExists(ConnectionInterfaceDescriptor id) const;
  void ConnectionClosed(ConnectionInterfaceDescriptor cid, std::unique_ptr<ConnectionInterface> connection);

  std::unordered_map<ConnectionInterfaceDescriptor, std::unique_ptr<ConnectionInterface>> cid_to_interface_map_;
  std::set<ConnectionInterfaceDescriptor> active_cid_set_;

  ConnectionInterfaceManager() = delete;
};

ConnectionInterfaceManager::ConnectionInterfaceManager(os::Handler* handler)
    : handler_(handler), current_connection_interface_descriptor_(kStartConnectionInterfaceDescriptor) {}

bool ConnectionInterfaceManager::ConnectionExists(ConnectionInterfaceDescriptor cid) const {
  return cid_to_interface_map_.find(cid) != cid_to_interface_map_.end();
}

bool ConnectionInterfaceManager::CidExists(ConnectionInterfaceDescriptor cid) const {
  return active_cid_set_.find(cid) != active_cid_set_.end();
}

ConnectionInterfaceDescriptor ConnectionInterfaceManager::AllocateConnectionInterfaceDescriptor() {
  ASSERT(HasResources());
  while (CidExists(current_connection_interface_descriptor_)) {
    if (++current_connection_interface_descriptor_ == kInvalidConnectionInterfaceDescriptor) {
      current_connection_interface_descriptor_ = kStartConnectionInterfaceDescriptor;
    }
  }
  active_cid_set_.insert(current_connection_interface_descriptor_);
  return current_connection_interface_descriptor_++;
}

void ConnectionInterfaceManager::FreeConnectionInterfaceDescriptor(ConnectionInterfaceDescriptor cid) {
  ASSERT(CidExists(cid));
  active_cid_set_.erase(cid);
}

void ConnectionInterfaceManager::ConnectionClosed(ConnectionInterfaceDescriptor cid,
                                                  std::unique_ptr<ConnectionInterface> connection) {
  cid_to_interface_map_.erase(cid);
  FreeConnectionInterfaceDescriptor(cid);
}

void ConnectionInterfaceManager::AddConnection(ConnectionInterfaceDescriptor cid,
                                               std::unique_ptr<l2cap::classic::DynamicChannel> channel) {
  ASSERT(cid_to_interface_map_.count(cid) == 0);
  cid_to_interface_map_.emplace(
      cid, std::make_unique<ConnectionInterface>(
               cid, std::move(channel), handler_, [this](ConnectionInterfaceDescriptor cid) {
                 LOG_DEBUG("Deleting connection interface cid:%hd", cid);
                 auto connection = std::move(cid_to_interface_map_.at(cid));
                 handler_->Post(common::BindOnce(&ConnectionInterfaceManager::ConnectionClosed,
                                                 common::Unretained(this), cid, std::move(connection)));
               }));
}

void ConnectionInterfaceManager::RemoveConnection(ConnectionInterfaceDescriptor cid) {
  if (cid_to_interface_map_.count(cid) == 1) {
    cid_to_interface_map_.find(cid)->second->Close();
  } else {
    LOG_WARN("Closing a pending connection cid:%hd", cid);
  }
}

bool ConnectionInterfaceManager::HasResources() const {
  return cid_to_interface_map_.size() < kMaxConnections;
}

void ConnectionInterfaceManager::SetReadDataReadyCallback(ConnectionInterfaceDescriptor cid,
                                                          ReadDataReadyCallback on_data_ready) {
  ASSERT(ConnectionExists(cid));
  return cid_to_interface_map_[cid]->SetReadDataReadyCallback(on_data_ready);
}

void ConnectionInterfaceManager::SetConnectionClosedCallback(ConnectionInterfaceDescriptor cid,
                                                             ConnectionClosedCallback on_closed) {
  ASSERT(ConnectionExists(cid));
  return cid_to_interface_map_[cid]->SetConnectionClosedCallback(on_closed);
}

bool ConnectionInterfaceManager::Write(ConnectionInterfaceDescriptor cid, std::unique_ptr<packet::RawBuilder> packet) {
  if (!ConnectionExists(cid)) {
    return false;
  }
  cid_to_interface_map_[cid]->Write(std::move(packet));
  return true;
}

class PendingConnection {
 public:
  PendingConnection(ConnectionInterfaceDescriptor cid, l2cap::Psm psm, hci::Address address,
                    ConnectionCompleteCallback on_complete, PendingConnectionOpen pending_open,
                    PendingConnectionFail pending_fail)
      : cid_(cid), psm_(psm), address_(address), on_complete_(std::move(on_complete)), pending_open_(pending_open),
        pending_fail_(pending_fail) {}

  void OnConnectionOpen(std::unique_ptr<l2cap::classic::DynamicChannel> channel) {
    LOG_DEBUG("Local initiated connection is open to device:%s for psm:%hd", address_.ToString().c_str(), psm_);
    ASSERT_LOG(address_ == channel->GetDevice(), " Expected remote device does not match actual remote device");
    pending_open_(std::move(channel));
  }

  void OnConnectionFailure(l2cap::classic::DynamicChannelManager::ConnectionResult result) {
    LOG_DEBUG("Connection failed to device:%s for psm:%hd", address_.ToString().c_str(), psm_);
    switch (result.connection_result_code) {
      case l2cap::classic::DynamicChannelManager::ConnectionResultCode::SUCCESS:
        LOG_WARN("Connection failed result:success hci:%s", hci::ErrorCodeText(result.hci_error).c_str());
        break;
      case l2cap::classic::DynamicChannelManager::ConnectionResultCode::FAIL_NO_SERVICE_REGISTERED:
        LOG_DEBUG("Connection failed result:no service registered hci:%s",
                  hci::ErrorCodeText(result.hci_error).c_str());
        break;
      case l2cap::classic::DynamicChannelManager::ConnectionResultCode::FAIL_HCI_ERROR:
        LOG_DEBUG("Connection failed result:hci error hci:%s", hci::ErrorCodeText(result.hci_error).c_str());
        break;
      case l2cap::classic::DynamicChannelManager::ConnectionResultCode::FAIL_L2CAP_ERROR:
        LOG_DEBUG("Connection failed result:l2cap error hci:%s l2cap:%s", hci::ErrorCodeText(result.hci_error).c_str(),
                  l2cap::ConnectionResponseResultText(result.l2cap_connection_response_result).c_str());
        break;
    }
    pending_fail_(result);
  }

  std::string ToString() const {
    return address_.ToString() + "." + std::to_string(psm_);
  }

  const ConnectionInterfaceDescriptor cid_;
  const l2cap::Psm psm_;
  const hci::Address address_;
  const ConnectionCompleteCallback on_complete_;

 private:
  const PendingConnectionOpen pending_open_;
  const PendingConnectionFail pending_fail_;

  DISALLOW_COPY_AND_ASSIGN(PendingConnection);
};

class ServiceInterface {
 public:
  ServiceInterface(l2cap::Psm psm, l2cap::SecurityPolicy security_policy, ConnectionCompleteCallback on_complete,
                   RegisterServiceComplete register_complete, ServiceConnectionOpen connection_open,
                   RegisterServicePromise register_promise)
      : psm_(psm), security_policy_(security_policy), on_complete_(on_complete),
        register_complete_(std::move(register_complete)), connection_open_(std::move(connection_open)),
        register_promise_(std::move(register_promise)) {}

  void NotifyRegistered(l2cap::Psm psm) {
    register_promise_.set_value(psm);
  }

  void NotifyUnregistered() {
    unregister_promise_.set_value();
  }

  void UnregisterService(os::Handler* handler, UnregisterServicePromise unregister_promise,
                         UnregisterServiceDone unregister_done) {
    unregister_promise_ = std::move(unregister_promise);
    unregister_done_ = std::move(unregister_done);

    service_->Unregister(common::BindOnce(&ServiceInterface::OnUnregistrationComplete, common::Unretained(this)),
                         handler);
  }

  l2cap::SecurityPolicy GetSecurityPolicy() const {
    return security_policy_;
  }

  void OnRegistrationComplete(l2cap::classic::DynamicChannelManager::RegistrationResult result,
                              std::unique_ptr<l2cap::classic::DynamicChannelService> service) {
    ASSERT(service_ == nullptr);
    ASSERT(service->GetPsm() == psm_);
    service_ = std::move(service);

    switch (result) {
      case l2cap::classic::DynamicChannelManager::RegistrationResult::SUCCESS:
        LOG_DEBUG("Service is registered for psm:%hd", psm_);
        register_complete_(psm_, kRegistrationSuccess);
        break;
      case l2cap::classic::DynamicChannelManager::RegistrationResult::FAIL_DUPLICATE_SERVICE:
        LOG_WARN("Failed to register duplicate service has psm:%hd", psm_);
        register_complete_(l2cap::kDefaultPsm, kRegistrationFailed);
        break;
      case l2cap::classic::DynamicChannelManager::RegistrationResult::FAIL_INVALID_SERVICE:
        LOG_WARN("Failed to register invalid service psm:%hd", psm_);
        register_complete_(l2cap::kDefaultPsm, kRegistrationFailed);
        break;
    }
  }

  void OnUnregistrationComplete() {
    LOG_DEBUG("Unregistered psm:%hd", psm_);
    unregister_done_();
  }

  void OnConnectionOpen(std::unique_ptr<l2cap::classic::DynamicChannel> channel) {
    LOG_DEBUG("Remote initiated connection is open from device:%s for psm:%hd", channel->GetDevice().ToString().c_str(),
              psm_);
    connection_open_(on_complete_, std::move(channel));
  }

 private:
  const l2cap::Psm psm_;
  const l2cap::SecurityPolicy security_policy_;
  const ConnectionCompleteCallback on_complete_;
  const RegisterServiceComplete register_complete_;
  const ServiceConnectionOpen connection_open_;
  RegisterServicePromise register_promise_;
  UnregisterServicePromise unregister_promise_;
  UnregisterServiceDone unregister_done_;

  std::unique_ptr<l2cap::classic::DynamicChannelService> service_;

  DISALLOW_COPY_AND_ASSIGN(ServiceInterface);
};

struct L2cap::impl {
  void RegisterService(l2cap::Psm psm, l2cap::classic::DynamicChannelConfigurationOption option,
                       ConnectionCompleteCallback on_complete, RegisterServicePromise register_promise);
  void UnregisterService(l2cap::Psm psm, UnregisterServicePromise unregister_promise);

  void CreateConnection(l2cap::Psm psm, hci::Address address, ConnectionCompleteCallback on_complete,
                        CreateConnectionPromise create_promise);
  void CloseConnection(ConnectionInterfaceDescriptor cid);

  void SetReadDataReadyCallback(ConnectionInterfaceDescriptor cid, ReadDataReadyCallback on_data_ready);
  void SetConnectionClosedCallback(ConnectionInterfaceDescriptor cid, ConnectionClosedCallback on_closed);

  void Write(ConnectionInterfaceDescriptor cid, std::unique_ptr<packet::RawBuilder> packet);

  void SendLoopbackResponse(std::function<void()> function);

  void Dump(int fd);

  impl(L2cap& module, l2cap::classic::L2capClassicModule* l2cap_module);

 private:
  L2cap& module_;
  l2cap::classic::L2capClassicModule* l2cap_module_;
  os::Handler* handler_;
  ConnectionInterfaceManager connection_interface_manager_;

  std::unique_ptr<l2cap::classic::DynamicChannelManager> dynamic_channel_manager_;

  std::unordered_map<l2cap::Psm, std::unique_ptr<ServiceInterface>> psm_to_service_interface_map_;

  PendingConnectionId pending_connection_id_{0};
  std::unordered_map<PendingConnectionId, std::unique_ptr<PendingConnection>> pending_connection_map_;

  void PendingConnectionOpen(PendingConnectionId id, std::unique_ptr<PendingConnection> connection,
                             std::unique_ptr<l2cap::classic::DynamicChannel> channel);
  void PendingConnectionFail(PendingConnectionId id, std::unique_ptr<PendingConnection> connection,
                             l2cap::classic::DynamicChannelManager::ConnectionResult result);
  void ServiceUnregistered(l2cap::Psm psm, std::unique_ptr<ServiceInterface> service);
  const l2cap::SecurityPolicy GetSecurityPolicy(l2cap::Psm psm) const;
};

const ModuleFactory L2cap::Factory = ModuleFactory([]() { return new L2cap(); });

L2cap::impl::impl(L2cap& module, l2cap::classic::L2capClassicModule* l2cap_module)
    : module_(module), l2cap_module_(l2cap_module), handler_(module_.GetHandler()),
      connection_interface_manager_(handler_) {
  dynamic_channel_manager_ = l2cap_module_->GetDynamicChannelManager();
}

void L2cap::impl::Dump(int fd) {
  if (psm_to_service_interface_map_.empty()) {
    dprintf(fd, "%s no psms registered\n", kModuleName);
  } else {
    for (auto& service : psm_to_service_interface_map_) {
      dprintf(fd, "%s psm registered:%hd\n", kModuleName, service.first);
    }
  }

  if (pending_connection_map_.empty()) {
    dprintf(fd, "%s no pending classic connections\n", kModuleName);
  } else {
    for (auto& pending : pending_connection_map_) {
      if (pending.second != nullptr) {
        dprintf(fd, "%s pending connection:%s\n", kModuleName, pending.second->ToString().c_str());
      } else {
        dprintf(fd, "%s old pending connection:%d\n", kModuleName, pending.first);
      }
    }
  }
}

void L2cap::impl::ServiceUnregistered(l2cap::Psm psm, std::unique_ptr<ServiceInterface> service) {
  LOG_INFO("Unregistered service psm:%hd", psm);
  psm_to_service_interface_map_.erase(psm);
  service->NotifyUnregistered();
}

const l2cap::SecurityPolicy L2cap::impl::GetSecurityPolicy(l2cap::Psm psm) const {
  l2cap::SecurityPolicy security_policy;
  if (psm == 1) {
    security_policy.security_level_ = l2cap::SecurityPolicy::Level::LEVEL_0;
  } else {
    security_policy.security_level_ = l2cap::SecurityPolicy::Level::LEVEL_3;
  }
  return security_policy;
}

void L2cap::impl::RegisterService(l2cap::Psm psm, l2cap::classic::DynamicChannelConfigurationOption option,
                                  ConnectionCompleteCallback on_complete, RegisterServicePromise register_promise) {
  ASSERT(psm_to_service_interface_map_.find(psm) == psm_to_service_interface_map_.end());

  const l2cap::SecurityPolicy security_policy = GetSecurityPolicy(psm);

  psm_to_service_interface_map_.emplace(
      psm,
      std::make_unique<ServiceInterface>(
          psm, security_policy, on_complete,
          [this, psm](l2cap::Psm actual_psm, bool is_registered) {
            psm_to_service_interface_map_.at(psm)->NotifyRegistered(actual_psm);
            if (!is_registered) {
              auto service = std::move(psm_to_service_interface_map_.at(psm));
            }
          },
          [this, psm](ConnectionCompleteCallback on_complete, std::unique_ptr<l2cap::classic::DynamicChannel> channel) {
            ConnectionInterfaceDescriptor cid = connection_interface_manager_.AllocateConnectionInterfaceDescriptor();
            connection_interface_manager_.AddConnection(cid, std::move(channel));
            connection_interface_manager_.ConnectionOpened(on_complete, psm, cid);
            LOG_DEBUG("connection open");
          },
          std::move(register_promise)));

  bool rc = dynamic_channel_manager_->RegisterService(
      psm, option, security_policy,
      common::BindOnce(&ServiceInterface::OnRegistrationComplete,
                       common::Unretained(psm_to_service_interface_map_.at(psm).get())),
      common::Bind(&ServiceInterface::OnConnectionOpen,
                   common::Unretained(psm_to_service_interface_map_.at(psm).get())),
      handler_);
  ASSERT_LOG(rc == true, "Failed to register classic service");
}

void L2cap::impl::UnregisterService(l2cap::Psm psm, UnregisterServicePromise unregister_promise) {
  ASSERT(psm_to_service_interface_map_.find(psm) != psm_to_service_interface_map_.end());
  psm_to_service_interface_map_[psm]->UnregisterService(handler_, std::move(unregister_promise), [this, psm]() {
    auto service = std::move(psm_to_service_interface_map_.at(psm));
    handler_->Post(
        common::BindOnce(&L2cap::impl::ServiceUnregistered, common::Unretained(this), psm, std::move(service)));
  });
}

void L2cap::impl::PendingConnectionOpen(PendingConnectionId id, std::unique_ptr<PendingConnection> connection,
                                        std::unique_ptr<l2cap::classic::DynamicChannel> channel) {
  connection_interface_manager_.AddConnection(connection->cid_, std::move(channel));
  connection_interface_manager_.ConnectionOpened(std::move(connection->on_complete_), connection->psm_,
                                                 connection->cid_);
  pending_connection_map_.erase(id);
}

void L2cap::impl::PendingConnectionFail(PendingConnectionId id, std::unique_ptr<PendingConnection> connection,
                                        l2cap::classic::DynamicChannelManager::ConnectionResult result) {
  connection_interface_manager_.ConnectionFailed(std::move(connection->on_complete_), connection->address_,
                                                 connection->psm_, connection->cid_);
  connection_interface_manager_.FreeConnectionInterfaceDescriptor(connection->cid_);
  pending_connection_map_.erase(id);
}

void L2cap::impl::CreateConnection(l2cap::Psm psm, hci::Address address, ConnectionCompleteCallback on_complete,
                                   CreateConnectionPromise create_promise) {
  ConnectionInterfaceDescriptor cid = connection_interface_manager_.AllocateConnectionInterfaceDescriptor();
  create_promise.set_value(cid);

  if (cid == kInvalidConnectionInterfaceDescriptor) {
    LOG_WARN("No resources to create a connection");
    return;
  }

  PendingConnectionId id = ++pending_connection_id_;
  pending_connection_map_.emplace(
      id, std::make_unique<PendingConnection>(
              cid, psm, address, on_complete,
              [this, id](std::unique_ptr<l2cap::classic::DynamicChannel> channel) {
                auto connection = std::move(pending_connection_map_.at(id));
                handler_->Post(common::BindOnce(&L2cap::impl::PendingConnectionOpen, common::Unretained(this), id,
                                                std::move(connection), std::move(channel)));
              },
              [this, id](l2cap::classic::DynamicChannelManager::ConnectionResult result) {
                auto connection = std::move(pending_connection_map_.at(id));
                handler_->Post(common::BindOnce(&L2cap::impl::PendingConnectionFail, common::Unretained(this), id,
                                                std::move(connection), result));
              }));

  bool rc = dynamic_channel_manager_->ConnectChannel(
      address, l2cap::classic::DynamicChannelConfigurationOption(), psm,
      common::Bind(&PendingConnection::OnConnectionOpen, common::Unretained(pending_connection_map_.at(id).get())),
      common::BindOnce(&PendingConnection::OnConnectionFailure,
                       common::Unretained(pending_connection_map_.at(id).get())),
      handler_);
  ASSERT_LOG(rc == true, "Failed to create classic connection");
}

void L2cap::impl::CloseConnection(ConnectionInterfaceDescriptor cid) {
  connection_interface_manager_.RemoveConnection(cid);
}

void L2cap::impl::SetReadDataReadyCallback(ConnectionInterfaceDescriptor cid, ReadDataReadyCallback on_data_ready) {
  connection_interface_manager_.SetReadDataReadyCallback(cid, on_data_ready);
}

void L2cap::impl::SetConnectionClosedCallback(ConnectionInterfaceDescriptor cid, ConnectionClosedCallback on_closed) {
  connection_interface_manager_.SetConnectionClosedCallback(cid, std::move(on_closed));
}

void L2cap::impl::Write(ConnectionInterfaceDescriptor cid, std::unique_ptr<packet::RawBuilder> packet) {
  connection_interface_manager_.Write(cid, std::move(packet));
}

void L2cap::impl::SendLoopbackResponse(std::function<void()> function) {
  function();
}

void L2cap::RegisterService(uint16_t raw_psm, bool use_ertm, uint16_t mtu, ConnectionCompleteCallback on_complete,
                            RegisterServicePromise register_promise) {
  l2cap::Psm psm{raw_psm};
  l2cap::classic::DynamicChannelConfigurationOption option;
  if (use_ertm) {
    option.channel_mode =
        l2cap::classic::DynamicChannelConfigurationOption::RetransmissionAndFlowControlMode::ENHANCED_RETRANSMISSION;
  }
  option.incoming_mtu = mtu;
  GetHandler()->Post(common::BindOnce(&L2cap::impl::RegisterService, common::Unretained(pimpl_.get()), psm, option,
                                      on_complete, std::move(register_promise)));
}

void L2cap::UnregisterService(uint16_t raw_psm, UnregisterServicePromise unregister_promise) {
  l2cap::Psm psm{raw_psm};
  GetHandler()->Post(common::BindOnce(&L2cap::impl::UnregisterService, common::Unretained(pimpl_.get()), psm,
                                      std::move(unregister_promise)));
}

void L2cap::CreateConnection(uint16_t raw_psm, const std::string address_string, ConnectionCompleteCallback on_complete,
                             CreateConnectionPromise create_promise) {
  l2cap::Psm psm{raw_psm};
  hci::Address address;
  hci::Address::FromString(address_string, address);

  GetHandler()->Post(common::BindOnce(&L2cap::impl::CreateConnection, common::Unretained(pimpl_.get()), psm, address,
                                      on_complete, std::move(create_promise)));
}

void L2cap::CloseConnection(uint16_t raw_cid) {
  ConnectionInterfaceDescriptor cid(raw_cid);
  GetHandler()->Post(common::Bind(&L2cap::impl::CloseConnection, common::Unretained(pimpl_.get()), cid));
}

void L2cap::SetReadDataReadyCallback(uint16_t raw_cid, ReadDataReadyCallback on_data_ready) {
  ConnectionInterfaceDescriptor cid(raw_cid);
  GetHandler()->Post(
      common::Bind(&L2cap::impl::SetReadDataReadyCallback, common::Unretained(pimpl_.get()), cid, on_data_ready));
}

void L2cap::SetConnectionClosedCallback(uint16_t raw_cid, ConnectionClosedCallback on_closed) {
  ConnectionInterfaceDescriptor cid(raw_cid);
  GetHandler()->Post(common::Bind(&L2cap::impl::SetConnectionClosedCallback, common::Unretained(pimpl_.get()), cid,
                                  std::move(on_closed)));
}

void L2cap::Write(uint16_t raw_cid, const uint8_t* data, size_t len) {
  ConnectionInterfaceDescriptor cid(raw_cid);
  auto packet = MakeUniquePacket(data, len);
  GetHandler()->Post(common::BindOnce(&L2cap::impl::Write, common::Unretained(pimpl_.get()), cid, std::move(packet)));
}

void L2cap::SendLoopbackResponse(std::function<void()> function) {
  GetHandler()->Post(common::BindOnce(&L2cap::impl::SendLoopbackResponse, common::Unretained(pimpl_.get()), function));
}

/**
 * Module methods
 */
void L2cap::ListDependencies(ModuleList* list) {
  list->add<shim::Dumpsys>();
  list->add<l2cap::classic::L2capClassicModule>();
}

void L2cap::Start() {
  pimpl_ = std::make_unique<impl>(*this, GetDependency<l2cap::classic::L2capClassicModule>());
  GetDependency<shim::Dumpsys>()->RegisterDumpsysFunction(static_cast<void*>(this),
                                                          [this](int fd) { pimpl_->Dump(fd); });
}

void L2cap::Stop() {
  GetDependency<shim::Dumpsys>()->UnregisterDumpsysFunction(static_cast<void*>(this));
  pimpl_.reset();
}

std::string L2cap::ToString() const {
  return kModuleName;
}

}  // namespace shim
}  // namespace bluetooth
