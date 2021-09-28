#
# Copyright 2020 The Android Open-Source Project
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
#

#
# This makefile exports
#
# NATIVE_BRIDGE_PRODUCT_PACKAGES: Add this to PRODUCT_PACKAGES for your project to facilitate
# native bridge support.
#
# NATIVE_BRIDGE_MODIFIED_GUEST_LIBS: List of modified guest libraries that require host counterpart.
#

NATIVE_BRIDGE_PRODUCT_PACKAGES := \
    libnative_bridge_vdso.native_bridge \
    native_bridge_guest_app_process.native_bridge \
    native_bridge_guest_linker.native_bridge

# Original guest libraries.
NATIVE_BRIDGE_ORIG_GUEST_LIBS := \
    libcompiler_rt \
    libcrypto \
    libcutils \
    libdl.bootstrap \
    libdl_android.bootstrap \
    liblog \
    libm.bootstrap \
    libsqlite \
    libssl \
    libstdc++ \
    libsync \
    libutils \
    libz

# TODO(b/141167717): hack to make libandroidicu compatible with APEX.
#
# If library is APEX-enabled:
#
#   Then "libraryname" is not installed in the  /system/lib since it's
#   supposed to be installed into APEX.
#
#   However, "libraryname.bootstrap" goes into /system/lib/bootstrap.
#
#   Native bridge isn't compatible with APEX yet thus "libraryname.native_bridge"
#   is not installed anywhere at all.
#
#   However, "libraryname.bootstrap.native_bridge" gets installed into
#   /system/lib/$GUEST_ARCH/ - as we need for native bridge.
#
# Note: this doesn't affect native libraries at all.
NATIVE_BRIDGE_ORIG_GUEST_LIBS += \
    libandroidicu.bootstrap

NATIVE_BRIDGE_PRODUCT_PACKAGES += \
    libclcore.bc \
    libclcore_neon.bc

NATIVE_BRIDGE_ORIG_GUEST_LIBS += \
    libRS \
    libRSDriver \
    libnative_bridge_guest_libRSSupport

# These native libraries are needed to pass CtsJniTestCases, we do not use them in any way and
# once/if build system allows us to build dummy arm libraries they can be replaced with empty ones.
#NATIVE_BRIDGE_ORIG_GUEST_LIBS += \
#    libart \
#    libvorbisidec

# These libraries need special support on the native bridge implementation side.
NATIVE_BRIDGE_MODIFIED_GUEST_LIBS := \
    libaaudio \
    libamidi \
    libandroid \
    libandroid_runtime \
    libbinder_ndk \
    libc \
    libcamera2ndk \
    libEGL \
    libGLESv1_CM \
    libGLESv2 \
    libGLESv3 \
    libicui18n \
    libicuuc \
    libjnigraphics \
    libmediandk \
    libnativehelper \
    libnativewindow \
    libneuralnetworks \
    libOpenMAXAL \
    libOpenSLES \
    libvulkan \
    libwebviewchromium_plat_support

# Original guest libraries are built for native_bridge
NATIVE_BRIDGE_PRODUCT_PACKAGES += \
    $(addsuffix .native_bridge,$(NATIVE_BRIDGE_ORIG_GUEST_LIBS))

# Modified guest libraries are built for native_bridge and
# have special build target prefix
NATIVE_BRIDGE_PRODUCT_PACKAGES += \
    $(addprefix libnative_bridge_guest_,$(addsuffix .native_bridge,$(NATIVE_BRIDGE_MODIFIED_GUEST_LIBS)))

NATIVE_BRIDGE_ORIG_GUEST_LIBS :=
