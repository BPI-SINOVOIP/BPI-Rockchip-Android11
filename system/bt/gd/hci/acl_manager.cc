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

#include "hci/acl_manager.h"

#include <future>
#include <queue>
#include <set>
#include <utility>

#include "acl_fragmenter.h"
#include "acl_manager.h"
#include "common/bidi_queue.h"
#include "hci/controller.h"
#include "hci/hci_layer.h"

namespace bluetooth {
namespace hci {

constexpr uint16_t kQualcommDebugHandle = 0xedc;
constexpr size_t kMaxQueuedPacketsPerConnection = 10;

using common::Bind;
using common::BindOnce;

namespace {
class PacketViewForRecombination : public packet::PacketView<kLittleEndian> {
 public:
  PacketViewForRecombination(const PacketView& packetView) : PacketView(packetView) {}
  void AppendPacketView(packet::PacketView<kLittleEndian> to_append) {
    Append(to_append);
  }
};

constexpr int kL2capBasicFrameHeaderSize = 4;

// Per spec 5.1 Vol 2 Part B 5.3, ACL link shall carry L2CAP data. Therefore, an ACL packet shall contain L2CAP PDU.
// This function returns the PDU size of the L2CAP data if it's a starting packet. Returns 0 if it's invalid.
uint16_t GetL2capPduSize(AclPacketView packet) {
  auto l2cap_payload = packet.GetPayload();
  if (l2cap_payload.size() < kL2capBasicFrameHeaderSize) {
    LOG_ERROR("Controller sent an invalid L2CAP starting packet!");
    return 0;
  }
  return (l2cap_payload.at(1) << 8) + l2cap_payload.at(0);
}

}  // namespace

struct AclManager::acl_connection {
  acl_connection(AddressWithType address_with_type, os::Handler* handler)
      : address_with_type_(address_with_type), handler_(handler) {}
  friend AclConnection;
  AddressWithType address_with_type_;
  os::Handler* handler_;
  std::unique_ptr<AclConnection::Queue> queue_ = std::make_unique<AclConnection::Queue>(10);
  bool is_disconnected_ = false;
  ErrorCode disconnect_reason_;
  os::Handler* command_complete_handler_ = nullptr;
  os::Handler* disconnect_handler_ = nullptr;
  ConnectionManagementCallbacks* command_complete_callbacks_ = nullptr;
  common::OnceCallback<void(ErrorCode)> on_disconnect_callback_;
  // For LE Connection parameter update from L2CAP
  common::OnceCallback<void(ErrorCode)> on_connection_update_complete_callback_;
  os::Handler* on_connection_update_complete_callback_handler_ = nullptr;
  // Round-robin: Track if dequeue is registered for this connection
  bool is_registered_ = false;
  // Credits: Track the number of packets which have been sent to the controller
  uint16_t number_of_sent_packets_ = 0;
  PacketViewForRecombination recombination_stage_{std::make_shared<std::vector<uint8_t>>()};
  int remaining_sdu_continuation_packet_size_ = 0;
  bool enqueue_registered_ = false;
  std::queue<packet::PacketView<kLittleEndian>> incoming_queue_;

  std::unique_ptr<packet::PacketView<kLittleEndian>> on_incoming_data_ready() {
    auto packet = incoming_queue_.front();
    incoming_queue_.pop();
    if (incoming_queue_.empty()) {
      auto queue_end = queue_->GetDownEnd();
      queue_end->UnregisterEnqueue();
      enqueue_registered_ = false;
    }
    return std::make_unique<PacketView<kLittleEndian>>(packet);
  }

  void on_incoming_packet(AclPacketView packet) {
    // TODO: What happens if the connection is stalled and fills up?
    PacketView<kLittleEndian> payload = packet.GetPayload();
    auto payload_size = payload.size();
    auto packet_boundary_flag = packet.GetPacketBoundaryFlag();
    if (packet_boundary_flag == PacketBoundaryFlag::FIRST_NON_AUTOMATICALLY_FLUSHABLE) {
      LOG_ERROR("Controller is not allowed to send FIRST_NON_AUTOMATICALLY_FLUSHABLE to host except loopback mode");
      return;
    }
    if (packet_boundary_flag == PacketBoundaryFlag::CONTINUING_FRAGMENT) {
      if (remaining_sdu_continuation_packet_size_ < payload_size) {
        LOG_WARN("Remote sent unexpected L2CAP PDU. Drop the entire L2CAP PDU");
        recombination_stage_ = PacketViewForRecombination(std::make_shared<std::vector<uint8_t>>());
        remaining_sdu_continuation_packet_size_ = 0;
        return;
      }
      remaining_sdu_continuation_packet_size_ -= payload_size;
      recombination_stage_.AppendPacketView(payload);
      if (remaining_sdu_continuation_packet_size_ != 0) {
        return;
      } else {
        payload = recombination_stage_;
        recombination_stage_ = PacketViewForRecombination(std::make_shared<std::vector<uint8_t>>());
      }
    } else if (packet_boundary_flag == PacketBoundaryFlag::FIRST_AUTOMATICALLY_FLUSHABLE) {
      if (recombination_stage_.size() > 0) {
        LOG_ERROR("Controller sent a starting packet without finishing previous packet. Drop previous one.");
      }
      auto l2cap_pdu_size = GetL2capPduSize(packet);
      remaining_sdu_continuation_packet_size_ = l2cap_pdu_size - (payload_size - kL2capBasicFrameHeaderSize);
      if (remaining_sdu_continuation_packet_size_ > 0) {
        recombination_stage_ = payload;
        return;
      }
    }
    if (incoming_queue_.size() > kMaxQueuedPacketsPerConnection) {
      LOG_ERROR("Dropping packet due to congestion from remote:%s", address_with_type_.ToString().c_str());
      return;
    }

    incoming_queue_.push(payload);
    if (!enqueue_registered_) {
      enqueue_registered_ = true;
      auto queue_end = queue_->GetDownEnd();
      queue_end->RegisterEnqueue(
          handler_, common::Bind(&AclManager::acl_connection::on_incoming_data_ready, common::Unretained(this)));
    }
  }

  void call_disconnect_callback() {
    disconnect_handler_->Post(BindOnce(std::move(on_disconnect_callback_), disconnect_reason_));
  }
};

struct AclManager::impl {
  impl(const AclManager& acl_manager) : acl_manager_(acl_manager) {}

  void Start() {
    hci_layer_ = acl_manager_.GetDependency<HciLayer>();
    handler_ = acl_manager_.GetHandler();
    controller_ = acl_manager_.GetDependency<Controller>();
    max_acl_packet_credits_ = controller_->GetControllerNumAclPacketBuffers();
    acl_packet_credits_ = max_acl_packet_credits_;
    acl_buffer_length_ = controller_->GetControllerAclPacketLength();
    controller_->RegisterCompletedAclPacketsCallback(
        common::Bind(&impl::incoming_acl_credits, common::Unretained(this)), handler_);

    // TODO: determine when we should reject connection
    should_accept_connection_ = common::Bind([](Address, ClassOfDevice) { return true; });
    hci_queue_end_ = hci_layer_->GetAclQueueEnd();
    hci_queue_end_->RegisterDequeue(
        handler_, common::Bind(&impl::dequeue_and_route_acl_packet_to_connection, common::Unretained(this)));
    hci_layer_->RegisterEventHandler(EventCode::CONNECTION_COMPLETE,
                                     Bind(&impl::on_connection_complete, common::Unretained(this)), handler_);
    hci_layer_->RegisterEventHandler(EventCode::DISCONNECTION_COMPLETE,
                                     Bind(&impl::on_disconnection_complete, common::Unretained(this)), handler_);
    hci_layer_->RegisterEventHandler(EventCode::CONNECTION_REQUEST,
                                     Bind(&impl::on_incoming_connection, common::Unretained(this)), handler_);
    hci_layer_->RegisterLeEventHandler(SubeventCode::CONNECTION_COMPLETE,
                                       Bind(&impl::on_le_connection_complete, common::Unretained(this)), handler_);
    hci_layer_->RegisterLeEventHandler(SubeventCode::ENHANCED_CONNECTION_COMPLETE,
                                       Bind(&impl::on_le_enhanced_connection_complete, common::Unretained(this)),
                                       handler_);
    hci_layer_->RegisterLeEventHandler(SubeventCode::CONNECTION_UPDATE_COMPLETE,
                                       Bind(&impl::on_le_connection_update_complete, common::Unretained(this)),
                                       handler_);
    hci_layer_->RegisterEventHandler(EventCode::CONNECTION_PACKET_TYPE_CHANGED,
                                     Bind(&impl::on_connection_packet_type_changed, common::Unretained(this)),
                                     handler_);
    hci_layer_->RegisterEventHandler(EventCode::AUTHENTICATION_COMPLETE,
                                     Bind(&impl::on_authentication_complete, common::Unretained(this)), handler_);
    hci_layer_->RegisterEventHandler(EventCode::READ_CLOCK_OFFSET_COMPLETE,
                                     Bind(&impl::on_read_clock_offset_complete, common::Unretained(this)), handler_);
    hci_layer_->RegisterEventHandler(EventCode::MODE_CHANGE, Bind(&impl::on_mode_change, common::Unretained(this)),
                                     handler_);
    hci_layer_->RegisterEventHandler(EventCode::QOS_SETUP_COMPLETE,
                                     Bind(&impl::on_qos_setup_complete, common::Unretained(this)), handler_);
    hci_layer_->RegisterEventHandler(EventCode::ROLE_CHANGE, Bind(&impl::on_role_change, common::Unretained(this)),
                                     handler_);
    hci_layer_->RegisterEventHandler(EventCode::FLOW_SPECIFICATION_COMPLETE,
                                     Bind(&impl::on_flow_specification_complete, common::Unretained(this)), handler_);
    hci_layer_->RegisterEventHandler(EventCode::FLUSH_OCCURRED,
                                     Bind(&impl::on_flush_occurred, common::Unretained(this)), handler_);
    hci_layer_->RegisterEventHandler(EventCode::READ_REMOTE_SUPPORTED_FEATURES_COMPLETE,
                                     Bind(&impl::on_read_remote_supported_features_complete, common::Unretained(this)),
                                     handler_);
    hci_layer_->RegisterEventHandler(EventCode::READ_REMOTE_EXTENDED_FEATURES_COMPLETE,
                                     Bind(&impl::on_read_remote_extended_features_complete, common::Unretained(this)),
                                     handler_);
    hci_layer_->RegisterEventHandler(EventCode::READ_REMOTE_VERSION_INFORMATION_COMPLETE,
                                     Bind(&impl::on_read_remote_version_information_complete, common::Unretained(this)),
                                     handler_);
    hci_layer_->RegisterEventHandler(EventCode::ENCRYPTION_CHANGE,
                                     Bind(&impl::on_encryption_change, common::Unretained(this)), handler_);
    hci_layer_->RegisterEventHandler(EventCode::LINK_SUPERVISION_TIMEOUT_CHANGED,
                                     Bind(&impl::on_link_supervision_timeout_changed, common::Unretained(this)),
                                     handler_);
    hci_mtu_ = controller_->GetControllerAclPacketLength();
  }

