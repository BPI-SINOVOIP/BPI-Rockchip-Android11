/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.compatibility.common.util.mainline;

/**
 * Enum containing metadata for mainline modules.
 */
public enum MainlineModule {

    // Added in Q

    // Security
    MEDIA_SOFTWARE_CODEC("com.google.android.media.swcodec",
            true, ModuleType.APEX,
            "0C:2B:13:87:6D:E5:6A:E6:4E:D1:DE:93:42:2A:8A:3F:EA:6F:34:C0:FC:5D:7D:A1:BD:CF:EF"
                    + ":C1:A7:B7:C9:1D"),
    MEDIA("com.google.android.media",
            true, ModuleType.APEX,
            "16:C1:5C:FA:15:D0:FD:D0:7E:BE:CB:5A:76:6B:40:8B:05:DD:92:7E:1F:3A:DD:C5:AB:F6:8E"
                    + ":E8:B9:98:F9:FD"),
    DNS_RESOLVER("com.google.android.resolv",
            true, ModuleType.APEX,
            "EC:82:21:76:5E:4F:7E:2C:6D:8D:0F:0C:E9:BD:82:5B:98:BE:D2:0C:07:2C:C6:C8:08:DD:E4"
                    + ":68:5F:EB:A6:FF"),
    CONSCRYPT("com.google.android.conscrypt",
            true, ModuleType.APEX,
            "8C:5D:A9:10:E6:11:21:B9:D6:E0:3B:42:D3:20:6A:7D:AD:29:DD:C1:63:AE:CD:4B:8E:E9:3F"
                    + ":D3:83:79:CA:2A"),
    // Privacy
    PERMISSION_CONTROLLER("com.google.android.permissioncontroller",
            false, ModuleType.APK,
            "89:DF:B5:04:7E:E0:19:29:C2:18:4D:68:EF:49:64:F2:A9:0A:F1:24:C3:23:38:28:B8:F6:40"
                    + ":D9:E6:C0:0F:83"),
    ANDROID_SERVICES("com.google.android.ext.services",
            false, ModuleType.APK,
            "18:46:05:09:5B:E6:CA:22:D0:55:F3:4E:FA:F0:13:44:FD:3A:B3:B5:63:8C:30:62:76:10:EE"
                    + ":AE:8A:26:0B:29"),
    DOCUMENTS_UI("com.google.android.documentsui",
            true, ModuleType.APK,
            "9A:4B:85:34:44:86:EC:F5:1F:F8:05:EB:9D:23:17:97:79:BE:B7:EC:81:91:93:5A:CA:67:F0"
                    + ":F4:09:02:52:97"),
    // Consistency
    NETWORK_STACK("com.google.android.networkstack",
            true, ModuleType.APK,
            "5F:A4:22:12:AD:40:3E:22:DD:6E:FE:75:F3:F3:11:84:05:1F:EF:74:4C:0B:05:BE:5C:73:ED"
                    + ":F6:0B:F6:2C:1E"),
    CAPTIVE_PORTAL_LOGIN("com.google.android.captiveportallogin",
            true, ModuleType.APK,
            "5F:A4:22:12:AD:40:3E:22:DD:6E:FE:75:F3:F3:11:84:05:1F:EF:74:4C:0B:05:BE:5C:73:ED"
                    + ":F6:0B:F6:2C:1E"),
    NETWORK_PERMISSION_CONFIGURATION("com.google.android.networkstack.permissionconfig",
            true, ModuleType.APK,
            "5F:A4:22:12:AD:40:3E:22:DD:6E:FE:75:F3:F3:11:84:05:1F:EF:74:4C:0B:05:BE:5C:73:ED"
                    + ":F6:0B:F6:2C:1E"),
    MODULE_METADATA("com.google.android.modulemetadata",
            true, ModuleType.APK,
            "BF:62:23:1E:28:F0:85:42:75:5C:F3:3C:9D:D8:3C:5D:1D:0F:A3:20:64:50:EF:BC:4C:3F:F3"
                    + ":D5:FD:A0:33:0F"),

    // Added in R

