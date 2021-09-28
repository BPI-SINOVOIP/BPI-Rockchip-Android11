// FIXME: your file license if you have one

#pragma once

#include <android/hardware/weaver/1.0/IWeaver.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <map>

namespace android {
namespace hardware {
namespace weaver {
namespace V1_0 {
namespace implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct Weaver : public IWeaver {
    Weaver();
    ~Weaver();

    // Methods from ::android::hardware::weaver::V1_0::IWeaver follow.
    Return<void> getConfig(getConfig_cb _hidl_cb) override;
    Return<::android::hardware::weaver::V1_0::WeaverStatus> write(uint32_t slotId, const hidl_vec<uint8_t>& key, const hidl_vec<uint8_t>& value) override;
    Return<void> read(uint32_t slotId, const hidl_vec<uint8_t>& key, read_cb _hidl_cb) override;


    // Methods from ::android::hidl::base::V1_0::IBase follow.
private:
    WeaverConfig _config;
    uint8_t* pkey;
    uint8_t* pvalue;
    uint32_t errorCount = 0;
    std::map<uint32_t,std::vector<uint8_t>> _key;
    std::map<uint32_t,std::vector<uint8_t>> _value;

};

// FIXME: most likely delete, this is only for passthrough implementations
 extern "C" IWeaver* HIDL_FETCH_IWeaver(const char* name);

}  // namespace implementation
}  // namespace V1_0
}  // namespace weaver
}  // namespace hardware
}  // namespace android
