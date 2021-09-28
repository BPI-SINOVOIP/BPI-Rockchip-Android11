platform_tests += \
    ActivityManagerPerfTests \
    ActivityManagerPerfTestsStubApp1 \
    ActivityManagerPerfTestsStubApp2 \
    ActivityManagerPerfTestsStubApp3 \
    ActivityManagerPerfTestsTestApp \
    AndroidTVJankTests \
    ApiDemos \
    AppCompatibilityTest \
    AppLaunch \
    AppLaunchWear \
    AppLinkFunctionalTests \
    AppLinkTestApp \
    AppSmoke \
    AppTransitionTests \
    AutoLocTestApp \
    AutoLocVersionedTestApp_v1 \
    AutoLocVersionedTestApp_v2 \
    BackgroundDexOptServiceIntegrationTests \
    BandwidthEnforcementTest \
    BandwidthTests \
    BluetoothTests \
    BootHelperApp \
    BusinessCard \
    CalculatorFunctionalTests \
    CalendarTests \
    camera_client_test \
    camera_metadata_tests \
    CellBroadcastReceiverTests \
    ConnectivityManagerTest \
    ContactsTests \
    CtsCameraTestCases \
    CtsHardwareTestCases \
    DataIdleTest \
    Development \
    DeviceHealthChecks \
    DeviceHealthTests \
    DynamicCodeLoggerIntegrationTests \
    DialerJankTests \
    DownloadManagerTestApp \
    DummyIME \
    ExternalLocAllPermsTestApp \
    ExternalLocTestApp \
    ExternalLocVersionedTestApp_v1 \
    ExternalLocVersionedTestApp_v2 \
    ExternalSharedPermsBTTestApp \
    ExternalSharedPermsDiffKeyTestApp \
    ExternalSharedPermsFLTestApp \
    ExternalSharedPermsTestApp \
    flatland \
    FrameworkPerf \
    FrameworkPermissionTests \
    FrameworksCoreTests \
    FrameworksMockingCoreTests \
    FrameworksPrivacyLibraryTests \
    FrameworksUtilTests \
    InternalLocTestApp \
    JankMicroBenchmarkTests \
    long_trace_config.textproto \
    MemoryUsage \
    MultiDexLegacyTestApp \
    MultiDexLegacyTestApp2 \
    MultiDexLegacyTestServices \
    MultiDexLegacyTestServicesTests \
    MultiDexLegacyVersionedTestApp_v1 \
    MultiDexLegacyVersionedTestApp_v2 \
    MultiDexLegacyVersionedTestApp_v3 \
    net_test_bluetooth \
    net_test_btcore \
    net_test_device \
    net_test_hci \
    net_test_osi \
    NoLocTestApp \
    NoLocVersionedTestApp_v1 \
    NoLocVersionedTestApp_v2 \
    NotificationFunctionalTests \
    NotificationStressTests \
    OverviewFunctionalTests \
    perfetto_trace_processor_shell \
    PerformanceAppTest \
    PerformanceLaunch \
    PermissionFunctionalTests \
    PermissionTestAppMV1 \
    PermissionUtils \
    PlatformCommonScenarioTests \
    PowerPerfTest \
    SettingsUITests \
    SimpleServiceTestApp \
    SimpleTestApp \
    skia_dm \
    skia_nanobench \
    sl4a \
    SmokeTest \
    SmokeTestApp \
    SysAppJankTestsWear \
    TouchLatencyJankTestWear \
    trace_config.textproto \
    trace_config_detailed.textproto \
    trace_config_experimental.textproto \
    UbSystemUiJankTests \
    UbWebViewJankTests \
    UiBench \
    UiBenchJankTests \
    UiBenchJankTestsWear \
    UiBenchMicrobenchmark \
    UpdateExternalLocTestApp_v1_ext \
    UpdateExternalLocTestApp_v2_none \
    UpdateExtToIntLocTestApp_v1_ext \
    UpdateExtToIntLocTestApp_v2_int \
    VersatileTestApp_Auto \
    VersatileTestApp_External \
    VersatileTestApp_Internal \
    VersatileTestApp_None \
    VoiceInteraction \
    WifiStrengthScannerUtil \

ifneq ($(strip $(BOARD_PERFSETUP_SCRIPT)),)
platform_tests += perf-setup.sh
endif

ifneq ($(filter vsoc_arm vsoc_arm64 vsoc_x86 vsoc_x86_64, $(TARGET_BOARD_PLATFORM)),)
  platform_tests += \
    CuttlefishRilTests \
    CuttlefishWifiTests
endif

ifeq ($(HOST_OS),linux)
platform_tests += root-canal
endif
