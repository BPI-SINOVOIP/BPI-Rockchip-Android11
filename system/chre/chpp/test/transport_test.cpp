/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>

#include "chpp/transport.h"
#include "transport_test.h"

namespace {

// Max size of payload sent to chppRxDataCb (bytes)
constexpr size_t kMaxChunkSize = 20000;

constexpr size_t kMaxPacketSize = kMaxChunkSize + CHPP_PREAMBLE_LEN_BYTES +
                                  sizeof(ChppTransportHeader) +
                                  sizeof(ChppTransportFooter);

// Input sizes to test the entire range of sizes with a few tests
constexpr int kChunkSizes[] = {0,  1,   2,   3,    4,     5,    6,
                               7,  8,   10,  16,   20,    30,   40,
                               51, 100, 201, 1000, 10001, 20000};

/**
 * Adds a CHPP preamble to the specified location of buf
 *
 * @param buf The CHPP preamble will be added to buf
 * @param loc Location of buf where the CHPP preamble will be added
 */
void chppAddPreamble(uint8_t *buf, size_t loc) {
  for (size_t i = 0; i < CHPP_PREAMBLE_LEN_BYTES; i++) {
    buf[loc + i] = static_cast<uint8_t>(
        CHPP_PREAMBLE_DATA >> (CHPP_PREAMBLE_LEN_BYTES - 1 - i) & 0xff);
  }
}

/*
 * Test suite for the CHPP Transport Layer
 */
class TransportTests : public testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    chppTransportInit(&context);
  }

  ChppTransportState context = {};
  uint8_t buf[kMaxPacketSize] = {};
};

/**
 * A series of zeros shouldn't change state from CHPP_STATE_PREAMBLE
 */
TEST_P(TransportTests, ZeroNoPreambleInput) {
  size_t len = static_cast<size_t>(GetParam());
  if (len <= kMaxChunkSize) {
    EXPECT_TRUE(chppRxDataCb(&context, buf, len));
    EXPECT_EQ(context.rxStatus.state, CHPP_STATE_PREAMBLE);
  }
}

/**
 * A preamble after a series of zeros input should change state from
 * CHPP_STATE_PREAMBLE to CHPP_STATE_HEADER
 */
TEST_P(TransportTests, ZeroThenPreambleInput) {
  size_t len = static_cast<size_t>(GetParam());

  if (len <= kMaxChunkSize) {
    // Add preamble at the end of buf
    chppAddPreamble(buf, MAX(0, len - CHPP_PREAMBLE_LEN_BYTES));

    if (len >= CHPP_PREAMBLE_LEN_BYTES) {
      EXPECT_FALSE(chppRxDataCb(&context, buf, len));
      EXPECT_EQ(context.rxStatus.state, CHPP_STATE_HEADER);
    } else {
      EXPECT_TRUE(chppRxDataCb(&context, buf, len));
      EXPECT_EQ(context.rxStatus.state, CHPP_STATE_PREAMBLE);
    }
  }
}

/**
 * Rx Testing with various length payloads of zeros
 */
