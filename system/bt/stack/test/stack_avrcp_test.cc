/*
 * Copyright 2020 The Android Open Source Project
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

#include <arpa/inet.h>  // htons
#include <dlfcn.h>
#include <gtest/gtest.h>

#include "stack/include/avrc_api.h"

class StackAvrcpTest : public ::testing::Test {
 protected:
  StackAvrcpTest() = default;

  virtual ~StackAvrcpTest() = default;
};

TEST_F(StackAvrcpTest, test_avrcp_parse_browse_cmd) {
  uint8_t scratch_buf[512]{};
  tAVRC_MSG msg{};
  tAVRC_COMMAND result{};
  uint8_t browse_cmd_buf[512]{};

  msg.hdr.opcode = AVRC_OP_BROWSE;
  msg.browse.p_browse_data = browse_cmd_buf;
  msg.browse.browse_len = 2;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_BAD_CMD);

  memset(browse_cmd_buf, 0, sizeof(browse_cmd_buf));
  browse_cmd_buf[0] = AVRC_PDU_SET_BROWSED_PLAYER;
  msg.browse.browse_len = 3;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_BAD_CMD);

  msg.browse.browse_len = 5;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_NO_ERROR);

  memset(browse_cmd_buf, 0, sizeof(browse_cmd_buf));
  browse_cmd_buf[0] = AVRC_PDU_GET_FOLDER_ITEMS;
  msg.browse.browse_len = 3;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_BAD_CMD);

  msg.browse.browse_len = 13;
  uint8_t* p = &browse_cmd_buf[3];
  UINT8_TO_STREAM(p, AVRC_SCOPE_NOW_PLAYING);  // scope
  UINT32_TO_STREAM(p, 0x00000001);             // start_item
  UINT32_TO_STREAM(p, 0x00000002);             // end_item
  browse_cmd_buf[12] = 0;                      // attr_count
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_NO_ERROR);

  memset(browse_cmd_buf, 0, sizeof(browse_cmd_buf));
  browse_cmd_buf[0] = AVRC_PDU_CHANGE_PATH;
  msg.browse.browse_len = 3;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_BAD_CMD);

  msg.browse.browse_len = 14;
  p = &browse_cmd_buf[3];
  UINT16_TO_STREAM(p, 0x1234);      // uid_counter
  UINT8_TO_STREAM(p, AVRC_DIR_UP);  // direction
  UINT8_TO_STREAM(p, 0);            // attr_count
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_NO_ERROR);

  memset(browse_cmd_buf, 0, sizeof(browse_cmd_buf));
  browse_cmd_buf[0] = AVRC_PDU_GET_ITEM_ATTRIBUTES;
  msg.browse.browse_len = 3;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_BAD_CMD);

  msg.browse.browse_len = 15;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_NO_ERROR);

  memset(browse_cmd_buf, 0, sizeof(browse_cmd_buf));
  browse_cmd_buf[0] = AVRC_PDU_GET_TOTAL_NUM_OF_ITEMS;
  msg.browse.browse_len = 3;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_BAD_CMD);

  msg.browse.browse_len = 4;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_NO_ERROR);

  memset(browse_cmd_buf, 0, sizeof(browse_cmd_buf));
  browse_cmd_buf[0] = AVRC_PDU_SEARCH;
  msg.browse.browse_len = 3;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_BAD_CMD);

  p = &browse_cmd_buf[3];
  UINT16_TO_STREAM(p, 0x0000);  // charset_id
  UINT16_TO_STREAM(p, 0x0000);  // str_len
  msg.browse.browse_len = 7;
  EXPECT_EQ(AVRC_ParsCommand(&msg, &result, scratch_buf, sizeof(scratch_buf)),
            AVRC_STS_NO_ERROR);
}

TEST_F(StackAvrcpTest, test_avrcp_pdu_register_notification) {
  ASSERT_EQ(htons(0x500), 5);

  struct {
    uint8_t pdu;
    uint8_t reserved;
    uint16_t len;
    struct {
      uint8_t event_id;
      uint32_t param;
    } payload;
  } data = {
      AVRC_PDU_REGISTER_NOTIFICATION,
      0,  // reserved
      htons(sizeof(data.payload)),
      .payload =
          {
              .event_id = 0,
              .param = 0x1234,
          },
  };

  tAVRC_MSG msg = {
      .vendor =
          {
              .hdr =
                  {
                      .ctype = AVRC_CMD_NOTIF,
                      .opcode = AVRC_OP_VENDOR,
                  },
              .p_vendor_data = (uint8_t*)&data,
              .vendor_len = sizeof(data),
          },
  };
  tAVRC_COMMAND result{};

  // Run through all possible event ids
  uint8_t id = 0;
  do {
    data.payload.event_id = id;
    ASSERT_EQ((id == 0 || id > AVRC_NUM_NOTIF_EVENTS) ? AVRC_STS_BAD_PARAM
                                                      : AVRC_STS_NO_ERROR,
              AVRC_Ctrl_ParsCommand(&msg, &result));
  } while (++id != 0);
}
