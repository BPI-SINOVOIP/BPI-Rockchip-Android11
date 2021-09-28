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
#include "security/facade.h"

#include "grpc/grpc_event_queue.h"
#include "hci/address_with_type.h"
#include "os/handler.h"
#include "security/facade.grpc.pb.h"
#include "security/security_manager_listener.h"
#include "security/security_module.h"

namespace bluetooth {
namespace security {

class SecurityModuleFacadeService : public SecurityModuleFacade::Service, public ISecurityManagerListener, public UI {
 public:
  SecurityModuleFacadeService(SecurityModule* security_module, ::bluetooth::os::Handler* security_handler)
      : security_module_(security_module), security_handler_(security_handler) {
    security_module_->GetSecurityManager()->RegisterCallbackListener(this, security_handler_);
  }

  ::grpc::Status CreateBond(::grpc::ServerContext* context, const facade::BluetoothAddressWithType* request,
                            ::google::protobuf::Empty* response) override {
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->address().address(), peer));
    hci::AddressType peer_type = hci::AddressType::PUBLIC_DEVICE_ADDRESS;
    security_module_->GetSecurityManager()->CreateBond(hci::AddressWithType(peer, peer_type));
    return ::grpc::Status::OK;
  }

  ::grpc::Status CancelBond(::grpc::ServerContext* context, const facade::BluetoothAddressWithType* request,
                            ::google::protobuf::Empty* response) override {
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->address().address(), peer));
    hci::AddressType peer_type = hci::AddressType::PUBLIC_DEVICE_ADDRESS;
    security_module_->GetSecurityManager()->CancelBond(hci::AddressWithType(peer, peer_type));
    return ::grpc::Status::OK;
  }

  ::grpc::Status RemoveBond(::grpc::ServerContext* context, const facade::BluetoothAddressWithType* request,
                            ::google::protobuf::Empty* response) override {
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->address().address(), peer));
    hci::AddressType peer_type = hci::AddressType::PUBLIC_DEVICE_ADDRESS;
    security_module_->GetSecurityManager()->RemoveBond(hci::AddressWithType(peer, peer_type));
    return ::grpc::Status::OK;
  }

  ::grpc::Status FetchUiEvents(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                               ::grpc::ServerWriter<UiMsg>* writer) override {
    return ui_events_.RunLoop(context, writer);
  }

  ::grpc::Status SendUiCallback(::grpc::ServerContext* context, const UiCallbackMsg* request,
                                ::google::protobuf::Empty* response) override {
    switch (request->message_type()) {
      case UiCallbackType::PASSKEY:
        // TODO: security_module_->GetSecurityManager()->OnPasskeyEntry();
        break;
      case UiCallbackType::YES_NO:
        // TODO: security_module_->GetSecurityManager()->OnConfirmYesNo(request->boolean());
        break;
      default:
        LOG_ERROR("Unknown UiCallbackType %d", static_cast<int>(request->message_type()));
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "Unknown UiCallbackType");
    }
    return ::grpc::Status::OK;
  }

  ::grpc::Status FetchBondEvents(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                 ::grpc::ServerWriter<BondMsg>* writer) override {
    return bond_events_.RunLoop(context, writer);
  }

  void DisplayPairingPrompt(const bluetooth::hci::AddressWithType& peer, std::string name) {
    LOG_INFO("%s", peer.ToString().c_str());
    UiMsg display_yes_no;
    display_yes_no.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_yes_no.mutable_peer()->set_type(facade::BluetoothAddressTypeEnum::PUBLIC_DEVICE_ADDRESS);
    display_yes_no.set_message_type(UiMsgType::DISPLAY_YES_NO);
    display_yes_no.set_unique_id(unique_id++);
  }

  virtual void DisplayConfirmValue(const bluetooth::hci::AddressWithType& peer, std::string name,
                                   uint32_t numeric_value) {
    LOG_INFO("%s value = 0x%x", peer.ToString().c_str(), numeric_value);
    UiMsg display_with_value;
    display_with_value.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_with_value.mutable_peer()->set_type(facade::BluetoothAddressTypeEnum::PUBLIC_DEVICE_ADDRESS);
    display_with_value.set_message_type(UiMsgType::DISPLAY_YES_NO_WITH_VALUE);
    display_with_value.set_numeric_value(numeric_value);
    display_with_value.set_unique_id(unique_id++);
    ui_events_.OnIncomingEvent(display_with_value);
  }

  void DisplayYesNoDialog(const bluetooth::hci::AddressWithType& peer, std::string name) override {
    LOG_INFO("%s", peer.ToString().c_str());
    UiMsg display_yes_no;
    display_yes_no.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_yes_no.mutable_peer()->set_type(facade::BluetoothAddressTypeEnum::PUBLIC_DEVICE_ADDRESS);
    display_yes_no.set_message_type(UiMsgType::DISPLAY_YES_NO);
    display_yes_no.set_unique_id(unique_id++);
  }

  void DisplayPasskey(const bluetooth::hci::AddressWithType& peer, std::string name, uint32_t passkey) override {
    LOG_INFO("%s value = 0x%x", peer.ToString().c_str(), passkey);
    UiMsg display_passkey;
    display_passkey.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_passkey.mutable_peer()->set_type(facade::BluetoothAddressTypeEnum::PUBLIC_DEVICE_ADDRESS);
    display_passkey.set_message_type(UiMsgType::DISPLAY_PASSKEY);
    display_passkey.set_numeric_value(passkey);
    display_passkey.set_unique_id(unique_id++);
    ui_events_.OnIncomingEvent(display_passkey);
  }

  void DisplayEnterPasskeyDialog(const bluetooth::hci::AddressWithType& peer, std::string name) override {
    LOG_INFO("%s", peer.ToString().c_str());
    UiMsg display_passkey_input;
    display_passkey_input.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_passkey_input.mutable_peer()->set_type(facade::BluetoothAddressTypeEnum::PUBLIC_DEVICE_ADDRESS);
    display_passkey_input.set_message_type(UiMsgType::DISPLAY_PASSKEY_ENTRY);
    display_passkey_input.set_unique_id(unique_id++);
    ui_events_.OnIncomingEvent(display_passkey_input);
  }

  void Cancel(const bluetooth::hci::AddressWithType& peer) override {
    LOG_INFO("%s", peer.ToString().c_str());
    UiMsg display_cancel;
    display_cancel.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_cancel.mutable_peer()->set_type(facade::BluetoothAddressTypeEnum::PUBLIC_DEVICE_ADDRESS);
    display_cancel.set_message_type(UiMsgType::DISPLAY_CANCEL);
    display_cancel.set_unique_id(unique_id++);
    ui_events_.OnIncomingEvent(display_cancel);
  }

  void OnDeviceBonded(hci::AddressWithType peer) override {
    LOG_INFO("%s", peer.ToString().c_str());
    BondMsg bonded;
    bonded.mutable_peer()->mutable_address()->set_address(peer.ToString());
    bonded.mutable_peer()->set_type(facade::BluetoothAddressTypeEnum::PUBLIC_DEVICE_ADDRESS);
    bonded.set_message_type(BondMsgType::DEVICE_BONDED);
    bond_events_.OnIncomingEvent(bonded);
  }

  void OnDeviceUnbonded(hci::AddressWithType peer) override {
    LOG_INFO("%s", peer.ToString().c_str());
    BondMsg unbonded;
    unbonded.mutable_peer()->mutable_address()->set_address(peer.ToString());
    unbonded.mutable_peer()->set_type(facade::BluetoothAddressTypeEnum::PUBLIC_DEVICE_ADDRESS);
    unbonded.set_message_type(BondMsgType::DEVICE_UNBONDED);
    bond_events_.OnIncomingEvent(unbonded);
  }

  void OnDeviceBondFailed(hci::AddressWithType peer) override {
    LOG_INFO("%s", peer.ToString().c_str());
    BondMsg bond_failed;
    bond_failed.mutable_peer()->mutable_address()->set_address(peer.ToString());
    bond_failed.mutable_peer()->set_type(facade::BluetoothAddressTypeEnum::PUBLIC_DEVICE_ADDRESS);
    bond_failed.set_message_type(BondMsgType::DEVICE_BOND_FAILED);
    bond_events_.OnIncomingEvent(bond_failed);
  }

 private:
  SecurityModule* security_module_;
  ::bluetooth::os::Handler* security_handler_;
  ::bluetooth::grpc::GrpcEventQueue<UiMsg> ui_events_{"UI events"};
  ::bluetooth::grpc::GrpcEventQueue<BondMsg> bond_events_{"Bond events"};
  uint32_t unique_id{1};
  std::map<uint32_t, common::OnceCallback<void(bool)>> user_yes_no_callbacks_;
  std::map<uint32_t, common::OnceCallback<void(uint32_t)>> user_passkey_callbacks_;
};

void SecurityModuleFacadeModule::ListDependencies(ModuleList* list) {
  ::bluetooth::grpc::GrpcFacadeModule::ListDependencies(list);
  list->add<SecurityModule>();
}

void SecurityModuleFacadeModule::Start() {
  ::bluetooth::grpc::GrpcFacadeModule::Start();
  service_ = new SecurityModuleFacadeService(GetDependency<SecurityModule>(), GetHandler());
}

void SecurityModuleFacadeModule::Stop() {
  delete service_;
  ::bluetooth::grpc::GrpcFacadeModule::Stop();
}

::grpc::Service* SecurityModuleFacadeModule::GetService() const {
  return service_;
}

const ModuleFactory SecurityModuleFacadeModule::Factory =
    ::bluetooth::ModuleFactory([]() { return new SecurityModuleFacadeModule(); });

}  // namespace security
}  // namespace bluetooth
