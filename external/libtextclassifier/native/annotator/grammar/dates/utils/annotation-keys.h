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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_UTILS_ANNOTATION_KEYS_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_UTILS_ANNOTATION_KEYS_H_

namespace libtextclassifier3 {
namespace dates {

// Date time specific constants not defined in standard schemas.
//
// Date annotator output two type of annotation. One is date&time like "May 1",
// "12:20pm", etc. Another is range like "2pm - 3pm". The two string identify
// the type of annotation and are used as type in Thing proto.
extern const char* const kDateTimeType;
extern const char* const kDateTimeRangeType;

// kDateTime contains most common field for date time. It's integer array and
// the format is (year, month, day, hour, minute, second, fraction_sec,
// day_of_week). All eight fields must be provided. If the field is not
// extracted, the value is -1 in the array.
extern const char* const kDateTime;

// kDateTimeSupplementary contains uncommon field like timespan, timezone. It's
// integer array and the format is (bc_ad, timespan_code, timezone_code,
// timezone_offset). Al four fields must be provided. If the field is not
// extracted, the value is -1 in the array.
extern const char* const kDateTimeSupplementary;

// kDateTimeRelative contains fields for relative date time. It's integer
// array and the format is (is_future, year, month, day, week, hour, minute,
// second, day_of_week, dow_interpretation*). The first nine fields must be
// provided and dow_interpretation could have zero or multiple values.
// If the field is not extracted, the value is -1 in the array.
extern const char* const kDateTimeRelative;

// Date time range specific constants not defined in standard schemas.
// kDateTimeRangeFrom and kDateTimeRangeTo define the from/to of a date/time
// range. The value is thing object which contains a date time.
extern const char* const kDateTimeRangeFrom;
extern const char* const kDateTimeRangeTo;

}  // namespace dates
}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_UTILS_ANNOTATION_KEYS_H_