  void Stop() {
    hci_layer_->UnregisterEventHandler(EventCode::DISCONNECTION_COMPLETE);
    hci_layer_->UnregisterEventHandler(EventCode::CONNECTION_COMPLETE);
    hci_layer_->UnregisterEventHandler(EventCode::CONNECTION_REQUEST);
    hci_layer_->UnregisterEventHandler(EventCode::AUTHENTICATION_COMPLETE);
    hci_layer_->UnregisterEventHandler(EventCode::READ_REMOTE_SUPPORTED_FEATURES_COMPLETE);
    hci_layer_->UnregisterEventHandler(EventCode::READ_REMOTE_EXTENDED_FEATURES_COMPLETE);
    hci_queue_end_->UnregisterDequeue();
    unregister_all_connections();
    acl_connections_.clear();
    hci_queue_end_ = nullptr;
    handler_ = nullptr;
    hci_layer_ = nullptr;
  }

  void incoming_acl_credits(uint16_t handle, uint16_t credits) {
    auto connection_pair = acl_connections_.find(handle);
    if (connection_pair == acl_connections_.end()) {
      LOG_INFO("Dropping %hx received credits to unknown connection 0x%0hx", credits, handle);
      return;
    }
    if (connection_pair->second.is_disconnected_) {
      LOG_INFO("Dropping %hx received credits to disconnected connection 0x%0hx", credits, handle);
      return;
    }
    connection_pair->second.number_of_sent_packets_ -= credits;
    acl_packet_credits_ += credits;
    ASSERT(acl_packet_credits_ <= max_acl_packet_credits_);
    start_round_robin();
  }

  // Round-robin scheduler
  void start_round_robin() {
    if (acl_packet_credits_ == 0) {
      return;
    }
    if (!fragments_to_send_.empty()) {
      send_next_fragment();
      return;
    }
    for (auto connection_pair = acl_connections_.begin(); connection_pair != acl_connections_.end();
         connection_pair = std::next(connection_pair)) {
      if (connection_pair->second.is_registered_) {
        continue;
      }
      connection_pair->second.is_registered_ = true;
      connection_pair->second.queue_->GetDownEnd()->RegisterDequeue(
          handler_, common::Bind(&impl::handle_dequeue_from_upper, common::Unretained(this), connection_pair));
    }
  }

  void handle_dequeue_from_upper(std::map<uint16_t, acl_connection>::iterator connection_pair) {
    current_connection_pair_ = connection_pair;
    buffer_packet();
  }

  void unregister_all_connections() {
    for (auto connection_pair = acl_connections_.begin(); connection_pair != acl_connections_.end();
         connection_pair = std::next(connection_pair)) {
      if (connection_pair->second.is_registered_) {
        connection_pair->second.is_registered_ = false;
        connection_pair->second.queue_->GetDownEnd()->UnregisterDequeue();
      }
    }
  }

  void buffer_packet() {
    unregister_all_connections();
    BroadcastFlag broadcast_flag = BroadcastFlag::POINT_TO_POINT;
    //   Wrap packet and enqueue it
    uint16_t handle = current_connection_pair_->first;

    auto packet = current_connection_pair_->second.queue_->GetDownEnd()->TryDequeue();
    ASSERT(packet != nullptr);

    if (packet->size() <= hci_mtu_) {
      fragments_to_send_.push_front(AclPacketBuilder::Create(handle, PacketBoundaryFlag::FIRST_AUTOMATICALLY_FLUSHABLE,
                                                             broadcast_flag, std::move(packet)));
    } else {
      auto fragments = AclFragmenter(hci_mtu_, std::move(packet)).GetFragments();
      PacketBoundaryFlag packet_boundary_flag = PacketBoundaryFlag::FIRST_AUTOMATICALLY_FLUSHABLE;
      for (size_t i = 0; i < fragments.size(); i++) {
        fragments_to_send_.push_back(
            AclPacketBuilder::Create(handle, packet_boundary_flag, broadcast_flag, std::move(fragments[i])));
        packet_boundary_flag = PacketBoundaryFlag::CONTINUING_FRAGMENT;
      }
    }
    ASSERT(fragments_to_send_.size() > 0);

    current_connection_pair_->second.number_of_sent_packets_ += fragments_to_send_.size();
    send_next_fragment();
  }

  void send_next_fragment() {
    hci_queue_end_->RegisterEnqueue(handler_,
                                    common::Bind(&impl::handle_enqueue_next_fragment, common::Unretained(this)));
  }

  std::unique_ptr<AclPacketBuilder> handle_enqueue_next_fragment() {
    ASSERT(acl_packet_credits_ > 0);
    if (acl_packet_credits_ == 1 || fragments_to_send_.size() == 1) {
      hci_queue_end_->UnregisterEnqueue();
      if (fragments_to_send_.size() == 1) {
        handler_->Post(common::BindOnce(&impl::start_round_robin, common::Unretained(this)));
      }
    }
    ASSERT(fragments_to_send_.size() > 0);
    auto raw_pointer = fragments_to_send_.front().release();
    acl_packet_credits_ -= 1;
    fragments_to_send_.pop_front();
    return std::unique_ptr<AclPacketBuilder>(raw_pointer);
  }

  void dequeue_and_route_acl_packet_to_connection() {
    auto packet = hci_queue_end_->TryDequeue();
    ASSERT(packet != nullptr);
    if (!packet->IsValid()) {
      LOG_INFO("Dropping invalid packet of size %zu", packet->size());
      return;
    }
    uint16_t handle = packet->GetHandle();
    if (handle == kQualcommDebugHandle) {
      return;
    }
    auto connection_pair = acl_connections_.find(handle);
    if (connection_pair == acl_connections_.end()) {
      LOG_INFO("Dropping packet of size %zu to unknown connection 0x%0hx", packet->size(), handle);
      return;
    }

    connection_pair->second.on_incoming_packet(*packet);
  }

  void on_incoming_connection(EventPacketView packet) {
    ConnectionRequestView request = ConnectionRequestView::Create(packet);
    ASSERT(request.IsValid());
    Address address = request.GetBdAddr();
    if (client_callbacks_ == nullptr) {
      LOG_ERROR("No callbacks to call");
      auto reason = RejectConnectionReason::LIMITED_RESOURCES;
      this->reject_connection(RejectConnectionRequestBuilder::Create(address, reason));
      return;
    }
    connecting_.insert(address);
    if (is_classic_link_already_connected(address)) {
      auto reason = RejectConnectionReason::UNACCEPTABLE_BD_ADDR;
      this->reject_connection(RejectConnectionRequestBuilder::Create(address, reason));
    } else if (should_accept_connection_.Run(address, request.GetClassOfDevice())) {
      this->accept_connection(address);
    } else {
      auto reason = RejectConnectionReason::LIMITED_RESOURCES;  // TODO: determine reason
      this->reject_connection(RejectConnectionRequestBuilder::Create(address, reason));
    }
  }

  void on_classic_connection_complete(Address address) {
    auto connecting_addr = connecting_.find(address);
    if (connecting_addr == connecting_.end()) {
      LOG_WARN("No prior connection request for %s", address.ToString().c_str());
    } else {
      connecting_.erase(connecting_addr);
    }
  }

  void on_common_le_connection_complete(AddressWithType address_with_type) {
    auto connecting_addr_with_type = connecting_le_.find(address_with_type);
    if (connecting_addr_with_type == connecting_le_.end()) {
      LOG_WARN("No prior connection request for %s", address_with_type.ToString().c_str());
    } else {
      connecting_le_.erase(connecting_addr_with_type);
    }
  }

  void on_le_connection_complete(LeMetaEventView packet) {
    LeConnectionCompleteView connection_complete = LeConnectionCompleteView::Create(packet);
    ASSERT(connection_complete.IsValid());
    auto status = connection_complete.GetStatus();
    auto address = connection_complete.GetPeerAddress();
    auto peer_address_type = connection_complete.GetPeerAddressType();
    // TODO: find out which address and type was used to initiate the connection
    AddressWithType address_with_type(address, peer_address_type);
    on_common_le_connection_complete(address_with_type);
    if (status != ErrorCode::SUCCESS) {
      le_client_handler_->Post(common::BindOnce(&LeConnectionCallbacks::OnLeConnectFail,
                                                common::Unretained(le_client_callbacks_), address_with_type, status));
      return;
    }
    // TODO: Check and save other connection parameters
    uint16_t handle = connection_complete.GetConnectionHandle();
    ASSERT(acl_connections_.count(handle) == 0);
    acl_connections_.emplace(std::piecewise_construct, std::forward_as_tuple(handle),
                             std::forward_as_tuple(address_with_type, handler_));
    if (acl_connections_.size() == 1 && fragments_to_send_.size() == 0) {
      start_round_robin();
    }
    auto role = connection_complete.GetRole();
    std::unique_ptr<AclConnection> connection_proxy(
        new AclConnection(&acl_manager_, handle, address, peer_address_type, role));
    le_client_handler_->Post(common::BindOnce(&LeConnectionCallbacks::OnLeConnectSuccess,
                                              common::Unretained(le_client_callbacks_), address_with_type,
                                              std::move(connection_proxy)));
  }

