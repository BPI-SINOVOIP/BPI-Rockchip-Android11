hardware_modules := \
    camera \
    gralloc \
    sensors \
    hw_output
include $(call all-named-subdir-makefiles,$(hardware_modules))