TEST_P(TransportTests, RxPayloadOfZeros) {
  context.rxStatus.state = CHPP_STATE_HEADER;
  size_t len = static_cast<size_t>(GetParam());

  if (len <= kMaxChunkSize) {
    ChppTransportHeader header{};
    header.flags = 0;
    header.errorCode = 0;
    header.ackSeq = 1;
    header.seq = 0;
    header.length = len;

    memcpy(buf, &header, sizeof(header));

    // Send header and check for correct state
    EXPECT_FALSE(chppRxDataCb(&context, buf, sizeof(ChppTransportHeader)));
    if (len > 0) {
      EXPECT_EQ(context.rxStatus.state, CHPP_STATE_PAYLOAD);
    } else {
      EXPECT_EQ(context.rxStatus.state, CHPP_STATE_FOOTER);
    }

    // Correct decoding of packet length
    EXPECT_EQ(context.rxHeader.length, len);
    EXPECT_EQ(context.rxStatus.locInDatagram, 0);
    EXPECT_EQ(context.rxDatagram.length, len);

    // Send payload if any and check for correct state
    if (len > 0) {
      EXPECT_FALSE(
          chppRxDataCb(&context, &buf[sizeof(ChppTransportHeader)], len));
      EXPECT_EQ(context.rxStatus.state, CHPP_STATE_FOOTER);
    }

    // Should have complete packet payload by now
    EXPECT_EQ(context.rxStatus.locInDatagram, len);

    // But no ACK yet
    EXPECT_FALSE(context.txStatus.hasPacketsToSend);
    EXPECT_EQ(context.txStatus.errorCodeToSend, CHPP_ERROR_NONE);
    EXPECT_EQ(context.rxStatus.expectedSeq, header.seq);

    // Send footer and check for correct state
    EXPECT_TRUE(chppRxDataCb(&context, &buf[sizeof(ChppTransportHeader) + len],
                             sizeof(ChppTransportFooter)));
    EXPECT_EQ(context.rxStatus.state, CHPP_STATE_PREAMBLE);

    // Should have reset loc and length for next packet / datagram
    EXPECT_EQ(context.rxStatus.locInDatagram, 0);
    EXPECT_EQ(context.rxDatagram.length, 0);

    // If payload packet, expect next packet with incremented sequence #
    // Otherwise, should keep previous sequence #
    uint8_t nextSeq = header.seq + ((len > 0) ? 1 : 0);
    EXPECT_EQ(context.rxStatus.expectedSeq, nextSeq);

    // Check for correct ACK crafting if applicable
    // TODO: This will need updating once signalling goes in
    if (len > 0) {
      EXPECT_TRUE(context.txStatus.hasPacketsToSend);
      EXPECT_EQ(context.txStatus.errorCodeToSend, CHPP_ERROR_NONE);
      EXPECT_EQ(context.txDatagramQueue.pending, 0);

      chppTransportDoWork(&context);

      struct ChppTransportHeader *txHeader =
          (struct ChppTransportHeader *)&context.packetToSend
              .payload[CHPP_PREAMBLE_LEN_BYTES];

      EXPECT_EQ(txHeader->flags, CHPP_TRANSPORT_FLAG_FINISHED_DATAGRAM);
      EXPECT_EQ(txHeader->errorCode, CHPP_ERROR_NONE);
      EXPECT_EQ(txHeader->ackSeq, nextSeq);
      EXPECT_EQ(txHeader->length, 0);

      EXPECT_EQ(context.packetToSend.length,
                CHPP_PREAMBLE_LEN_BYTES + sizeof(struct ChppTransportHeader) +
                    sizeof(struct ChppTransportFooter));
    }
  }
}

TEST_P(TransportTests, EnqueueDatagrams) {
  size_t len = static_cast<size_t>(GetParam());

  if (len <= CHPP_TX_DATAGRAM_QUEUE_LEN) {
    // Add (len) datagrams of various length to queue

    int fr = 0;

    for (int j = 0; j == CHPP_TX_DATAGRAM_QUEUE_LEN; j++) {
      for (size_t i = 1; i <= len; i++) {
        uint8_t *buf = (uint8_t *)chppMalloc(i + 100);
        EXPECT_TRUE(chppEnqueueTxDatagram(&context, i + 100, buf));

        EXPECT_EQ(context.txDatagramQueue.pending, i);
        EXPECT_EQ(context.txDatagramQueue.front, fr);
        EXPECT_EQ(context.txDatagramQueue
                      .datagram[(i - 1 + fr) % CHPP_TX_DATAGRAM_QUEUE_LEN]
                      .length,
                  i + 100);
      }

      if (context.txDatagramQueue.pending == CHPP_TX_DATAGRAM_QUEUE_LEN) {
        uint8_t *buf = (uint8_t *)chppMalloc(100);
        EXPECT_FALSE(chppEnqueueTxDatagram(&context, 100, buf));
        chppFree(buf);
      }

      for (size_t i = len; i > 0; i--) {
        fr++;
        fr %= CHPP_TX_DATAGRAM_QUEUE_LEN;

        EXPECT_TRUE(chppDequeueTxDatagram(&context));

        EXPECT_EQ(context.txDatagramQueue.front, fr);
        EXPECT_EQ(context.txDatagramQueue.pending, i - 1);
      }

      EXPECT_FALSE(chppDequeueTxDatagram(&context));

      EXPECT_EQ(context.txDatagramQueue.front, fr);
      EXPECT_EQ(context.txDatagramQueue.pending, 0);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(TransportTestRange, TransportTests,
                         testing::ValuesIn(kChunkSizes));

}  // namespace
