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

#include "chpp/transport.h"


/************************************************
 *  Prototypes
 ***********************************************/

static void chppSetRxState(struct ChppTransportState *context,
                           enum ChppRxState newState);
static size_t chppConsumePreamble(struct ChppTransportState *context,
                                  const uint8_t *buf, size_t len);
static size_t chppConsumeHeader(struct ChppTransportState *context,
                                const uint8_t *buf, size_t len);
static size_t chppConsumePayload(struct ChppTransportState *context,
                                 const uint8_t *buf, size_t len);
static size_t chppConsumeFooter(struct ChppTransportState *context,
                                const uint8_t *buf, size_t len);
static void chppProcessRxPayload(struct ChppTransportState *context);
static bool chppRxChecksumIsOk(const struct ChppTransportState *context);
static enum ChppErrorCode chppRxHeaderCheck(
    const struct ChppTransportState *context);
static void chppRegisterRxAck(struct ChppTransportState *context);

static void chppEnqueueTxPacket(struct ChppTransportState *context,
                                enum ChppErrorCode errorCode);
static size_t chppAddPreamble(uint8_t *buf);
static uint32_t chppCalculateChecksum(uint8_t *buf, size_t len);
bool chppDequeueTxDatagram(struct ChppTransportState *context);
void chppTransportDoWork(struct ChppTransportState *context);

/************************************************
 *  Private Functions
 ***********************************************/

/**
 * Called any time the Rx state needs to be changed. Ensures that the location
 * counter among that state (rxStatus.locInState) is also reset at the same
 * time.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 * @param newState Next Rx state
 */
static void chppSetRxState(struct ChppTransportState *context,
                           enum ChppRxState newState) {
  LOGD("Changing state from %d to %d", context->rxStatus.state, newState);
  context->rxStatus.locInState = 0;
  context->rxStatus.state = newState;
}

/**
 * Called by chppRxDataCb to find a preamble (i.e. packet start delimiter) in
 * the incoming data stream.
 * Moves the state to CHPP_STATE_HEADER as soon as it has seen a complete
 * preamble.
 * Any future backwards-incompatible versions of CHPP Transport will use a
 * different preamble.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 * @param buf Input data
 * @param len Length of input data in bytes
 *
 * @return Length of consumed data in bytes
 */
static size_t chppConsumePreamble(struct ChppTransportState *context,
                                  const uint8_t *buf, size_t len) {
  size_t consumed = 0;

  // TODO: Optimize loop, maybe using memchr() / memcmp() / SIMD, especially if
  // serial port calling chppRxDataCb does not implement zero filter
  while (consumed < len &&
         context->rxStatus.locInState < CHPP_PREAMBLE_LEN_BYTES) {
    if (buf[consumed] ==
        ((CHPP_PREAMBLE_DATA >>
          (CHPP_PREAMBLE_LEN_BYTES - context->rxStatus.locInState - 1)) &
         0xff)) {
      // Correct byte of preamble observed
      context->rxStatus.locInState++;
    } else if (buf[consumed] ==
               ((CHPP_PREAMBLE_DATA >> (CHPP_PREAMBLE_LEN_BYTES - 1)) & 0xff)) {
      // Previous search failed but first byte of another preamble observed
      context->rxStatus.locInState = 1;
    } else {
      // Continue search for a valid preamble from the start
      context->rxStatus.locInState = 0;
    }

    consumed++;
  }

  // Let's see why we exited the above loop
  if (context->rxStatus.locInState == CHPP_PREAMBLE_LEN_BYTES) {
    // Complete preamble observed, move on to next state
    chppSetRxState(context, CHPP_STATE_HEADER);
  }

  return consumed;
}

/**
 * Called by chppRxDataCb to process the packet header from the incoming data
 * stream.
 * Moves the Rx state to CHPP_STATE_PAYLOAD afterwards.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 * @param buf Input data
 * @param len Length of input data in bytes
 *
 * @return Length of consumed data in bytes
 */
