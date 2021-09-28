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

#ifndef AUDIO_UTILS_HISTOGRAM_H
#define AUDIO_UTILS_HISTOGRAM_H

#include <memory>
#include <sstream>
#include <vector>

namespace android::audio_utils {

class Histogram {
public:
    Histogram(int32_t numBinsInRange, int32_t binWidth)
    : mBinWidth(binWidth)
    , mBins(numBinsInRange + kExtraBins)
    , mLastItemNumbers(mBins.size())
    {
    }

    /**
     * Add another item to the histogram.
     * The value will be divided by the binWidth to determine the bin index.
     * @param value
     */
    void add(int32_t value) {
        int32_t binIndex = (value + mBinWidth) / mBinWidth;
        binIndex = std::max(binIndex, 0); // put values below range in bottom bin
        binIndex = std::min(binIndex, (int32_t)mBins.size() - 1);  // put values below range in top bin
        mBins[binIndex]++;
        mLastItemNumbers[binIndex] = mItemCount++;
    }

    /**
     * Reset all counters to zero.
     */
    void clear() {
        std::fill(mBins.begin(), mBins.end(), 0);
        std::fill(mLastItemNumbers.begin(), mLastItemNumbers.end(), 0);
        mItemCount = 0;
    }

    /**
     * @return original number of bins passed to the constructor
     */
    int32_t getNumBinsInRange() const {
        return mBins.size() - kExtraBins;
    }

    /**
     * @return number of items below the lowest bin
     */
    uint64_t getCountBelowRange() const {
        return mBins[0];
    }

    /**
     * @param binIndex between 0 and numBins-1
     * @return number of items for the given bin index
     */
    uint64_t getCount(int32_t binIndex) const {
        if (binIndex < 0 || binIndex >= getNumBinsInRange()) {
            return 0;
        }
        return mBins[binIndex + 1];
    }

    /**
     * @return total number of items added
     */
    uint64_t getCount() const {
        return mItemCount;
    }

    /**
     * This can be used to determine whether outlying bins were incremented
     * early or late in the process.
     * @param binIndex between 0 and numBins-1
     * @return number of the last item added to this bin
     */
    uint64_t getLastItemNumber(int32_t binIndex) const {
        if (binIndex < 0 || binIndex >= getNumBinsInRange()) {
            return 0;
        }
        return mLastItemNumbers[binIndex + 1];
    }

    /**
     * @return number of items above the highest bin
     */
    uint64_t getCountAboveRange() const {
        return mBins[mBins.size() - 1];
    }

    /**
     * Dump the bins in CSV format, which can be easily imported into a spreadsheet.
     * @return string bins in CSV format
     */
    std::string dump() const {
        std::stringstream result;
        uint64_t count = getCountBelowRange();
        if (count > 0) {
            result << "below range = " << count << std::endl;
        }
        result << "index, start, count, last" << std::endl;
        for (int32_t i = 1; i < mBins.size() - 1; i++) {
            if (mBins[i] > 0) {
                int32_t properIndex = i - 1;
                result << properIndex;
                result << ", "<< (properIndex * mBinWidth);
                result << ", " << mBins[i];
                result << ", " << mLastItemNumbers[i];
                result << std::endl;
            }
        }
        count = getCountAboveRange();
        if (count > 0) {
            result << "above range = " << count << std::endl;
        }
        return result.str();
    }

private:
    static constexpr int kExtraBins = 2; // for out of range values
    const int32_t mBinWidth;
    int64_t mItemCount = 0;
    std::vector<uint64_t> mBins;  // count of the number of items in the range of this bin
    std::vector<uint64_t> mLastItemNumbers; // number of the last item added this bin
};

} // namespace
#endif //AUDIO_UTILS_HISTOGRAM_H