  void on_le_enhanced_connection_complete(LeMetaEventView packet) {
    LeEnhancedConnectionCompleteView connection_complete = LeEnhancedConnectionCompleteView::Create(packet);
    ASSERT(connection_complete.IsValid());
    auto status = connection_complete.GetStatus();
    auto address = connection_complete.GetPeerAddress();
    auto peer_address_type = connection_complete.GetPeerAddressType();
    auto peer_resolvable_address = connection_complete.GetPeerResolvablePrivateAddress();
    AddressWithType reporting_address_with_type(address, peer_address_type);
    if (!peer_resolvable_address.IsEmpty()) {
      reporting_address_with_type = AddressWithType(peer_resolvable_address, AddressType::RANDOM_DEVICE_ADDRESS);
    }
    on_common_le_connection_complete(reporting_address_with_type);
    if (status != ErrorCode::SUCCESS) {
      le_client_handler_->Post(common::BindOnce(&LeConnectionCallbacks::OnLeConnectFail,
                                                common::Unretained(le_client_callbacks_), reporting_address_with_type,
                                                status));
      return;
    }
    // TODO: Check and save other connection parameters
    uint16_t handle = connection_complete.GetConnectionHandle();
    ASSERT(acl_connections_.count(handle) == 0);
    acl_connections_.emplace(std::piecewise_construct, std::forward_as_tuple(handle),
                             std::forward_as_tuple(reporting_address_with_type, handler_));
    if (acl_connections_.size() == 1 && fragments_to_send_.size() == 0) {
      start_round_robin();
    }
    auto role = connection_complete.GetRole();
    std::unique_ptr<AclConnection> connection_proxy(
        new AclConnection(&acl_manager_, handle, address, peer_address_type, role));
    le_client_handler_->Post(common::BindOnce(&LeConnectionCallbacks::OnLeConnectSuccess,
                                              common::Unretained(le_client_callbacks_), reporting_address_with_type,
                                              std::move(connection_proxy)));
  }

  void on_connection_complete(EventPacketView packet) {
    ConnectionCompleteView connection_complete = ConnectionCompleteView::Create(packet);
    ASSERT(connection_complete.IsValid());
    auto status = connection_complete.GetStatus();
    auto address = connection_complete.GetBdAddr();
    on_classic_connection_complete(address);
    if (status != ErrorCode::SUCCESS) {
      client_handler_->Post(common::BindOnce(&ConnectionCallbacks::OnConnectFail, common::Unretained(client_callbacks_),
                                             address, status));
      return;
    }
    uint16_t handle = connection_complete.GetConnectionHandle();
    ASSERT(acl_connections_.count(handle) == 0);
    acl_connections_.emplace(
        std::piecewise_construct, std::forward_as_tuple(handle),
        std::forward_as_tuple(AddressWithType{address, AddressType::PUBLIC_DEVICE_ADDRESS}, handler_));
    if (acl_connections_.size() == 1 && fragments_to_send_.size() == 0) {
      start_round_robin();
    }
    std::unique_ptr<AclConnection> connection_proxy(new AclConnection(&acl_manager_, handle, address));
    client_handler_->Post(common::BindOnce(&ConnectionCallbacks::OnConnectSuccess,
                                           common::Unretained(client_callbacks_), std::move(connection_proxy)));
    while (!pending_outgoing_connections_.empty()) {
      auto create_connection_packet_and_address = std::move(pending_outgoing_connections_.front());
      pending_outgoing_connections_.pop();
      if (!is_classic_link_already_connected(create_connection_packet_and_address.first)) {
        connecting_.insert(create_connection_packet_and_address.first);
        hci_layer_->EnqueueCommand(std::move(create_connection_packet_and_address.second),
                                   common::BindOnce([](CommandStatusView status) {
                                     ASSERT(status.IsValid());
                                     ASSERT(status.GetCommandOpCode() == OpCode::CREATE_CONNECTION);
                                   }),
                                   handler_);
        break;
      }
    }
  }

  void on_disconnection_complete(EventPacketView packet) {
    DisconnectionCompleteView disconnection_complete = DisconnectionCompleteView::Create(packet);
    ASSERT(disconnection_complete.IsValid());
    uint16_t handle = disconnection_complete.GetConnectionHandle();
    auto status = disconnection_complete.GetStatus();
    if (status == ErrorCode::SUCCESS) {
      ASSERT(acl_connections_.count(handle) == 1);
      auto& acl_connection = acl_connections_.find(handle)->second;
      acl_connection.is_disconnected_ = true;
      acl_connection.disconnect_reason_ = disconnection_complete.GetReason();
      acl_connection.call_disconnect_callback();
      // Reclaim outstanding packets
      acl_packet_credits_ += acl_connection.number_of_sent_packets_;
      acl_connection.number_of_sent_packets_ = 0;
    } else {
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received disconnection complete with error code %s, handle 0x%02hx", error_code.c_str(), handle);
    }
  }

  void on_connection_packet_type_changed(EventPacketView packet) {
    ConnectionPacketTypeChangedView packet_type_changed = ConnectionPacketTypeChangedView::Create(packet);
    if (!packet_type_changed.IsValid()) {
      LOG_ERROR("Received on_connection_packet_type_changed with invalid packet");
      return;
    } else if (packet_type_changed.GetStatus() != ErrorCode::SUCCESS) {
      auto status = packet_type_changed.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_connection_packet_type_changed with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = packet_type_changed.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      uint16_t packet_type = packet_type_changed.GetPacketType();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnConnectionPacketTypeChanged,
                           common::Unretained(acl_connection.command_complete_callbacks_), packet_type));
    }
  }

