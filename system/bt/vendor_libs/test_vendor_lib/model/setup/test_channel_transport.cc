/*
 * Copyright 2015 The Android Open Source Project
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

#include "test_channel_transport.h"

#include "os/log.h"
#include "osi/include/osi.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using std::vector;

namespace test_vendor_lib {

int TestChannelTransport::SetUp(int port) {
  struct sockaddr_in listen_address;
  socklen_t sockaddr_in_size = sizeof(struct sockaddr_in);
  memset(&listen_address, 0, sockaddr_in_size);

  OSI_NO_INTR(listen_fd_ = socket(AF_INET, SOCK_STREAM, 0));
  if (listen_fd_ < 0) {
    LOG_INFO("Error creating socket for test channel.");
    return -1;
  }

  int enable = 1;
  if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    LOG_ERROR("setsockopt(SO_REUSEADDR) failed: %s", strerror(errno));
  }

  LOG_INFO("port: %d", port);
  listen_address.sin_family = AF_INET;
  listen_address.sin_port = htons(port);
  listen_address.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&listen_address), sockaddr_in_size) < 0) {
    LOG_INFO("Error binding test channel listener socket to address: %s", strerror(errno));
    close(listen_fd_);
    return -1;
  }

  if (listen(listen_fd_, 1) < 0) {
    LOG_INFO("Error listening for test channel: %s", strerror(errno));
    close(listen_fd_);
    return -1;
  }
  return listen_fd_;
}

void TestChannelTransport::CleanUp() {
  if (listen_fd_ == -1) {
    return;
  }
  if (close(listen_fd_)) {
    LOG_ERROR("Error closing listen_fd_.");
  }
  listen_fd_ = -1;
}

int TestChannelTransport::Accept(int listen_fd_) {
  int accept_fd = -1;

  OSI_NO_INTR(accept_fd = accept(listen_fd_, NULL, NULL));

  if (accept_fd < 0) {
    LOG_INFO("Error accepting test channel connection errno=%d (%s).", errno, strerror(errno));

    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      LOG_ERROR("Closing listen_fd_ (won't try again).");
      close(listen_fd_);
      return -1;
    }
  }

  LOG_INFO("accept_fd = %d.", accept_fd);

  return accept_fd;
}

void TestChannelTransport::OnCommandReady(int fd, std::function<void(void)> unwatch) {
  uint8_t command_name_size = 0;
  int bytes_read = read(fd, &command_name_size, 1);
  if (bytes_read != 1) {
    LOG_INFO("Unexpected (command_name_size) bytes_read: %d != %d", bytes_read, 1);
  }
  vector<uint8_t> command_name_raw;
  command_name_raw.resize(command_name_size);
  bytes_read = read(fd, &command_name_raw[0], command_name_size);
  if (bytes_read != command_name_size) {
    LOG_INFO("Unexpected (command_name) bytes_read: %d != %d", bytes_read, command_name_size);
  }
  std::string command_name(command_name_raw.begin(), command_name_raw.end());

  if (command_name == "CLOSE_TEST_CHANNEL" || command_name == "") {
    LOG_INFO("Test channel closed");
    unwatch();
    close(fd);
    return;
  }

  uint8_t num_args = 0;
  bytes_read = read(fd, &num_args, 1);
  if (bytes_read != 1) {
    LOG_INFO("Unexpected (num_args) bytes_read: %d != %d", bytes_read, 1);
  }
  vector<std::string> args;
  for (uint8_t i = 0; i < num_args; ++i) {
    uint8_t arg_size = 0;
    bytes_read = read(fd, &arg_size, 1);
    if (bytes_read != 1) {
      LOG_INFO("Unexpected (arg_size) bytes_read: %d != %d", bytes_read, 1);
    }
    vector<uint8_t> arg;
    arg.resize(arg_size);
    bytes_read = read(fd, &arg[0], arg_size);
    if (bytes_read != arg_size) {
      LOG_INFO("Unexpected (arg) bytes_read: %d != %d", bytes_read, arg_size);
    }
    args.push_back(std::string(arg.begin(), arg.end()));
  }

  command_handler_(command_name, args);
}

void TestChannelTransport::SendResponse(int fd, const std::string& response) const {
  size_t size = response.size();
  // Cap to 64K
  if (size > 0xffff) {
    size = 0xffff;
  }
  uint8_t size_buf[4] = {static_cast<uint8_t>(size & 0xff), static_cast<uint8_t>((size >> 8) & 0xff),
                         static_cast<uint8_t>((size >> 16) & 0xff), static_cast<uint8_t>((size >> 24) & 0xff)};
  int written = write(fd, size_buf, 4);
  ASSERT_LOG(written == 4, "What happened? written = %d errno = %d", written, errno);
  written = write(fd, response.c_str(), size);
  ASSERT_LOG(written == static_cast<int>(size), "What happened? written = %d errno = %d", written, errno);
}

void TestChannelTransport::RegisterCommandHandler(
    const std::function<void(const std::string&, const std::vector<std::string>&)>& callback) {
  command_handler_ = callback;
}

}  // namespace test_vendor_lib
