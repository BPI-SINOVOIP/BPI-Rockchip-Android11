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

#include "PowerEntityResidencyDataProvider.h"
#include "DataProviderHelper.h"

#include <android-base/logging.h>
#include <android/hardware/power/stats/1.0/IPowerStats.h>

using android::sp;
using android::hardware::Return;

/**
 * Power Entity State Residency data provider:
 * Provides data monitored by Power Stats HAL 1.0
 **/

int PowerEntityResidencyDataProvider::getImpl(PowerStatistic* stat) const {
    sp<android::hardware::power::stats::V1_0::IPowerStats> powerStatsService =
            android::hardware::power::stats::V1_0::IPowerStats::getService();
    if (powerStatsService == nullptr) {
        LOG(ERROR) << "unable to get power.stats HAL service";
        return 1;
    }

    std::unordered_map<uint32_t, std::string> entityNames;
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>> stateNames;

    // Create map of entity names based on entity id
    Return<void> ret;
    ret = powerStatsService->getPowerEntityInfo([&entityNames](auto infos, auto /* status */) {
        for (auto const& info : infos) {
            entityNames.emplace(info.powerEntityId, info.powerEntityName);
        }
    });
    if (!ret.isOk()) {
        LOG(ERROR) << __func__ << ": unable to get entity info";
        return 1;
    }

    // Create map of each entity's states based on entity and state id
    ret = powerStatsService->getPowerEntityStateInfo({}, [&stateNames](auto stateSpaces,
                                                                       auto /* status */) {
        for (auto const& stateSpace : stateSpaces) {
            stateNames.emplace(stateSpace.powerEntityId,
                               std::unordered_map<uint32_t, std::string>());
            auto& entityStateNames = stateNames.at(stateSpace.powerEntityId);
            for (auto const& state : stateSpace.states) {
                entityStateNames.emplace(state.powerEntityStateId, state.powerEntityStateName);
            }
        }
    });
    if (!ret.isOk()) {
        LOG(ERROR) << __func__ << ": unable to get state info";
        return 1;
    }

    // Retrieve residency data and create the PowerStatistic::PowerEntityStateResidency
    ret = powerStatsService->getPowerEntityStateResidencyData({}, [&entityNames, &stateNames,
                                                                   &stat](auto results,
                                                                          auto /* status */) {
        auto residencies = stat->mutable_power_entity_state_residency();
        for (auto const& result : results) {
            for (auto const& curStateResidency : result.stateResidencyData) {
                auto residency = residencies->add_residency();
                residency->set_entity_name(entityNames.at(result.powerEntityId));
                residency->set_state_name(stateNames.at(result.powerEntityId)
                                                  .at(curStateResidency.powerEntityStateId));
                residency->set_time_ms(static_cast<uint64_t>(curStateResidency.totalTimeInStateMs));
            }
        }

        // Sort entries first by entity_name, then by state_name.
        // Sorting is needed to make interval processing efficient.
        std::sort(residencies->mutable_residency()->begin(),
                  residencies->mutable_residency()->end(), [](const auto& a, const auto& b) {
                      if (a.entity_name() != b.entity_name()) {
                          return a.entity_name() < b.entity_name();
                      }

                      return a.state_name() < b.state_name();
                  });
    });
    if (!ret.isOk()) {
        LOG(ERROR) << __func__ << ": Unable to get residency info";
        return 1;
    }

    return 0;
}

int PowerEntityResidencyDataProvider::getImpl(const PowerStatistic& start,
                                              PowerStatistic* interval) const {
    auto startResidency = start.power_entity_state_residency().residency();
    auto intervalResidency = interval->mutable_power_entity_state_residency()->mutable_residency();

    if (0 != StateResidencyInterval(startResidency, intervalResidency)) {
        interval->clear_power_entity_state_residency();
        return 1;
    }

    return 0;
}

void PowerEntityResidencyDataProvider::dumpImpl(const PowerStatistic& stat,
                                                std::ostream* output) const {
    *output << "Power Entity State Residencies:" << std::endl;
    StateResidencyDump(stat.power_entity_state_residency().residency(), output);
}

PowerStatCase PowerEntityResidencyDataProvider::typeOf() const {
    return PowerStatCase::kPowerEntityStateResidency;
}
