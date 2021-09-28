//
// Copyright 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "test_environment.h"

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "os/log.h"

namespace android {
namespace bluetooth {
namespace root_canal {

using test_vendor_lib::AsyncTaskId;
using test_vendor_lib::DualModeController;
using test_vendor_lib::TaskCallback;

void TestEnvironment::initialize(std::promise<void> barrier) {
  LOG_INFO("%s", __func__);

  barrier_ = std::move(barrier);

  test_channel_transport_.RegisterCommandHandler([this](const std::string& name, const std::vector<std::string>& args) {
    async_manager_.ExecAsync(std::chrono::milliseconds(0),
                             [this, name, args]() { test_channel_.HandleCommand(name, args); });
  });

  test_model_.Reset();

  SetUpTestChannel();
  SetUpHciServer([this](int fd) { test_model_.IncomingHciConnection(fd); });
  SetUpLinkLayerServer([this](int fd) { test_model_.IncomingLinkLayerConnection(fd); });

  // In case the client socket is closed, and rootcanal doesn't detect it due to
  // TimerTick not fired, writing to the socket causes a SIGPIPE and we need to
  // catch it to prevent rootcanal from crash
  signal(SIGPIPE, SIG_IGN);

  LOG_INFO("%s: Finished", __func__);
}

void TestEnvironment::close() {
  LOG_INFO("%s", __func__);
}

void TestEnvironment::SetUpHciServer(const std::function<void(int)>& connection_callback) {
  int socket_fd = remote_hci_transport_.SetUp(hci_server_port_);

  test_channel_.RegisterSendResponse(
      [](const std::string& response) { LOG_INFO("No HCI Response channel: %s", response.c_str()); });

  if (socket_fd == -1) {
    LOG_ERROR("Remote HCI channel SetUp(%d) failed.", hci_server_port_);
    return;
  }

  async_manager_.WatchFdForNonBlockingReads(socket_fd, [this, connection_callback](int socket_fd) {
    int conn_fd = remote_hci_transport_.Accept(socket_fd);
    if (conn_fd < 0) {
      LOG_ERROR("Error watching remote HCI channel fd.");
      return;
    }
    int flags = fcntl(conn_fd, F_GETFL, NULL);
    int ret;
    ret = fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK);
    ASSERT_LOG(ret != -1, "Error setting O_NONBLOCK %s", strerror(errno));

    connection_callback(conn_fd);
  });
}

void TestEnvironment::SetUpLinkLayerServer(const std::function<void(int)>& connection_callback) {
  int socket_fd = remote_link_layer_transport_.SetUp(link_server_port_);

  test_channel_.RegisterSendResponse(
      [](const std::string& response) { LOG_INFO("No LinkLayer Response channel: %s", response.c_str()); });

  if (socket_fd == -1) {
    LOG_ERROR("Remote LinkLayer channel SetUp(%d) failed.", link_server_port_);
    return;
  }

  async_manager_.WatchFdForNonBlockingReads(socket_fd, [this, connection_callback](int socket_fd) {
    int conn_fd = remote_link_layer_transport_.Accept(socket_fd);
    if (conn_fd < 0) {
      LOG_ERROR("Error watching remote LinkLayer channel fd.");
      return;
    }
    int flags = fcntl(conn_fd, F_GETFL, NULL);
    int ret = fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK);
    ASSERT_LOG(ret != -1, "Error setting O_NONBLOCK %s", strerror(errno));

    connection_callback(conn_fd);
  });
}

int TestEnvironment::ConnectToRemoteServer(const std::string& server, int port) {
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd < 1) {
    LOG_INFO("socket() call failed: %s", strerror(errno));
    return -1;
  }

  struct hostent* host;
  host = gethostbyname(server.c_str());
  if (host == NULL) {
    LOG_INFO("gethostbyname() failed for %s: %s", server.c_str(), strerror(errno));
    return -1;
  }

  struct sockaddr_in serv_addr;
  memset((void*)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  int result = connect(socket_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (result < 0) {
    LOG_INFO("connect() failed for %s@%d: %s", server.c_str(), port, strerror(errno));
    return -1;
  }

  int flags = fcntl(socket_fd, F_GETFL, NULL);
  int ret = fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
  ASSERT_LOG(ret != -1, "Error setting O_NONBLOCK %s", strerror(errno));

  return socket_fd;
}

void TestEnvironment::SetUpTestChannel() {
  int socket_fd = test_channel_transport_.SetUp(test_port_);
  test_channel_.AddPhy({"BR_EDR"});
  test_channel_.AddPhy({"LOW_ENERGY"});
  test_channel_.SetTimerPeriod({"10"});
  test_channel_.StartTimer({});

  test_channel_.RegisterSendResponse(
      [](const std::string& response) { LOG_INFO("No test channel: %s", response.c_str()); });

  if (socket_fd == -1) {
    LOG_ERROR("Test channel SetUp(%d) failed.", test_port_);
    return;
  }

  LOG_INFO("Test channel SetUp() successful");
  async_manager_.WatchFdForNonBlockingReads(socket_fd, [this](int socket_fd) {
    int conn_fd = test_channel_transport_.Accept(socket_fd);
    if (conn_fd < 0) {
      LOG_ERROR("Error watching test channel fd.");
      barrier_.set_value();
      return;
    }
    LOG_INFO("Test channel connection accepted.");
    test_channel_.RegisterSendResponse(
        [this, conn_fd](const std::string& response) { test_channel_transport_.SendResponse(conn_fd, response); });

    async_manager_.WatchFdForNonBlockingReads(conn_fd, [this](int conn_fd) {
      test_channel_transport_.OnCommandReady(conn_fd, [this, conn_fd]() {
        async_manager_.StopWatchingFileDescriptor(conn_fd);
        barrier_.set_value();
      });
    });
  });
}

}  // namespace root_canal
}  // namespace bluetooth
}  // namespace android