static size_t chppConsumeHeader(struct ChppTransportState *context,
                                const uint8_t *buf, size_t len) {
  CHPP_ASSERT(context->rxStatus.locInState <
              sizeof(struct ChppTransportHeader));
  size_t bytesToCopy = MIN(
      len, (sizeof(struct ChppTransportHeader) - context->rxStatus.locInState));

  LOGD("Copying %zu bytes of header", bytesToCopy);
  memcpy(((uint8_t *)&context->rxHeader) + context->rxStatus.locInState, buf,
         bytesToCopy);

  context->rxStatus.locInState += bytesToCopy;
  if (context->rxStatus.locInState == sizeof(struct ChppTransportHeader)) {
    // Header fully copied. Move on

    enum ChppErrorCode headerSanity = chppRxHeaderCheck(context);
    if (headerSanity != CHPP_ERROR_NONE) {
      // Header fails sanity check. NACK and return to preamble state
      chppEnqueueTxPacket(context, headerSanity);
      chppSetRxState(context, CHPP_STATE_PREAMBLE);

    } else {
      // Header passes sanity check

      if (context->rxHeader.length == 0) {
        // Non-payload packet
        chppSetRxState(context, CHPP_STATE_FOOTER);

      } else {
        // Payload bearing packet
        uint8_t *tempPayload;

        if (context->rxDatagram.length == 0) {
          // Packet is a new datagram
          tempPayload = chppMalloc(context->rxHeader.length);
        } else {
          // Packet is a continuation of a fragmented datagram
          tempPayload =
              chppRealloc(context->rxDatagram.payload,
                          context->rxDatagram.length + context->rxHeader.length,
                          context->rxDatagram.length);
        }

        if (tempPayload == NULL) {
          LOGE("OOM for packet# %d, len=%u. Previous fragment(s) total len=%zu",
               context->rxHeader.seq, context->rxHeader.length,
               context->rxDatagram.length);
          chppEnqueueTxPacket(context, CHPP_ERROR_OOM);
          chppSetRxState(context, CHPP_STATE_PREAMBLE);
        } else {
          context->rxDatagram.payload = tempPayload;
          context->rxDatagram.length += context->rxHeader.length;
          chppSetRxState(context, CHPP_STATE_PAYLOAD);
        }
      }
    }
  }

  return bytesToCopy;
}

/**
 * Called by chppRxDataCb to copy the payload, the length of which is determined
 * by the header, from the incoming data stream.
 * Moves the Rx state to CHPP_STATE_FOOTER afterwards.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 * @param buf Input data
 * @param len Length of input data in bytes
 *
 * @return Length of consumed data in bytes
 */
static size_t chppConsumePayload(struct ChppTransportState *context,
                                 const uint8_t *buf, size_t len) {
  CHPP_ASSERT(context->rxStatus.locInState < context->rxHeader.length);
  size_t bytesToCopy =
      MIN(len, (context->rxHeader.length - context->rxStatus.locInState));

  LOGD("Copying %zu bytes of payload", bytesToCopy);

  memcpy(context->rxDatagram.payload + context->rxStatus.locInDatagram, buf,
         bytesToCopy);
  context->rxStatus.locInDatagram += bytesToCopy;

  context->rxStatus.locInState += bytesToCopy;
  if (context->rxStatus.locInState == context->rxHeader.length) {
    // Payload copied. Move on

    chppSetRxState(context, CHPP_STATE_FOOTER);
  }

  return bytesToCopy;
}

/**
 * Called by chppRxDataCb to process the packet footer from the incoming data
 * stream. Checks checksum, triggering the correct response (ACK / NACK).
 * Moves the Rx state to CHPP_STATE_PREAMBLE afterwards.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 * @param buf Input data
 * @param len Length of input data in bytes
 *
 * @return Length of consumed data in bytes
 */
