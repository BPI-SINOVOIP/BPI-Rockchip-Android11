## 9.16\. Application Data Migration

If device implementations include a capability to migrate data from a device to
another device and do not limit the application data it copies to what is
configured by the application developer in the manifest via
[android:fullBackupContent](https://developer.android.com/guide/topics/data/autobackup#IncludingFiles)
attribute, they:

*    [C-1-1] MUST NOT initiate transfers of application data from
     devices on which the user has not set a primary authentication as
     described in [9.11.1 Secure Lock Screen and Authentication](
     #9_11_1_secure_lock_screen_and_authentication).
*    [C-1-2] MUST securely confirm the primary authentication on the source
     device and confirm with the user intent to copy the data on the source
     device before any data is transferred.
*    [C-1-3] MUST use security key attestation to ensure that both the source
     device and the target device in the device-to-device migration are
     legitimate Android devices and have a locked bootloader.
*    [C-1-4] MUST only migrate application data to the same application on the
     target device, with the same package name AND signing certificate.
*    [C-1-5] MUST show an indication that the source device has had data
     migrated by a device-to-device data migration in the settings menu. A user
     SHOULD NOT be able to remove this indication.