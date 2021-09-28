#GENERATE MANIFEST
$(warning Generating manifest snapshot at $(OUT_DIR)/commit_id.xml...)
$(warning You can disable this by removing this and setting BOARD_RECORD_COMMIT_ID := false in BoardConfig.mk)
$(shell test -d .repo && .repo/repo/repo manifest -r -o $(OUT_DIR)/commit_id.xml)
-include $(TARGET_DEVICE_DIR)/prebuild.mk

