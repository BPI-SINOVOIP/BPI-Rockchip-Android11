# 512MB specific properties.
# lmkd can kill more now.
PRODUCT_PROPERTY_OVERRIDES += \
    ro.lmk.medium=700
# madvise random in ART to reduce page cache thrashing.
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.madvise-random=true
