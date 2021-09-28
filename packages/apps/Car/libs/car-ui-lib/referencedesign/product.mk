# Inherit from this product to include the "Reference Design" RROs for CarUi

# Include generated RROs
PRODUCT_PACKAGES += \
    googlecarui-com-android-car-ui-paintbooth \
    googlecarui-com-google-android-car-ui-paintbooth \
    googlecarui-com-android-car-rotaryplayground \
    googlecarui-com-android-car-themeplayground \
    googlecarui-com-android-car-carlauncher \
    googlecarui-com-android-car-home \
    googlecarui-com-android-car-media \
    googlecarui-com-android-car-radio \
    googlecarui-com-android-car-calendar \
    googlecarui-com-android-car-companiondevicesupport \
    googlecarui-com-android-car-systemupdater \
    googlecarui-com-android-car-dialer \
    googlecarui-com-android-car-linkviewer \
    googlecarui-com-android-car-settings \
    googlecarui-com-android-car-voicecontrol \
    googlecarui-com-android-car-faceenroll \
    googlecarui-com-android-settings-intelligence \
    googlecarui-com-google-android-apps-automotive-inputmethod \
    googlecarui-com-google-android-apps-automotive-inputmethod-dev \
    googlecarui-com-google-android-apps-automotive-templates-host \
    googlecarui-com-google-android-embedded-projection \
    googlecarui-com-google-android-gms \
    googlecarui-com-google-android-gsf \
    googlecarui-com-google-android-packageinstaller \
    googlecarui-com-google-android-carassistant \
    googlecarui-com-google-android-tts \
    googlecarui-com-android-managedprovisioning \
    googlecarui-com-android-vending \


# Include generated RROs that that use targetName
PRODUCT_PACKAGES += \
    googlecarui-overlayable-com-google-android-apps-automotive-inputmethod \
    googlecarui-overlayable-com-google-android-apps-automotive-inputmethod-dev \
    googlecarui-overlayable-com-google-android-apps-automotive-templates-host \
    googlecarui-overlayable-com-google-android-embedded-projection \
    googlecarui-overlayable-com-google-android-gms \
    googlecarui-overlayable-com-google-android-gsf \
    googlecarui-overlayable-com-google-android-packageinstaller \
    googlecarui-overlayable-com-google-android-permissioncontroller \
    googlecarui-overlayable-com-google-android-carassistant \
    googlecarui-overlayable-com-google-android-tts \
    googlecarui-overlayable-com-android-vending \

# This system property is used to enable the RROs on startup via
# the requiredSystemPropertyName/Value attributes in the manifest
PRODUCT_PRODUCT_PROPERTIES += ro.build.car_ui_rros_enabled=true
