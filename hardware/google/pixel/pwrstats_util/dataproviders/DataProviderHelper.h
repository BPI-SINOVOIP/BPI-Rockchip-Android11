/*
 * Copyright (C) 2019 The Android Open Source Project
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
#ifndef DATAPROVIDERHELPER_H
#define DATAPROVIDERHELPER_H

#include <pwrstatsutil.pb.h>

using StateResidency_Residency = ::google::protobuf::RepeatedPtrField<
        ::com::google::android::pwrstatsutil::StateResidency_Residency>;
int StateResidencyInterval(const StateResidency_Residency& start,
                           StateResidency_Residency* interval);

void StateResidencyDump(const StateResidency_Residency& stateResidency, std::ostream* output);

#endif  // DATAPROVIDERHELPER_H