static size_t chppConsumeFooter(struct ChppTransportState *context,
                                const uint8_t *buf, size_t len) {
  CHPP_ASSERT(context->rxStatus.locInState <
              sizeof(struct ChppTransportFooter));
  size_t bytesToCopy = MIN(
      len, (sizeof(struct ChppTransportFooter) - context->rxStatus.locInState));

  LOGD("Copying %zu bytes of footer (checksum)", bytesToCopy);
  memcpy(((uint8_t *)&context->rxFooter) + context->rxStatus.locInState, buf,
         bytesToCopy);

  context->rxStatus.locInState += bytesToCopy;
  if (context->rxStatus.locInState == sizeof(struct ChppTransportFooter)) {
    // Footer copied. Move on

    bool hasPayload = (context->rxHeader.length > 0);

    if (!chppRxChecksumIsOk(context)) {
      // Packet is bad. Discard bad payload data (if any) and NACK
      LOGE("Discarding CHPP packet# %d len=%u because of bad checksum",
           context->rxHeader.seq, context->rxHeader.length);

      if (hasPayload) {
        context->rxDatagram.length -= context->rxHeader.length;
        context->rxStatus.locInDatagram -= context->rxHeader.length;

        if (context->rxDatagram.length == 0) {
          // Discarding this packet == discarding entire datagram
          chppFree(context->rxDatagram.payload);
          context->rxDatagram.payload = NULL;

        } else {
          // Discarding this packet == discarding part of datagram
          uint8_t *tempPayload = chppRealloc(
              context->rxDatagram.payload, context->rxDatagram.length,
              context->rxDatagram.length + context->rxHeader.length);
          if (tempPayload == NULL) {
            LOGE(
                "OOM discarding bad continuation packet# %d len=%u. Previous "
                "fragment(s) total len=%zu",
                context->rxHeader.seq, context->rxHeader.length,
                context->rxDatagram.length);
          } else {
            context->rxDatagram.payload = tempPayload;
          }
        }
      }

      chppEnqueueTxPacket(context, CHPP_ERROR_CHECKSUM);

    } else {
      // Packet is good. Save received ACK info and process payload if any

      context->rxStatus.receivedErrorCode = context->rxHeader.errorCode;

      chppRegisterRxAck(context);

      if (context->txDatagramQueue.pending > 0) {
        // There are packets to send out (could be new or retx)
        chppEnqueueTxPacket(context, CHPP_ERROR_NONE);
      }

      if (hasPayload) {
        chppProcessRxPayload(context);
      }
    }

    // Done with this packet. Wait for next packet
    chppSetRxState(context, CHPP_STATE_PREAMBLE);
  }

  return bytesToCopy;
}

/**
 * Process the payload of a validated payload-bearing packet and send out the
 * ACK
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 */
static void chppProcessRxPayload(struct ChppTransportState *context) {
  if (context->rxHeader.flags & CHPP_TRANSPORT_FLAG_UNFINISHED_DATAGRAM) {
    // packet is part of a larger datagram
    LOGD(
        "Received continuation packet# %d len=%u. Previous fragment(s) "
        "total len=%zu",
        context->rxHeader.seq, context->rxHeader.length,
        context->rxDatagram.length);

  } else {
    // End of this packet is end of a datagram
    LOGD(
        "Received packet# %d len=%u completing a datagram. Previous "
        "fragment(s) total len=%zu",
        context->rxHeader.seq, context->rxHeader.length,
        context->rxDatagram.length);

    // TODO: do something with the data

    context->rxStatus.locInDatagram = 0;
    context->rxDatagram.length = 0;
    chppFree(context->rxDatagram.payload);
    context->rxDatagram.payload = NULL;
  }

  // Update next expected sequence number and send ACK
  context->rxStatus.expectedSeq = context->rxHeader.seq + 1;
  chppEnqueueTxPacket(context, CHPP_ERROR_NONE);
}

/**
 * Validates the checksum of an incoming packet.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 *
 * @return True if and only if the checksum is correct
 */
static bool chppRxChecksumIsOk(const struct ChppTransportState *context) {
  // TODO
  UNUSED_VAR(context);

  LOGE("Blindly assuming checksum is correct");
  return true;
}

/**
 * Performs sanity check on received packet header. Discards packet if header is
 * obviously corrupt / invalid.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 *
 * @return True if and only if header passes sanity check
 */
