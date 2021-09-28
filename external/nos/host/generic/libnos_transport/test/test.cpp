/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <vector>

#include <gmock/gmock.h>

#include <application.h>
#include <nos/transport.h>

#include "crc16.h"

using ::testing::_;
using ::testing::Args;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::ElementsAreArray;
using ::testing::InSequence;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::StrictMock;

namespace {

struct Device {
  virtual int Read(uint32_t command, uint8_t* buf, uint32_t len) = 0;
  virtual int Write(uint32_t command, const uint8_t* buf, uint32_t len) = 0;
  virtual int WaitForInterrupt(int msecs) = 0;
  virtual int Reset() = 0;
};

struct MockDevice : public Device {
  MOCK_METHOD3(Read, int(uint32_t command, uint8_t* buf, uint32_t len));
  MOCK_METHOD3(Write, int(uint32_t command, const uint8_t* buf, uint32_t len));
  MOCK_METHOD1(WaitForInterrupt, int(int msecs));
  MOCK_METHOD0(Reset, int());
};

// We want to closely examine the interactions with the device to make it a
// strict mock
using CtxType = StrictMock<MockDevice>;

// Forward calls onto the mock
int read_datagram(void* ctx, uint32_t command, uint8_t* buf, uint32_t len) {
  return reinterpret_cast<CtxType*>(ctx)->Read(command, buf, len);
}
int write_datagram(void* ctx, uint32_t command, const uint8_t* buf, uint32_t len) {
  return reinterpret_cast<CtxType*>(ctx)->Write(command, buf, len);
}
int wait_for_interrupt(void* ctx, int msecs) {
  return reinterpret_cast<CtxType*>(ctx)->WaitForInterrupt(msecs);
}
int reset(void* ctx) {
  return reinterpret_cast<CtxType*>(ctx)->Reset();
}
void close_device(void* ctx) {
  delete reinterpret_cast<CtxType*>(ctx);
}

// Implement the datagram API that calls a mock.
extern "C" {
int nos_device_open(const char* device_name, struct nos_device* dev) {
  EXPECT_THAT(device_name, IsNull());
  dev->ctx = new CtxType;
  dev->ops.read = read_datagram;
  dev->ops.write = write_datagram;
  dev->ops.wait_for_interrupt = wait_for_interrupt;
  dev->ops.reset = reset;
  dev->ops.close = close_device;
  return 0;
}
}

// Test fixture that sets up the mocked device.
struct TransportTest : public ::testing::Test {
  virtual void SetUp() override {
    nos_device_open(nullptr, &dev_);
    mock_dev_ = reinterpret_cast<CtxType*>(dev_.ctx);
  }
  virtual void TearDown() override {
    dev_.ops.close(dev_.ctx);
  }

  nos_device* dev() { return &dev_; }
  CtxType& mock_dev() { return *mock_dev_; }

private:
  nos_device dev_;
  CtxType* mock_dev_;
};

uint16_t command_crc(uint32_t command, const uint8_t* args, uint16_t args_len,
                     const transport_command_info* command_info) {
  uint16_t crc = crc16(&args_len, sizeof(args_len));
  crc = crc16_update(args, args_len, crc);
  crc = crc16_update(&command, sizeof(command), crc);
  crc = crc16_update(command_info, sizeof(*command_info), crc);
  return htole16(crc);
}

} // namespace

/* Actions to return mock data */

#define READ_UNSET 0xdf

ACTION(ReadStatusV0_Idle) {
  transport_status* status = (transport_status*)arg1;
  memset(status, READ_UNSET, sizeof(*status));
  status->status = APP_STATUS_IDLE;
  status->reply_len = 0;
}

ACTION(ReadStatusV1_Idle) {
  transport_status* status = (transport_status*)arg1;
  memset(status, READ_UNSET, sizeof(*status));
  status->status = APP_STATUS_IDLE;
  status->reply_len = 0;
  status->length = sizeof(transport_status);
  status->version = TRANSPORT_V1;
  status->flags = 0;
  status->crc = STATUS_CRC_FOR_IDLE;
  status->reply_crc = 0;
}

