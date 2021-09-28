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
#include "hal/serialize_packet.h"

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "os/log.h"
#include "os/thread.h"
#include "os/utils.h"
#include "packet/raw_builder.h"

using ::bluetooth::os::Thread;

namespace bluetooth {
namespace hal {
namespace {

uint16_t kTestPort = 6537;

constexpr uint8_t kH4Command = 0x01;
constexpr uint8_t kH4Acl = 0x02;
constexpr uint8_t kH4Sco = 0x03;
constexpr uint8_t kH4Event = 0x04;

using H4Packet = std::vector<uint8_t>;

std::queue<std::pair<uint8_t, HciPacket>> incoming_packets_queue_;

class TestHciHalCallbacks : public HciHalCallbacks {
 public:
  void hciEventReceived(HciPacket packet) override {
    incoming_packets_queue_.emplace(kH4Event, packet);
  }

  void aclDataReceived(HciPacket packet) override {
    incoming_packets_queue_.emplace(kH4Acl, packet);
  }

  void scoDataReceived(HciPacket packet) override {
    incoming_packets_queue_.emplace(kH4Sco, packet);
  }
};

// An implementation of rootcanal desktop HCI server which listens on localhost:kListeningPort
class FakeRootcanalDesktopHciServer {
 public:
  FakeRootcanalDesktopHciServer() {
    struct sockaddr_in listen_address;
    socklen_t sockaddr_in_size = sizeof(struct sockaddr_in);
    memset(&listen_address, 0, sockaddr_in_size);

    RUN_NO_INTR(listen_fd_ = socket(AF_INET, SOCK_STREAM, 0));
    if (listen_fd_ < 0) {
      LOG_WARN("Error creating socket for test channel.");
      return;
    }

    listen_address.sin_family = AF_INET;
    listen_address.sin_port = htons(HciHalHostRootcanalConfig::Get()->GetPort());
    listen_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&listen_address), sockaddr_in_size) < 0) {
      LOG_WARN("Error binding test channel listener socket to address.");
      close(listen_fd_);
      return;
    }

    if (listen(listen_fd_, 1) < 0) {
      LOG_WARN("Error listening for test channel.");
      close(listen_fd_);
      return;
    }
  }

  ~FakeRootcanalDesktopHciServer() {
    close(listen_fd_);
  }

  int Accept() {
    int accept_fd;

    RUN_NO_INTR(accept_fd = accept(listen_fd_, nullptr, nullptr));

    int flags = fcntl(accept_fd, F_GETFL, NULL);
    int ret = fcntl(accept_fd, F_SETFL, flags | O_NONBLOCK);
    if (ret == -1) {
      LOG_ERROR("Can't fcntl");
      return -1;
    }

    if (accept_fd < 0) {
      LOG_WARN("Error accepting test channel connection errno=%d (%s).", errno, strerror(errno));

      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        LOG_ERROR("Closing listen_fd_ (won't try again).");
        close(listen_fd_);
        return -1;
      }
    }

    return accept_fd;
  }

 private:
  int listen_fd_ = -1;
};

class HciHalRootcanalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    thread_ = new Thread("test_thread", Thread::Priority::NORMAL);

    HciHalHostRootcanalConfig::Get()->SetPort(kTestPort);
    fake_server_ = new FakeRootcanalDesktopHciServer;
    hal_ = fake_registry_.Start<HciHal>(thread_);
    hal_->registerIncomingPacketCallback(&callbacks_);
    fake_server_socket_ = fake_server_->Accept();  // accept() after client is connected to avoid blocking
    std::queue<std::pair<uint8_t, HciPacket>> empty;
    std::swap(incoming_packets_queue_, empty);
  }

  void TearDown() override {
    hal_->unregisterIncomingPacketCallback();
    fake_registry_.StopAll();
    close(fake_server_socket_);
    delete fake_server_;
    delete thread_;
  }

  void SetFakeServerSocketToBlocking() {
    int flags = fcntl(fake_server_socket_, F_GETFL, NULL);
    int ret = fcntl(fake_server_socket_, F_SETFL, flags & ~O_NONBLOCK);
    EXPECT_NE(ret, -1) << "Can't set accept fd to blocking";
  }

  FakeRootcanalDesktopHciServer* fake_server_ = nullptr;
  HciHal* hal_ = nullptr;
  ModuleRegistry fake_registry_;
  TestHciHalCallbacks callbacks_;
  int fake_server_socket_ = -1;
  Thread* thread_;
};

void check_packet_equal(std::pair<uint8_t, HciPacket> hci_packet1_type_data_pair, H4Packet h4_packet2) {
  auto packet1_hci_size = hci_packet1_type_data_pair.second.size();
  ASSERT_EQ(packet1_hci_size + 1, h4_packet2.size());
  ASSERT_EQ(hci_packet1_type_data_pair.first, h4_packet2[0]);
  ASSERT_EQ(memcmp(hci_packet1_type_data_pair.second.data(), h4_packet2.data() + 1, packet1_hci_size), 0);
}

