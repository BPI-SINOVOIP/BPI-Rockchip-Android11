/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

const char* kDomainSocketName = "@RunasConnectAppSocket";

void SetUnixSocketAddr(const char* name, struct sockaddr_un* addr_un,
                       socklen_t* addr_len) {
  memset(addr_un, 0, sizeof(*addr_un));
  addr_un->sun_family = AF_UNIX;
  strcpy(addr_un->sun_path, name);
  *addr_len = offsetof(struct sockaddr_un, sun_path) + strlen(name);

  if (addr_un->sun_path[0] == '@') {
    addr_un->sun_path[0] = '\0';
  }
}

int main() {
  int fd;
  struct sockaddr_un addr_un;
  socklen_t addr_len;

  if ((fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)) == -1) {
    perror("socket error");
  }

  SetUnixSocketAddr(kDomainSocketName, &addr_un, &addr_len);
  if (connect(fd, (struct sockaddr*)&addr_un, addr_len) == -1) {
    perror("connect error");
    return EXIT_FAILURE;
  }

  const int kBufferSize = 128;
  char buf[kBufferSize];
  memset(buf, 0, kBufferSize);
  ssize_t result = TEMP_FAILURE_RETRY(read(fd, buf, kBufferSize - 1));
  if (result >= 0) {
    printf("%s\n", buf);
  } else {
    printf("[connector] read() failed");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
