// FIXME: your file license if you have one

#include "Weaver.h"

#include <thread>

#include <log/log.h>
#include <android-base/logging.h>
#include <cutils/properties.h>

extern "C" {
    #include "rk_weaver_entry.h"
}

namespace android {
namespace hardware {
namespace weaver {
namespace V1_0 {
namespace implementation {

Weaver::Weaver() {
    LOG(INFO)<<"Weaver()";

    int rc = rk_tee_weaver_open_session();
    if (rc < 0) {
        LOG(ERROR)<<"Error initializing optee session:" << rc;
    }

    rc = rk_tee_weaver_getconfig((uint8_t*) &_config,sizeof(_config));
    if (rc < 0) {
        LOG(ERROR)<<"Error weaver getconfig:" << rc;
    }

    pkey = (uint8_t*) malloc(sizeof(uint8_t)* _config.slots * _config.keySize);
    pvalue = (uint8_t*) malloc(sizeof(uint8_t)* _config.slots * _config.valueSize);

    rc = rk_tee_weaver_read(pkey, sizeof(uint8_t)* _config.slots * _config.keySize,pvalue,sizeof(uint8_t)* _config.slots * _config.valueSize);
    if (rc < 0) {
        LOG(ERROR)<<"Error weaver read:" << rc;
    }

    for (int i = 0; i < _config.slots; i++) {
        std::vector<uint8_t> key(32);
        std::vector<uint8_t> value(32);
        _key.insert(make_pair(i,key));
        _value.insert(make_pair(i,value));
    }
}

Weaver::~Weaver() {
    LOG(INFO)<<"~Weaver()";
    rk_tee_weaver_close_session();
}

// Methods from ::android::hardware::weaver::V1_0::IWeaver follow.
Return<void> Weaver::getConfig(getConfig_cb _hidl_cb) {
    int rc = rk_tee_weaver_getconfig((uint8_t*) &_config,sizeof(_config));
    if (rc < 0) {
        LOG(ERROR)<<"Error weaver getconfig:" << rc;
    }

    LOG(INFO)<<"getConfig: slots:"<<_config.slots<<" keySize:"<< _config.keySize<<" valueSize:"<<_config.valueSize;
    _hidl_cb(WeaverStatus::OK,_config);
    return Void();
}

Return<::android::hardware::weaver::V1_0::WeaverStatus> Weaver::write(uint32_t slotId, const hidl_vec<uint8_t>& key, const hidl_vec<uint8_t>& value) {
    LOG(INFO)<<"Weaver::write"<<"slotId:"<<slotId;

    if (slotId < 0 || slotId >= _config.slots) {
        LOG(ERROR)<<"Invalid slotId:"<<slotId;
        return WeaverStatus::FAILED;
    }
    if (key.size()> _config.keySize) {
        LOG(ERROR)<<"Invalid key size, large than :"<<_config.keySize;
        return WeaverStatus::FAILED;
    }
    if (value.size()> _config.valueSize) {
        LOG(ERROR)<<"Invalid value size, large than :"<<_config.valueSize;
        return WeaverStatus::FAILED;
    }

    int count = key.size();
    for (int i = 0; i < count;i++)
    {
        pkey[slotId*_config.keySize + i] = key[i];
        //LOG(INFO)<< "key:"<< std::to_string(pkey[slotId*_config.keySize + i]);
    }
    count = value.size();

    for (int i = 0; i < count;i++)
    {
        pvalue[slotId*_config.valueSize + i] = value[i];
        //	LOG(INFO) <<"value:"<<std::to_string(pvalue[slotId*_config.valueSize + i]);
    }

    bool block_thread = property_get_bool("cts_gts.weaver_block_thread", true);
    if (!block_thread) {
        // Use async mode for normal use.
        LOG(INFO)<<"Starting async write...";
        std::thread th([&](){
            LOG(INFO)<<"async write";
            int rc = rk_tee_weaver_write(pkey, sizeof(uint8_t)* _config.slots * _config.keySize,pvalue,sizeof(uint8_t)* _config.slots * _config.valueSize);
            if (rc < 0) {
                LOG(ERROR)<<"Error weaver write:" << rc;
            }
            LOG(INFO)<<"async write complete";
        });
        th.detach();
    } else {
        // Use sync mode for cts & cts-on-gsi.
        LOG(INFO)<<"Starting sync write...";
        int rc = rk_tee_weaver_write(pkey, sizeof(uint8_t)* _config.slots * _config.keySize,pvalue,sizeof(uint8_t)* _config.slots * _config.valueSize);
        if (rc < 0) {
            LOG(ERROR)<<"Error weaver write:" << rc;
        }
        LOG(INFO)<<"Ending sync write...";
    }
    return ::android::hardware::weaver::V1_0::WeaverStatus {};
}

static uint32_t calculateTimeout(int error_count) {
    static const int failure_timeout_ms = 30000;
    if (error_count == 0) return 0;

    if (error_count > 0 && error_count <= 10) {
        if (error_count % 5 == 0) {
            return failure_timeout_ms;
        } else {
            return 0;
        }
    } else if (error_count < 30) {
        return failure_timeout_ms;
    } else if (error_count < 140) {
        return failure_timeout_ms << ((error_count - 30) / 10);
    }

    return 24 * 60 * 60 * 1000;// one day
}

Return<void> Weaver::read(uint32_t slotId, const hidl_vec<uint8_t>& key, read_cb _hidl_cb) {
    LOG(INFO)<<"Weaver::read"<<"slotId:"<<slotId;
    WeaverReadResponse response;

    if (slotId < 0 || slotId >= _config.slots) {
        LOG(ERROR)<<"Invalid slotId:"<<slotId;
        _hidl_cb(WeaverReadStatus::FAILED,response);
        return Void();
    }
    if (key.size()> _config.keySize) {
        LOG(ERROR)<<"Invalid key size, large than :"<<_config.keySize;
        _hidl_cb(WeaverReadStatus::FAILED,response);
        return Void();
    }

    int count = key.size();
    std::vector<uint8_t> responseValue(count);
    for (int i = 0; i < count;i++)
    {
        if (pkey[slotId*_config.keySize + i] != key[i]) {
            errorCount++;
            response.timeout = calculateTimeout(errorCount);
            _hidl_cb(WeaverReadStatus::INCORRECT_KEY,response);
            return Void();
        } else {
            responseValue[i]=pvalue[i+ slotId * _config.valueSize];
            //LOG(INFO) <<"response  value:"<<std::to_string(pvalue[i+ slotId * _config.valueSize]);
        }
    }
    response.value =  responseValue;
    _hidl_cb(WeaverReadStatus::OK,response);
    errorCount = 0;

    return Void();
}


// Methods from ::android::hidl::base::V1_0::IBase follow.

IWeaver* HIDL_FETCH_IWeaver(const char* /* name */) {
    LOG(INFO)<<"HIDL_FETCH_IWeaver";
    return new Weaver();
}
//
}  // namespace implementation
}  // namespace V1_0
}  // namespace weaver
}  // namespace hardware
}  // namespace android
