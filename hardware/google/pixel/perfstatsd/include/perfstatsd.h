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

#ifndef _PERFSTATSD_H_
#define _PERFSTATSD_H_

#include "cpu_usage.h"
#include "io_usage.h"
#include "statstype.h"

#define DEFAULT_DATA_COLLECT_PERIOD (10)  // seconds
#define PERFSTATSD_PERIOD "perfstatsd.period"

namespace android {
namespace pixel {
namespace perfstatsd {

class Perfstatsd : public RefBase {
  private:
    std::list<std::unique_ptr<StatsType>> mStats;
    uint32_t mRefreshPeriod;

  public:
    Perfstatsd(void);
    void refresh(void);
    void pause(void) { sleep(mRefreshPeriod); }
    void getHistory(std::string *ret);
    void setOptions(const std::string &key, const std::string &value);
};

}  // namespace perfstatsd
}  // namespace pixel
}  // namespace android

#endif /* _PERFSTATSD_H_ */