HciPacket make_sample_hci_cmd_pkt(uint8_t parameter_total_length) {
  HciPacket pkt;
  pkt.assign(2 + 1 + parameter_total_length, 0x01);
  pkt[2] = parameter_total_length;
  return pkt;
}

HciPacket make_sample_hci_acl_pkt(uint8_t payload_size) {
  HciPacket pkt;
  pkt.assign(2 + 2 + payload_size, 0x01);
  pkt[2] = payload_size;
  return pkt;
}

HciPacket make_sample_hci_sco_pkt(uint8_t payload_size) {
  HciPacket pkt;
  pkt.assign(3 + payload_size, 0x01);
  pkt[2] = payload_size;
  return pkt;
}

H4Packet make_sample_h4_evt_pkt(uint8_t parameter_total_length) {
  H4Packet pkt;
  pkt.assign(1 + 1 + 1 + parameter_total_length, 0x01);
  pkt[0] = kH4Event;
  pkt[2] = parameter_total_length;
  return pkt;
}

HciPacket make_sample_h4_acl_pkt(uint8_t payload_size) {
  HciPacket pkt;
  pkt.assign(1 + 2 + 2 + payload_size, 0x01);
  pkt[0] = kH4Acl;
  pkt[3] = payload_size;
  pkt[4] = 0;
  return pkt;
}

HciPacket make_sample_h4_sco_pkt(uint8_t payload_size) {
  HciPacket pkt;
  pkt.assign(1 + 3 + payload_size, 0x01);
  pkt[0] = kH4Sco;
  pkt[3] = payload_size;
  return pkt;
}

size_t read_with_retry(int socket, uint8_t* data, size_t length) {
  size_t bytes_read = 0;
  ssize_t bytes_read_current = 0;
  do {
    bytes_read_current = read(socket, data + bytes_read, length - bytes_read);
    bytes_read += bytes_read_current;
  } while (length > bytes_read && bytes_read_current > 0);
  return bytes_read;
}

TEST_F(HciHalRootcanalTest, init_and_close) {}

TEST_F(HciHalRootcanalTest, receive_hci_evt) {
  H4Packet incoming_packet = make_sample_h4_evt_pkt(3);
  write(fake_server_socket_, incoming_packet.data(), incoming_packet.size());
  while (incoming_packets_queue_.size() != 1) {
  }
  auto packet = incoming_packets_queue_.front();
  incoming_packets_queue_.pop();
  check_packet_equal(packet, incoming_packet);
}

TEST_F(HciHalRootcanalTest, receive_hci_acl) {
  H4Packet incoming_packet = make_sample_h4_acl_pkt(3);
  write(fake_server_socket_, incoming_packet.data(), incoming_packet.size());
  while (incoming_packets_queue_.size() != 1) {
  }
  auto packet = incoming_packets_queue_.front();
  incoming_packets_queue_.pop();
  check_packet_equal(packet, incoming_packet);
}

TEST_F(HciHalRootcanalTest, receive_hci_sco) {
  H4Packet incoming_packet = make_sample_h4_sco_pkt(3);
  write(fake_server_socket_, incoming_packet.data(), incoming_packet.size());
  while (incoming_packets_queue_.size() != 1) {
  }
  auto packet = incoming_packets_queue_.front();
  incoming_packets_queue_.pop();
  check_packet_equal(packet, incoming_packet);
}

TEST_F(HciHalRootcanalTest, receive_two_hci_evts) {
  H4Packet incoming_packet = make_sample_h4_evt_pkt(3);
  H4Packet incoming_packet2 = make_sample_h4_evt_pkt(5);
  write(fake_server_socket_, incoming_packet.data(), incoming_packet.size());
  write(fake_server_socket_, incoming_packet2.data(), incoming_packet2.size());
  while (incoming_packets_queue_.size() != 2) {
  }
  auto packet = incoming_packets_queue_.front();
  incoming_packets_queue_.pop();
  check_packet_equal(packet, incoming_packet);
  packet = incoming_packets_queue_.front();
  incoming_packets_queue_.pop();
  check_packet_equal(packet, incoming_packet2);
}

TEST_F(HciHalRootcanalTest, receive_evt_and_acl) {
  H4Packet incoming_packet = make_sample_h4_evt_pkt(3);
  H4Packet incoming_packet2 = make_sample_h4_acl_pkt(5);
  write(fake_server_socket_, incoming_packet.data(), incoming_packet.size());
  write(fake_server_socket_, incoming_packet2.data(), incoming_packet2.size());
  while (incoming_packets_queue_.size() != 2) {
  }
  auto packet = incoming_packets_queue_.front();
  incoming_packets_queue_.pop();
  check_packet_equal(packet, incoming_packet);
  packet = incoming_packets_queue_.front();
  incoming_packets_queue_.pop();
  check_packet_equal(packet, incoming_packet2);
}

