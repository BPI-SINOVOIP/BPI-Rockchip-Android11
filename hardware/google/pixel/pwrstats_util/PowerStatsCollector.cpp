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

#include "PowerStatsCollector.h"

#include <android-base/logging.h>

void PowerStatsCollector::addDataProvider(std::unique_ptr<IPowerStatProvider> p) {
    mStatProviders.emplace(p->typeOf(), std::move(p));
}

int PowerStatsCollector::get(std::vector<PowerStatistic>* stats) const {
    if (!stats) {
        LOG(ERROR) << __func__ << ": bad args; stat is null";
        return 1;
    }

    stats->clear();
    for (auto&& provider : mStatProviders) {
        PowerStatistic stat;
        if (provider.second->get(&stat) != 0) {
            LOG(ERROR) << __func__ << ": a data provider failed";
            stats->clear();
            return 1;
        }

        stats->emplace_back(stat);
    }
    return 0;
}

int PowerStatsCollector::get(const std::vector<PowerStatistic>& start,
                             std::vector<PowerStatistic>* interval) const {
    if (!interval) {
        LOG(ERROR) << __func__ << ": bad args; interval is null";
        return 1;
    }

    interval->clear();
    for (auto const& curStat : start) {
        auto provider = mStatProviders.find(curStat.power_stat_case());
        if (provider == mStatProviders.end()) {
            LOG(ERROR) << __func__ << ": a provider is missing";
            interval->clear();
            return 1;
        }

        PowerStatistic curInterval;
        if (provider->second->get(curStat, &curInterval) != 0) {
            LOG(ERROR) << __func__ << ": a data provider failed";
            interval->clear();
            return 1;
        }
        interval->emplace_back(curInterval);
    }
    return 0;
}

void PowerStatsCollector::dump(const std::vector<PowerStatistic>& stats,
                               std::ostream* output) const {
    if (!output) {
        LOG(ERROR) << __func__ << ": bad args; output is null";
        return;
    }

    for (auto const& stat : stats) {
        auto provider = mStatProviders.find(stat.power_stat_case());
        if (provider == mStatProviders.end()) {
            LOG(ERROR) << __func__ << ": a provider is missing";
            return;
        }

        provider->second->dump(stat, output);
    }
}

int IPowerStatProvider::get(PowerStatistic* stat) const {
    if (!stat) {
        LOG(ERROR) << __func__ << ": bad args; stat is null";
        return 1;
    }

    return getImpl(stat);
}

int IPowerStatProvider::get(const PowerStatistic& start, PowerStatistic* interval) const {
    if (!interval) {
        LOG(ERROR) << __func__ << ": bad args; interval is null";
        return 1;
    }

    if (typeOf() != start.power_stat_case()) {
        LOG(ERROR) << __func__ << ": bad args; start is incorrect type";
        return 1;
    }

    if (0 != getImpl(interval)) {
        LOG(ERROR) << __func__ << ": unable to retrieve stats";
        return 1;
    }

    return getImpl(start, interval);
}

void IPowerStatProvider::dump(const PowerStatistic& stat, std::ostream* output) const {
    if (!output) {
        LOG(ERROR) << __func__ << ": bad args; output is null";
        return;
    }

    if (typeOf() != stat.power_stat_case()) {
        LOG(ERROR) << __func__ << ": bad args; stat is incorrect type";
        return;
    }

    dumpImpl(stat, output);
}
