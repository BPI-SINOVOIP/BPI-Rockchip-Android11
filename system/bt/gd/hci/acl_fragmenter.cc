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

#include "hci/acl_fragmenter.h"

#include "os/log.h"
#include "packet/fragmenting_inserter.h"

namespace bluetooth {
namespace hci {

AclFragmenter::AclFragmenter(size_t mtu, std::unique_ptr<packet::BasePacketBuilder> packet)
    : mtu_(mtu), packet_(std::move(packet)) {}

std::vector<std::unique_ptr<packet::RawBuilder>> AclFragmenter::GetFragments() {
  std::vector<std::unique_ptr<packet::RawBuilder>> to_return;
  packet::FragmentingInserter fragmenting_inserter(mtu_, std::back_insert_iterator(to_return));
  packet_->Serialize(fragmenting_inserter);
  fragmenting_inserter.finalize();
  return to_return;
}

}  // namespace hci
}  // namespace bluetooth