TEST_F(HciHalRootcanalTest, receive_multiple_acl_batch) {
  H4Packet incoming_packet = make_sample_h4_acl_pkt(5);
  int num_packets = 1000;
  for (int i = 0; i < num_packets; i++) {
    write(fake_server_socket_, incoming_packet.data(), incoming_packet.size());
  }
  while (incoming_packets_queue_.size() != num_packets) {
  }
  for (int i = 0; i < num_packets; i++) {
    auto packet = incoming_packets_queue_.front();
    incoming_packets_queue_.pop();
    check_packet_equal(packet, incoming_packet);
  }
}

TEST_F(HciHalRootcanalTest, receive_multiple_acl_sequential) {
  H4Packet incoming_packet = make_sample_h4_acl_pkt(5);
  int num_packets = 1000;
  for (int i = 0; i < num_packets; i++) {
    write(fake_server_socket_, incoming_packet.data(), incoming_packet.size());
    while (incoming_packets_queue_.empty()) {
    }
    auto packet = incoming_packets_queue_.front();
    incoming_packets_queue_.pop();
    check_packet_equal(packet, incoming_packet);
  }
}

TEST_F(HciHalRootcanalTest, send_hci_cmd) {
  uint8_t hci_cmd_param_size = 2;
  HciPacket hci_data = make_sample_hci_cmd_pkt(hci_cmd_param_size);
  hal_->sendHciCommand(hci_data);
  H4Packet read_buf(1 + 2 + 1 + hci_cmd_param_size);
  SetFakeServerSocketToBlocking();
  auto size_read = read_with_retry(fake_server_socket_, read_buf.data(), read_buf.size());

  ASSERT_EQ(size_read, 1 + hci_data.size());
  check_packet_equal({kH4Command, hci_data}, read_buf);
}

TEST_F(HciHalRootcanalTest, send_acl) {
  uint8_t acl_payload_size = 200;
  HciPacket acl_packet = make_sample_hci_acl_pkt(acl_payload_size);
  hal_->sendAclData(acl_packet);
  H4Packet read_buf(1 + 2 + 2 + acl_payload_size);
  SetFakeServerSocketToBlocking();
  auto size_read = read_with_retry(fake_server_socket_, read_buf.data(), read_buf.size());

  ASSERT_EQ(size_read, 1 + acl_packet.size());
  check_packet_equal({kH4Acl, acl_packet}, read_buf);
}

TEST_F(HciHalRootcanalTest, send_sco) {
  uint8_t sco_payload_size = 200;
  HciPacket sco_packet = make_sample_hci_sco_pkt(sco_payload_size);
  hal_->sendScoData(sco_packet);
  H4Packet read_buf(1 + 3 + sco_payload_size);
  SetFakeServerSocketToBlocking();
  auto size_read = read_with_retry(fake_server_socket_, read_buf.data(), read_buf.size());

  ASSERT_EQ(size_read, 1 + sco_packet.size());
  check_packet_equal({kH4Sco, sco_packet}, read_buf);
}

TEST_F(HciHalRootcanalTest, send_multiple_acl_batch) {
  uint8_t acl_payload_size = 200;
  int num_packets = 1000;
  HciPacket acl_packet = make_sample_hci_acl_pkt(acl_payload_size);
  for (int i = 0; i < num_packets; i++) {
    hal_->sendAclData(acl_packet);
  }
  H4Packet read_buf(1 + 2 + 2 + acl_payload_size);
  SetFakeServerSocketToBlocking();
  for (int i = 0; i < num_packets; i++) {
    auto size_read = read_with_retry(fake_server_socket_, read_buf.data(), read_buf.size());
    ASSERT_EQ(size_read, 1 + acl_packet.size());
    check_packet_equal({kH4Acl, acl_packet}, read_buf);
  }
}

TEST_F(HciHalRootcanalTest, send_multiple_acl_sequential) {
  uint8_t acl_payload_size = 200;
  int num_packets = 1000;
  HciPacket acl_packet = make_sample_hci_acl_pkt(acl_payload_size);
  SetFakeServerSocketToBlocking();
  for (int i = 0; i < num_packets; i++) {
    hal_->sendAclData(acl_packet);
    H4Packet read_buf(1 + 2 + 2 + acl_payload_size);
    auto size_read = read_with_retry(fake_server_socket_, read_buf.data(), read_buf.size());
    ASSERT_EQ(size_read, 1 + acl_packet.size());
    check_packet_equal({kH4Acl, acl_packet}, read_buf);
  }
}

TEST(HciHalHidlTest, serialize) {
  std::vector<uint8_t> bytes = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  auto packet_bytes = hal::SerializePacket(std::unique_ptr<packet::BasePacketBuilder>(new packet::RawBuilder(bytes)));
  ASSERT_EQ(bytes, packet_bytes);
}
}  // namespace
}  // namespace hal
}  // namespace bluetooth
