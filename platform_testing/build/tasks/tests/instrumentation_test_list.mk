# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

instrumentation_tests := \
    HelloWorldTests \
    BluetoothInstrumentationTests \
    crashcollector \
    LongevityPlatformLibTests \
    ManagedProvisioningTests \
    FrameworksCoreTests \
    FrameworksMockingCoreTests \
    BinderProxyCountingTestApp \
    BinderProxyCountingTestService \
    FrameworksNetTests \
    FrameworksUiServicesTests \
    BstatsTestApp \
    ConnTestApp \
    FrameworksServicesTests \
    FrameworksMockingServicesTests \
    WmTests \
    JobTestApp \
    SuspendTestApp \
    FrameworksUtilTests \
    MtpServiceTests \
    MtpTests \
    DocumentsUITests \
    ShellTests \
    SystemUITests \
    TestablesTests \
    FrameworksWifiApiTests \
    FrameworksWifiTests \
    FrameworksTelephonyTests \
    ContactsProviderTests \
    ContactsProviderTests2 \
    SettingsUnitTests \
    TelecomUnitTests \
    TraceurUiTests \
    AndroidVCardTests \
    PermissionFunctionalTests \
    BlockedNumberProviderTest \
    DownloadAppFunctionalTests \
    NotificationFunctionalTests \
    DynamicCodeLoggerIntegrationTests \
    UsbTests \
    DownloadProviderTests \
    EmergencyInfoUnitTests \
    CalendarProviderTests \
    SettingsLibTests \
    RSTest \
    PrintSpoolerOutOfProcessTests \
    CellBroadcastReceiverUnitTests \
    CellBroadcastServiceTests \
    TelephonyProviderTests \
    CarrierConfigTests \
    TeleServiceTests \
    PresencePollingTests \
    SettingsProviderTest \
    FrameworksLocationTests \
    FrameworksPrivacyLibraryTests \
    SettingsUITests \
    SettingsPerfTests \
    ExtServicesUnitTests\
    FrameworksNetSmokeTests\


# Storage Manager may not exist on device
ifneq ($(filter StorageManager, $(PRODUCT_PACKAGES)),)

instrumentation_tests += StorageManagerUnitTests

endif
