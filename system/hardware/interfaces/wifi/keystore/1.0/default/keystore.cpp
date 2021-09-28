#include <android-base/logging.h>
#include <android/security/keystore/BnKeystoreOperationResultCallback.h>
#include <android/security/keystore/BnKeystoreResponseCallback.h>
#include <android/security/keystore/IKeystoreService.h>
#include <binder/IServiceManager.h>
#include <private/android_filesystem_config.h>

#include <keystore/KeyCharacteristics.h>
#include <keystore/KeymasterArguments.h>
#include <keystore/KeymasterBlob.h>
#include <keystore/KeystoreResponse.h>
#include <keystore/OperationResult.h>
#include <keystore/keymaster_types.h>
#include <keystore/keystore.h>
#include <keystore/keystore_hidl_support.h>
#include <keystore/keystore_promises.h>
#include <keystore/keystore_return_types.h>

#include <future>
#include <vector>
#include "include/wifikeystorehal/keystore.h"

#include <ctype.h>
#include <openssl/base.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AT __func__ << ":" << __LINE__ << " "

using android::hardware::keymaster::V4_0::Algorithm;
using android::hardware::keymaster::V4_0::authorizationValue;
using android::hardware::keymaster::V4_0::Digest;
using android::hardware::keymaster::V4_0::KeyFormat;
using android::hardware::keymaster::V4_0::KeyParameter;
using android::hardware::keymaster::V4_0::KeyPurpose;
using android::hardware::keymaster::V4_0::NullOr;
using android::hardware::keymaster::V4_0::PaddingMode;
using android::hardware::keymaster::V4_0::TAG_ALGORITHM;
using android::hardware::keymaster::V4_0::TAG_DIGEST;
using android::hardware::keymaster::V4_0::TAG_PADDING;
using android::security::keymaster::ExportResult;
using android::security::keymaster::KeyCharacteristics;
using android::security::keymaster::KeymasterArguments;
using android::security::keymaster::KeymasterBlob;
using android::security::keymaster::OperationResult;

using KSReturn = keystore::KeyStoreNativeReturnCode;

namespace {

constexpr const char kKeystoreServiceName[] = "android.security.keystore";
constexpr int32_t UID_SELF = -1;

using keystore::KeyCharacteristicsPromise;
using keystore::KeystoreExportPromise;
using keystore::KeystoreResponsePromise;
using keystore::OperationResultPromise;

NullOr<const Algorithm&> getKeyAlgorithmFromKeyCharacteristics(
    const ::android::security::keymaster::KeyCharacteristics& characteristics) {
    for (const auto& param : characteristics.hardwareEnforced.getParameters()) {
        auto algo = authorizationValue(TAG_ALGORITHM, param);
        if (algo.isOk()) return algo;
    }
    for (const auto& param : characteristics.softwareEnforced.getParameters()) {
        auto algo = authorizationValue(TAG_ALGORITHM, param);
        if (algo.isOk()) return algo;
    }
    return {};
}

// Helper method to convert certs in DER format to PERM format required by
// openssl library used by supplicant.
std::vector<uint8_t> convertCertToPem(const std::vector<uint8_t>& cert_bytes) {
    bssl::UniquePtr<BIO> cert_bio(BIO_new_mem_buf(cert_bytes.data(), cert_bytes.size()));
    // Check if the cert is already in PEM format, on devices which have saved
    // credentials from previous releases when upgrading to R.
    bssl::UniquePtr<X509> cert_pem(PEM_read_bio_X509(cert_bio.get(), nullptr, nullptr, nullptr));
    if (cert_pem) {
        LOG(INFO) << AT << "Certificate already in PEM format, returning";
        return cert_bytes;
    }
    // Reset the bio since the pointers will be moved by |PEM_read_bio_X509|.
    BIO_reset(cert_bio.get());
    bssl::UniquePtr<X509> cert(d2i_X509_bio(cert_bio.get(), nullptr));
    if (!cert) {
        LOG(ERROR) << AT << "Could not create cert from BIO";
        return cert_bytes;
    }
    bssl::UniquePtr<BIO> pem_bio(BIO_new(BIO_s_mem()));
    if (!PEM_write_bio_X509(pem_bio.get(), cert.get())) {
        LOG(ERROR) << AT << "Could not convert cert to PEM format";
        return {};
    }
    const uint8_t* pem_bytes;
    size_t pem_len;
    if (!BIO_mem_contents(pem_bio.get(), &pem_bytes, &pem_len)) {
        return {};
    }
    return {pem_bytes, pem_bytes + pem_len};
}
};  // namespace


