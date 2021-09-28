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

#include "hal/snoop_logger.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <bitset>
#include <chrono>

#include "os/log.h"

namespace bluetooth {
namespace hal {

namespace {
typedef struct {
  uint32_t length_original;
  uint32_t length_captured;
  uint32_t flags;
  uint32_t dropped_packets;
  uint64_t timestamp;
  uint8_t type;
} __attribute__((__packed__)) btsnoop_packet_header_t;

typedef struct {
  uint8_t identification_pattern[8];
  uint32_t version_number;
  uint32_t datalink_type;
} __attribute__((__packed__)) btsnoop_file_header_t;

constexpr uint64_t BTSNOOP_EPOCH_DELTA = 0x00dcddb30f2f8000ULL;

constexpr uint32_t kBytesToTest = 0x12345678;
constexpr uint8_t kFirstByte = (const uint8_t&)kBytesToTest;
constexpr bool isLittleEndian = kFirstByte == 0x78;
constexpr bool isBigEndian = kFirstByte == 0x12;
static_assert(isLittleEndian || isBigEndian && isLittleEndian != isBigEndian);

constexpr uint32_t BTSNOOP_VERSION_NUMBER = isLittleEndian ? 0x01000000 : 1;
constexpr uint32_t BTSNOOP_DATALINK_TYPE =
    isLittleEndian ? 0xea030000 : 0x03ea;  // Datalink Type code for HCI UART (H4) is 1002
uint64_t htonll(uint64_t ll) {
  if constexpr (isLittleEndian) {
    return static_cast<uint64_t>(htonl(ll & 0xffffffff)) << 32 | htonl(ll >> 32);
  } else {
    return ll;
  }
}

constexpr btsnoop_file_header_t BTSNOOP_FILE_HEADER = {
    .identification_pattern = {'b', 't', 's', 'n', 'o', 'o', 'p', 0x00},
    .version_number = BTSNOOP_VERSION_NUMBER,
    .datalink_type = BTSNOOP_DATALINK_TYPE};
}  // namespace

SnoopLogger::SnoopLogger() {
  bool file_exists;
  {
    std::ifstream btsnoop_istream(file_path);
    file_exists = btsnoop_istream.is_open();
  }
  btsnoop_ostream_.open(file_path, std::ios::binary | std::ios::app | std::ios::out);
  if (!file_exists) {
    LOG_INFO("Creating new BTSNOOP");
    btsnoop_ostream_.write(reinterpret_cast<const char*>(&BTSNOOP_FILE_HEADER), sizeof(btsnoop_file_header_t));
  } else {
    LOG_INFO("Appending to old BTSNOOP");
  }
}

void SnoopLogger::SetFilePath(const std::string& filename) {
  file_path = filename;
}

void SnoopLogger::capture(const HciPacket& packet, Direction direction, PacketType type) {
  uint64_t timestamp_us =
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
          .count();
  std::lock_guard<std::mutex> lock(file_mutex_);
  std::bitset<32> flags = 0;
  switch (type) {
    case PacketType::CMD:
      flags.set(0, false);
      flags.set(1, true);
      break;
    case PacketType::ACL:
      flags.set(0, direction == Direction::INCOMING);
      flags.set(1, false);
      break;
    case PacketType::SCO:
      flags.set(0, direction == Direction::INCOMING);
      flags.set(1, false);
      break;
    case PacketType::EVT:
      flags.set(0, true);
      flags.set(1, true);
      break;
  }
  uint32_t length = packet.size() + /* type byte */ 1;
  btsnoop_packet_header_t header = {.length_original = htonl(length),
                                    .length_captured = htonl(length),
                                    .flags = htonl(static_cast<uint32_t>(flags.to_ulong())),
                                    .dropped_packets = 0,
                                    .timestamp = htonll(timestamp_us + BTSNOOP_EPOCH_DELTA),
                                    .type = static_cast<uint8_t>(type)};
  btsnoop_ostream_.write(reinterpret_cast<const char*>(&header), sizeof(btsnoop_packet_header_t));
  btsnoop_ostream_.write(reinterpret_cast<const char*>(packet.data()), packet.size());
  if (AlwaysFlush) btsnoop_ostream_.flush();
}

void SnoopLogger::ListDependencies(ModuleList* list) {
  // We have no dependencies
}

void SnoopLogger::Start() {}

void SnoopLogger::Stop() {}

std::string SnoopLogger::file_path = SnoopLogger::DefaultFilePath;

const ModuleFactory SnoopLogger::Factory = ModuleFactory([]() {
  return new SnoopLogger();
});

}  // namespace hal
}  // namespace bluetooth
