/*
 * Copyright 2020 The Android Open Source Project
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

#include "shim/l2cap.h"

#include <gtest/gtest.h>
#include <future>
#include <memory>

#include "common/bind.h"
#include "hci/address.h"
#include "hci/address_with_type.h"
#include "l2cap/classic/dynamic_channel_configuration_option.h"
#include "l2cap/classic/dynamic_channel_manager.h"
#include "l2cap/classic/internal/dynamic_channel_service_manager_impl_mock.h"
#include "l2cap/classic/l2cap_classic_module.h"
#include "l2cap/internal/ilink.h"
#include "l2cap/psm.h"
#include "l2cap/security_policy.h"
#include "os/handler.h"

namespace bluetooth {
namespace shim {
namespace {

constexpr uint16_t kPsm = 123;
constexpr uint16_t kPsm2 = kPsm + 2;
constexpr uint16_t kCid = 456;
constexpr uint16_t kCid2 = kCid + 1;
constexpr char device_address[] = "11:22:33:44:55:66";
constexpr char device_address2[] = "aa:bb:cc:dd:ee:ff";
constexpr bool kNoUseErtm = false;
constexpr uint16_t kMtu = 1000;

class TestDynamicChannelService : public l2cap::classic::DynamicChannelService {
 public:
  TestDynamicChannelService(l2cap::Psm psm, l2cap::classic::internal::DynamicChannelServiceManagerImpl* manager,
                            os::Handler* handler)
      : DynamicChannelService(psm, manager, handler) {}
};

class TestLink : public l2cap::internal::ILink {
 public:
  hci::AddressWithType GetDevice() {
    return device_with_type_;
  }
  hci::AddressWithType device_with_type_;

  void SendLeCredit(l2cap::Cid local_cid, uint16_t credit) {}

  void SendDisconnectionRequest(l2cap::Cid cid, l2cap::Cid remote_cid) {
    connection_closed_promise_.set_value();
  }
  std::promise<void> connection_closed_promise_;
};

class TestDynamicChannelManagerImpl {
 public:
  bool ConnectChannel(hci::Address device, l2cap::classic::DynamicChannelConfigurationOption configuration_option,
                      l2cap::Psm psm, l2cap::classic::DynamicChannelManager::OnConnectionOpenCallback on_open_callback,
                      l2cap::classic::DynamicChannelManager::OnConnectionFailureCallback on_fail_callback,
                      os::Handler* handler) {
    connections_++;
    on_open_callback_ = std::move(on_open_callback);
    on_fail_callback_ = std::move(on_fail_callback);

    connected_promise_.set_value();
    return true;
  }
  int connections_{0};

  bool RegisterService(l2cap::Psm psm, l2cap::classic::DynamicChannelConfigurationOption configuration_option,
                       const l2cap::SecurityPolicy& security_policy,
                       l2cap::classic::DynamicChannelManager::OnRegistrationCompleteCallback on_registration_complete,
                       l2cap::classic::DynamicChannelManager::OnConnectionOpenCallback on_open_callback,
                       os::Handler* handler) {
    services_++;
    on_registration_complete_ = std::move(on_registration_complete);
    on_open_callback_ = std::move(on_open_callback);

    register_promise_.set_value();
    return true;
  }
  int services_{0};

  void SetConnectionFuture() {
    connected_promise_ = std::promise<void>();
  }

  void WaitConnectionFuture() {
    connected_future_ = connected_promise_.get_future();
    connected_future_.wait();
  }

  void SetRegistrationFuture() {
    register_promise_ = std::promise<void>();
  }

  void WaitRegistrationFuture() {
    register_future_ = register_promise_.get_future();
    register_future_.wait();
  }

  void SetConnectionOnFail(l2cap::classic::DynamicChannelManager::ConnectionResult result, std::promise<void> promise) {
    std::move(on_fail_callback_).Run(result);
    promise.set_value();
  }

  void SetConnectionOnOpen(std::unique_ptr<l2cap::DynamicChannel> channel, std::promise<void> promise) {
    std::move(on_open_callback_).Run(std::move(channel));
    promise.set_value();
  }

  l2cap::classic::DynamicChannelManager::OnRegistrationCompleteCallback on_registration_complete_{};
  l2cap::classic::DynamicChannelManager::OnConnectionOpenCallback on_open_callback_{};
  l2cap::classic::DynamicChannelManager::OnConnectionFailureCallback on_fail_callback_{};

  TestDynamicChannelManagerImpl() = default;
  ~TestDynamicChannelManagerImpl() = default;

 private:
  std::promise<void> connected_promise_;
  std::future<void> connected_future_;

  std::promise<void> register_promise_;
  std::future<void> register_future_;
};

class TestDynamicChannelManager : public l2cap::classic::DynamicChannelManager {
 public:
  bool ConnectChannel(hci::Address device, l2cap::classic::DynamicChannelConfigurationOption configuration_option,
                      l2cap::Psm psm, l2cap::classic::DynamicChannelManager::OnConnectionOpenCallback on_open_callback,
                      l2cap::classic::DynamicChannelManager::OnConnectionFailureCallback on_fail_callback,
                      os::Handler* handler) override {
    return impl_.ConnectChannel(device, configuration_option, psm, std::move(on_open_callback),
                                std::move(on_fail_callback), handler);
  }

  bool RegisterService(l2cap::Psm psm, l2cap::classic::DynamicChannelConfigurationOption configuration_option,
                       const l2cap::SecurityPolicy& security_policy,
                       l2cap::classic::DynamicChannelManager::OnRegistrationCompleteCallback on_registration_complete,
                       l2cap::classic::DynamicChannelManager::OnConnectionOpenCallback on_open_callback,
                       os::Handler* handler) override {
    return impl_.RegisterService(psm, configuration_option, security_policy, std::move(on_registration_complete),
                                 std::move(on_open_callback), handler);
  }
  TestDynamicChannelManager(TestDynamicChannelManagerImpl& impl) : impl_(impl) {}
  TestDynamicChannelManagerImpl& impl_;
};

class TestL2capClassicModule : public l2cap::classic::L2capClassicModule {
 public:
  std::unique_ptr<l2cap::classic::DynamicChannelManager> GetDynamicChannelManager() override {
    ASSERT(impl_ != nullptr);
    return std::make_unique<TestDynamicChannelManager>(*impl_);
  }

  void ListDependencies(ModuleList* list) override {}
  void Start() override;
  void Stop() override;

  std::unique_ptr<TestDynamicChannelManagerImpl> impl_;
};

void TestL2capClassicModule::Start() {
  impl_ = std::make_unique<TestDynamicChannelManagerImpl>();
}

void TestL2capClassicModule::Stop() {
  impl_.reset();
}

class ShimL2capTest : public ::testing::Test {
 public:
  void OnConnectionComplete(std::string string_address, uint16_t psm, uint16_t cid, bool connected) {
    connection_string_address_ = string_address;
    connection_psm_ = psm;
    connection_cid_ = cid;
    connection_connected_ = connected;
    connection_complete_promise_.set_value();
  }

  uint16_t CreateConnection(uint16_t psm, std::string device_address) {
    std::promise<uint16_t> promise;
    auto future = promise.get_future();

    shim_l2cap_->CreateConnection(
        psm, device_address,
        std::bind(&bluetooth::shim::ShimL2capTest::OnConnectionComplete, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
        std::move(promise));
    return future.get();
  }

  void SetConnectionFuture() {
    test_l2cap_classic_module_->impl_->SetConnectionFuture();
  }

  void WaitConnectionFuture() {
    test_l2cap_classic_module_->impl_->WaitConnectionFuture();
  }

  void SetRegistrationFuture() {
    test_l2cap_classic_module_->impl_->SetRegistrationFuture();
  }

  void WaitRegistrationFuture() {
    test_l2cap_classic_module_->impl_->WaitRegistrationFuture();
  }

  int NumberOfConnections() const {
    return test_l2cap_classic_module_->impl_->connections_;
  }

  int NumberOfServices() const {
    return test_l2cap_classic_module_->impl_->services_;
  }

  std::string connection_string_address_;
  uint16_t connection_psm_{0};
  uint16_t connection_cid_{0};
  bool connection_connected_{false};

  shim::L2cap* shim_l2cap_ = nullptr;
  TestL2capClassicModule* test_l2cap_classic_module_{nullptr};

  TestLink test_link_;
  std::promise<void> connection_complete_promise_;

 protected:
  void SetUp() override {
    handler_ = new os::Handler(&thread_);

    test_l2cap_classic_module_ = new TestL2capClassicModule();
    test_l2cap_classic_module_->Start();
    fake_registry_.InjectTestModule(&l2cap::classic::L2capClassicModule::Factory, test_l2cap_classic_module_);

    fake_registry_.Start<shim::L2cap>(&thread_);
    shim_l2cap_ = static_cast<shim::L2cap*>(fake_registry_.GetModuleUnderTest(&shim::L2cap::Factory));
  }

  void TearDown() override {
    fake_registry_.StopAll();
    handler_->Clear();
    delete handler_;
  }
  os::Handler* handler_ = nullptr;
  l2cap::classic::internal::testing::MockDynamicChannelServiceManagerImpl mock_;

 private:
  TestModuleRegistry fake_registry_;
  os::Thread& thread_ = fake_registry_.GetTestThread();
};

TEST_F(ShimL2capTest, CreateThenDisconnectBeforeCompletion) {
  SetConnectionFuture();

  ASSERT(NumberOfConnections() == 0);
  uint16_t cid = CreateConnection(kPsm, device_address);
  ASSERT(cid != 0);

  WaitConnectionFuture();
  ASSERT(NumberOfConnections() == 1);

  shim_l2cap_->CloseConnection(cid);
}

TEST_F(ShimL2capTest, MaxCreatedConnections) {
  for (int i = 0; i < 65536 - 64; i++) {
    SetConnectionFuture();
    uint16_t cid = CreateConnection(kPsm, device_address);
    ASSERT(cid != 0);
    WaitConnectionFuture();

    ASSERT(NumberOfConnections() == i + 1);
  }
  uint16_t cid = CreateConnection(kPsm, device_address);
  ASSERT(cid == 0);

  ASSERT(NumberOfConnections() == 65536 - 64);
}

TEST_F(ShimL2capTest, TwoDifferentCreatedConnections) {
  {
    SetConnectionFuture();
    uint16_t cid = CreateConnection(kPsm, device_address);
    ASSERT(cid != 0);
    WaitConnectionFuture();

    ASSERT(NumberOfConnections() == 1);
  }

  {
    SetConnectionFuture();
    uint16_t cid = CreateConnection(kPsm2, device_address2);
    ASSERT(cid != 0);
    WaitConnectionFuture();

    ASSERT(NumberOfConnections() == 2);
  }
}

TEST_F(ShimL2capTest, ConnectFail) {
  SetConnectionFuture();
  uint16_t cid = CreateConnection(kPsm, device_address);
  ASSERT(cid != 0);
  WaitConnectionFuture();

  ASSERT(NumberOfConnections() == 1);

  l2cap::classic::DynamicChannelManager::ConnectionResult result{
      .connection_result_code = TestDynamicChannelManager::ConnectionResultCode::FAIL_NO_SERVICE_REGISTERED,
      .hci_error = hci::ErrorCode::SUCCESS,
      .l2cap_connection_response_result = l2cap::ConnectionResponseResult::SUCCESS,
  };

  std::promise<void> on_fail_promise;
  auto on_fail_future = on_fail_promise.get_future();
  handler_->Post(common::BindOnce(&TestDynamicChannelManagerImpl::SetConnectionOnFail,
                                  common::Unretained(test_l2cap_classic_module_->impl_.get()), result,
                                  std::move(on_fail_promise)));
  on_fail_future.wait();

  ASSERT(connection_connected_ == false);

  shim_l2cap_->CloseConnection(cid);
}

TEST_F(ShimL2capTest, ConnectOpen) {
  SetConnectionFuture();
  uint16_t cid = CreateConnection(kPsm, device_address);
  ASSERT(cid != 0);
  WaitConnectionFuture();

  ASSERT(NumberOfConnections() == 1);

  hci::Address address;
  hci::Address::FromString(device_address, address);
  test_link_.device_with_type_ = hci::AddressWithType(address, hci::AddressType::PUBLIC_DEVICE_ADDRESS);

  l2cap::Psm psm = kPsm;
  l2cap::Cid local_cid = kCid;
  l2cap::Cid remote_cid = kCid2;

  std::shared_ptr<l2cap::internal::DynamicChannelImpl> impl =
      std::make_shared<l2cap::internal::DynamicChannelImpl>(psm, local_cid, remote_cid, &test_link_, handler_);

  auto channel = std::make_unique<l2cap::DynamicChannel>(impl, handler_);

  std::promise<void> on_fail_promise;
  auto on_fail_future = on_fail_promise.get_future();

  auto connection_complete_future = connection_complete_promise_.get_future();
  handler_->Post(common::BindOnce(&TestDynamicChannelManagerImpl::SetConnectionOnOpen,
                                  common::Unretained(test_l2cap_classic_module_->impl_.get()), std::move(channel),
                                  std::move(on_fail_promise)));
  connection_complete_future.wait();

  on_fail_future.wait();

  ASSERT(connection_connected_ == true);

  auto future = test_link_.connection_closed_promise_.get_future();
  shim_l2cap_->CloseConnection(cid);
  future.wait();
}

TEST_F(ShimL2capTest, RegisterService_Success) {
  std::promise<uint16_t> registration_promise;
  auto registration_pending = registration_promise.get_future();

  SetRegistrationFuture();
  shim_l2cap_->RegisterService(
      kPsm, kNoUseErtm, kMtu,
      std::bind(&bluetooth::shim::ShimL2capTest::OnConnectionComplete, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
      std::move(registration_promise));
  WaitRegistrationFuture();
  ASSERT_LOG(test_l2cap_classic_module_->impl_->on_registration_complete_, "Synchronization failure");
  ASSERT(test_l2cap_classic_module_->impl_->services_ == 1);

  l2cap::classic::DynamicChannelManager::RegistrationResult result{
      l2cap::classic::DynamicChannelManager::RegistrationResult::SUCCESS,
  };
  auto service = std::make_unique<TestDynamicChannelService>(kPsm, &mock_, handler_);

  handler_->Post(common::BindOnce(std::move(test_l2cap_classic_module_->impl_->on_registration_complete_), result,
                                  std::move(service)));
  uint16_t psm = registration_pending.get();
  ASSERT(psm == kPsm);
}

TEST_F(ShimL2capTest, RegisterService_Duplicate) {
  std::promise<uint16_t> promise;
  auto future = promise.get_future();

  SetRegistrationFuture();
  shim_l2cap_->RegisterService(
      kPsm, kNoUseErtm, kMtu,
      std::bind(&bluetooth::shim::ShimL2capTest::OnConnectionComplete, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
      std::move(promise));
  WaitRegistrationFuture();
  ASSERT_LOG(test_l2cap_classic_module_->impl_->on_registration_complete_, "Synchronization failure");
  ASSERT(test_l2cap_classic_module_->impl_->services_ == 1);

  l2cap::classic::DynamicChannelManager::RegistrationResult result{
      l2cap::classic::DynamicChannelManager::RegistrationResult::FAIL_DUPLICATE_SERVICE,
  };
  auto service = std::make_unique<TestDynamicChannelService>(kPsm, &mock_, handler_);

  handler_->Post(common::BindOnce(std::move(test_l2cap_classic_module_->impl_->on_registration_complete_), result,
                                  std::move(service)));
  uint16_t psm = future.get();
  ASSERT(psm == l2cap::kDefaultPsm);
}

TEST_F(ShimL2capTest, RegisterService_Invalid) {
  std::promise<uint16_t> promise;
  auto future = promise.get_future();

  SetRegistrationFuture();

  shim_l2cap_->RegisterService(
      kPsm, kNoUseErtm, kMtu,
      std::bind(&bluetooth::shim::ShimL2capTest::OnConnectionComplete, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
      std::move(promise));

  l2cap::classic::DynamicChannelManager::RegistrationResult result{
      l2cap::classic::DynamicChannelManager::RegistrationResult::FAIL_INVALID_SERVICE,
  };
  auto service = std::make_unique<TestDynamicChannelService>(kPsm, &mock_, handler_);
  WaitRegistrationFuture();

  ASSERT_LOG(test_l2cap_classic_module_->impl_->on_registration_complete_, "Synchronization failure");
  handler_->Post(common::BindOnce(std::move(test_l2cap_classic_module_->impl_->on_registration_complete_), result,
                                  std::move(service)));
  uint16_t psm = future.get();
  ASSERT(psm == l2cap::kDefaultPsm);
  ASSERT(test_l2cap_classic_module_->impl_->services_ == 1);
}

}  // namespace
}  // namespace shim
}  // namespace bluetooth
