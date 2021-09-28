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
#ifndef POWERSTATSCOLLECTOR_H
#define POWERSTATSCOLLECTOR_H

#include <memory>
#include <unordered_map>
#include <vector>

#include <pwrstatsutil.pb.h>

using com::google::android::pwrstatsutil::PowerStatistic;
using PowerStatCase = com::google::android::pwrstatsutil::PowerStatistic::PowerStatCase;

/**
 * Classes that inherit from this can be used to provide stats in the form of key/value
 * pairs to PwrStatsUtil.
 **/
class IPowerStatProvider {
  public:
    virtual ~IPowerStatProvider() = default;
    int get(PowerStatistic* stat) const;
    int get(const PowerStatistic& start, PowerStatistic* interval) const;
    void dump(const PowerStatistic& stat, std::ostream* output) const;
    virtual PowerStatCase typeOf() const = 0;

  private:
    virtual int getImpl(PowerStatistic* stat) const = 0;
    virtual int getImpl(const PowerStatistic& start, PowerStatistic* interval) const = 0;
    virtual void dumpImpl(const PowerStatistic& stat, std::ostream* output) const = 0;
};

/**
 * This class is used to return stats in the form of key/value pairs for all registered classes
 * that implement IPowerStatProvider.
 **/
class PowerStatsCollector {
  public:
    PowerStatsCollector() = default;
    int get(std::vector<PowerStatistic>* stats) const;
    int get(const std::vector<PowerStatistic>& start, std::vector<PowerStatistic>* interval) const;
    void dump(const std::vector<PowerStatistic>& stats, std::ostream* output) const;
    void addDataProvider(std::unique_ptr<IPowerStatProvider> statProvider);

  private:
    std::unordered_map<PowerStatCase, std::unique_ptr<IPowerStatProvider>> mStatProviders;
};

int run(int argc, char** argv, const PowerStatsCollector& collector);

#endif  // POWERSTATSCOLLECTOR_H
