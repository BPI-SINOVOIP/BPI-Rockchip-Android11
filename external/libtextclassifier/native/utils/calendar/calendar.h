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

#ifndef LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_H_
#define LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_H_

#if defined TC3_CALENDAR_ICU
#include "utils/calendar/calendar-icu.h"
#define INIT_CALENDARLIB_FOR_TESTING(VAR) VAR()
#elif defined TC3_CALENDAR_DUMMY
#include "utils/calendar/calendar-dummy.h"
#define INIT_CALENDARLIB_FOR_TESTING(VAR) VAR()
#elif defined TC3_CALENDAR_APPLE
#include "utils/calendar/calendar-apple.h"
#define INIT_CALENDARLIB_FOR_TESTING(VAR) VAR()
#elif defined TC3_CALENDAR_JAVAICU
#include "utils/calendar/calendar-javaicu.h"
#define INIT_CALENDARLIB_FOR_TESTING(VAR) VAR(nullptr)
#else
#error No TC3_CALENDAR implementation specified.
#endif

#endif  // LIBTEXTCLASSIFIER_UTILS_CALENDAR_CALENDAR_H_