  void on_master_link_key_complete(EventPacketView packet) {
    MasterLinkKeyCompleteView complete_view = MasterLinkKeyCompleteView::Create(packet);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_master_link_key_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_master_link_key_complete with error code %s", error_code.c_str());
      return;
    }
    if (acl_manager_client_callbacks_ != nullptr) {
      uint16_t connection_handle = complete_view.GetConnectionHandle();
      KeyFlag key_flag = complete_view.GetKeyFlag();
      acl_manager_client_handler_->Post(common::BindOnce(&AclManagerCallbacks::OnMasterLinkKeyComplete,
                                                         common::Unretained(acl_manager_client_callbacks_),
                                                         connection_handle, key_flag));
    }
  }

  void on_authentication_complete(EventPacketView packet) {
    AuthenticationCompleteView authentication_complete = AuthenticationCompleteView::Create(packet);
    if (!authentication_complete.IsValid()) {
      LOG_ERROR("Received on_authentication_complete with invalid packet");
      return;
    } else if (authentication_complete.GetStatus() != ErrorCode::SUCCESS) {
      auto status = authentication_complete.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_authentication_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = authentication_complete.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnAuthenticationComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_)));
    }
  }

  void on_encryption_change(EventPacketView packet) {
    EncryptionChangeView encryption_change_view = EncryptionChangeView::Create(packet);
    if (!encryption_change_view.IsValid()) {
      LOG_ERROR("Received on_encryption_change with invalid packet");
      return;
    } else if (encryption_change_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = encryption_change_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_change_connection_link_key_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = encryption_change_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      EncryptionEnabled enabled = encryption_change_view.GetEncryptionEnabled();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnEncryptionChange,
                           common::Unretained(acl_connection.command_complete_callbacks_), enabled));
    }
  }

  void on_change_connection_link_key_complete(EventPacketView packet) {
    ChangeConnectionLinkKeyCompleteView complete_view = ChangeConnectionLinkKeyCompleteView::Create(packet);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_change_connection_link_key_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_change_connection_link_key_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnChangeConnectionLinkKeyComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_)));
    }
  }

  void on_read_clock_offset_complete(EventPacketView packet) {
    ReadClockOffsetCompleteView complete_view = ReadClockOffsetCompleteView::Create(packet);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_clock_offset_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_clock_offset_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      uint16_t clock_offset = complete_view.GetClockOffset();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnReadClockOffsetComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), clock_offset));
    }
  }

  void on_mode_change(EventPacketView packet) {
    ModeChangeView mode_change_view = ModeChangeView::Create(packet);
    if (!mode_change_view.IsValid()) {
      LOG_ERROR("Received on_mode_change with invalid packet");
      return;
    } else if (mode_change_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = mode_change_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_mode_change with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = mode_change_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      Mode current_mode = mode_change_view.GetCurrentMode();
      uint16_t interval = mode_change_view.GetInterval();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnModeChange,
                           common::Unretained(acl_connection.command_complete_callbacks_), current_mode, interval));
    }
  }

  void on_qos_setup_complete(EventPacketView packet) {
    QosSetupCompleteView complete_view = QosSetupCompleteView::Create(packet);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_qos_setup_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_qos_setup_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      ServiceType service_type = complete_view.GetServiceType();
      uint32_t token_rate = complete_view.GetTokenRate();
      uint32_t peak_bandwidth = complete_view.GetPeakBandwidth();
      uint32_t latency = complete_view.GetLatency();
      uint32_t delay_variation = complete_view.GetDelayVariation();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnQosSetupComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), service_type, token_rate,
                           peak_bandwidth, latency, delay_variation));
    }
  }

  void on_role_change(EventPacketView packet) {
    RoleChangeView role_change_view = RoleChangeView::Create(packet);
    if (!role_change_view.IsValid()) {
      LOG_ERROR("Received on_role_change with invalid packet");
      return;
    } else if (role_change_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = role_change_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_role_change with error code %s", error_code.c_str());
      return;
    }
    if (acl_manager_client_callbacks_ != nullptr) {
      Address bd_addr = role_change_view.GetBdAddr();
      Role new_role = role_change_view.GetNewRole();
      acl_manager_client_handler_->Post(common::BindOnce(
          &AclManagerCallbacks::OnRoleChange, common::Unretained(acl_manager_client_callbacks_), bd_addr, new_role));
    }
  }

  void on_flow_specification_complete(EventPacketView packet) {
    FlowSpecificationCompleteView complete_view = FlowSpecificationCompleteView::Create(packet);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_flow_specification_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_flow_specification_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      FlowDirection flow_direction = complete_view.GetFlowDirection();
      ServiceType service_type = complete_view.GetServiceType();
      uint32_t token_rate = complete_view.GetTokenRate();
      uint32_t token_bucket_size = complete_view.GetTokenBucketSize();
      uint32_t peak_bandwidth = complete_view.GetPeakBandwidth();
      uint32_t access_latency = complete_view.GetAccessLatency();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnFlowSpecificationComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), flow_direction, service_type,
                           token_rate, token_bucket_size, peak_bandwidth, access_latency));
    }
  }

  void on_flush_occurred(EventPacketView packet) {
    FlushOccurredView flush_occurred_view = FlushOccurredView::Create(packet);
    if (!flush_occurred_view.IsValid()) {
      LOG_ERROR("Received on_flush_occurred with invalid packet");
      return;
    }
    uint16_t handle = flush_occurred_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnFlushOccurred,
                           common::Unretained(acl_connection.command_complete_callbacks_)));
    }
  }

  void on_read_remote_version_information_complete(EventPacketView packet) {
    auto view = ReadRemoteVersionInformationCompleteView::Create(packet);
    ASSERT_LOG(view.IsValid(), "Read remote version information packet invalid");
    LOG_INFO("UNIMPLEMENTED called");
  }

  void on_read_remote_supported_features_complete(EventPacketView packet) {
    auto view = ReadRemoteSupportedFeaturesCompleteView::Create(packet);
    ASSERT_LOG(view.IsValid(), "Read remote supported features packet invalid");
    LOG_INFO("UNIMPLEMENTED called");
  }

  void on_read_remote_extended_features_complete(EventPacketView packet) {
    auto view = ReadRemoteExtendedFeaturesCompleteView::Create(packet);
    ASSERT_LOG(view.IsValid(), "Read remote extended features packet invalid");
    LOG_INFO("UNIMPLEMENTED called");
  }

  void on_link_supervision_timeout_changed(EventPacketView packet) {
    auto view = LinkSupervisionTimeoutChangedView::Create(packet);
    ASSERT_LOG(view.IsValid(), "Link supervision timeout changed packet invalid");
    LOG_INFO("UNIMPLEMENTED called");
  }

  void on_role_discovery_complete(CommandCompleteView view) {
    auto complete_view = RoleDiscoveryCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_role_discovery_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_role_discovery_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      Role role = complete_view.GetCurrentRole();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnRoleDiscoveryComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), role));
    }
  }

  void on_read_link_policy_settings_complete(CommandCompleteView view) {
    auto complete_view = ReadLinkPolicySettingsCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_link_policy_settings_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_link_policy_settings_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      uint16_t link_policy_settings = complete_view.GetLinkPolicySettings();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnReadLinkPolicySettingsComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), link_policy_settings));
    }
  }

  void on_read_default_link_policy_settings_complete(CommandCompleteView view) {
    auto complete_view = ReadDefaultLinkPolicySettingsCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_link_policy_settings_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_link_policy_settings_complete with error code %s", error_code.c_str());
      return;
    }
    if (acl_manager_client_callbacks_ != nullptr) {
      uint16_t default_link_policy_settings = complete_view.GetDefaultLinkPolicySettings();
      acl_manager_client_handler_->Post(common::BindOnce(&AclManagerCallbacks::OnReadDefaultLinkPolicySettingsComplete,
                                                         common::Unretained(acl_manager_client_callbacks_),
                                                         default_link_policy_settings));
    }
  }

  void on_read_automatic_flush_timeout_complete(CommandCompleteView view) {
    auto complete_view = ReadAutomaticFlushTimeoutCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_automatic_flush_timeout_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_automatic_flush_timeout_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      uint16_t flush_timeout = complete_view.GetFlushTimeout();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnReadAutomaticFlushTimeoutComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), flush_timeout));
    }
  }

  void on_read_transmit_power_level_complete(CommandCompleteView view) {
    auto complete_view = ReadTransmitPowerLevelCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_transmit_power_level_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_transmit_power_level_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      uint8_t transmit_power_level = complete_view.GetTransmitPowerLevel();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnReadTransmitPowerLevelComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), transmit_power_level));
    }
  }

  void on_read_link_supervision_timeout_complete(CommandCompleteView view) {
    auto complete_view = ReadLinkSupervisionTimeoutCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_link_supervision_timeout_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_link_supervision_timeout_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      uint16_t link_supervision_timeout = complete_view.GetLinkSupervisionTimeout();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnReadLinkSupervisionTimeoutComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), link_supervision_timeout));
    }
  }

  void on_read_failed_contact_counter_complete(CommandCompleteView view) {
    auto complete_view = ReadFailedContactCounterCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_failed_contact_counter_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_failed_contact_counter_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      uint16_t failed_contact_counter = complete_view.GetFailedContactCounter();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnReadFailedContactCounterComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), failed_contact_counter));
    }
  }

  void on_read_link_quality_complete(CommandCompleteView view) {
    auto complete_view = ReadLinkQualityCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_link_quality_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_link_quality_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      uint8_t link_quality = complete_view.GetLinkQuality();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnReadLinkQualityComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), link_quality));
    }
  }

  void on_read_afh_channel_map_complete(CommandCompleteView view) {
    auto complete_view = ReadAfhChannelMapCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_afh_channel_map_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_afh_channel_map_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      AfhMode afh_mode = complete_view.GetAfhMode();
      std::array<uint8_t, 10> afh_channel_map = complete_view.GetAfhChannelMap();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnReadAfhChannelMapComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), afh_mode, afh_channel_map));
    }
  }

  void on_read_rssi_complete(CommandCompleteView view) {
    auto complete_view = ReadRssiCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_rssi_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_rssi_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      uint8_t rssi = complete_view.GetRssi();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnReadRssiComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), rssi));
    }
  }

  void on_read_remote_version_information_status(CommandStatusView view) {
    ASSERT_LOG(view.IsValid(), "Bad status packet!");
    LOG_INFO("UNIMPLEMENTED called: %s", hci::ErrorCodeText(view.GetStatus()).c_str());
  }

  void on_read_remote_supported_features_status(CommandStatusView view) {
    ASSERT_LOG(view.IsValid(), "Bad status packet!");
    LOG_INFO("UNIMPLEMENTED called: %s", hci::ErrorCodeText(view.GetStatus()).c_str());
  }

  void on_read_remote_extended_features_status(CommandStatusView view) {
    ASSERT_LOG(view.IsValid(), "Broken");
    LOG_INFO("UNIMPLEMENTED called: %s", hci::ErrorCodeText(view.GetStatus()).c_str());
  }

  void on_read_clock_complete(CommandCompleteView view) {
    auto complete_view = ReadClockCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_read_clock_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_read_clock_complete with error code %s", error_code.c_str());
      return;
    }
    uint16_t handle = complete_view.GetConnectionHandle();
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.command_complete_handler_ != nullptr) {
      uint32_t clock = complete_view.GetClock();
      uint16_t accuracy = complete_view.GetAccuracy();
      acl_connection.command_complete_handler_->Post(
          common::BindOnce(&ConnectionManagementCallbacks::OnReadClockComplete,
                           common::Unretained(acl_connection.command_complete_callbacks_), clock, accuracy));
    }
  }

  void on_le_connection_update_complete(LeMetaEventView view) {
    auto complete_view = LeConnectionUpdateCompleteView::Create(view);
    if (!complete_view.IsValid()) {
      LOG_ERROR("Received on_le_connection_update_complete with invalid packet");
      return;
    } else if (complete_view.GetStatus() != ErrorCode::SUCCESS) {
      auto status = complete_view.GetStatus();
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received on_le_connection_update_complete with error code %s", error_code.c_str());
      return;
    }
    auto handle = complete_view.GetConnectionHandle();
    if (acl_connections_.find(handle) == acl_connections_.end()) {
      LOG_WARN("Can't find connection");
      return;
    }
    auto& connection = acl_connections_.find(handle)->second;
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return;
    }
    if (!connection.on_connection_update_complete_callback_.is_null()) {
      connection.on_connection_update_complete_callback_handler_->Post(
          common::BindOnce(std::move(connection.on_connection_update_complete_callback_), complete_view.GetStatus()));
      connection.on_connection_update_complete_callback_handler_ = nullptr;
    }
  }

  bool is_classic_link_already_connected(Address address) {
    for (const auto& connection : acl_connections_) {
      if (connection.second.address_with_type_.GetAddress() == address) {
        return true;
      }
    }
    return false;
  }

  void create_connection(Address address) {
    // TODO: Configure default connection parameters?
    uint16_t packet_type = 0x4408 /* DM 1,3,5 */ | 0x8810 /*DH 1,3,5 */;
    PageScanRepetitionMode page_scan_repetition_mode = PageScanRepetitionMode::R1;
    uint16_t clock_offset = 0;
    ClockOffsetValid clock_offset_valid = ClockOffsetValid::INVALID;
    CreateConnectionRoleSwitch allow_role_switch = CreateConnectionRoleSwitch::ALLOW_ROLE_SWITCH;
    ASSERT(client_callbacks_ != nullptr);
    std::unique_ptr<CreateConnectionBuilder> packet = CreateConnectionBuilder::Create(
        address, packet_type, page_scan_repetition_mode, clock_offset, clock_offset_valid, allow_role_switch);

    if (connecting_.empty()) {
      if (is_classic_link_already_connected(address)) {
        LOG_WARN("already connected: %s", address.ToString().c_str());
        return;
      }
      connecting_.insert(address);
      hci_layer_->EnqueueCommand(std::move(packet), common::BindOnce([](CommandStatusView status) {
                                   ASSERT(status.IsValid());
                                   ASSERT(status.GetCommandOpCode() == OpCode::CREATE_CONNECTION);
                                 }),
                                 handler_);
    } else {
      pending_outgoing_connections_.emplace(address, std::move(packet));
    }
  }

  void create_le_connection(AddressWithType address_with_type) {
    // TODO: Add white list handling.
    // TODO: Configure default LE connection parameters?
    uint16_t le_scan_interval = 0x0060;
    uint16_t le_scan_window = 0x0030;
    InitiatorFilterPolicy initiator_filter_policy = InitiatorFilterPolicy::USE_PEER_ADDRESS;
    OwnAddressType own_address_type = OwnAddressType::RANDOM_DEVICE_ADDRESS;
    uint16_t conn_interval_min = 0x0018;
    uint16_t conn_interval_max = 0x0028;
    uint16_t conn_latency = 0x0000;
    uint16_t supervision_timeout = 0x001f4;
    ASSERT(le_client_callbacks_ != nullptr);

    connecting_le_.insert(address_with_type);

    // TODO: make features check nicer, like HCI_LE_EXTENDED_ADVERTISING_SUPPORTED
    if (controller_->GetControllerLeLocalSupportedFeatures() & 0x0010) {
      LeCreateConnPhyScanParameters tmp;
      tmp.scan_interval_ = le_scan_interval;
      tmp.scan_window_ = le_scan_window;
      tmp.conn_interval_min_ = conn_interval_min;
      tmp.conn_interval_max_ = conn_interval_max;
      tmp.conn_latency_ = conn_latency;
      tmp.supervision_timeout_ = supervision_timeout;
      tmp.min_ce_length_ = 0x00;
      tmp.max_ce_length_ = 0x00;

      // With real controllers, we must set random address before using it to establish connection
      // TODO: have separate state machine generate new address when needed, consider using auto-generation in
      // controller
      hci_layer_->EnqueueCommand(hci::LeSetRandomAddressBuilder::Create(Address{{0x00, 0x11, 0xFF, 0xFF, 0x33, 0x22}}),
                                 common::BindOnce([](CommandCompleteView status) {}), handler_);

      hci_layer_->EnqueueCommand(LeExtendedCreateConnectionBuilder::Create(
                                     initiator_filter_policy, own_address_type, address_with_type.GetAddressType(),
                                     address_with_type.GetAddress(), 0x01 /* 1M PHY ONLY */, {tmp}),
                                 common::BindOnce([](CommandStatusView status) {
                                   ASSERT(status.IsValid());
                                   ASSERT(status.GetCommandOpCode() == OpCode::LE_EXTENDED_CREATE_CONNECTION);
                                 }),
                                 handler_);
    } else {
      hci_layer_->EnqueueCommand(
          LeCreateConnectionBuilder::Create(le_scan_interval, le_scan_window, initiator_filter_policy,
                                            address_with_type.GetAddressType(), address_with_type.GetAddress(),
                                            own_address_type, conn_interval_min, conn_interval_max, conn_latency,
                                            supervision_timeout, kMinimumCeLength, kMaximumCeLength),
          common::BindOnce([](CommandStatusView status) {
            ASSERT(status.IsValid());
            ASSERT(status.GetCommandOpCode() == OpCode::LE_CREATE_CONNECTION);
          }),
          handler_);
    }
  }

  void cancel_connect(Address address) {
    auto connecting_addr = connecting_.find(address);
    if (connecting_addr == connecting_.end()) {
      LOG_INFO("Cannot cancel non-existent connection to %s", address.ToString().c_str());
      return;
    }
    std::unique_ptr<CreateConnectionCancelBuilder> packet = CreateConnectionCancelBuilder::Create(address);
    hci_layer_->EnqueueCommand(std::move(packet), common::BindOnce([](CommandCompleteView complete) { /* TODO */ }),
                               handler_);
  }

  void master_link_key(KeyFlag key_flag) {
    std::unique_ptr<MasterLinkKeyBuilder> packet = MasterLinkKeyBuilder::Create(key_flag);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        common::BindOnce(&impl::check_command_status<MasterLinkKeyStatusView>, common::Unretained(this)), handler_);
  }

  void switch_role(Address address, Role role) {
    std::unique_ptr<SwitchRoleBuilder> packet = SwitchRoleBuilder::Create(address, role);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        common::BindOnce(&impl::check_command_status<SwitchRoleStatusView>, common::Unretained(this)), handler_);
  }

  void read_default_link_policy_settings() {
    std::unique_ptr<ReadDefaultLinkPolicySettingsBuilder> packet = ReadDefaultLinkPolicySettingsBuilder::Create();
    hci_layer_->EnqueueCommand(
        std::move(packet),
        common::BindOnce(&impl::on_read_default_link_policy_settings_complete, common::Unretained(this)), handler_);
  }

  void write_default_link_policy_settings(uint16_t default_link_policy_settings) {
    std::unique_ptr<WriteDefaultLinkPolicySettingsBuilder> packet =
        WriteDefaultLinkPolicySettingsBuilder::Create(default_link_policy_settings);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_complete<WriteDefaultLinkPolicySettingsCompleteView>,
                 common::Unretained(this)),
        handler_);
  }

  void accept_connection(Address address) {
    auto role = AcceptConnectionRequestRole::BECOME_MASTER;  // We prefer to be master
    hci_layer_->EnqueueCommand(AcceptConnectionRequestBuilder::Create(address, role),
                               common::BindOnce(&impl::on_accept_connection_status, common::Unretained(this), address),
                               handler_);
  }

  void handle_disconnect(uint16_t handle, DisconnectReason reason) {
    ASSERT(acl_connections_.count(handle) == 1);
    std::unique_ptr<DisconnectBuilder> packet = DisconnectBuilder::Create(handle, reason);
    hci_layer_->EnqueueCommand(std::move(packet), BindOnce([](CommandStatusView status) { /* TODO: check? */ }),
                               handler_);
  }

  void handle_change_connection_packet_type(uint16_t handle, uint16_t packet_type) {
    ASSERT(acl_connections_.count(handle) == 1);
    std::unique_ptr<ChangeConnectionPacketTypeBuilder> packet =
        ChangeConnectionPacketTypeBuilder::Create(handle, packet_type);
    hci_layer_->EnqueueCommand(std::move(packet),
                               BindOnce(&AclManager::impl::check_command_status<ChangeConnectionPacketTypeStatusView>,
                                        common::Unretained(this)),
                               handler_);
  }

  void handle_authentication_requested(uint16_t handle) {
    std::unique_ptr<AuthenticationRequestedBuilder> packet = AuthenticationRequestedBuilder::Create(handle);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_status<AuthenticationRequestedStatusView>, common::Unretained(this)),
        handler_);
  }

  void handle_set_connection_encryption(uint16_t handle, Enable enable) {
    std::unique_ptr<SetConnectionEncryptionBuilder> packet = SetConnectionEncryptionBuilder::Create(handle, enable);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_status<SetConnectionEncryptionStatusView>, common::Unretained(this)),
        handler_);
  }

  void handle_change_connection_link_key(uint16_t handle) {
    std::unique_ptr<ChangeConnectionLinkKeyBuilder> packet = ChangeConnectionLinkKeyBuilder::Create(handle);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_status<ChangeConnectionLinkKeyStatusView>, common::Unretained(this)),
        handler_);
  }

  void handle_read_clock_offset(uint16_t handle) {
    std::unique_ptr<ReadClockOffsetBuilder> packet = ReadClockOffsetBuilder::Create(handle);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_status<ReadClockOffsetStatusView>, common::Unretained(this)),
        handler_);
  }

  void handle_hold_mode(uint16_t handle, uint16_t max_interval, uint16_t min_interval) {
    std::unique_ptr<HoldModeBuilder> packet = HoldModeBuilder::Create(handle, max_interval, min_interval);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_status<HoldModeStatusView>, common::Unretained(this)), handler_);
  }

  void handle_sniff_mode(uint16_t handle, uint16_t max_interval, uint16_t min_interval, int16_t attempt,
                         uint16_t timeout) {
    std::unique_ptr<SniffModeBuilder> packet =
        SniffModeBuilder::Create(handle, max_interval, min_interval, attempt, timeout);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_status<SniffModeStatusView>, common::Unretained(this)), handler_);
  }

  void handle_exit_sniff_mode(uint16_t handle) {
    std::unique_ptr<ExitSniffModeBuilder> packet = ExitSniffModeBuilder::Create(handle);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_status<ExitSniffModeStatusView>, common::Unretained(this)), handler_);
  }

  void handle_qos_setup_mode(uint16_t handle, ServiceType service_type, uint32_t token_rate, uint32_t peak_bandwidth,
                             uint32_t latency, uint32_t delay_variation) {
    std::unique_ptr<QosSetupBuilder> packet =
        QosSetupBuilder::Create(handle, service_type, token_rate, peak_bandwidth, latency, delay_variation);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_status<QosSetupStatusView>, common::Unretained(this)), handler_);
  }

  void handle_role_discovery(uint16_t handle) {
    std::unique_ptr<RoleDiscoveryBuilder> packet = RoleDiscoveryBuilder::Create(handle);
    hci_layer_->EnqueueCommand(std::move(packet),
                               common::BindOnce(&impl::on_role_discovery_complete, common::Unretained(this)), handler_);
  }

  void handle_read_link_policy_settings(uint16_t handle) {
    std::unique_ptr<ReadLinkPolicySettingsBuilder> packet = ReadLinkPolicySettingsBuilder::Create(handle);
    hci_layer_->EnqueueCommand(std::move(packet),
                               common::BindOnce(&impl::on_read_link_policy_settings_complete, common::Unretained(this)),
                               handler_);
  }

  void handle_write_link_policy_settings(uint16_t handle, uint16_t link_policy_settings) {
    std::unique_ptr<WriteLinkPolicySettingsBuilder> packet =
        WriteLinkPolicySettingsBuilder::Create(handle, link_policy_settings);
    hci_layer_->EnqueueCommand(std::move(packet),
                               BindOnce(&AclManager::impl::check_command_complete<WriteLinkPolicySettingsCompleteView>,
                                        common::Unretained(this)),
                               handler_);
  }

  void handle_flow_specification(uint16_t handle, FlowDirection flow_direction, ServiceType service_type,
                                 uint32_t token_rate, uint32_t token_bucket_size, uint32_t peak_bandwidth,
                                 uint32_t access_latency) {
    std::unique_ptr<FlowSpecificationBuilder> packet = FlowSpecificationBuilder::Create(
        handle, flow_direction, service_type, token_rate, token_bucket_size, peak_bandwidth, access_latency);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_status<FlowSpecificationStatusView>, common::Unretained(this)),
        handler_);
  }

  void handle_sniff_subrating(uint16_t handle, uint16_t maximum_latency, uint16_t minimum_remote_timeout,
                              uint16_t minimum_local_timeout) {
    std::unique_ptr<SniffSubratingBuilder> packet =
        SniffSubratingBuilder::Create(handle, maximum_latency, minimum_remote_timeout, minimum_local_timeout);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_complete<SniffSubratingCompleteView>, common::Unretained(this)),
        handler_);
  }

  void handle_flush(uint16_t handle) {
    std::unique_ptr<FlushBuilder> packet = FlushBuilder::Create(handle);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_complete<FlushCompleteView>, common::Unretained(this)), handler_);
  }

  void handle_read_automatic_flush_timeout(uint16_t handle) {
    std::unique_ptr<ReadAutomaticFlushTimeoutBuilder> packet = ReadAutomaticFlushTimeoutBuilder::Create(handle);
    hci_layer_->EnqueueCommand(
        std::move(packet), common::BindOnce(&impl::on_read_automatic_flush_timeout_complete, common::Unretained(this)),
        handler_);
  }

  void handle_write_automatic_flush_timeout(uint16_t handle, uint16_t flush_timeout) {
    std::unique_ptr<WriteAutomaticFlushTimeoutBuilder> packet =
        WriteAutomaticFlushTimeoutBuilder::Create(handle, flush_timeout);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_complete<WriteAutomaticFlushTimeoutCompleteView>,
                 common::Unretained(this)),
        handler_);
  }

  void handle_read_transmit_power_level(uint16_t handle, TransmitPowerLevelType type) {
    std::unique_ptr<ReadTransmitPowerLevelBuilder> packet = ReadTransmitPowerLevelBuilder::Create(handle, type);
    hci_layer_->EnqueueCommand(std::move(packet),
                               common::BindOnce(&impl::on_read_transmit_power_level_complete, common::Unretained(this)),
                               handler_);
  }

  void handle_read_link_supervision_timeout(uint16_t handle) {
    std::unique_ptr<ReadLinkSupervisionTimeoutBuilder> packet = ReadLinkSupervisionTimeoutBuilder::Create(handle);
    hci_layer_->EnqueueCommand(
        std::move(packet), common::BindOnce(&impl::on_read_link_supervision_timeout_complete, common::Unretained(this)),
        handler_);
  }

  void handle_write_link_supervision_timeout(uint16_t handle, uint16_t link_supervision_timeout) {
    std::unique_ptr<WriteLinkSupervisionTimeoutBuilder> packet =
        WriteLinkSupervisionTimeoutBuilder::Create(handle, link_supervision_timeout);
    hci_layer_->EnqueueCommand(
        std::move(packet),
        BindOnce(&AclManager::impl::check_command_complete<WriteLinkSupervisionTimeoutCompleteView>,
                 common::Unretained(this)),
        handler_);
  }

  void handle_read_failed_contact_counter(uint16_t handle) {
    std::unique_ptr<ReadFailedContactCounterBuilder> packet = ReadFailedContactCounterBuilder::Create(handle);
    hci_layer_->EnqueueCommand(
        std::move(packet), common::BindOnce(&impl::on_read_failed_contact_counter_complete, common::Unretained(this)),
        handler_);
  }

  void handle_reset_failed_contact_counter(uint16_t handle) {
    std::unique_ptr<ResetFailedContactCounterBuilder> packet = ResetFailedContactCounterBuilder::Create(handle);
    hci_layer_->EnqueueCommand(std::move(packet), BindOnce([](CommandCompleteView view) { /* TODO: check? */ }),
                               handler_);
  }

  void handle_read_link_quality(uint16_t handle) {
    std::unique_ptr<ReadLinkQualityBuilder> packet = ReadLinkQualityBuilder::Create(handle);
    hci_layer_->EnqueueCommand(
        std::move(packet), common::BindOnce(&impl::on_read_link_quality_complete, common::Unretained(this)), handler_);
  }

  void handle_afh_channel_map(uint16_t handle) {
    std::unique_ptr<ReadAfhChannelMapBuilder> packet = ReadAfhChannelMapBuilder::Create(handle);
    hci_layer_->EnqueueCommand(std::move(packet),
                               common::BindOnce(&impl::on_read_afh_channel_map_complete, common::Unretained(this)),
                               handler_);
  }

  void handle_read_rssi(uint16_t handle) {
    std::unique_ptr<ReadRssiBuilder> packet = ReadRssiBuilder::Create(handle);
    hci_layer_->EnqueueCommand(std::move(packet),
                               common::BindOnce(&impl::on_read_rssi_complete, common::Unretained(this)), handler_);
  }

  void handle_read_remote_version_information(uint16_t handle) {
    hci_layer_->EnqueueCommand(
        ReadRemoteVersionInformationBuilder::Create(handle),
        common::BindOnce(&impl::on_read_remote_version_information_status, common::Unretained(this)), handler_);
  }

  void handle_read_remote_supported_features(uint16_t handle) {
    hci_layer_->EnqueueCommand(
        ReadRemoteSupportedFeaturesBuilder::Create(handle),
        common::BindOnce(&impl::on_read_remote_supported_features_status, common::Unretained(this)), handler_);
  }

  void handle_read_remote_extended_features(uint16_t handle) {
    // TODO(optedoblivion): Read the other pages until max pages
    hci_layer_->EnqueueCommand(
        ReadRemoteExtendedFeaturesBuilder::Create(handle, 1),
        common::BindOnce(&impl::on_read_remote_extended_features_status, common::Unretained(this)), handler_);
  }

  void handle_read_clock(uint16_t handle, WhichClock which_clock) {
    std::unique_ptr<ReadClockBuilder> packet = ReadClockBuilder::Create(handle, which_clock);
    hci_layer_->EnqueueCommand(std::move(packet),
                               common::BindOnce(&impl::on_read_clock_complete, common::Unretained(this)), handler_);
  }

  void handle_le_connection_update(uint16_t handle, uint16_t conn_interval_min, uint16_t conn_interval_max,
                                   uint16_t conn_latency, uint16_t supervision_timeout) {
    auto packet = LeConnectionUpdateBuilder::Create(handle, conn_interval_min, conn_interval_max, conn_latency,
                                                    supervision_timeout, kMinimumCeLength, kMaximumCeLength);
    hci_layer_->EnqueueCommand(std::move(packet), common::BindOnce([](CommandStatusView status) {
                                 ASSERT(status.IsValid());
                                 ASSERT(status.GetCommandOpCode() == OpCode::LE_CREATE_CONNECTION);
                               }),
                               handler_);
  }

  template <class T>
  void check_command_complete(CommandCompleteView view) {
    ASSERT(view.IsValid());
    auto status_view = T::Create(view);
    if (!status_view.IsValid()) {
      LOG_ERROR("Received command complete with invalid packet, opcode 0x%02hx", view.GetCommandOpCode());
      return;
    }
    ErrorCode status = status_view.GetStatus();
    OpCode op_code = status_view.GetCommandOpCode();
    if (status != ErrorCode::SUCCESS) {
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received command complete with error code %s, opcode 0x%02hx", error_code.c_str(), op_code);
      return;
    }
  }

  template <class T>
  void check_command_status(CommandStatusView view) {
    ASSERT(view.IsValid());
    auto status_view = T::Create(view);
    if (!status_view.IsValid()) {
      LOG_ERROR("Received command status with invalid packet, opcode 0x%02hx", view.GetCommandOpCode());
      return;
    }
    ErrorCode status = status_view.GetStatus();
    OpCode op_code = status_view.GetCommandOpCode();
    if (status != ErrorCode::SUCCESS) {
      std::string error_code = ErrorCodeText(status);
      LOG_ERROR("Received command status with error code %s, opcode 0x%02hx", error_code.c_str(), op_code);
      return;
    }
  }

  void cleanup(uint16_t handle) {
    ASSERT(acl_connections_.count(handle) == 1);
    auto& acl_connection = acl_connections_.find(handle)->second;
    if (acl_connection.is_registered_) {
      acl_connection.is_registered_ = false;
      acl_connection.queue_->GetDownEnd()->UnregisterDequeue();
    }
    acl_connections_.erase(handle);
  }

  void on_accept_connection_status(Address address, CommandStatusView status) {
    auto accept_status = AcceptConnectionRequestStatusView::Create(status);
    ASSERT(accept_status.IsValid());
    if (status.GetStatus() != ErrorCode::SUCCESS) {
      cancel_connect(address);
    }
  }

  void reject_connection(std::unique_ptr<RejectConnectionRequestBuilder> builder) {
    hci_layer_->EnqueueCommand(std::move(builder), BindOnce([](CommandStatusView status) { /* TODO: check? */ }),
                               handler_);
  }

  void handle_register_callbacks(ConnectionCallbacks* callbacks, os::Handler* handler) {
    ASSERT(client_callbacks_ == nullptr);
    ASSERT(client_handler_ == nullptr);
    client_callbacks_ = callbacks;
    client_handler_ = handler;
  }

  void handle_register_le_callbacks(LeConnectionCallbacks* callbacks, os::Handler* handler) {
    ASSERT(le_client_callbacks_ == nullptr);
    ASSERT(le_client_handler_ == nullptr);
    le_client_callbacks_ = callbacks;
    le_client_handler_ = handler;
  }

  void handle_register_acl_manager_callbacks(AclManagerCallbacks* callbacks, os::Handler* handler) {
    ASSERT(acl_manager_client_callbacks_ == nullptr);
    ASSERT(acl_manager_client_handler_ == nullptr);
    acl_manager_client_callbacks_ = callbacks;
    acl_manager_client_handler_ = handler;
  }

  void handle_register_le_acl_manager_callbacks(AclManagerCallbacks* callbacks, os::Handler* handler) {
    ASSERT(le_acl_manager_client_callbacks_ == nullptr);
    ASSERT(le_acl_manager_client_handler_ == nullptr);
    le_acl_manager_client_callbacks_ = callbacks;
    le_acl_manager_client_handler_ = handler;
  }

  acl_connection& check_and_get_connection(uint16_t handle) {
    auto connection = acl_connections_.find(handle);
    ASSERT(connection != acl_connections_.end());
    return connection->second;
  }

  AclConnection::QueueUpEnd* get_acl_queue_end(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    return connection.queue_->GetUpEnd();
  }

  void RegisterCallbacks(uint16_t handle, ConnectionManagementCallbacks* callbacks, os::Handler* handler) {
    auto& connection = check_and_get_connection(handle);
    ASSERT(connection.command_complete_callbacks_ == nullptr);
    connection.command_complete_callbacks_ = callbacks;
    connection.command_complete_handler_ = handler;
  }

  void UnregisterCallbacks(uint16_t handle, ConnectionManagementCallbacks* callbacks) {
    auto& connection = check_and_get_connection(handle);
    ASSERT(connection.command_complete_callbacks_ == callbacks);
    connection.command_complete_callbacks_ = nullptr;
  }

  void RegisterDisconnectCallback(uint16_t handle, common::OnceCallback<void(ErrorCode)> on_disconnect,
                                  os::Handler* handler) {
    auto& connection = check_and_get_connection(handle);
    connection.on_disconnect_callback_ = std::move(on_disconnect);
    connection.disconnect_handler_ = handler;
    if (connection.is_disconnected_) {
      connection.call_disconnect_callback();
    }
  }

  bool Disconnect(uint16_t handle, DisconnectReason reason) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_disconnect, common::Unretained(this), handle, reason));
    return true;
  }

  bool ChangeConnectionPacketType(uint16_t handle, uint16_t packet_type) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(
        BindOnce(&impl::handle_change_connection_packet_type, common::Unretained(this), handle, packet_type));
    return true;
  }

  bool AuthenticationRequested(uint16_t handle) {
    LOG_INFO("Auth reqiuest");
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_authentication_requested, common::Unretained(this), handle));
    return true;
  }

  bool SetConnectionEncryption(uint16_t handle, Enable enable) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_set_connection_encryption, common::Unretained(this), handle, enable));
    return true;
  }

  bool ChangeConnectionLinkKey(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_change_connection_link_key, common::Unretained(this), handle));
    return true;
  }

  bool ReadClockOffset(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_clock_offset, common::Unretained(this), handle));
    return true;
  }

  bool HoldMode(uint16_t handle, uint16_t max_interval, uint16_t min_interval) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_hold_mode, common::Unretained(this), handle, max_interval, min_interval));
    return true;
  }

  bool SniffMode(uint16_t handle, uint16_t max_interval, uint16_t min_interval, int16_t attempt, uint16_t timeout) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_sniff_mode, common::Unretained(this), handle, max_interval, min_interval,
                            attempt, timeout));
    return true;
  }

  bool ExitSniffMode(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_exit_sniff_mode, common::Unretained(this), handle));
    return true;
  }

  bool QosSetup(uint16_t handle, ServiceType service_type, uint32_t token_rate, uint32_t peak_bandwidth,
                uint32_t latency, uint32_t delay_variation) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_qos_setup_mode, common::Unretained(this), handle, service_type, token_rate,
                            peak_bandwidth, latency, delay_variation));
    return true;
  }

  bool RoleDiscovery(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_role_discovery, common::Unretained(this), handle));
    return true;
  }

  bool ReadLinkPolicySettings(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_link_policy_settings, common::Unretained(this), handle));
    return true;
  }

  bool WriteLinkPolicySettings(uint16_t handle, uint16_t link_policy_settings) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(
        BindOnce(&impl::handle_write_link_policy_settings, common::Unretained(this), handle, link_policy_settings));
    return true;
  }

  bool FlowSpecification(uint16_t handle, FlowDirection flow_direction, ServiceType service_type, uint32_t token_rate,
                         uint32_t token_bucket_size, uint32_t peak_bandwidth, uint32_t access_latency) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_flow_specification, common::Unretained(this), handle, flow_direction,
                            service_type, token_rate, token_bucket_size, peak_bandwidth, access_latency));
    return true;
  }

  bool SniffSubrating(uint16_t handle, uint16_t maximum_latency, uint16_t minimum_remote_timeout,
                      uint16_t minimum_local_timeout) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_sniff_subrating, common::Unretained(this), handle, maximum_latency,
                            minimum_remote_timeout, minimum_local_timeout));
    return true;
  }

  bool Flush(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_flush, common::Unretained(this), handle));
    return true;
  }

  bool ReadAutomaticFlushTimeout(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_automatic_flush_timeout, common::Unretained(this), handle));
    return true;
  }

  bool WriteAutomaticFlushTimeout(uint16_t handle, uint16_t flush_timeout) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(
        BindOnce(&impl::handle_write_automatic_flush_timeout, common::Unretained(this), handle, flush_timeout));
    return true;
  }

  bool ReadTransmitPowerLevel(uint16_t handle, TransmitPowerLevelType type) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_transmit_power_level, common::Unretained(this), handle, type));
    return true;
  }

  bool ReadLinkSupervisionTimeout(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_link_supervision_timeout, common::Unretained(this), handle));
    return true;
  }

  bool WriteLinkSupervisionTimeout(uint16_t handle, uint16_t link_supervision_timeout) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_write_link_supervision_timeout, common::Unretained(this), handle,
                            link_supervision_timeout));
    return true;
  }

  bool ReadFailedContactCounter(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_failed_contact_counter, common::Unretained(this), handle));
    return true;
  }

  bool ResetFailedContactCounter(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_reset_failed_contact_counter, common::Unretained(this), handle));
    return true;
  }

  bool ReadLinkQuality(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_link_quality, common::Unretained(this), handle));
    return true;
  }

  bool ReadAfhChannelMap(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_afh_channel_map, common::Unretained(this), handle));
    return true;
  }

  bool ReadRssi(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_rssi, common::Unretained(this), handle));
    return true;
  }

  bool ReadRemoteVersionInformation(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_remote_version_information, common::Unretained(this), handle));
    return true;
  }

  bool ReadRemoteSupportedFeatures(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_remote_supported_features, common::Unretained(this), handle));
    return true;
  }

  bool ReadRemoteExtendedFeatures(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_remote_extended_features, common::Unretained(this), handle));
    return true;
  }

  bool ReadClock(uint16_t handle, WhichClock which_clock) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_read_clock, common::Unretained(this), handle, which_clock));
    return true;
  }

  bool LeConnectionUpdate(uint16_t handle, uint16_t conn_interval_min, uint16_t conn_interval_max,
                          uint16_t conn_latency, uint16_t supervision_timeout,
                          common::OnceCallback<void(ErrorCode)> done_callback, os::Handler* handler) {
    auto& connection = check_and_get_connection(handle);
    if (connection.is_disconnected_) {
      LOG_INFO("Already disconnected");
      return false;
    }
    if (!connection.on_connection_update_complete_callback_.is_null()) {
      LOG_INFO("There is another pending connection update");
      return false;
    }
    connection.on_connection_update_complete_callback_ = std::move(done_callback);
    connection.on_connection_update_complete_callback_handler_ = handler;
    if (conn_interval_min < 0x0006 || conn_interval_min > 0x0C80 || conn_interval_max < 0x0006 ||
        conn_interval_max > 0x0C80 || conn_latency > 0x01F3 || supervision_timeout < 0x000A ||
        supervision_timeout > 0x0C80) {
      LOG_ERROR("Invalid parameter");
      return false;
    }
    handler_->Post(BindOnce(&impl::handle_le_connection_update, common::Unretained(this), handle, conn_interval_min,
                            conn_interval_max, conn_latency, supervision_timeout));
    return true;
  }

  void Finish(uint16_t handle) {
    auto& connection = check_and_get_connection(handle);
    ASSERT_LOG(connection.is_disconnected_, "Finish must be invoked after disconnection (handle 0x%04hx)", handle);
    handler_->Post(BindOnce(&impl::cleanup, common::Unretained(this), handle));
  }

  const AclManager& acl_manager_;

  static constexpr uint16_t kMinimumCeLength = 0x0002;
  static constexpr uint16_t kMaximumCeLength = 0x0C00;

  Controller* controller_ = nullptr;
  uint16_t max_acl_packet_credits_ = 0;
  uint16_t acl_packet_credits_ = 0;
  uint16_t acl_buffer_length_ = 0;

  std::list<std::unique_ptr<AclPacketBuilder>> fragments_to_send_;
  std::map<uint16_t, acl_connection>::iterator current_connection_pair_;

  HciLayer* hci_layer_ = nullptr;
  os::Handler* handler_ = nullptr;
  ConnectionCallbacks* client_callbacks_ = nullptr;
  os::Handler* client_handler_ = nullptr;
  LeConnectionCallbacks* le_client_callbacks_ = nullptr;
  os::Handler* le_client_handler_ = nullptr;
  AclManagerCallbacks* acl_manager_client_callbacks_ = nullptr;
  os::Handler* acl_manager_client_handler_ = nullptr;
  AclManagerCallbacks* le_acl_manager_client_callbacks_ = nullptr;
  os::Handler* le_acl_manager_client_handler_ = nullptr;
  common::BidiQueueEnd<AclPacketBuilder, AclPacketView>* hci_queue_end_ = nullptr;
  std::map<uint16_t, AclManager::acl_connection> acl_connections_;
  std::set<Address> connecting_;
  std::set<AddressWithType> connecting_le_;
  common::Callback<bool(Address, ClassOfDevice)> should_accept_connection_;
  std::queue<std::pair<Address, std::unique_ptr<CreateConnectionBuilder>>> pending_outgoing_connections_;
  size_t hci_mtu_{0};
};

