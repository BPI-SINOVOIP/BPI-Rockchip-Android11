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

#include <stdint.h>

int dn_expand(const uint8_t* msg, const uint8_t* eom, const uint8_t* src, char* dst, int dstsiz);
int dn_comp(const char* src, uint8_t* dst, int dstsiz, uint8_t** dnptrs, uint8_t** lastdnptr);
int dn_skipname(const uint8_t* ptr, const uint8_t* eom);

bool res_hnok(const char* dn);
bool res_dnok(const char* dn);
