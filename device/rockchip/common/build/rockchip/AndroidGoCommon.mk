PRODUCT_PROPERTY_OVERRIDES += \
    ro.lmk.critical_upgrade=true \
    ro.lmk.upgrade_pressure=40 \
    ro.lmk.downgrade_pressure=60 \
    ro.lmk.kill_heaviest_task=false \
    ro.statsd.enable=true

# set threshold to filter unused apps
PRODUCT_PROPERTY_OVERRIDES += \
    pm.dexopt.downgrade_after_inactive_days=10

# set the compiler filter for shared apks to quicken.
# Rationale: speed has a lot of dex code expansion, it uses more ram and space
# compared to quicken. Using quicken for shared APKs on Go devices may save RAM.
# Note that this is a trade-off: here we trade clean pages for dirty pages,
# extra cpu and battery. That's because the quicken files will be jit-ed in all
# the processes that load of shared apk and the code cache is not shared.
# Some notable apps that will be affected by this are gms and chrome.
# b/65591595.
PRODUCT_PROPERTY_OVERRIDES += \
    pm.dexopt.shared=quicken

# Default heap sizes. Allow up to 256m for large heaps to make sure a single app
# doesn't take all of the RAM.
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.heapgrowthlimit=128m \
    dalvik.vm.heapsize=256m
