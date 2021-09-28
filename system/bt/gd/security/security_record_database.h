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

#pragma once

#include "security/record/security_record.h"

namespace bluetooth {
namespace security {

class SecurityRecordDatabase {
 public:
  using iterator = std::vector<record::SecurityRecord>::iterator;

  record::SecurityRecord& FindOrCreate(hci::AddressWithType address) {
    auto it = Find(address);
    // Security record check
    if (it != records_.end()) return *it;

    // No security record, create one
    records_.emplace_back(address);
    return records_.back();
  }

  void Remove(const hci::AddressWithType& address) {
    auto it = Find(address);

    // No record exists
    if (it == records_.end()) return;

    record::SecurityRecord& last = records_.back();
    *it = std::move(last);
    records_.pop_back();
  }

  iterator Find(hci::AddressWithType address) {
    for (auto it = records_.begin(); it != records_.end(); ++it) {
      record::SecurityRecord& record = *it;
      if (record.identity_address_.has_value() && record.identity_address_.value() == address) return it;
      if (record.GetPseudoAddress() == address) return it;
      if (record.irk.has_value() && address.IsRpaThatMatchesIrk(record.irk.value())) return it;
    }
    return records_.end();
  }

  std::vector<record::SecurityRecord> records_;
};

}  // namespace security
}  // namespace bluetooth