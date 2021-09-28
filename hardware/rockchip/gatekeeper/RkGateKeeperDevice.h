#ifndef RK_GATEKEEPER_DEVICE_H_
#define RK_GATEKEEPER_DEVICE_H_

#include <android/hardware/gatekeeper/1.0/IGatekeeper.h>
#include <hidl/Status.h>



namespace android {

/**
 * OPTEE based GateKeeper implementation
 */
class RkGateKeeperDevice : public ::android::hardware::gatekeeper::V1_0::IGatekeeper {
  public:
    RkGateKeeperDevice();

    // Wrappers to translate the gatekeeper HAL API to the Kegyuard Messages API.

    /**
     * Enrolls password_payload, which should be derived from a user selected pin or password,
     * with the authentication factor private key used only for enrolling authentication
     * factor data.
     *
     * Returns: 0 on success or an error code less than 0 on error.
     * On error, enrolled_password_handle will not be allocated.
     */
    ::android::hardware::Return<void> enroll(
            uint32_t uid, const ::android::hardware::hidl_vec<uint8_t>& currentPasswordHandle,
            const ::android::hardware::hidl_vec<uint8_t>& currentPassword,
            const ::android::hardware::hidl_vec<uint8_t>& desiredPassword,
            enroll_cb _hidl_cb) override;

    /**
     * Verifies provided_password matches enrolled_password_handle.
     *
     * Implementations of this module may retain the result of this call
     * to attest to the recency of authentication.
     *
     * On success, writes the address of a verification token to auth_token,
     * usable to attest password verification to other trusted services. Clients
     * may pass NULL for this value.
     *
     * Returns: 0 on success or an error code less than 0 on error
     * On error, verification token will not be allocated
     */
    ::android::hardware::Return<void> verify(
            uint32_t uid, uint64_t challenge,
            const ::android::hardware::hidl_vec<uint8_t>& enrolledPasswordHandle,
            const ::android::hardware::hidl_vec<uint8_t>& providedPassword,
            verify_cb _hidl_cb) override;

    ::android::hardware::Return<void> deleteUser(uint32_t uid, deleteUser_cb _hidl_cb) override;

    ::android::hardware::Return<void> deleteAllUsers(deleteAllUsers_cb _hidl_cb) override;

};

}  // namespace android

#endif  // RK_GATEKEEPER_DEVICE_H_
