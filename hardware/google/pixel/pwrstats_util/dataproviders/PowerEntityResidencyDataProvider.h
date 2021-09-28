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
#ifndef POWERENTITYRESIDENCYDATAPROVIDER_H
#define POWERENTITYRESIDENCYDATAPROVIDER_H

#include "PowerStatsCollector.h"

class PowerEntityResidencyDataProvider : public IPowerStatProvider {
  public:
    PowerEntityResidencyDataProvider() = default;
    PowerStatCase typeOf() const override;

  private:
    int getImpl(PowerStatistic* stat) const override;
    int getImpl(const PowerStatistic& start, PowerStatistic* interval) const override;
    void dumpImpl(const PowerStatistic& stat, std::ostream* output) const override;
};

#endif  // POWERENTITYRESIDENCYDATAPROVIDER_H
