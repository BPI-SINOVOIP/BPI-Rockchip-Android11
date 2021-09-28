## 9.9\. Data Storage Encryption

All devices MUST meet the requirements of section 9.9.1.
Devices which launched on an API level earlier than that of this document are
exempted from the requirements of sections 9.9.2 and 9.9.3; instead they
MUST meet the requirements in section 9.9 of the Android Compatibility
Definition document corresponding to the API level on which the device launched.

### 9.9.1\. Direct Boot

Device implementations:

*    [C-0-1] MUST implement the [Direct Boot mode](
http://developer.android.com/preview/features/direct-boot.html) APIs even if
they do not support Storage Encryption.

*     [C-0-2] The [`ACTION_LOCKED_BOOT_COMPLETED`](
https://developer.android.com/reference/android/content/Intent.html#ACTION_LOCKED_BOOT_COMPLETED)
and [`ACTION_USER_UNLOCKED`](https://developer.android.com/reference/android/content/Intent.html#ACTION_USER_UNLOCKED)
Intents MUST still be broadcast to signal Direct Boot aware applications that
Device Encrypted (DE) and Credential Encrypted (CE) storage locations are
available for user.

### 9.9.2\. Encryption requirements

Device implementations:

*   [C-0-1] MUST encrypt the application private
data (`/data` partition), as well as the application shared storage partition
(`/sdcard` partition) if it is a permanent, non-removable part of the device.
*   [C-0-2] MUST enable the data storage encryption by default at the time
the user has completed the out-of-box setup experience.
*   [C-0-3] MUST meet the above data storage encryption
requirement via implementing [File Based Encryption](
https://source.android.com/security/encryption/file-based.html) (FBE) and
[Metadata Encryption](https://source.android.com/security/encryption/metadata).

### 9.9.3\. Encryption Methods

If device implementations are encrypted, they:

*    [C-1-1] MUST boot up without challenging the user for credentials and
allow Direct Boot aware apps to access to the Device Encrypted (DE) storage
after the `ACTION_LOCKED_BOOT_COMPLETED` message is broadcasted.
*    [C-1-2] MUST only allow access to Credential Encrypted (CE) storage after
the user has unlocked the device by supplying their credentials
(eg. passcode, pin, pattern or fingerprint) and the `ACTION_USER_UNLOCKED`
message is broadcasted.
*    [C-1-13] MUST NOT offer any method to unlock the CE protected storage
without either the user-supplied credentials, a registered escrow key or a
resume on reboot implementation meeting the requirements in
[section 9.9.4](#9_9_4_resume_on_reboot).
*    [C-1-4] MUST use Verified Boot.
*    [C-1-5] MUST encrypt file contents and filesystem metadata using
AES-256-XTS or Adiantum.  AES-256-XTS refers to the Advanced Encryption Standard
with a 256-bit cipher key length, operated in XTS mode; the full length of the
key is 512 bits.  Adiantum refers to Adiantum-XChaCha12-AES, as specified at
https://github.com/google/adiantum. Filesystem metadata is data such as file
sizes, ownership, modes, and extended attributes (xattrs).
*    [C-1-6] MUST encrypt file names using AES-256-CBC-CTS
or Adiantum.
*    [C-1-12] If the device has Advanced Encryption Standard (AES)
instructions (such as ARMv8 Cryptography Extensions on ARM-based devices, or
AES-NI on x86-based devices) then the AES-based options above for file name,
file contents, and filesystem metadata encryption MUST be used, not Adiantum.
*    [C-1-13] MUST use a cryptographically strong and non-reversible key
derivation function (e.g. HKDF-SHA512) to derive any needed subkeys (e.g.
per-file keys) from the CE and DE keys.  "Cryptographically strong and
non-reversible" means that the key derivation function has a security strength
of at least 256 bits and behaves as a [pseudorandom function
family](https://en.wikipedia.org/w/index.php?title=Pseudorandom_function_family&oldid=928163524)
over its inputs.
*    [C-1-14] MUST NOT use the same File Based Encryption (FBE) keys or subkeys
for different cryptographic purposes (e.g. for both encryption and key
derivation, or for two different encryption algorithms).

*   The keys protecting CE and DE storage areas and filesystem metadata:

   *   [C-1-7] MUST be cryptographically bound to a hardware-backed Keystore.
   This keystore MUST be bound to Verified Boot and the device's hardware
   root of trust.
   *   [C-1-8] CE keys MUST be bound to a user's lock screen credentials.
   *   [C-1-9] CE keys MUST be bound to a default passcode when the user has
not specified lock screen credentials.
   *   [C-1-10] MUST be unique and distinct, in other words no user's CE or DE
   key matches any other user's CE or DE keys.
   *    [C-1-11] MUST use the mandatorily supported ciphers, key lengths and
   modes.

*    SHOULD make preinstalled essential apps (e.g. Alarm, Phone, Messenger)
Direct Boot aware.

The upstream Android Open Source project provides a preferred implementation of
File Based Encryption based on the Linux kernel "fscrypt" encryption feature,
and of Metadata Encryption based on the Linux kernel "dm-default-key" feature.

### 9.9.4\. Resume on Reboot

Resume on Reboot allows unlocking the CE storage of all apps, including those
that do not yet support Direct Boot, after a reboot initiated by an OTA. This
feature enables users to receive notifications from installed apps after the
reboot.

An implementation of Resume-on-Reboot must continue to ensure that when a
device falls into an attacker’s hands, it is extremely difficult for that
attacker to recover the user’s CE-encrypted data, even if the device is powered
on, CE storage is unlocked, and the user has unlocked the device after receiving
an OTA. For insider attack resistance, we also assume the attacker gains access
to broadcast cryptographic signing keys.

Specifically:

*   [C-0-1] CE storage MUST NOT be readable even for the attacker who physically has
the device and then has these capabilities and limitations:

    *   Can use the signing key of any vendor or company to sign arbitrary
        messages.
    *   Can cause an OTA to be received by the device.
    *   Can modify the operation of any hardware (AP, flash etc) except as
        detailed below, but such modification involves a delay of at least an
        hour and a power cycle that destroys RAM contents.
    *   Cannot modify the operation of tamper-resistant hardware (eg Titan M).
    *   Cannot read the RAM of the live device.
    *   Cannot obtain the user’s credential (PIN, pattern, password) or
        otherwise cause it to be entered.

By way of example, a device implementation that implements and complies with all
of the descriptions found [here](https://source.android.com/devices/tech/ota/resume-on-reboot)
will be compliant with [C-0-1].