static enum ChppErrorCode chppRxHeaderCheck(
    const struct ChppTransportState *context) {
  enum ChppErrorCode result = CHPP_ERROR_NONE;

  bool invalidSeqNo = (context->rxHeader.seq != context->rxStatus.expectedSeq);
  bool hasPayload = (context->rxHeader.length > 0);
  if (invalidSeqNo && hasPayload) {
    // Note: For a future ACK window > 1, might make more sense to keep quiet
    // instead of flooding the sender with out of order NACKs
    result = CHPP_ERROR_ORDER;
  }

  // TODO: More sanity checks

  return result;
}

/**
 * Registers a received ACK. If an outgoing datagram is fully ACKed, it is
 * popped from the Tx queue.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 */
static void chppRegisterRxAck(struct ChppTransportState *context) {
  if (context->txStatus.ackedSeq != context->rxHeader.ackSeq) {
    // A previously sent packet was actually ACKed
    context->txStatus.ackedSeq = context->rxHeader.ackSeq;

    // Process and if necessary pop from Tx datagram queue
    context->txStatus.ackedLocInDatagram += CHPP_TRANSPORT_MTU_BYTES;
    if (context->txStatus.ackedLocInDatagram >=
        context->txDatagramQueue.datagram[context->txDatagramQueue.front]
            .length) {
      // We are done with datagram

      context->txStatus.ackedLocInDatagram = 0;
      context->txStatus.sentLocInDatagram = 0;

      // Note: For a future ACK window >1, we should update which datagram too

      chppDequeueTxDatagram(context);
    }
  }  // else {nothing was ACKed}
}

/**
 * Enqueues an outgoing packet with the specified error code. The error code
 * refers to the optional reason behind a NACK, if any. An error code of
 * CHPP_ERROR_NONE indicates that no error was reported (i.e. either an ACK or
 * an implicit NACK)
 *
 * Note that the decision as to wheather to include a payload will be taken
 * later, i.e. before the packet is being sent out from the queue. A payload is
 * expected to be included if there is one or more pending Tx datagrams and we
 * are not waiting on a pending ACK. A (repeat) payload is also included if we
 * have received a NACK.
 *
 * Further note that even for systems with an ACK window greater than one, we
 * would only need to send an ACK for the last (correct) packet, hence we only
 * need a queue length of one here.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 * @param errorCode Error code for the next outgoing packet
 */
static void chppEnqueueTxPacket(struct ChppTransportState *context,
                                enum ChppErrorCode errorCode) {
  context->txStatus.hasPacketsToSend = true;
  context->txStatus.errorCodeToSend = errorCode;

  // TODO: Notify chppTransportDoWork
}

/**
 * Adds a CHPP preamble to the beginning of buf
 *
 * @param buf The CHPP preamble will be added to buf
 *
 * @return Size of the added preamble
 */
static size_t chppAddPreamble(uint8_t *buf) {
  for (size_t i = 0; i < CHPP_PREAMBLE_LEN_BYTES; i++) {
    buf[i] = (uint8_t)(CHPP_PREAMBLE_DATA >> (CHPP_PREAMBLE_LEN_BYTES - 1 - i) &
                       0xff);
  }
  return CHPP_PREAMBLE_LEN_BYTES;
}

/**
 * Calculates the checksum on a buffer indicated by buf with length len
 *
 * @param buf Pointer to buffer for the ckecksum to be calculated on.
 * @param len Length of the buffer.
 *
 * @return Calculated checksum.
 */
static uint32_t chppCalculateChecksum(uint8_t *buf, size_t len) {
  // TODO

  UNUSED_VAR(buf);
  UNUSED_VAR(len);
  return 1;
}

/**
 * Dequeues the datagram at the front of the datagram tx queue, if any, and
 * frees the payload. Returns false if the queue is empty.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 * @return True indicates success. False indicates failure, i.e. the queue was
 * empty.
 */
