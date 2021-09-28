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
#ifndef __CROS_EC_INCLUDE_APP_TRANSPORT_TEST_H
#define __CROS_EC_INCLUDE_APP_TRANSPORT_TEST_H

/* App commands */
#define TRANSPORT_TEST_NOP  5    /* Does nothing successfully */
#define TRANSPORT_TEST_1234 8    /* Returns 0x01020304 successfully */
#define TRANSPORT_TEST_9876 9    /* Only successful if the arg is 0x09080706 */
#define TRANSPORT_TEST_HANG 12   /* Forgets to call app_reply() */

#endif /* __CROS_EC_INCLUDE_APP_TRANSPORT_TEST_H */
