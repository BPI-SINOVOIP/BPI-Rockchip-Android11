## 9.10\. Device Integrity

The following requirements ensure there is transparency to the status of the
device integrity. Device implementations:

*    [C-0-1] MUST correctly report through the System API method
`PersistentDataBlockManager.getFlashLockState()` whether their bootloader
state permits flashing of the system image. The `FLASH_LOCK_UNKNOWN` state is
reserved for device implementations upgrading from an earlier version of Android
where this new system API method did not exist.

*    [C-0-2] MUST support Verified Boot for device integrity.

If device implementations are already launched without supporting Verified Boot
on an earlier version of Android and can not add support for this
feature with a system software update, they MAY be exempted from the
requirement.

Verified Boot is a feature that guarantees the integrity of the device
software. If device implementations support the feature, they:

*    [C-1-1] MUST declare the platform feature flag
`android.software.verified_boot`.
*    [C-1-2] MUST perform verification on every boot sequence.
*    [C-1-3] MUST start verification from an immutable hardware key that is the
root of trust and go all the way up to the system partition.
*    [C-1-4] MUST implement each stage of verification to check the integrity
and authenticity of all the bytes in the next stage before executing the code in
the next stage.
*    [C-1-5] MUST use verification algorithms as strong as current
recommendations from NIST for hashing algorithms (SHA-256) and public key
sizes (RSA-2048).
*    [C-1-6] MUST NOT allow boot to complete when system verification fails,
unless the user consents to attempt booting anyway, in which case the data from
any non-verified storage blocks MUST not be used.
*    [C-1-7] MUST NOT allow verified partitions on the device to be modified
unless the user has explicitly unlocked the bootloader.
*    [C-SR] If there are multiple discrete chips in the device (e.g. radio,
specialized image processor), the boot process of each of those chips is
STRONGLY RECOMMENDED to verify every stage upon booting.
*    [C-1-8] MUST use tamper-evident storage: for storing whether the
bootloader is unlocked. Tamper-evident storage means that the bootloader can
detect if the storage has been tampered with from inside Android.
*    [C-1-9] MUST prompt the user, while using the device, and
require physical confirmation before allowing a transition from bootloader
locked mode to bootloader unlocked mode.
*    [C-1-10] MUST implement rollback protection for partitions used by Android
(e.g. boot, system partitions) and use tamper-evident storage for storing the
metadata used for determining the minimum allowable OS version.
*    [C-SR] Are STRONGLY RECOMMENDED to verify all privileged app APK files with
a chain of trust rooted in partitions protected by Verified Boot.
*    [C-SR] Are STRONGLY RECOMMENDED to verify any executable artifacts loaded by
a privileged app from outside its APK file (such as dynamically loaded code or
compiled code) before executing them or STRONGLY RECOMMENDED not to execute them
at all.
*    SHOULD implement rollback protection for any component with persistent
firmware (e.g. modem, camera) and SHOULD use tamper-evident storage for
storing the metadata used for determining the minimum allowable version.

If device implementations are already launched without supporting C-1-8 through
C-1-10 on an earlier version of Android and can not add support for
these requirements with a system software update, they MAY be exempted from the
requirements.

The upstream Android Open Source Project provides a preferred implementation of
this feature in the [`external/avb/`](
http://android.googlesource.com/platform/external/avb/)
repository, which can be integrated into the bootloader used for loading
Android.

Device implementations:

*    [C-0-3] MUST support cryptographically verifying file content against a
     trusted key without reading the whole file.
*    [C-0-4] MUST NOT allow the read requests on a protected file to succeed
     when the read content do not verify against a trusted key.
*    [C-0-5] MUST enable the above-described cryptographic file verification
     protection for all files for the package that is installed
     with trusted signature files as described [here](
     https://developer.android.com/preview/security/features/apk-verity).

If device implementations are already launched without the ability to verify
file content against a trusted key on an earlier Android version and can not add
support for this feature with a system software update, they MAY be exempted
from the requirement. The upstream Android Open Source project provides a
preferred implementation of this feature based on the Linux kernel [fs-verity](
https://www.kernel.org/doc/html/latest/filesystems/fsverity.html) feature.

Device implementations:

*    [C-R] Are RECOMMENDED to support the [Android Protected Confirmation API](
https://developer.android.com/preview/features/security.html#user-confirmation).

If device implementations support the Android Protected Confirmation
API they:

*    [C-3-1] MUST report `true` for the [`ConfirmationPrompt.isSupported()`](
https://developer.android.com/reference/android/security/ConfirmationPrompt.html#isSupported%28android.content.Context%29)
API.

*    [C-3-2] MUST ensure that code running in the Android OS including its
     kernel, malicious or otherwise, cannot generate a positive response without
     user interaction.

*    [C-3-3] MUST ensure that the user has been able to review and approve the
     prompted message even in the event that the Android OS, including its kernel,
     is compromised.