AclConnection::QueueUpEnd* AclConnection::GetAclQueueEnd() const {
  return manager_->pimpl_->get_acl_queue_end(handle_);
}

void AclConnection::RegisterCallbacks(ConnectionManagementCallbacks* callbacks, os::Handler* handler) {
  return manager_->pimpl_->RegisterCallbacks(handle_, callbacks, handler);
}

void AclConnection::UnregisterCallbacks(ConnectionManagementCallbacks* callbacks) {
  return manager_->pimpl_->UnregisterCallbacks(handle_, callbacks);
}

void AclConnection::RegisterDisconnectCallback(common::OnceCallback<void(ErrorCode)> on_disconnect,
                                               os::Handler* handler) {
  return manager_->pimpl_->RegisterDisconnectCallback(handle_, std::move(on_disconnect), handler);
}

bool AclConnection::Disconnect(DisconnectReason reason) {
  return manager_->pimpl_->Disconnect(handle_, reason);
}

bool AclConnection::ChangeConnectionPacketType(uint16_t packet_type) {
  return manager_->pimpl_->ChangeConnectionPacketType(handle_, packet_type);
}

bool AclConnection::AuthenticationRequested() {
  return manager_->pimpl_->AuthenticationRequested(handle_);
}

bool AclConnection::SetConnectionEncryption(Enable enable) {
  return manager_->pimpl_->SetConnectionEncryption(handle_, enable);
}