    ADBD("com.google.android.adbd",
            true, ModuleType.APEX,
            "87:3D:4E:43:58:25:1A:25:1A:2D:9C:18:E1:55:09:45:21:88:A8:1E:FE:9A:83:9D:43:0D:E8"
                    + ":D8:7E:C2:49:4C"),
    NEURAL_NETWORKS("com.google.android.neuralnetworks",
            true, ModuleType.APEX,
            "6F:AB:D5:72:9A:90:02:6B:74:E4:87:79:8F:DF:10:BB:E3:6C:9E:6C:B7:A6:59:04:3C:D8:15"
                    + ":61:6C:9E:60:50"),
    CELL_BROADCAST("com.google.android.cellbroadcast",
            true, ModuleType.APEX,
            "A8:2C:84:7A:A3:9D:DA:19:A5:6C:9E:D3:56:50:1A:76:4F:BD:5D:C9:60:98:66:16:E3:1D:48"
                    + ":EE:27:08:19:70"),
    EXT_SERVICES("com.google.android.extservices",
            true, ModuleType.APEX,
            "10:89:F2:7C:85:6A:83:D4:02:6B:6A:49:97:15:4C:A1:70:9A:F6:93:27:C8:EF:9A:2D:1D:56"
                    + ":AB:69:DE:07:0B"),
    IPSEC("com.google.android.ipsec",
            true, ModuleType.APEX,
            "64:3D:3E:A5:B7:BF:22:E5:94:42:29:77:7C:4B:FF:C6:C8:44:14:64:4D:E0:4B:E4:90:37:57"
                    + ":DE:83:CF:04:8B"),
    MEDIA_PROVIDER("com.google.android.mediaprovider",
            true, ModuleType.APEX,
            "1A:61:93:09:6D:DC:81:58:72:45:EF:2C:07:33:73:6E:8E:FF:9D:E9:0E:51:27:4B:F8:23:AC"
                    + ":F0:F7:49:00:A0"),
    PERMISSION_CONTROLLER_APEX("com.google.android.permission",
            true, ModuleType.APEX,
            "69:AC:92:BF:BA:D5:85:4C:61:8E:AB:AE:85:7F:AB:0B:1A:65:19:44:E9:19:EA:3C:86:DB:D4"
                    + ":07:04:1E:22:C1"),
    SDK_EXTENSIONS("com.google.android.sdkext",
            true, ModuleType.APEX,
            "99:90:29:2B:22:11:D2:78:17:BF:5B:10:98:84:8F:68:44:53:37:16:2B:47:FF:D1:A0:8E:10"
                    + ":CE:65:B1:CC:73"),
    STATSD("com.google.android.os.statsd",
            true, ModuleType.APEX,
            "DA:FE:D6:20:A7:0C:98:05:A9:A2:22:04:55:6B:0E:94:E8:E3:4D:ED:F4:16:EC:58:92:C6:48"
                    + ":86:53:39:B4:7B"),
    TELEMETRY_TVP("com.google.mainline.telemetry",
            true, ModuleType.APK,
            "9D:AC:CC:AE:4F:49:5A:E6:DB:C5:8A:0E:C2:33:C6:E5:2D:31:14:33:AC:57:3C:4D:A1:C7:39"
                    + ":DF:64:03:51:5D"),
    TETHERING("com.google.android.tethering",
            true, ModuleType.APEX,
            "E5:3F:52:F4:14:15:0C:05:BA:E0:E4:CE:E2:07:3D:D0:0F:E6:44:66:1D:5F:9A:0F:BE:49:4A"
                    + ":DC:07:F0:59:93"),
    TZDATA2("com.google.android.tzdata2",
            true, ModuleType.APEX,
            "48:F3:A2:98:76:1B:6D:46:75:7C:EE:62:43:66:6A:25:B9:15:B9:42:18:A6:C2:82:72:99:BE"
                    + ":DA:C9:92:AB:E7"),
    WIFI("com.google.android.wifi",
            false, ModuleType.APEX,
            "B7:A3:DB:7A:86:6D:18:51:3F:97:6C:63:20:BC:0F:E6:E4:01:BA:2F:26:96:B1:C3:94:2A:F0"
                    + ":FE:29:31:98:B1"),
    ;

    public final String packageName;
    public final boolean isPlayUpdated;
    public final ModuleType moduleType;
    public final String certSHA256;

    MainlineModule(String packageName, boolean isPlayUpdated, ModuleType moduleType,
            String certSHA256) {
        this.packageName = packageName;
        this.isPlayUpdated = isPlayUpdated;
        this.moduleType = moduleType;
        this.certSHA256 = certSHA256;
    }
}