ACTION(ReadStatusV1_IdleWithBadCrc) {
  transport_status* status = (transport_status*)arg1;
  memset(status, READ_UNSET, sizeof(*status));
  status->status = APP_STATUS_IDLE;
  status->reply_len = 0;
  status->length = sizeof(transport_status);
  status->version = TRANSPORT_V1;
  status->flags = 0;
  status->crc = STATUS_CRC_FOR_IDLE + 1; // <- wrong!
  status->reply_crc = 0;
}

ACTION(ReadStatusV1_Working) {
  transport_status* status = (transport_status*)arg1;
  memset(status, READ_UNSET, sizeof(*status));
  status->status = APP_STATUS_IDLE;
  status->reply_len = 0;
  status->length = sizeof(transport_status);
  status->version = TRANSPORT_V1;
  status->flags = STATUS_FLAG_WORKING;
  status->crc = STATUS_CRC_FOR_WORKING;
  status->reply_crc = 0;
}

ACTION_P(ReadStatusV0_DoneWithData, reply_len) {
  transport_status* status = (transport_status*)arg1;
  memset(status, READ_UNSET, sizeof(*status));
  status->status = APP_STATUS_DONE | APP_SUCCESS;
  status->reply_len = reply_len;
}

ACTION_P2(ReadStatusV1_DoneWithData, reply, reply_len) {
  transport_status* status = (transport_status*)arg1;
  memset(status, READ_UNSET, sizeof(*status));
  status->status = APP_STATUS_DONE | APP_SUCCESS;
  status->reply_len = reply_len;
  status->length = sizeof(transport_status);
  status->version = TRANSPORT_V1;
  status->flags = 0;
  status->reply_crc = crc16(reply, reply_len);
  status->crc = 0;
  status->crc = crc16(status, status->length);
}

ACTION(ReadStatusV1_BadCrc) {
  transport_status* status = (transport_status*)arg1;
  memset(status, READ_UNSET, sizeof(*status));
  status->status = APP_STATUS_DONE | APP_ERROR_CHECKSUM;
  status->reply_len = 0;
  status->length = sizeof(transport_status);
  status->version = TRANSPORT_V1;
  status->flags = 0;
  status->crc = 0x92c0;
  status->reply_crc = 0;
}

ACTION(ReadStatusV42_Working) {
  memset(arg1, 0xb3, STATUS_MAX_LENGTH);
  transport_status* status = (transport_status*)arg1;
  status->status = APP_STATUS_IDLE;
  status->reply_len = 0;
  status->length = STATUS_MAX_LENGTH;
  status->version = 42;
  status->flags = STATUS_FLAG_WORKING;
  status->crc = 0xaec0;
  status->reply_crc = 0;
}

ACTION_P3(ReadData, len, data, size) {
  memset(arg1, READ_UNSET, len);
  memcpy(arg1, data, size);
}

/* Helper macros to expect datagram calls */

#define EXPECT_GET_STATUS_V0_IDLE(app_id) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH)) \
      .WillOnce(DoAll(ReadStatusV0_Idle(), Return(0))); \
} while (0)

#define EXPECT_GET_STATUS_IDLE(app_id) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH)) \
      .WillOnce(DoAll(ReadStatusV1_Idle(), Return(0))); \
} while (0)

#define EXPECT_GET_STATUS_IDLE_WITH_BAD_CRC(app_id) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH)) \
      .WillOnce(DoAll(ReadStatusV1_IdleWithBadCrc(), Return(0))); \
} while (0)

#define EXPECT_GET_STATUS_BAD_CRC(app_id) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH)) \
      .WillOnce(DoAll(ReadStatusV1_BadCrc(), Return(0))); \
} while (0)

#define EXPECT_GET_STATUS_WORKING(app_id) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH)) \
      .WillOnce(DoAll(ReadStatusV1_Working(), Return(0))); \
} while (0)

#define EXPECT_GET_STATUS_V0_DONE(app_id) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH)) \
      .WillOnce(DoAll(ReadStatusV0_DoneWithData(0), Return(0))); \
} while (0)

#define EXPECT_GET_STATUS_V0_DONE_WITH_DATA(app_id, reply_len) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH)) \
      .WillOnce(DoAll(ReadStatusV0_DoneWithData((reply_len)), Return(0))); \
} while (0)

#define EXPECT_GET_STATUS_DONE(app_id) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH)) \
      .WillOnce(DoAll(ReadStatusV1_DoneWithData(nullptr, 0), Return(0))); \
} while (0)

