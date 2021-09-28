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
#ifndef NOS_TRANSPORT_CRC16_H
#define NOS_TRANSPORT_CRC16_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calculate the CRC-16 of the data.
 *
 * This uses the CRC-16-IBM variant: 0x8005 (x^16 + x^15 + x^2 + 1)
 */
uint16_t crc16(const void *buf, uint32_t len);

/**
 * Calculates the CRC-16 of the data to extend the given crc.
 *
 * CRC is a linear function so crc16( x | y ) == crc16(x, crc16(y)).
 *
 * This uses the CRC-16-IBM variant: 0x8005 (x^16 + x^15 + x^2 + 1)
 */
uint16_t crc16_update(const void *buf, uint32_t len, uint16_t crc);

#ifdef __cplusplus
}
#endif

#endif /* NOS_TRANSPORT_CRC16_H */
