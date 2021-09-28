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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_TYPES_TEST_UTIL_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_TYPES_TEST_UTIL_H_

#include <ostream>

#include "annotator/types.h"
#include "utils/base/logging.h"

namespace libtextclassifier3 {

#define TC3_DECLARE_PRINT_OPERATOR(TYPE_NAME)               \
  inline std::ostream& operator<<(std::ostream& stream,     \
                                  const TYPE_NAME& value) { \
    logging::LoggingStringStream tmp_stream;                \
    tmp_stream << value;                                    \
    return stream << tmp_stream.message;                    \
  }

TC3_DECLARE_PRINT_OPERATOR(AnnotatedSpan)
TC3_DECLARE_PRINT_OPERATOR(ClassificationResult)
TC3_DECLARE_PRINT_OPERATOR(DatetimeParsedData)
TC3_DECLARE_PRINT_OPERATOR(DatetimeParseResultSpan)
TC3_DECLARE_PRINT_OPERATOR(Token)

#undef TC3_DECLARE_PRINT_OPERATOR

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_TYPES_TEST_UTIL_H_
