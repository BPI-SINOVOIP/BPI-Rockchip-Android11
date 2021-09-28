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

#ifndef ANDROID_MEDIA_ECO_DATA_H_
#define ANDROID_MEDIA_ECO_DATA_H_

#include <binder/Parcel.h>
#include <binder/Parcelable.h>

#include <string>
#include <unordered_map>
#include <variant>

namespace android {
namespace media {
namespace eco {

enum class ECODataStatus {
    OK,
    FAIL_TO_INSERT,
    INVALID_ECODATA_TYPE,
    KEY_NOT_EXIST,
    INVALID_VALUE_TYPE,
    INVALID_ARGUMENT,
};

/**
* ECOData is the container for all messages passed between different components in ECOService.
* All messages in ECOServices are represented by a list of key-value pairs.
* For example:
*     "bit-rate" -> 22000000
*     "Provider-Name" -> "QCOM-Video-Encoder".
*     "avg-frame-qp" -> 40
* ECOData follows the same design pattern of AMessage and Metadata in Media Framework. The key
* must be non-empty string. Below are the supported data types:
*
*    //   Item types      set/find function suffixes
*    //   ==========================================
*    //     int32_t                Int32
*    //     int64_t                Int64
*    //     size_t                 Size
*    //     float                  Float
*    //     double                 Double
*    //     String                 String
*
* ECOData does not support duplicate keys with different values. When inserting a key-value pair,
* a new entry will be created if the key does not exist. Othewise, they key's value will be
* overwritten with the new value.
* 
*  Sample usage:
*
*   // Create the ECOData
*   std::unique_ptr<ECOData> data = std::make_unique<ECOData>(ECOData::DATA_TYPE_STATS, 1000);
*
*   // Set the encoder name.
*   data->setString("stats-encoder-type", "google-avc");
*
*   // Set encoding bitrate.
*   data->setInt32("stats-encoder-target-bitrate-bps", 22000000);
*/
class ECOData : public Parcelable {
public:
    using ECODataValueType =
            std::variant<int32_t, int64_t, size_t, float, double, std::string, int8_t>;
    using ECODataKeyValuePair = std::pair<std::string, ECODataValueType>;

    ECOData() : mDataType(0), mDataTimeUs(-1) {}
    ECOData(int32_t type) : mDataType(type), mDataTimeUs(-1) {}
    ECOData(int32_t type, int64_t timeUs) : mDataType(type), mDataTimeUs(timeUs) {}

    // Constants for mDataType.
    typedef enum {
        DATA_TYPE_UNKNOWN = 0,
        /* Data sent from the ECOServiceStatsProvider to ECOService. */
        DATA_TYPE_STATS = 1,
        /* Data sent from the ECOService to ECOServiceInfoListener. */
        DATA_TYPE_INFO = 2,
        /* Configuration data sent by ECOServiceStatsProvider when connects with ECOService. */
        DATA_TYPE_STATS_PROVIDER_CONFIG = 3,
        /* Configuration data sent by ECOServiceInfoListener when connects with ECOService. */
        DATA_TYPE_INFO_LISTENER_CONFIG = 4,
    } ECODatatype;

    // set/find functions that could be used for all the value types.
    ECODataStatus set(const std::string& key, const ECODataValueType& value);
    ECODataStatus find(const std::string& key, ECODataValueType* out) const;

    // Convenient set/find functions for string value type.
    ECODataStatus setString(const std::string& key, const std::string& value);
    ECODataStatus findString(const std::string& key, std::string* out) const;

    // Convenient set/find functions for int32_t value type.
    ECODataStatus setInt32(const std::string& key, int32_t value);
    ECODataStatus findInt32(const std::string& key, int32_t* out) const;

    // Convenient set/find functions for int64_t value type.
    ECODataStatus setInt64(const std::string& key, int64_t value);
    ECODataStatus findInt64(const std::string& key, int64_t* out) const;

    // Convenient set/find functions for float value type.
    ECODataStatus setFloat(const std::string& key, float value);
    ECODataStatus findFloat(const std::string& key, float* out) const;

    // Convenient set/find functions for double value type.
    ECODataStatus setDouble(const std::string& key, double value);
    ECODataStatus findDouble(const std::string& key, double* out) const;

    // Convenient set/find functions for size_t value type.
    ECODataStatus setSize(const std::string& key, size_t value);
    ECODataStatus findSize(const std::string& key, size_t* out) const;

    // Convenient set/find functions for int8_t value type.
    // TODO(hkuang): Add unit test.
    ECODataStatus setInt8(const std::string& key, int8_t value);
    ECODataStatus findInt8(const std::string& key, int8_t* out) const;

    /**
    * Serialization over Binder
    */
    status_t readFromParcel(const Parcel* parcel) override;
    status_t writeToParcel(Parcel* parcel) const override;

    /* Returns the type of the data. */
    int32_t getDataType() const;

    /* Returns the type of the data in string. */
    std::string getDataTypeString() const;

    /* Returns the timestamp associated with the data. */
    int64_t getDataTimeUs() const;

    /* Sets the type of the data. */
    void setDataType(int32_t type);

    /* Sets the timestamp associated with the data. */
    void setDataTimeUs();

    /* Gets the number of keys in the ECOData. */
    size_t getNumOfEntries() const { return mKeyValueStore.size(); }

    /* Whether the ECOData is empty. */
    size_t isEmpty() const { return mKeyValueStore.size() == 0; }

    friend class ECODataKeyValueIterator;

    friend bool copyKeyValue(const ECOData& src, ECOData* dst);

    // Dump the ECOData as a string.
    std::string debugString() const;

protected:
    // ValueType. This must match the index in ECODataValueType.
    enum ValueType {
        kTypeInt32 = 0,
        kTypeInt64 = 1,
        kTypeSize = 2,
        kTypeFloat = 3,
        kTypeDouble = 4,
        kTypeString = 5,
        kTypeInt8 = 6,
    };

    /* The type of the data */
    int32_t mDataType;

    // The timestamp time associated with the data in microseconds. The timestamp should be in
    // boottime time base. This is only used when the data type is stats or info. -1 means
    // unavailable.
    int64_t mDataTimeUs;

    // Internal store for the key value pairs.
    std::unordered_map<std::string, ECODataValueType> mKeyValueStore;

    template <typename T>
    ECODataStatus setValue(const std::string& key, T value);

    template <typename T>
    ECODataStatus findValue(const std::string& key, T* out) const;
};

// A simple ECOData iterator that will iterate over all the key value paris in ECOData.
// To be used like:
// while (it.hasNext()) {
//   entry = it.next();
// }
class ECODataKeyValueIterator {
public:
    ECODataKeyValueIterator(const ECOData& data)
          : mKeyValueStore(data.mKeyValueStore), mBeginReturned(false) {
        mIterator = mKeyValueStore.begin();
    }
    ~ECODataKeyValueIterator() = default;
    bool hasNext();
    ECOData::ECODataKeyValuePair next() const;

private:
    const std::unordered_map<std::string, ECOData::ECODataValueType>& mKeyValueStore;
    std::unordered_map<std::string, ECOData::ECODataValueType>::const_iterator mIterator;
    bool mBeginReturned;
};

}  // namespace eco
}  // namespace media
}  // namespace android

#endif  // ANDROID_MEDIA_ECO_DATA_H_
