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

#pragma once

namespace android {
namespace os {
namespace statsd {

const uint8_t ANNOTATION_ID_IS_UID = 1;
const uint8_t ANNOTATION_ID_TRUNCATE_TIMESTAMP = 2;
const uint8_t ANNOTATION_ID_PRIMARY_FIELD = 3;
const uint8_t ANNOTATION_ID_EXCLUSIVE_STATE = 4;
const uint8_t ANNOTATION_ID_PRIMARY_FIELD_FIRST_UID = 5;
const uint8_t ANNOTATION_ID_TRIGGER_STATE_RESET = 7;
const uint8_t ANNOTATION_ID_STATE_NESTED = 8;

} // namespace statsd
} // namespace os
} // namespace android
