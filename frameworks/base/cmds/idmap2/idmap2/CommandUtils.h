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

#ifndef IDMAP2_IDMAP2_COMMAND_UTILS_H_
#define IDMAP2_IDMAP2_COMMAND_UTILS_H_

#include "idmap2/PolicyUtils.h"
#include "idmap2/Result.h"

android::idmap2::Result<android::idmap2::Unit> Verify(const std::string& idmap_path,
                                                      const std::string& target_path,
                                                      const std::string& overlay_path,
                                                      PolicyBitmask fulfilled_policies,
                                                      bool enforce_overlayable);

#endif  // IDMAP2_IDMAP2_COMMAND_UTILS_H_
