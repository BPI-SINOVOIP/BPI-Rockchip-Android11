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

#define LOG_TAG "bt_headless"

#include "test/headless/sdp/sdp_db.h"
#include "base/logging.h"     // LOG() stdout and android log
#include "osi/include/log.h"  // android log only
#include "stack/include/sdp_api.h"
#include "types/bluetooth/uuid.h"
#include "types/raw_address.h"

using namespace bluetooth::test::headless;

SdpDb::SdpDb(unsigned int max_records) : max_records_(max_records) {
  db_ = (tSDP_DISCOVERY_DB*)malloc(max_records_ * sizeof(tSDP_DISC_REC) +
                                   sizeof(tSDP_DISCOVERY_DB));
}

SdpDb::~SdpDb() { free(db_); }

tSDP_DISCOVERY_DB* SdpDb::RawPointer() { return db_; }

uint32_t SdpDb::Length() const {
  return max_records_ * sizeof(tSDP_DISC_REC) + sizeof(tSDP_DISCOVERY_DB);
}

void SdpDb::Print(FILE* filep) const {
  fprintf(filep, "memory size:0x%x free:0x%x\n", db_->mem_size, db_->mem_free);
  fprintf(filep, "number of filters:%hd\n", db_->num_uuid_filters);
  for (int i = 0; i < db_->num_uuid_filters; i++) {
    fprintf(filep, "  uuid:%s\n", db_->uuid_filters[i].ToString().c_str());
  }
  fprintf(filep, "raw data size:0x%x used:0x%x\n", db_->raw_size,
          db_->raw_used);
}
