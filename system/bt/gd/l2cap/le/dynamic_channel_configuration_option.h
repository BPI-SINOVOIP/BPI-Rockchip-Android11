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

#include "l2cap/mtu.h"

namespace bluetooth {
namespace l2cap {
namespace le {

/**
 * Configuration Option specified by L2CAP Channel user on a dynamic channel. L2CAP module will configure the channel
 * based on user provided option.
 */
struct DynamicChannelConfigurationOption {
  /**
   * Maximum SDU size that the L2CAP Channel user is able to receive or send. When the channel is created, the actual
   * MTU is the minimum of the suggested MTU between two devices.
   */
  Mtu mtu = kDefaultClassicMtu;
};

}  // namespace le
}  // namespace l2cap
}  // namespace bluetooth
