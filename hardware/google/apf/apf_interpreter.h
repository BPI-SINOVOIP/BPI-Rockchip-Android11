/*
 * Copyright 2015, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef APF_INTERPRETER_H_
#define APF_INTERPRETER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Version of APF instruction set processed by accept_packet().
 * Should be returned by wifi_get_packet_filter_info.
 */
#define APF_VERSION 4

/**
 * Runs a packet filtering program over a packet.
 *
 * The text section containing the program instructions starts at address
 * program and stops at + program_len - 1, and the writable data section
 * begins at program + program_len and ends at program + ram_len - 1,
 * as described in the following diagram:
 *
 *     program         program + program_len    program + ram_len
 *        |    text section    |      data section      |
 *        +--------------------+------------------------+
 *
 * @param program the program bytecode, followed by the writable data region.
 * @param program_len the length in bytes of the read-only portion of the APF
 *                    buffer pointed to by {@code program}.
 * @param ram_len total length of the APF buffer pointed to by {@code program},
 *                including the read-only bytecode portion and the read-write
 *                data portion.
 * @param packet the packet bytes, starting from the 802.3 header and not
 *               including any CRC bytes at the end.
 * @param packet_len the length of {@code packet} in bytes.
 * @param filter_age the number of seconds since the filter was programmed.
 *
 * @return non-zero if packet should be passed to AP, zero if
 *         packet should be dropped.
 */
int accept_packet(uint8_t* program, uint32_t program_len, uint32_t ram_len,
                  const uint8_t* packet, uint32_t packet_len,
                  uint32_t filter_age);

#ifdef __cplusplus
}
#endif

#endif  // APF_INTERPRETER_H_
