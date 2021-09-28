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

#include "hal/hci_hal_host_rootcanal.h"
#include "hal/hci_hal.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <csignal>
#include <mutex>
#include <queue>

#include "hal/snoop_logger.h"
#include "os/log.h"
#include "os/reactor.h"
#include "os/thread.h"

namespace {
constexpr int INVALID_FD = -1;

constexpr uint8_t kH4Command = 0x01;
constexpr uint8_t kH4Acl = 0x02;
constexpr uint8_t kH4Sco = 0x03;
constexpr uint8_t kH4Event = 0x04;

constexpr uint8_t kH4HeaderSize = 1;
constexpr uint8_t kHciAclHeaderSize = 4;
constexpr uint8_t kHciScoHeaderSize = 3;
constexpr uint8_t kHciEvtHeaderSize = 2;
constexpr int kBufSize = 1024 + 4 + 1;  // DeviceProperties::acl_data_packet_size_ + ACL header + H4 header

int ConnectToRootCanal(const std::string& server, int port) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 1) {
    LOG_ERROR("can't create socket: %s", strerror(errno));
    return INVALID_FD;
  }

  struct hostent* host;
  host = gethostbyname(server.c_str());
  if (host == nullptr) {
    LOG_ERROR("can't get server name");
    return INVALID_FD;
  }

  struct sockaddr_in serv_addr;
  memset((void*)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  int result = connect(socket_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (result < 0) {
    LOG_ERROR("can't connect: %s", strerror(errno));
    return INVALID_FD;
  }

  timeval socket_timeout{
      .tv_sec = 3,
      .tv_usec = 0,
  };
  int ret = setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));
  if (ret == -1) {
    LOG_ERROR("can't control socket fd: %s", strerror(errno));
    return INVALID_FD;
  }
  return socket_fd;
}
}  // namespace

namespace bluetooth {
namespace hal {

const std::string SnoopLogger::DefaultFilePath = "/tmp/btsnoop_hci.log";
const bool SnoopLogger::AlwaysFlush = true;

class HciHalHostRootcanal : public HciHal {
 public:
  void registerIncomingPacketCallback(HciHalCallbacks* callback) override {
    std::lock_guard<std::mutex> lock(api_mutex_);
    LOG_INFO("%s before", __func__);
    {
      std::lock_guard<std::mutex> incoming_packet_callback_lock(incoming_packet_callback_mutex_);
      ASSERT(incoming_packet_callback_ == nullptr && callback != nullptr);
      incoming_packet_callback_ = callback;
    }
    LOG_INFO("%s after", __func__);
  }

  void unregisterIncomingPacketCallback() override {
    std::lock_guard<std::mutex> lock(api_mutex_);
    LOG_INFO("%s before", __func__);
    {
      std::lock_guard<std::mutex> incoming_packet_callback_lock(incoming_packet_callback_mutex_);
      incoming_packet_callback_ = nullptr;
    }
    LOG_INFO("%s after", __func__);
  }

  void sendHciCommand(HciPacket command) override {
    std::lock_guard<std::mutex> lock(api_mutex_);
    ASSERT(sock_fd_ != INVALID_FD);
    std::vector<uint8_t> packet = std::move(command);
    btsnoop_logger_->capture(packet, SnoopLogger::Direction::OUTGOING, SnoopLogger::PacketType::CMD);
    packet.insert(packet.cbegin(), kH4Command);
    write_to_rootcanal_fd(packet);
  }

  void sendAclData(HciPacket data) override {
    std::lock_guard<std::mutex> lock(api_mutex_);
    ASSERT(sock_fd_ != INVALID_FD);
    std::vector<uint8_t> packet = std::move(data);
    btsnoop_logger_->capture(packet, SnoopLogger::Direction::OUTGOING, SnoopLogger::PacketType::ACL);
    packet.insert(packet.cbegin(), kH4Acl);
    write_to_rootcanal_fd(packet);
  }

  void sendScoData(HciPacket data) override {
    std::lock_guard<std::mutex> lock(api_mutex_);
    ASSERT(sock_fd_ != INVALID_FD);
    std::vector<uint8_t> packet = std::move(data);
    btsnoop_logger_->capture(packet, SnoopLogger::Direction::OUTGOING, SnoopLogger::PacketType::SCO);
    packet.insert(packet.cbegin(), kH4Sco);
    write_to_rootcanal_fd(packet);
  }

 protected:
  void ListDependencies(ModuleList* list) override {
    list->add<SnoopLogger>();
  }

