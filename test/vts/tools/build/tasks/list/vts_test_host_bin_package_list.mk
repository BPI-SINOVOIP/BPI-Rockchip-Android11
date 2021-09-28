vts_test_host_bin_packages := \
    trace_processor \
    img2simg \
    simg2img \
    mkuserimg_mke2fs

# Need to package mkdtboimg.py since the tool is not just used by the VTS10 test.
vts_test_host_bin_packages += \
    mkdtboimg.py \
    fuzzy_fastboot \
    fastboot \