#define EXPECT_GET_STATUS_DONE_WITH_DATA(app_id, reply, reply_len) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH)) \
      .WillOnce(DoAll(ReadStatusV1_DoneWithData((reply), (reply_len)), Return(0))); \
} while (0)

#define EXPECT_GET_STATUS_DONE_BAD_CRC(app_id, reply, reply_len) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH)) \
      .WillOnce(DoAll(ReadStatusV1_DoneBadCrc((reply), (reply_len)), Return(0))); \
} while (0)

#define EXPECT_SEND_DATA(app_id, args, args_len) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_DATA | CMD_TRANSPORT | CMD_PARAM((args_len)); \
  EXPECT_CALL(mock_dev(), Write(command, _, (args_len))) \
      .With(Args<1,2>(ElementsAreArray((uint8_t*)(args), (args_len)))) \
      .WillOnce(Return(0)); \
} while (0)

#define EXPECT_GO_COMMAND(app_id, param, args, args_len, reply_len) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_PARAM((param)); \
  transport_command_info command_info = {}; \
  command_info.length = sizeof(command_info); \
  command_info.version = htole16(TRANSPORT_V1); \
  command_info.reply_len_hint = htole16((reply_len)); \
  command_info.crc = command_crc(command, (args), (args_len), &command_info); \
  EXPECT_CALL(mock_dev(), Write(command, _, command_info.length)) \
      .With(Args<1,2>(ElementsAreArray((uint8_t*)&command_info, command_info.length))) \
      .WillOnce(Return(0)); \
} while (0)

#define EXPECT_RECV_DATA(app_id, len, reply, reply_len) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_IS_DATA | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, (reply_len))) \
      .WillOnce(DoAll(ReadData((len), (reply), (reply_len)), Return(0))); \
} while (0)

#define EXPECT_RECV_MORE_DATA(app_id, len, reply, reply_len) do { \
  const uint32_t command = \
      CMD_ID((app_id)) | CMD_IS_READ | CMD_IS_DATA | CMD_MORE_TO_COME | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Read(command, _, (reply_len))) \
      .WillOnce(DoAll(ReadData((len), (reply), (reply_len)), Return(0))); \
} while (0)

#define EXPECT_CLEAR_STATUS(app_id) do { \
  const uint32_t command = CMD_ID((app_id)) | CMD_TRANSPORT; \
  EXPECT_CALL(mock_dev(), Write(command, _, 0)) \
      .WillOnce(Return(0)); \
} while (0)

/* Protocol tests */

TEST_F(TransportTest, WorkingAppIsBusy) {
  const uint8_t app_id = 213;
  EXPECT_GET_STATUS_WORKING(app_id);

  const uint16_t param = 2;
  uint32_t reply_len = 0;
  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, nullptr, &reply_len);
  EXPECT_THAT(res, Eq(APP_ERROR_BUSY));
}

TEST_F(TransportTest, WorkingIsForwardCompatible) {
  const uint8_t app_id = 25;
  const uint32_t command = CMD_ID(app_id) | CMD_IS_READ | CMD_TRANSPORT;
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH))
      .WillOnce(DoAll(ReadStatusV42_Working(), Return(0)));

  const uint16_t param = 2;
  uint32_t reply_len = 0;
  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, nullptr, &reply_len);
  EXPECT_THAT(res, Eq(APP_ERROR_BUSY));
}

TEST_F(TransportTest, SuccessIfStatusNotClear) {
  const uint8_t app_id = 12;
  const uint16_t param = 2;
  const uint8_t args[] = {1, 2, 3};
  const uint16_t args_len = 3;

  InSequence please;
  EXPECT_GET_STATUS_BAD_CRC(app_id);
  // Try and reset
  EXPECT_CLEAR_STATUS(app_id);
  // Try again
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, args, args_len);
  EXPECT_GO_COMMAND(app_id, param, args, args_len, 0);
  EXPECT_GET_STATUS_WORKING(app_id);
  EXPECT_GET_STATUS_DONE(app_id);
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, args, args_len, nullptr, nullptr);
  EXPECT_THAT(res, Eq(APP_SUCCESS));
}