  void Start() override {
    std::lock_guard<std::mutex> lock(api_mutex_);
    ASSERT(sock_fd_ == INVALID_FD);
    sock_fd_ = ConnectToRootCanal(config_->GetServerAddress(), config_->GetPort());
    ASSERT(sock_fd_ != INVALID_FD);
    reactable_ = hci_incoming_thread_.GetReactor()->Register(
        sock_fd_, common::Bind(&HciHalHostRootcanal::incoming_packet_received, common::Unretained(this)),
        common::Closure());
    btsnoop_logger_ = GetDependency<SnoopLogger>();
    LOG_INFO("Rootcanal HAL opened successfully");
  }

  void Stop() override {
    std::lock_guard<std::mutex> lock(api_mutex_);
    LOG_INFO("Rootcanal HAL is closing");
    if (reactable_ != nullptr) {
      hci_incoming_thread_.GetReactor()->Unregister(reactable_);
      LOG_INFO("Rootcanal HAL is stopping, start waiting for last callback");
      // Wait up to 1 second for the last incoming packet callback to finish
      hci_incoming_thread_.GetReactor()->WaitForUnregisteredReactable(std::chrono::milliseconds(1000));
      LOG_INFO("Rootcanal HAL is stopping, finished waiting for last callback");
      ASSERT(sock_fd_ != INVALID_FD);
    }
    reactable_ = nullptr;
    {
      std::lock_guard<std::mutex> incoming_packet_callback_lock(incoming_packet_callback_mutex_);
      incoming_packet_callback_ = nullptr;
    }
    ::close(sock_fd_);
    sock_fd_ = INVALID_FD;
    LOG_INFO("Rootcanal HAL is closed");
  }

 private:
  // Held when APIs are called, NOT to be held during callbacks
  std::mutex api_mutex_;
  HciHalHostRootcanalConfig* config_ = HciHalHostRootcanalConfig::Get();
  HciHalCallbacks* incoming_packet_callback_ = nullptr;
  std::mutex incoming_packet_callback_mutex_;
  int sock_fd_ = INVALID_FD;
  bluetooth::os::Thread hci_incoming_thread_ =
      bluetooth::os::Thread("hci_incoming_thread", bluetooth::os::Thread::Priority::NORMAL);
  bluetooth::os::Reactor::Reactable* reactable_ = nullptr;
  std::queue<std::vector<uint8_t>> hci_outgoing_queue_;
  SnoopLogger* btsnoop_logger_ = nullptr;

  void write_to_rootcanal_fd(HciPacket packet) {
    // TODO: replace this with new queue when it's ready
    hci_outgoing_queue_.emplace(packet);
    if (hci_outgoing_queue_.size() == 1) {
      hci_incoming_thread_.GetReactor()->ModifyRegistration(
          reactable_, common::Bind(&HciHalHostRootcanal::incoming_packet_received, common::Unretained(this)),
          common::Bind(&HciHalHostRootcanal::send_packet_ready, common::Unretained(this)));
    }
  }

  void send_packet_ready() {
    std::lock_guard<std::mutex> lock(this->api_mutex_);
    auto packet_to_send = this->hci_outgoing_queue_.front();
    auto bytes_written = write(this->sock_fd_, (void*)packet_to_send.data(), packet_to_send.size());
    this->hci_outgoing_queue_.pop();
    if (bytes_written == -1) {
      abort();
    }
    if (hci_outgoing_queue_.empty()) {
      this->hci_incoming_thread_.GetReactor()->ModifyRegistration(
          this->reactable_, common::Bind(&HciHalHostRootcanal::incoming_packet_received, common::Unretained(this)),
          common::Closure());
    }
  }

