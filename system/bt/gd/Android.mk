LOCAL_PATH := $(call my-dir)

bluetooth_cert_test_file_list := \
    $(call all-named-files-under,*.py,cert) \
    $(call all-named-files-under,*.sh,cert) \
    $(call all-named-files-under,*.proto,cert facade hal hci/cert hci/facade l2cap/classic \
	    l2cap/classic/cert neighbor/facade security) \
    cert/cert_testcases_facade_only \
    cert/android_devices_config.json \
    cert/host_only_config_facade_only.json \
    hal/cert/simple_hal_test.py \
    hci/cert/acl_manager_test.py \
    hci/cert/controller_test.py \
    hci/cert/direct_hci_test.py \
    hci/cert/le_acl_manager_test.py \
    hci/cert/le_advertising_manager_test.py \
    hci/cert/le_scanning_manager_test.py \
    l2cap/classic/cert/l2cap_test.py \
    l2cap/classic/cert/pts_l2cap_test.py \
    neighbor/cert/neighbor_test.py \
    security/cert/simple_security_test.py \
    shim/cert/stack_test.py


bluetooth_cert_test_file_list := $(addprefix $(LOCAL_PATH)/,$(bluetooth_cert_test_file_list))

bluetooth_cert_test_file_list += \
    $(HOST_OUT_EXECUTABLES)/bluetooth_stack_with_facade \
    $(HOST_OUT_SHARED_LIBRARIES)/bluetooth_packets_python3.so \
    $(HOST_OUT_SHARED_LIBRARIES)/libbluetooth_gd.so \
    $(HOST_OUT_SHARED_LIBRARIES)/libc++.so \
    $(HOST_OUT_SHARED_LIBRARIES)/libgrpc++_unsecure.so \
    $(TARGET_OUT_EXECUTABLES)/bluetooth_stack_with_facade \
    $(TARGET_OUT_SHARED_LIBRARIES)/libbluetooth_gd.so \
    $(TARGET_OUT_SHARED_LIBRARIES)/libgrpc++_unsecure.so \
    $(HOST_OUT_NATIVE_TESTS)/root-canal/root-canal

bluetooth_cert_zip_path := \
    $(call intermediates-dir-for,PACKAGING,bluetooth_cert_test_package,HOST)/bluetooth_cert_test.zip

$(bluetooth_cert_zip_path): PRIVATE_BLUETOOTH_CERT_TEST_FILE_LIST := $(bluetooth_cert_test_file_list)

$(bluetooth_cert_zip_path) : $(SOONG_ZIP) $(bluetooth_cert_test_file_list)
	$(hide) $(SOONG_ZIP) -d -o $@ $(addprefix -f ,$(PRIVATE_BLUETOOTH_CERT_TEST_FILE_LIST))

$(call dist-for-goals,bluetooth_stack_with_facade,$(bluetooth_cert_zip_path):bluetooth_cert_test.zip)