bool chppDequeueTxDatagram(struct ChppTransportState *context) {
  bool success = false;

  if (context->txDatagramQueue.pending > 0) {
    chppFree(context->txDatagramQueue.datagram[context->txDatagramQueue.front]
                 .payload);
    context->txDatagramQueue.datagram[context->txDatagramQueue.front].payload =
        NULL;
    context->txDatagramQueue.datagram[context->txDatagramQueue.front].length =
        0;

    context->txDatagramQueue.pending--;
    context->txDatagramQueue.front++;
    context->txDatagramQueue.front %= CHPP_TX_DATAGRAM_QUEUE_LEN;

    // Note: For a future ACK window >1, we need to update the queue position of
    // the datagram being sent as well (relative to the front-of-queue). i.e.
    // context->txStatus.datagramBeingSent--;

    success = true;
  }

  return success;
}

/**
 * Sends out a pending outgoing packet based on a notification from
 * chppEnqueueTxPacket.
 *
 * A payload may or may not be included be according the following:
 * No payload: If Tx datagram queue is empty OR we are waiting on a pending ACK.
 * New payload: If there is one or more pending Tx datagrams and we are not
 * waiting on a pending ACK.
 * Repeat payload: If we haven't received an ACK yet for our previous payload,
 * i.e. we have registered an explicit or implicit NACK.
 *
 * @param context Is used to maintain status. Must be provided and initialized
 * through chppTransportInit for each transport layer instance. Cannot be null.
 */
void chppTransportDoWork(struct ChppTransportState *context) {
  // Note: For a future ACK window >1, there needs to be a loop outside the lock

  chppMutexLock(&context->mutex);

  if (context->txStatus.hasPacketsToSend) {
    // There are pending outgoing packets

    // Lock linkLayerMutex before modifying packetToSend
    chppMutexLock(&context->linkLayerMutex);

    context->packetToSend.length = 0;
    memset(&context->packetToSend.payload, 0, CHPP_LINK_MTU_BYTES);

    // Add preamble
    context->packetToSend.length +=
        chppAddPreamble(&context->packetToSend.payload[0]);

    // Add header
    struct ChppTransportHeader *txHeader =
        (struct ChppTransportHeader *)&context->packetToSend
            .payload[context->packetToSend.length];
    context->packetToSend.length += sizeof(*txHeader);

    txHeader->errorCode = context->txStatus.errorCodeToSend;
    txHeader->ackSeq = context->rxStatus.expectedSeq;

    // If applicable, add payload
    if ((context->txDatagramQueue.pending > 0) &&
        (context->txStatus.sentSeq == context->txStatus.ackedSeq)) {
      // Note: For a future ACK window >1, seq # check should be against the
      // window size.

      // Note: For a future ACK window >1, this is only valid for the
      // (context->rxStatus.receivedErrorCode != CHPP_ERROR_NONE) case,
      // i.e. we have registered an explicit or implicit NACK. Else,
      // txHeader->seq = ++(context->txStatus.sentSeq)
      txHeader->seq = context->txStatus.ackedSeq + 1;
      context->txStatus.sentSeq = txHeader->seq;

      size_t remainingBytes =
          context->txDatagramQueue.datagram[context->txDatagramQueue.front]
              .length -
          context->txStatus.sentLocInDatagram;

      if (remainingBytes > CHPP_TRANSPORT_MTU_BYTES) {
        // Send an unfinished part of a datagram
        txHeader->flags = CHPP_TRANSPORT_FLAG_UNFINISHED_DATAGRAM;
        txHeader->length = CHPP_TRANSPORT_MTU_BYTES;

      } else {
        // Send final (or only) part of a datagram
        txHeader->flags = CHPP_TRANSPORT_FLAG_FINISHED_DATAGRAM;
        txHeader->length = remainingBytes;
      }

      // Copy payload
      memcpy(&context->packetToSend.payload[context->packetToSend.length],
             context->txDatagramQueue.datagram[context->txDatagramQueue.front]
                     .payload +
                 context->txStatus.sentLocInDatagram,
             txHeader->length);
      context->packetToSend.length += txHeader->length;

      context->txStatus.sentLocInDatagram += txHeader->length;

    }  // else {no payload}

    // Note: For a future ACK window >1, this needs to be updated
    context->txStatus.hasPacketsToSend = false;

    // We are done with context. Unlock mutex ASAP.
    chppMutexUnlock(&context->mutex);

    // Populate checksum
    uint32_t *checksum = (uint32_t *)&context->packetToSend
                             .payload[context->packetToSend.length];
    context->packetToSend.length += sizeof(*checksum);
    *checksum = chppCalculateChecksum(context->packetToSend.payload,
                                      context->packetToSend.length);

    // TODO: Send out notification to function that sends out the packet.
    // context->linkLayerMutex must be unlocked by the function that is actually
    // sending out the packet, and only after it is done sending.

    // TODO: Do we even need linkLayerMutex? We'll see once the new approach
    // to signalling is in.

    // TODO: For now, unlocking here, but remove once above is addressed
    chppMutexUnlock(&context->linkLayerMutex);

  } else {
    // There are no pending outgoing packets. Unlock mutex.
    chppMutexUnlock(&context->mutex);
  }
}

