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

#ifndef CHPP_TRANSPORT_H_
#define CHPP_TRANSPORT_H_

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "chpp/macros.h"
#include "chpp/memory.h"
#include "chpp/mutex.h"
#include "chpp/platform/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************************************
 *  Public Definitions
 ***********************************************/

/**
 * CHPP Transport header flags bitmap
 *
 * @defgroup CHPP_TRANSPORT_FLAG
 * @{
 */
// This packet concludes a (fragmented or unfragmented) datagram
#define CHPP_TRANSPORT_FLAG_FINISHED_DATAGRAM 0x00
// Set if packet is part of a fragmented datagram, except for the last fragment
#define CHPP_TRANSPORT_FLAG_UNFINISHED_DATAGRAM 0x01
// Set for first packet after bootup or to reset after irrecoverable error
#define CHPP_TRANSPORT_FLAG_RESET 0x02
// Reserved for future use
#define CHPP_TRANSPORT_FLAG_RESERVED 0xfc
/** @} */

/**
 * Preamble (i.e. packet start delimiter).
 * Any future backwards-incompatible versions of CHPP Transport will use a
 * different preamble.
 *
 * @defgroup CHPP_PREAMBLE
 * @{
 */
#define CHPP_PREAMBLE_DATA 0x6843
#define CHPP_PREAMBLE_LEN_BYTES 2
/** @} */

/**
 * Maximum number of datagrams in the Tx queue.
 * CHPP will return an error if it is provided with a new Tx datagram when this
 * queue is full.
 * To be safe, this should be less than half of the maximum uint8_t value.
 * Otherwise, ChppTxDatagramQueue should be updated accordingly.
 */
#define CHPP_TX_DATAGRAM_QUEUE_LEN 16

/**
 * Maximum payload of packets at the link layer.
 * TODO: Negotiate or advertise MTU
 */
#define CHPP_LINK_MTU_BYTES                                              \
  (1024 + CHPP_PREAMBLE_LEN_BYTES + sizeof(struct ChppTransportHeader) + \
   sizeof(struct ChppTransportFooter))

/**
 * Maximum payload of packets at the transport layer.
 */
#define CHPP_TRANSPORT_MTU_BYTES                   \
  (CHPP_LINK_MTU_BYTES - CHPP_PREAMBLE_LEN_BYTES - \
   sizeof(struct ChppTransportHeader) - sizeof(struct ChppTransportFooter))

/************************************************
 *  Status variables to store context in lieu of global variables (this)
 ***********************************************/

/**
 * Error codes optionally reported in ChppTransportHeader
 */
enum ChppErrorCode {
  // No error reported (either ACK or implicit NACK)
  CHPP_ERROR_NONE = 0,
  // Checksum failure
  CHPP_ERROR_CHECKSUM = 1,
  // Out of memory
  CHPP_ERROR_OOM = 2,
  // Busy
  CHPP_ERROR_BUSY = 3,
  // Invalid header
  CHPP_ERROR_HEADER = 4,
  // Out of order
  CHPP_ERROR_ORDER = 5,
  // Timeout (implicit, deduced and used internally only)
  CHPP_ERROR_TIMEOUT = 0xF,
};

/**
 * CHPP Transport Layer header (not including the preamble)
 */
CHPP_PACKED_START
struct ChppTransportHeader {
  // Flags bitmap, defined as CHPP_TRANSPORT_FLAG_...
  uint8_t flags;

  // Error info (2 nibbles)
  // LS Nibble: Defined in ChppErrorCode enum
  // MS Nibble: Reserved
  uint8_t errorCode;

  // Next expected sequence number for a payload-bearing packet
  uint8_t ackSeq;

  // Sequence number
  uint8_t seq;

  // Payload length in bytes (not including header / footer)
  uint16_t length;

  // Reserved? TBD
  uint16_t reserved;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

/**
 * CHPP Transport Layer footer (containing the checksum)
 */
CHPP_PACKED_START
struct ChppTransportFooter {
  // Checksum algo TBD. Maybe IEEE CRC-32?
  uint32_t checksum;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

enum ChppRxState {
  // Waiting for, or processing, the preamble (i.e. packet start delimiter)
  // Moves to CHPP_STATE_HEADER as soon as it has seen a complete preamble.
  CHPP_STATE_PREAMBLE = 0,

  // Processing the packet header. Moves to CHPP_STATE_PAYLOAD after processing
  // the expected length of the header.
  CHPP_STATE_HEADER = 1,

  // Copying the packet payload. The payload length is determined by the header.
  // Moves to CHPP_STATE_FOOTER afterwards.
  CHPP_STATE_PAYLOAD = 2,

  // Processing the packet footer (checksum) and responding accordingly. Moves
  // to CHPP_STATE_PREAMBLE afterwards.
  CHPP_STATE_FOOTER = 3,
};

struct ChppRxStatus {
  // Current receiving state, as described in ChppRxState.
  enum ChppRxState state;