bool AclConnection::ChangeConnectionLinkKey() {
  return manager_->pimpl_->ChangeConnectionLinkKey(handle_);
}

bool AclConnection::ReadClockOffset() {
  return manager_->pimpl_->ReadClockOffset(handle_);
}

bool AclConnection::HoldMode(uint16_t max_interval, uint16_t min_interval) {
  return manager_->pimpl_->HoldMode(handle_, max_interval, min_interval);
}

bool AclConnection::SniffMode(uint16_t max_interval, uint16_t min_interval, uint16_t attempt, uint16_t timeout) {
  return manager_->pimpl_->SniffMode(handle_, max_interval, min_interval, attempt, timeout);
}

bool AclConnection::ExitSniffMode() {
  return manager_->pimpl_->ExitSniffMode(handle_);
}

bool AclConnection::QosSetup(ServiceType service_type, uint32_t token_rate, uint32_t peak_bandwidth, uint32_t latency,
                             uint32_t delay_variation) {
  return manager_->pimpl_->QosSetup(handle_, service_type, token_rate, peak_bandwidth, latency, delay_variation);
}

bool AclConnection::RoleDiscovery() {
  return manager_->pimpl_->RoleDiscovery(handle_);
}

bool AclConnection::ReadLinkPolicySettings() {
  return manager_->pimpl_->ReadLinkPolicySettings(handle_);
}