TEST_F(TransportTest, StatusCrcError) {
  const uint8_t app_id = 53;
  const uint16_t param = 192;

  InSequence please;
  // Try 5 times
  EXPECT_GET_STATUS_IDLE_WITH_BAD_CRC(app_id);
  EXPECT_GET_STATUS_IDLE_WITH_BAD_CRC(app_id);
  EXPECT_GET_STATUS_IDLE_WITH_BAD_CRC(app_id);
  EXPECT_GET_STATUS_IDLE_WITH_BAD_CRC(app_id);
  EXPECT_GET_STATUS_IDLE_WITH_BAD_CRC(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, nullptr, nullptr);
  EXPECT_THAT(res, Eq(APP_ERROR_IO));
}

TEST_F(TransportTest, FailToClearStatus) {
  const uint8_t app_id = 12;
  const uint16_t param = 2;
  const uint8_t args[] = {1, 2, 3};
  const uint16_t args_len = 3;

  InSequence please;
  EXPECT_GET_STATUS_BAD_CRC(app_id);
  // Try and reset
  EXPECT_CLEAR_STATUS(app_id);
  // No luck
  EXPECT_GET_STATUS_BAD_CRC(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, args, args_len, nullptr, nullptr);
  EXPECT_THAT(res, Eq(APP_ERROR_IO));
}

TEST_F(TransportTest, FailToClearStatusAfterStatusCrcError) {
  const uint8_t app_id = 53;
  const uint16_t param = 192;

  InSequence please;
  // Try 5 times
  EXPECT_GET_STATUS_IDLE_WITH_BAD_CRC(app_id);
  EXPECT_GET_STATUS_IDLE_WITH_BAD_CRC(app_id);
  EXPECT_GET_STATUS_IDLE_WITH_BAD_CRC(app_id);
  EXPECT_GET_STATUS_IDLE_WITH_BAD_CRC(app_id);
  EXPECT_GET_STATUS_BAD_CRC(app_id);
  // Try and reset
  EXPECT_CLEAR_STATUS(app_id);
  // No luck
  EXPECT_GET_STATUS_BAD_CRC(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, nullptr, nullptr);
  EXPECT_THAT(res, Eq(APP_ERROR_IO));
}

TEST_F(TransportTest, RequestCrcError) {
  const uint8_t app_id = 58;
  const uint16_t param = 93;
  const uint8_t args[] = {4, 24, 183, 255, 219};
  const uint16_t args_len = 5;

  InSequence please;
  // Should try 5 times
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, args, args_len);
  EXPECT_GO_COMMAND(app_id, param, args, args_len, 0);
  EXPECT_GET_STATUS_BAD_CRC(app_id);
  // 4 more
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, args, args_len);
  EXPECT_GO_COMMAND(app_id, param, args, args_len, 0);
  EXPECT_GET_STATUS_BAD_CRC(app_id);
  // 3 more
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, args, args_len);
  EXPECT_GO_COMMAND(app_id, param, args, args_len, 0);
  EXPECT_GET_STATUS_BAD_CRC(app_id);
  // 2 more
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, args, args_len);
  EXPECT_GO_COMMAND(app_id, param, args, args_len, 0);
  EXPECT_GET_STATUS_BAD_CRC(app_id);
  // last one
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, args, args_len);
  EXPECT_GO_COMMAND(app_id, param, args, args_len, 0);
  EXPECT_GET_STATUS_BAD_CRC(app_id);
  // Clean up
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, args, args_len, nullptr, nullptr);
  EXPECT_THAT(res, Eq(APP_ERROR_IO));
}

TEST_F(TransportTest, SuccessAfterRequestCrcError) {
  const uint8_t app_id = 255;
  const uint16_t param = 163;
  const uint8_t args[] = {42, 89, 125, 0, 83, 92, 80};
  const uint16_t args_len = 7;

  InSequence please;
  // First request is CRC error
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, args, args_len);
  EXPECT_GO_COMMAND(app_id, param, args, args_len, 0);
  EXPECT_GET_STATUS_BAD_CRC(app_id);
  // The retry succeeds
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, args, args_len);
  EXPECT_GO_COMMAND(app_id, param, args, args_len, 0);
  EXPECT_GET_STATUS_WORKING(app_id);
  EXPECT_GET_STATUS_DONE(app_id);
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, args, args_len, nullptr, nullptr);
  EXPECT_THAT(res, Eq(APP_SUCCESS));
}

