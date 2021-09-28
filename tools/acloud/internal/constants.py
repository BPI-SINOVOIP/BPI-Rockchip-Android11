#!/usr/bin/env python
#
# Copyright 2016 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""This module holds constants used by the driver."""
BRANCH_PREFIX = "git_"
BUILD_TARGET_MAPPING = {
    # TODO: Add aosp goldfish targets and internal cf targets to vendor code
    # base.
    "aosp_phone": "aosp_cf_x86_phone-userdebug",
    "aosp_tablet": "aosp_cf_x86_tablet-userdebug",
}
SPEC_NAMES = {
    "nexus5", "nexus6", "nexus7_2012", "nexus7_2013", "nexus9", "nexus10"
}

DEFAULT_SERIAL_PORT = 1
LOGCAT_SERIAL_PORT = 2

# Remote image parameters
BUILD_TARGET = "build_target"
BUILD_BRANCH = "build_branch"
BUILD_ID = "build_id"

# AVD types
TYPE_CHEEPS = "cheeps"
TYPE_CF = "cuttlefish"
TYPE_GCE = "gce"
TYPE_GF = "goldfish"

# Image types
IMAGE_SRC_REMOTE = "remote_image"
IMAGE_SRC_LOCAL = "local_image"

# AVD types in build target
AVD_TYPES_MAPPING = {
    TYPE_GCE: "gce",
    TYPE_CF: "cf",
    TYPE_GF: "sdk",
    # Cheeps uses the cheets target.
    TYPE_CHEEPS: "cheets",
}

# Instance types
INSTANCE_TYPE_REMOTE = "remote"
INSTANCE_TYPE_LOCAL = "local"
INSTANCE_TYPE_HOST = "host"

# Flavor types
FLAVOR_PHONE = "phone"
FLAVOR_AUTO = "auto"
FLAVOR_WEAR = "wear"
FLAVOR_TV = "tv"
FLAVOR_IOT = "iot"
FLAVOR_TABLET = "tablet"
FLAVOR_TABLET_3G = "tablet_3g"
ALL_FLAVORS = [
    FLAVOR_PHONE, FLAVOR_AUTO, FLAVOR_WEAR, FLAVOR_TV, FLAVOR_IOT,
    FLAVOR_TABLET, FLAVOR_TABLET_3G
]

# HW Property
HW_ALIAS_CPUS = "cpu"
HW_ALIAS_RESOLUTION = "resolution"
HW_ALIAS_DPI = "dpi"
HW_ALIAS_MEMORY = "memory"
HW_ALIAS_DISK = "disk"
HW_PROPERTIES_CMD_EXAMPLE = (
    " %s:2,%s:1280x700,%s:160,%s:2g,%s:2g" %
    (HW_ALIAS_CPUS,
     HW_ALIAS_RESOLUTION,
     HW_ALIAS_DPI,
     HW_ALIAS_MEMORY,
     HW_ALIAS_DISK)
)
HW_PROPERTIES = [HW_ALIAS_CPUS, HW_ALIAS_RESOLUTION, HW_ALIAS_DPI,
                 HW_ALIAS_MEMORY, HW_ALIAS_DISK]
HW_X_RES = "x_res"
HW_Y_RES = "y_res"

USER_ANSWER_YES = {"y", "yes", "Y"}

# Cuttlefish groups
LIST_CF_USER_GROUPS = ["kvm", "cvdnetwork"]

IP = "ip"
INSTANCE_NAME = "instance_name"
GCE_USER = "vsoc-01"
VNC_PORT = "vnc_port"
ADB_PORT = "adb_port"
# For cuttlefish remote instances
CF_ADB_PORT = 6520
CF_VNC_PORT = 6444
# For cheeps remote instances
CHEEPS_ADB_PORT = 9222
CHEEPS_VNC_PORT = 5900
# For gce_x86_phones remote instances
GCE_ADB_PORT = 5555
GCE_VNC_PORT = 6444
# For goldfish remote instances
GF_ADB_PORT = 5555
GF_VNC_PORT = 6444

COMMAND_PS = ["ps", "aux"]
CMD_LAUNCH_CVD = "launch_cvd"
CMD_PGREP = "pgrep"
CMD_STOP_CVD = "stop_cvd"
CMD_RUN_CVD = "run_cvd"
ENV_ANDROID_BUILD_TOP = "ANDROID_BUILD_TOP"
ENV_ANDROID_EMULATOR_PREBUILTS = "ANDROID_EMULATOR_PREBUILTS"
ENV_ANDROID_HOST_OUT = "ANDROID_HOST_OUT"
ENV_ANDROID_PRODUCT_OUT = "ANDROID_PRODUCT_OUT"
ENV_ANDROID_TMP = "ANDROID_TMP"
ENV_BUILD_TARGET = "TARGET_PRODUCT"

LOCALHOST = "127.0.0.1"
LOCALHOST_ADB_SERIAL = LOCALHOST + ":%d"

SSH_BIN = "ssh"
SCP_BIN = "scp"
ADB_BIN = "adb"

LABEL_CREATE_BY = "created_by"

# for list and delete cmd
INS_KEY_NAME = "name"
INS_KEY_FULLNAME = "full_name"
INS_KEY_STATUS = "status"
INS_KEY_DISPLAY = "display"
INS_KEY_IP = "ip"
INS_KEY_ADB = "adb"
INS_KEY_VNC = "vnc"
INS_KEY_WEBRTC = "webrtc"
INS_KEY_CREATETIME = "creationTimestamp"
INS_KEY_AVD_TYPE = "avd_type"
INS_KEY_AVD_FLAVOR = "flavor"
INS_KEY_IS_LOCAL = "remote"
INS_KEY_ZONE = "zone"
INS_STATUS_RUNNING = "RUNNING"
LOCAL_INS_NAME = "local-instance"
ENV_CUTTLEFISH_CONFIG_FILE = "CUTTLEFISH_CONFIG_FILE"
ENV_CUTTLEFISH_INSTANCE = "CUTTLEFISH_INSTANCE"
ENV_CVD_HOME = "HOME"
CUTTLEFISH_CONFIG_FILE = "cuttlefish_config.json"

TEMP_ARTIFACTS_FOLDER = "acloud_image_artifacts"
CVD_HOST_PACKAGE = "cvd-host_package.tar.gz"
TOOL_NAME = "acloud"
# Exit code in metrics
EXIT_SUCCESS = 0
EXIT_BY_USER = 1
EXIT_BY_WRONG_CMD = 2
EXIT_BY_FAIL_REPORT = 3
EXIT_BY_ERROR = -99

# For reuse gce instance
SELECT_ONE_GCE_INSTANCE = "select_one_gce_instance"