namespace android {
namespace system {
namespace wifi {
namespace keystore {
namespace V1_0 {
namespace implementation {

using security::keystore::IKeystoreService;
// Methods from ::android::hardware::wifi::keystore::V1_0::IKeystore follow.
Return<void> Keystore::getBlob(const hidl_string& key, getBlob_cb _hidl_cb) {
    sp<IKeystoreService> service = interface_cast<IKeystoreService>(
        defaultServiceManager()->getService(String16(kKeystoreServiceName)));
    if (service == nullptr) {
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }
    ::std::vector<uint8_t> value;
    // Retrieve the blob as wifi user.
    auto ret = service->get(String16(key.c_str()), AID_WIFI, &value);
    if (!ret.isOk()) {
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }
    // convert to PEM before sending it to openssl library.
    std::vector<uint8_t> pem_cert = convertCertToPem(value);
    _hidl_cb(KeystoreStatusCode::SUCCESS, pem_cert);
    return Void();
}

Return<void> Keystore::getPublicKey(const hidl_string& keyId, getPublicKey_cb _hidl_cb) {
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder = sm->getService(String16(kKeystoreServiceName));
    sp<IKeystoreService> service = interface_cast<IKeystoreService>(binder);

    if (service == nullptr) {
        LOG(ERROR) << AT << "could not contact keystore";
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    int32_t error_code;
    android::sp<KeystoreExportPromise> promise(new KeystoreExportPromise);
    auto future = promise->get_future();
    auto binder_result = service->exportKey(
        promise, String16(keyId.c_str()), static_cast<int32_t>(KeyFormat::X509),
        KeymasterBlob() /* clientId */, KeymasterBlob() /* appData */, UID_SELF, &error_code);
    if (!binder_result.isOk()) {
        LOG(ERROR) << AT << "communication error while calling keystore";
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    KSReturn rc(error_code);
    if (!rc.isOk()) {
        LOG(ERROR) << AT << "exportKey failed: " << error_code;
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    auto export_result = future.get();
    if (!export_result.resultCode.isOk()) {
        LOG(ERROR) << AT << "exportKey failed: " << export_result.resultCode;
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    _hidl_cb(KeystoreStatusCode::SUCCESS, export_result.exportData);
    return Void();
}

Return<void> Keystore::sign(const hidl_string& keyId, const hidl_vec<uint8_t>& dataToSign,
                            sign_cb _hidl_cb) {
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder = sm->getService(String16(kKeystoreServiceName));
    sp<IKeystoreService> service = interface_cast<IKeystoreService>(binder);

    if (service == nullptr) {
        LOG(ERROR) << AT << "could not contact keystore";
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    String16 key_name16(keyId.c_str());
    int32_t error_code;
    android::sp<KeyCharacteristicsPromise> kc_promise(new KeyCharacteristicsPromise);
    auto kc_future = kc_promise->get_future();
    auto binder_result = service->getKeyCharacteristics(kc_promise, key_name16, KeymasterBlob(),
                                                        KeymasterBlob(), UID_SELF, &error_code);
    if (!binder_result.isOk()) {
        LOG(ERROR) << AT << "communication error while calling keystore";
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }
    KSReturn rc(error_code);
    if (!rc.isOk()) {
        LOG(ERROR) << AT << "getKeyCharacteristics failed: " << error_code;
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    auto [km_response, characteristics] = kc_future.get();

    if (!KSReturn(km_response.response_code()).isOk()) {
        LOG(ERROR) << AT << "getKeyCharacteristics failed: " << km_response.response_code();
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    auto algorithm = getKeyAlgorithmFromKeyCharacteristics(characteristics);
    if (!algorithm.isOk()) {
        LOG(ERROR) << AT << "could not get algorithm from key characteristics";
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    hidl_vec<KeyParameter> params(3);
    params[0] = Authorization(TAG_DIGEST, Digest::NONE);
    params[1] = Authorization(TAG_PADDING, PaddingMode::NONE);
    params[2] = Authorization(TAG_ALGORITHM, algorithm.value());

    android::sp<android::IBinder> token(new android::BBinder);
    sp<OperationResultPromise> promise(new OperationResultPromise());
    auto future = promise->get_future();
    binder_result = service->begin(promise, token, key_name16, (int)KeyPurpose::SIGN,
                                   true /*pruneable*/, KeymasterArguments(params),
                                   std::vector<uint8_t>() /* entropy */, UID_SELF, &error_code);
    if (!binder_result.isOk()) {
        LOG(ERROR) << AT << "communication error while calling keystore";
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    rc = KSReturn(error_code);
    if (!rc.isOk()) {
        LOG(ERROR) << AT << "Keystore begin returned: " << rc;
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    OperationResult result = future.get();
    if (!result.resultCode.isOk()) {
        LOG(ERROR) << AT << "begin failed: " << result.resultCode;
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }
    auto handle = std::move(result.token);

    const uint8_t* in = dataToSign.data();
    size_t len = dataToSign.size();
    do {
        promise = new OperationResultPromise();
        future = promise->get_future();
        binder_result = service->update(promise, handle, KeymasterArguments(params),
                                        std::vector<uint8_t>(in, in + len), &error_code);
        if (!binder_result.isOk()) {
            LOG(ERROR) << AT << "communication error while calling keystore";
            _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
            return Void();
        }

        rc = KSReturn(error_code);
        if (!rc.isOk()) {
            LOG(ERROR) << AT << "Keystore update returned: " << rc;
            _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
            return Void();
        }

        result = future.get();

        if (!result.resultCode.isOk()) {
            LOG(ERROR) << AT << "update failed: " << result.resultCode;
            _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
            return Void();
        }
        if ((size_t)result.inputConsumed > len) {
            LOG(ERROR) << AT << "update consumed more data than provided";
            sp<KeystoreResponsePromise> abortPromise(new KeystoreResponsePromise);
            auto abortFuture = abortPromise->get_future();
            binder_result = service->abort(abortPromise, handle, &error_code);
            if (!binder_result.isOk()) {
                LOG(ERROR) << AT << "communication error while calling keystore";
                _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
                return Void();
            }
            // This is mainly for logging since we already failed.
            // But if abort returned OK we have to wait untill abort calls the callback
            // hence the call to abortFuture.get().
            if (!KSReturn(error_code).isOk()) {
                LOG(ERROR) << AT << "abort failed: " << error_code;
            } else if (!(rc = KSReturn(abortFuture.get().response_code())).isOk()) {
                LOG(ERROR) << AT << "abort failed: " << rc;
            }
            _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
            return Void();
        }
        len -= result.inputConsumed;
        in += result.inputConsumed;
    } while (len > 0);

    future = {};
    promise = new OperationResultPromise();
    future = promise->get_future();

    binder_result = service->finish(
        promise, handle, KeymasterArguments(params), std::vector<uint8_t>() /* input */,
        std::vector<uint8_t>() /* signature */, std::vector<uint8_t>() /* entropy */, &error_code);
    if (!binder_result.isOk()) {
        LOG(ERROR) << AT << "communication error while calling keystore";
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    rc = KSReturn(error_code);
    if (!rc.isOk()) {
        LOG(ERROR) << AT << "Keystore finish returned: " << rc;
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    result = future.get();

    if (!result.resultCode.isOk()) {
        LOG(ERROR) << AT << "finish failed: " << result.resultCode;
        _hidl_cb(KeystoreStatusCode::ERROR_UNKNOWN, {});
        return Void();
    }

    _hidl_cb(KeystoreStatusCode::SUCCESS, result.data);
    return Void();
}

IKeystore* HIDL_FETCH_IKeystore(const char* /* name */) {
    return new Keystore();
}
}  // namespace implementation
}  // namespace V1_0
}  // namespace keystore
}  // namespace wifi
}  // namespace system
}  // namespace android