TEST_F(TransportTest, SuccessWithoutReply) {
  const uint8_t app_id = 12;
  const uint16_t param = 2;
  const uint8_t args[] = {1, 2, 3};
  const uint16_t args_len = 3;

  InSequence please;
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, args, args_len);
  EXPECT_GO_COMMAND(app_id, param, args, args_len, 0);
  EXPECT_GET_STATUS_WORKING(app_id);
  EXPECT_GET_STATUS_DONE(app_id);
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, args, args_len, nullptr, nullptr);
  EXPECT_THAT(res, Eq(APP_SUCCESS));
}

TEST_F(TransportTest, DetectAppAbort) {
  const uint8_t app_id = 25;
  const uint16_t param = 252;
  const uint8_t args[] = {17, 27, 43, 193};
  const uint16_t args_len = 4;

  InSequence please;
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, args, args_len);
  EXPECT_GO_COMMAND(app_id, param, args, args_len, 0);
  EXPECT_GET_STATUS_WORKING(app_id);
  EXPECT_GET_STATUS_WORKING(app_id);
  EXPECT_GET_STATUS_WORKING(app_id);
  // It just stopped working
  EXPECT_GET_STATUS_IDLE(app_id);
  // It's probably already clear but just making sure
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, args, args_len, nullptr, nullptr);
  EXPECT_THAT(res, Eq(APP_ERROR_INTERNAL));
}

TEST_F(TransportTest, SuccessWithReply) {
  const uint8_t app_id = 165;
  const uint16_t param = 16;
  const uint8_t data[] = {5, 6, 7, 8};
  uint8_t reply[4];
  uint32_t reply_len = 4;

  InSequence please;
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, nullptr, 0);
  EXPECT_GO_COMMAND(app_id, param, nullptr, 0, reply_len);
  EXPECT_GET_STATUS_DONE_WITH_DATA(app_id, data, sizeof(data));
  EXPECT_RECV_DATA(app_id, reply_len, data, sizeof(data));
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, reply, &reply_len);
  EXPECT_THAT(res, Eq(APP_SUCCESS));
  EXPECT_THAT(reply_len, Eq(4));
  EXPECT_THAT(reply, ElementsAreArray(data, sizeof(data)));
}

TEST_F(TransportTest, SuccessWithReplyInMultipleDatagrams) {
  const uint8_t app_id = 165;
  const uint16_t param = 16;
  std::vector<uint8_t> data(MAX_DEVICE_TRANSFER + 24, 0xea);
  std::vector<uint8_t> reply(data.size());
  uint32_t reply_len = reply.size();

  InSequence please;
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, nullptr, 0);
  EXPECT_GO_COMMAND(app_id, param, nullptr, 0, reply_len);
  EXPECT_GET_STATUS_DONE_WITH_DATA(app_id, data.data(), data.size());
  EXPECT_RECV_DATA(app_id, reply_len, data.data(), MAX_DEVICE_TRANSFER);
  EXPECT_RECV_MORE_DATA(app_id, 24, data.data() + MAX_DEVICE_TRANSFER, 24);
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, reply.data(), &reply_len);
  EXPECT_THAT(res, Eq(APP_SUCCESS));
  EXPECT_THAT(reply_len, Eq(MAX_DEVICE_TRANSFER + 24));
  EXPECT_THAT(reply, ElementsAreArray(data));
}

TEST_F(TransportTest, ReplyCrcError) {
  const uint8_t app_id = 5;
  const uint16_t param = 0;
  const uint8_t data[] = {1, 1, 2, 3, 5, 7};
  const uint8_t wrong_data[] = {3, 1, 2, 3, 5, 7};
  uint8_t reply[6];
  uint32_t reply_len = 6;

  InSequence please;
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, nullptr, 0);
  EXPECT_GO_COMMAND(app_id, param, nullptr, 0, reply_len);
  EXPECT_GET_STATUS_DONE_WITH_DATA(app_id, data, sizeof(data));
  // Try 5 times to read data
  EXPECT_RECV_DATA(app_id, reply_len, wrong_data, sizeof(wrong_data));
  EXPECT_RECV_DATA(app_id, reply_len, wrong_data, sizeof(wrong_data));
  EXPECT_RECV_DATA(app_id, reply_len, wrong_data, sizeof(wrong_data));
  EXPECT_RECV_DATA(app_id, reply_len, wrong_data, sizeof(wrong_data));
  EXPECT_RECV_DATA(app_id, reply_len, wrong_data, sizeof(wrong_data));

  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, reply, &reply_len);
  EXPECT_THAT(res, Eq(APP_ERROR_IO));
}

