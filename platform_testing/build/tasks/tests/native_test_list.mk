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

native_tests := \
    adbd_test \
    audio_health_tests \
    backtrace_test \
    bionic-unit-tests \
    bionic-unit-tests-static \
    bluetoothtbd_test \
    bluetooth_test_common \
    bootstat_tests \
    boringssl_crypto_test \
    boringssl_ssl_test \
    buffer_hub-test \
    buffer_hub_queue-test \
    buffer_hub_queue_producer-test \
    bugreportz_test \
    bsdiff_unittest \
    camera_client_test \
    clatd_test \
    confirmationui_invocation_test \
    crashcollector \
    debuggerd_test \
    dumpstate_test \
    dumpstate_test_fixture \
    dumpsys_test \
    dvr_api-test \
    dvr_buffer_queue-test \
    dvr_display-test \
    gpuservice_unittest \
    gwp_asan_unittest \
    hello_world_test \
    hwui_unit_tests \
    incident_helper_test \
    incidentd_test \
    inputflinger_tests \
    installd_cache_test \
    installd_dexopt_test \
    installd_otapreopt_test \
    installd_service_test \
    installd_utils_test \
    JniInvocation_test \
    libandroidfw_tests \
    libappfuse_test \
    libbase_test \
    libbpf_android_test \
    libcutils_test \
    libcutils_test_static \
    libgui_test \
    libhidl_test \
    libinput_tests \
    libjavacore-unit-tests \
    liblog-unit-tests \
    libminijail_unittest_gtest \
    libnetdbpf_test \
    libprocinfo_test \
    libtextclassifier_tests \
    libsurfaceflinger_unittest \
    libunwindstack_test \
    libvintf_test \
    linker-unit-tests \
    logcat-unit-tests \
    logd-unit-tests \
    kernel-config-unit-tests \
    malloc_debug_unit_tests \
    memory_replay_tests \
    memunreachable_test \
    minadbd_test \
    minikin_tests \
    mj_system_unittest_gtest \
    mj_util_unittest_gtest \
    mtp_ffs_handle_test \
    net_test_audio_a2dp_hw \
    net_test_avrcp \
    net_test_bluetooth \
    net_test_bta \
    net_test_btcore \
    net_test_btif \
    net_test_btif_profile_queue \
    net_test_btpackets \
    net_test_btu_message_loop \
    net_test_device \
    net_test_hci \
    net_test_osi \
    net_test_performance \
    net_test_stack \
    net_test_stack_ad_parser \
    net_test_stack_multi_adv \
    net_test_stack_rfcomm \
    net_test_stack_smp \
    net_test_types \
    netd_integration_test \
    netd_unit_test \
    netdutils_test \
    nfc_test_utils \
    perfetto_integrationtests \
    posix_async_io_test \
    prioritydumper_test \
    puffin_unittest \
    recovery_unit_test \
    resolv_gold_test \
    resolv_integration_test \
    resolv_unit_test \
    scrape_mmap_addr \
    simpleperf_cpu_hotplug_test \
    simpleperf_unit_test \
    statsd_test \
    syscall_filter_unittest_gtest \
    time-unit-tests \
    update_engine_unittests \
    vintf_object_test \
    wificond_unit_test \
    ziparchive-tests \
    GraphicBuffer_test \
    NeuralNetworksTest_mt_static \
    NeuralNetworksTest_operations \
    NeuralNetworksTest_static \
    NeuralNetworksTest_utils \
    SurfaceFlinger_test \
    lmkd_unit_test \
    vrflinger_test

ifeq ($(BOARD_IS_AUTOMOTIVE), true)
native_tests += libwatchdog_test
endif