/************************************************
 *  Public Functions
 ***********************************************/

void chppTransportInit(struct ChppTransportState *context) {
  CHPP_NOT_NULL(context);

  memset(context, 0, sizeof(struct ChppTransportState));
  chppMutexInit(&context->mutex);
}

bool chppRxDataCb(struct ChppTransportState *context, const uint8_t *buf,
                  size_t len) {
  CHPP_NOT_NULL(buf);
  CHPP_NOT_NULL(context);

  LOGD("chppRxDataCb received %zu bytes (state = %d)", len,
       context->rxStatus.state);

  size_t consumed = 0;
  while (consumed < len) {
    chppMutexLock(&context->mutex);
    // TODO: Investigate fine-grained locking, e.g. separating variables that
    // are only relevant to a particular path.
    // Also consider removing some of the finer-grained locks altogether for
    // non-multithreaded environments with clear documentation.

    switch (context->rxStatus.state) {
      case CHPP_STATE_PREAMBLE:
        consumed +=
            chppConsumePreamble(context, &buf[consumed], len - consumed);
        break;

      case CHPP_STATE_HEADER:
        consumed += chppConsumeHeader(context, &buf[consumed], len - consumed);
        break;

      case CHPP_STATE_PAYLOAD:
        consumed += chppConsumePayload(context, &buf[consumed], len - consumed);
        break;

      case CHPP_STATE_FOOTER:
        consumed += chppConsumeFooter(context, &buf[consumed], len - consumed);
        break;

      default:
        LOGE("Invalid state %d", context->rxStatus.state);
        chppSetRxState(context, CHPP_STATE_PREAMBLE);
    }

    LOGD("chppRxDataCb consumed %zu of %zu bytes (state = %d)", consumed, len,
         context->rxStatus.state);

    chppMutexUnlock(&context->mutex);
  }

  return (context->rxStatus.state == CHPP_STATE_PREAMBLE &&
          context->rxStatus.locInState == 0);
}

void chppTxTimeoutTimerCb(struct ChppTransportState *context) {
  chppMutexLock(&context->mutex);

  // Implicit NACK. Set received error code accordingly
  context->rxStatus.receivedErrorCode = CHPP_ERROR_TIMEOUT;

  // Enqueue Tx packet which will be a retransmission based on the above
  chppEnqueueTxPacket(context, CHPP_ERROR_NONE);

  chppMutexUnlock(&context->mutex);
}

bool chppEnqueueTxDatagram(struct ChppTransportState *context, size_t len,
                           uint8_t *buf) {
  bool success = false;
  chppMutexLock(&context->mutex);

  if (context->txDatagramQueue.pending < CHPP_TX_DATAGRAM_QUEUE_LEN) {
    uint16_t end =
        (context->txDatagramQueue.front + context->txDatagramQueue.pending) %
        CHPP_TX_DATAGRAM_QUEUE_LEN;

    context->txDatagramQueue.datagram[end].length = len;
    context->txDatagramQueue.datagram[end].payload = buf;
    context->txDatagramQueue.pending++;

    if (context->txDatagramQueue.pending == 1) {
      // Queue was empty prior. Need to kickstart transmission.
      chppEnqueueTxPacket(context, CHPP_ERROR_NONE);
    }

    success = true;
  }

  chppMutexUnlock(&context->mutex);

  return success;
}