TEST_F(TransportTest, SuccessAfterReplyCrcError) {
  const uint8_t app_id = 5;
  const uint16_t param = 0;
  const uint8_t data[] = {2, 4, 9, 16};
  const uint8_t wrong_data[] = {2, 4, 9, 48};
  uint8_t reply[4];
  uint32_t reply_len = 4;

  InSequence please;
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, nullptr, 0);
  EXPECT_GO_COMMAND(app_id, param, nullptr, 0, reply_len);
  EXPECT_GET_STATUS_DONE_WITH_DATA(app_id, data, sizeof(data));
  // Retry due to crc error
  EXPECT_RECV_DATA(app_id, reply_len, wrong_data, sizeof(wrong_data));
  EXPECT_RECV_DATA(app_id, reply_len, wrong_data, sizeof(wrong_data));
  EXPECT_RECV_DATA(app_id, reply_len, data, sizeof(data));
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, reply, &reply_len);
  EXPECT_THAT(res, Eq(APP_SUCCESS));
  EXPECT_THAT(reply_len, Eq(4));
  EXPECT_THAT(reply, ElementsAreArray(data, sizeof(data)));
}

TEST_F(TransportTest, V0SuccessWithoutReply) {
  const uint8_t app_id = 6;
  const uint16_t param = 92;

  InSequence please;
  EXPECT_GET_STATUS_V0_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, nullptr, 0);
  EXPECT_GO_COMMAND(app_id, param, nullptr, 0, 0);
  EXPECT_GET_STATUS_V0_DONE(app_id);
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, nullptr, nullptr);
  EXPECT_THAT(res, Eq(APP_SUCCESS));
}

TEST_F(TransportTest, V0SuccessWithReply) {
  const uint8_t app_id = 0;
  const uint16_t param = 18;
  const uint8_t data[] = {15, 20, 25, 30, 35, 40};
  uint8_t reply[6];
  uint32_t reply_len = 6;

  InSequence please;
  EXPECT_GET_STATUS_V0_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, nullptr, 0);
  EXPECT_GO_COMMAND(app_id, param, nullptr, 0, reply_len);
  EXPECT_GET_STATUS_V0_DONE_WITH_DATA(app_id, sizeof(data));
  EXPECT_RECV_DATA(app_id, reply_len, data, sizeof(data));
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, reply, &reply_len);
  EXPECT_THAT(res, Eq(APP_SUCCESS));
  EXPECT_THAT(reply_len, Eq(6));
  EXPECT_THAT(reply, ElementsAreArray(data, sizeof(data)));
}

TEST_F(TransportTest, ErrorIfArgsLenButNotArgs) {
  uint8_t reply[] = {1, 2, 3};
  uint32_t reply_len = 0;
  uint32_t status = nos_call_application(dev(), 1, 2, nullptr, 5, reply, &reply_len);
  EXPECT_THAT(status, Eq(APP_ERROR_IO));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#ifdef TEST_TIMEOUT
TEST_F(TransportTest, Timeout) {
  const uint8_t app_id = 49;
  const uint16_t param = 64;

  InSequence please;
  EXPECT_GET_STATUS_IDLE(app_id);
  EXPECT_SEND_DATA(app_id, nullptr, 0);
  EXPECT_GO_COMMAND(app_id, param, nullptr, 0, 0);

  // Keep saying we're working on it
  const uint32_t command = CMD_ID((app_id)) | CMD_IS_READ | CMD_TRANSPORT;
  EXPECT_CALL(mock_dev(), Read(command, _, STATUS_MAX_LENGTH))
      .WillRepeatedly(DoAll(ReadStatusV1_Working(), Return(0)));

  // We'll still try and clean up
  EXPECT_CLEAR_STATUS(app_id);

  uint32_t res = nos_call_application(dev(), app_id, param, nullptr, 0, nullptr, nullptr);
  EXPECT_THAT(res, Eq(APP_ERROR_TIMEOUT));
}
#endif
