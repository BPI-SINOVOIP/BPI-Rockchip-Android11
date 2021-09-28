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

#include "RailEnergyDataProvider.h"
#include <android-base/logging.h>
#include <android/hardware/power/stats/1.0/IPowerStats.h>

using android::sp;
using android::hardware::Return;
using android::hardware::power::stats::V1_0::IPowerStats;
using android::hardware::power::stats::V1_0::Status;

int RailEnergyDataProvider::getImpl(PowerStatistic* stat) const {
    sp<android::hardware::power::stats::V1_0::IPowerStats> powerStatsService =
            android::hardware::power::stats::V1_0::IPowerStats::getService();
    if (powerStatsService == nullptr) {
        LOG(ERROR) << "unable to get power.stats HAL service";
        return 1;
    }

    std::unordered_map<uint32_t, std::string> railNames;

    Return<void> ret;
    Status retStatus = Status::SUCCESS;
    ret = powerStatsService->getRailInfo([&railNames, &retStatus](auto railInfos, auto status) {
        retStatus = status;
        if (status != Status::SUCCESS) {
            return;
        }

        for (auto const& info : railInfos) {
            railNames.emplace(info.index, info.railName);
        }
    });
    if (retStatus == Status::NOT_SUPPORTED) {
        LOG(WARNING) << __func__ << ": rail energy stats not supported";
        return 0;
    }
    if (!ret.isOk() || retStatus != Status::SUCCESS) {
        LOG(ERROR) << __func__ << ": no rail information available";
        return 1;
    }

    auto railEntries = stat->mutable_rail_energy();
    bool resultSuccess = true;
    ret = powerStatsService->getEnergyData(
            {}, [&railEntries, &railNames, &resultSuccess](auto energyData, auto status) {
                if (status != Status::SUCCESS) {
                    LOG(ERROR) << __func__ << ": unable to get rail energy";
                    resultSuccess = false;
                    return;
                }

                for (auto const& energyDatum : energyData) {
                    auto entry = railEntries->add_entry();
                    entry->set_rail_name(railNames.at(energyDatum.index));
                    entry->set_energy_uws(energyDatum.energy);
                }
            });
    if (!ret.isOk() || !resultSuccess) {
        stat->clear_rail_energy();
        LOG(ERROR) << __func__ << ": failed to get rail energy stats";
        return 1;
    }

    // Sort entries by name. Sorting is needed to make interval processing efficient.
    std::sort(railEntries->mutable_entry()->begin(), railEntries->mutable_entry()->end(),
              [](const auto& a, const auto& b) { return a.rail_name() < b.rail_name(); });

    return 0;
}

int RailEnergyDataProvider::getImpl(const PowerStatistic& start, PowerStatistic* interval) const {
    auto startEnergy = start.rail_energy().entry();
    auto intervalEnergy = interval->mutable_rail_energy()->mutable_entry();

    // If start and interval are not the same size then they cannot have matching data
    if (startEnergy.size() != intervalEnergy->size()) {
        LOG(ERROR) << __func__ << ": mismatched data";
        interval->clear_rail_energy();
        return 1;
    }

    for (int i = 0; i < startEnergy.size(); ++i) {
        // Check and make sure each entry matches. Data are in sorted order so if there is a
        // mismatch then we will bail.
        if (startEnergy.Get(i).rail_name() != intervalEnergy->Get(i).rail_name()) {
            LOG(ERROR) << __func__ << ": mismatched data";
            interval->clear_rail_energy();
            return 1;
        }

        auto delta = intervalEnergy->Get(i).energy_uws() - startEnergy.Get(i).energy_uws();
        intervalEnergy->Mutable(i)->set_energy_uws(delta);
    }
    return 0;
}

void RailEnergyDataProvider::dumpImpl(const PowerStatistic& stat, std::ostream* output) const {
    *output << "Rail Energy:" << std::endl;
    for (auto const& rail : stat.rail_energy().entry()) {
        *output << rail.rail_name() << "=" << rail.energy_uws() << std::endl;
    }
    *output << std::endl;
}

PowerStatCase RailEnergyDataProvider::typeOf() const {
    return PowerStatCase::kRailEnergy;
}
