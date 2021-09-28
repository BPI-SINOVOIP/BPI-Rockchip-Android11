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
#define LOG_TAG "pwrstats_util"

#include "DataProviderHelper.h"
#include <android-base/logging.h>

int StateResidencyInterval(const StateResidency_Residency& startResidency,
                           StateResidency_Residency* intervalResidency) {
    // If start and interval are not the same size then they cannot have matching data
    if (startResidency.size() != intervalResidency->size()) {
        LOG(ERROR) << __func__ << ": mismatched data";
        return 1;
    }

    for (int i = 0; i < startResidency.size(); ++i) {
        // Check and make sure each entry matches. Data are in sorted order so if there is a
        // mismatch then we will bail.
        if (startResidency.Get(i).entity_name() != intervalResidency->Get(i).entity_name() ||
            startResidency.Get(i).state_name() != intervalResidency->Get(i).state_name()) {
            LOG(ERROR) << __func__ << ": mismatched data";
            return 1;
        }

        auto delta = intervalResidency->Get(i).time_ms() - startResidency.Get(i).time_ms();
        intervalResidency->Mutable(i)->set_time_ms(delta);
    }
    return 0;
}

void StateResidencyDump(const StateResidency_Residency& stateResidency, std::ostream* output) {
    for (auto const& residency : stateResidency) {
        *output << residency.entity_name() << ":" << residency.state_name() << "="
                << residency.time_ms() << std::endl;
    }
    *output << std::endl;
}