bool AclConnection::WriteLinkPolicySettings(uint16_t link_policy_settings) {
  return manager_->pimpl_->WriteLinkPolicySettings(handle_, link_policy_settings);
}

bool AclConnection::FlowSpecification(FlowDirection flow_direction, ServiceType service_type, uint32_t token_rate,
                                      uint32_t token_bucket_size, uint32_t peak_bandwidth, uint32_t access_latency) {
  return manager_->pimpl_->FlowSpecification(handle_, flow_direction, service_type, token_rate, token_bucket_size,
                                             peak_bandwidth, access_latency);
}

bool AclConnection::SniffSubrating(uint16_t maximum_latency, uint16_t minimum_remote_timeout,
                                   uint16_t minimum_local_timeout) {
  return manager_->pimpl_->SniffSubrating(handle_, maximum_latency, minimum_remote_timeout, minimum_local_timeout);
}

bool AclConnection::Flush() {
  return manager_->pimpl_->Flush(handle_);
}

bool AclConnection::ReadAutomaticFlushTimeout() {
  return manager_->pimpl_->ReadAutomaticFlushTimeout(handle_);
}

bool AclConnection::WriteAutomaticFlushTimeout(uint16_t flush_timeout) {
  return manager_->pimpl_->WriteAutomaticFlushTimeout(handle_, flush_timeout);
}