  void incoming_packet_received() {
    {
      std::lock_guard<std::mutex> incoming_packet_callback_lock(incoming_packet_callback_mutex_);
      if (incoming_packet_callback_ == nullptr) {
        LOG_INFO("Dropping a packet");
        return;
      }
    }
    uint8_t buf[kBufSize] = {};

    ssize_t received_size;
    RUN_NO_INTR(received_size = recv(sock_fd_, buf, kH4HeaderSize, 0));
    ASSERT_LOG(received_size != -1, "Can't receive from socket: %s", strerror(errno));
    if (received_size == 0) {
      LOG_WARN("Can't read H4 header. EOF received");
      raise(SIGINT);
      return;
    }

    if (buf[0] == kH4Event) {
      RUN_NO_INTR(received_size = recv(sock_fd_, buf + kH4HeaderSize, kHciEvtHeaderSize, 0));
      ASSERT_LOG(received_size != -1, "Can't receive from socket: %s", strerror(errno));
      ASSERT_LOG(received_size == kHciEvtHeaderSize, "malformed HCI event header received");

      uint8_t hci_evt_parameter_total_length = buf[2];
      ssize_t payload_size;
      RUN_NO_INTR(payload_size =
                      recv(sock_fd_, buf + kH4HeaderSize + kHciEvtHeaderSize, hci_evt_parameter_total_length, 0));
      ASSERT_LOG(payload_size != -1, "Can't receive from socket: %s", strerror(errno));
      ASSERT_LOG(payload_size == hci_evt_parameter_total_length,
                 "malformed HCI event total parameter size received: %zu != %d", payload_size,
                 hci_evt_parameter_total_length);

      HciPacket receivedHciPacket;
      receivedHciPacket.assign(buf + kH4HeaderSize, buf + kH4HeaderSize + kHciEvtHeaderSize + payload_size);
      btsnoop_logger_->capture(receivedHciPacket, SnoopLogger::Direction::INCOMING, SnoopLogger::PacketType::EVT);
      {
        std::lock_guard<std::mutex> incoming_packet_callback_lock(incoming_packet_callback_mutex_);
        if (incoming_packet_callback_ == nullptr) {
          LOG_INFO("Dropping an event after processing");
          return;
        }
        incoming_packet_callback_->hciEventReceived(receivedHciPacket);
      }
    }

    if (buf[0] == kH4Acl) {
      RUN_NO_INTR(received_size = recv(sock_fd_, buf + kH4HeaderSize, kHciAclHeaderSize, 0));
      ASSERT_LOG(received_size != -1, "Can't receive from socket: %s", strerror(errno));
      ASSERT_LOG(received_size == kHciAclHeaderSize, "malformed ACL header received");

      uint16_t hci_acl_data_total_length = (buf[4] << 8) + buf[3];
      int payload_size;
      RUN_NO_INTR(payload_size = recv(sock_fd_, buf + kH4HeaderSize + kHciAclHeaderSize, hci_acl_data_total_length, 0));
      ASSERT_LOG(payload_size != -1, "Can't receive from socket: %s", strerror(errno));
      ASSERT_LOG(payload_size == hci_acl_data_total_length, "malformed ACL length received: %d != %d", payload_size,
                 hci_acl_data_total_length);
      ASSERT_LOG(hci_acl_data_total_length <= kBufSize - kH4HeaderSize - kHciAclHeaderSize, "packet too long");

      HciPacket receivedHciPacket;
      receivedHciPacket.assign(buf + kH4HeaderSize, buf + kH4HeaderSize + kHciAclHeaderSize + payload_size);
      btsnoop_logger_->capture(receivedHciPacket, SnoopLogger::Direction::INCOMING, SnoopLogger::PacketType::ACL);
      {
        std::lock_guard<std::mutex> incoming_packet_callback_lock(incoming_packet_callback_mutex_);
        if (incoming_packet_callback_ == nullptr) {
          LOG_INFO("Dropping an ACL packet after processing");
          return;
        }
        incoming_packet_callback_->aclDataReceived(receivedHciPacket);
      }
    }

    if (buf[0] == kH4Sco) {
      RUN_NO_INTR(received_size = recv(sock_fd_, buf + kH4HeaderSize, kHciScoHeaderSize, 0));
      ASSERT_LOG(received_size != -1, "Can't receive from socket: %s", strerror(errno));
      ASSERT_LOG(received_size == kHciScoHeaderSize, "malformed SCO header received");

      uint8_t hci_sco_data_total_length = buf[3];
      int payload_size;
      RUN_NO_INTR(payload_size = recv(sock_fd_, buf + kH4HeaderSize + kHciScoHeaderSize, hci_sco_data_total_length, 0));
      ASSERT_LOG(payload_size != -1, "Can't receive from socket: %s", strerror(errno));
      ASSERT_LOG(payload_size == hci_sco_data_total_length, "malformed SCO packet received: size mismatch");

      HciPacket receivedHciPacket;
      receivedHciPacket.assign(buf + kH4HeaderSize, buf + kH4HeaderSize + kHciScoHeaderSize + payload_size);
      btsnoop_logger_->capture(receivedHciPacket, SnoopLogger::Direction::INCOMING, SnoopLogger::PacketType::SCO);
      {
        std::lock_guard<std::mutex> incoming_packet_callback_lock(incoming_packet_callback_mutex_);
        if (incoming_packet_callback_ == nullptr) {
          LOG_INFO("Dropping a SCO packet after processing");
          return;
        }
        incoming_packet_callback_->scoDataReceived(receivedHciPacket);
      }
    }
    memset(buf, 0, kBufSize);
  }
};

const ModuleFactory HciHal::Factory = ModuleFactory([]() { return new HciHalHostRootcanal(); });

}  // namespace hal
}  // namespace bluetooth
