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

#ifndef IORAP_BINDER_JOB_SCHEDULED_EVENT_H_
#define IORAP_BINDER_JOB_SCHEDULED_EVENT_H_

#include "binder/common.h"
#include "binder/auto_parcelable.h"
#include "common/introspection.h"

namespace iorap {
namespace binder {

struct JobScheduledEvent : public AutoParcelable<JobScheduledEvent> {
  enum class Type : int32_t {
    kStartJob = 0,
    kStopJob = 1,
  };

  Type type;
  int32_t job_id;

  enum class Sort : int32_t {
    kIdleMaintenance = 0,
  };

  Sort sort;

  constexpr bool operator==(const JobScheduledEvent& other) const {
    return type == other.type
        && job_id == other.job_id
        && sort == other.sort;
  }

  constexpr bool operator!=(const JobScheduledEvent& other) const {
    return !(*this == other);
  }
};

IORAP_INTROSPECT_ADAPT_STRUCT(JobScheduledEvent, type, job_id, sort);

}
}
IORAP_JAVA_NAMESPACE_BINDER_TYPEDEF(JobScheduledEvent)

#endif  // IORAP_BINDER_JOB_SCHEDULED_EVENT_H_