bool AclConnection::ReadTransmitPowerLevel(TransmitPowerLevelType type) {
  return manager_->pimpl_->ReadTransmitPowerLevel(handle_, type);
}

bool AclConnection::ReadLinkSupervisionTimeout() {
  return manager_->pimpl_->ReadLinkSupervisionTimeout(handle_);
}

bool AclConnection::WriteLinkSupervisionTimeout(uint16_t link_supervision_timeout) {
  return manager_->pimpl_->WriteLinkSupervisionTimeout(handle_, link_supervision_timeout);
}

bool AclConnection::ReadFailedContactCounter() {
  return manager_->pimpl_->ReadFailedContactCounter(handle_);
}

bool AclConnection::ResetFailedContactCounter() {
  return manager_->pimpl_->ResetFailedContactCounter(handle_);
}

bool AclConnection::ReadLinkQuality() {
  return manager_->pimpl_->ReadLinkQuality(handle_);
}

bool AclConnection::ReadAfhChannelMap() {
  return manager_->pimpl_->ReadAfhChannelMap(handle_);
}

bool AclConnection::ReadRssi() {
  return manager_->pimpl_->ReadRssi(handle_);
}

bool AclConnection::ReadRemoteVersionInformation() {
  return manager_->pimpl_->ReadRemoteVersionInformation(handle_);
}

bool AclConnection::ReadRemoteSupportedFeatures() {
  return manager_->pimpl_->ReadRemoteSupportedFeatures(handle_);
}

bool AclConnection::ReadRemoteExtendedFeatures() {
  return manager_->pimpl_->ReadRemoteExtendedFeatures(handle_);
}

bool AclConnection::ReadClock(WhichClock which_clock) {
  return manager_->pimpl_->ReadClock(handle_, which_clock);
}

bool AclConnection::LeConnectionUpdate(uint16_t conn_interval_min, uint16_t conn_interval_max, uint16_t conn_latency,
                                       uint16_t supervision_timeout,
                                       common::OnceCallback<void(ErrorCode)> done_callback, os::Handler* handler) {
  return manager_->pimpl_->LeConnectionUpdate(handle_, conn_interval_min, conn_interval_max, conn_latency,
                                              supervision_timeout, std::move(done_callback), handler);
}

void AclConnection::Finish() {
  return manager_->pimpl_->Finish(handle_);
}

AclManager::AclManager() : pimpl_(std::make_unique<impl>(*this)) {}

void AclManager::RegisterCallbacks(ConnectionCallbacks* callbacks, os::Handler* handler) {
  ASSERT(callbacks != nullptr && handler != nullptr);
  GetHandler()->Post(common::BindOnce(&impl::handle_register_callbacks, common::Unretained(pimpl_.get()),
                                      common::Unretained(callbacks), common::Unretained(handler)));
}

void AclManager::RegisterLeCallbacks(LeConnectionCallbacks* callbacks, os::Handler* handler) {
  ASSERT(callbacks != nullptr && handler != nullptr);
  GetHandler()->Post(common::BindOnce(&impl::handle_register_le_callbacks, common::Unretained(pimpl_.get()),
                                      common::Unretained(callbacks), common::Unretained(handler)));
}

void AclManager::RegisterAclManagerCallbacks(AclManagerCallbacks* callbacks, os::Handler* handler) {
  ASSERT(callbacks != nullptr && handler != nullptr);
  GetHandler()->Post(common::BindOnce(&impl::handle_register_acl_manager_callbacks, common::Unretained(pimpl_.get()),
                                      common::Unretained(callbacks), common::Unretained(handler)));
}

void AclManager::RegisterLeAclManagerCallbacks(AclManagerCallbacks* callbacks, os::Handler* handler) {
  ASSERT(callbacks != nullptr && handler != nullptr);
  GetHandler()->Post(common::BindOnce(&impl::handle_register_le_acl_manager_callbacks, common::Unretained(pimpl_.get()),
                                      common::Unretained(callbacks), common::Unretained(handler)));
}

void AclManager::CreateConnection(Address address) {
  GetHandler()->Post(common::BindOnce(&impl::create_connection, common::Unretained(pimpl_.get()), address));
}

void AclManager::CreateLeConnection(AddressWithType address_with_type) {
  GetHandler()->Post(
      common::BindOnce(&impl::create_le_connection, common::Unretained(pimpl_.get()), address_with_type));
}

void AclManager::CancelConnect(Address address) {
  GetHandler()->Post(BindOnce(&impl::cancel_connect, common::Unretained(pimpl_.get()), address));
}

void AclManager::MasterLinkKey(KeyFlag key_flag) {
  GetHandler()->Post(BindOnce(&impl::master_link_key, common::Unretained(pimpl_.get()), key_flag));
}

void AclManager::SwitchRole(Address address, Role role) {
  GetHandler()->Post(BindOnce(&impl::switch_role, common::Unretained(pimpl_.get()), address, role));
}

void AclManager::ReadDefaultLinkPolicySettings() {
  GetHandler()->Post(BindOnce(&impl::read_default_link_policy_settings, common::Unretained(pimpl_.get())));
}

void AclManager::WriteDefaultLinkPolicySettings(uint16_t default_link_policy_settings) {
  GetHandler()->Post(BindOnce(&impl::write_default_link_policy_settings, common::Unretained(pimpl_.get()),
                              default_link_policy_settings));
}

void AclManager::ListDependencies(ModuleList* list) {
  list->add<HciLayer>();
  list->add<Controller>();
}

void AclManager::Start() {
  pimpl_->Start();
}

void AclManager::Stop() {
  pimpl_->Stop();
}

std::string AclManager::ToString() const {
  return "Acl Manager";
}

const ModuleFactory AclManager::Factory = ModuleFactory([]() { return new AclManager(); });

AclManager::~AclManager() = default;

}  // namespace hci
}  // namespace bluetooth