  // Location counter in bytes within each state. Must always be reinitialized
  // to 0 when switching states.
  size_t locInState;

  // Next expected sequence number (for a payload-bearing packet)
  uint8_t expectedSeq;

  // Error code, if any, of the last received packet
  enum ChppErrorCode receivedErrorCode;

  // Location counter in bytes within the current Rx datagram.
  size_t locInDatagram;
};

struct ChppTxStatus {
  // Last received ACK sequence number (i.e. next expected sequence number for
  // an outgoing payload-bearing packet)
  uint8_t ackedSeq;

  // Last sent sequence number (irrespective of whether it has been received /
  // ACKed or not)
  uint8_t sentSeq;

  // Does the transport layer have any packets (with or without payload) it
  // needs to send out?
  bool hasPacketsToSend;

  // Error code, if any, of the next packet the transport layer will send out.
  enum ChppErrorCode errorCodeToSend;

  // How many bytes of the front-of-queue datagram has been sent out
  size_t sentLocInDatagram;

  // Note: For a future ACK window >1, sentLocInDatagram doesn't always apply to
  // the front-of-queue datagram. Instead, we need to track the queue position
  // the datagram being sent as well (relative to the front-of-queue). e.g.
  // uint8_t datagramBeingSent

  // How many bytes of the front-of-queue datagram has been acked
  size_t ackedLocInDatagram;
};

struct PacketToSend {
  // Length of outgoing packet to the Link Layer
  size_t length;

  // Payload of outgoing packet to the Link Layer
  uint8_t payload[CHPP_LINK_MTU_BYTES];
};

struct ChppDatagram {
  // Length of datagram payload in bytes (A datagram can be constituted from one
  // or more packets)
  size_t length;

  // Datagram payload
  uint8_t *payload;
};

struct ChppTxDatagramQueue {
  // Number of pending datagrams in the queue.
  uint8_t pending;

  // Index of the datagram at the front of the queue.
  uint8_t front;

  // Location counter within the front datagram (i.e. the datagram at the front
  // of the queue), showing how many bytes of this datagram have already been
  // packetized and processed.
  size_t loc;

  // Array of datagrams
  struct ChppDatagram datagram[CHPP_TX_DATAGRAM_QUEUE_LEN];
};

struct ChppTransportState {
  struct ChppRxStatus rxStatus;         // Rx state and location within
  struct ChppTransportHeader rxHeader;  // Rx packet header
  struct ChppTransportFooter rxFooter;  // Rx packet footer (checksum)
  struct ChppDatagram rxDatagram;       // Rx datagram

  struct ChppTxStatus txStatus;                // Tx state
  struct ChppTxDatagramQueue txDatagramQueue;  // Queue of datagrams to be Tx
  struct PacketToSend packetToSend;            // Outgoing packet to Link Layer

  struct ChppMutex mutex;           // Lock for transport state (i.e. context)
  struct ChppMutex linkLayerMutex;  // Lock for the link layer
};

/************************************************
 *  Public functions
 ***********************************************/

/**
 * Initializes the CHPP transport layer state stored in the parameter context.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 */
void chppTransportInit(struct ChppTransportState *context);

/**
 * Processes all incoming data on the serial port based on the Rx state.
 * stream. Checks checksum, triggering the correct response (ACK / NACK).
 * Moves the state to CHPP_STATE_PREAMBLE afterwards.
 *
 * TODO: Add requirements, e.g. context must not be modified unless locked via
 * mutex.
 *
 * TODO: Add sufficient outward facing documentation
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 * @param buf Input data. Cannot be null.
 * @param len Length of input data in bytes.
 *
 * @return true informs the serial port driver that we are waiting for a
 * preamble. This allows the driver to (optionally) filter incoming zeros and
 * save processing
 */
bool chppRxDataCb(struct ChppTransportState *context, const uint8_t *buf,
                  size_t len);

/**
 * Callback function for the timer that detects timeouts during transmit
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 */
void chppTxTimeoutTimerCb(struct ChppTransportState *context);

/**
 * Enqueues an outgoing datagram of a specified length. The payload must have
 * been allocated by the caller using chppMalloc. If enqueueing is successful,
 * the payload shall be freed only by the transport layer (once it has been sent
 * out). If enqueueing is unsuccessful, it is up to the sender to decide whether
 * to free the payload and/or resend it later.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 * @param len Datagram length in bytes.
 * @param buf Datagram payload allocated through chppMalloc. Cannot be null.
 * @return True informs the sender that the datagram was successfully enqueued.
 * False informs the sender that the queue was full.
 */
bool chppEnqueueTxDatagram(struct ChppTransportState *context, size_t len,
                           uint8_t *buf);


#ifdef __cplusplus
}
#endif

#endif  // CHPP_TRANSPORT_H_
