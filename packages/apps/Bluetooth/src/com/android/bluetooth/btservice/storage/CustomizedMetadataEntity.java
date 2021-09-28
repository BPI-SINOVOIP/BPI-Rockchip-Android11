/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.bluetooth.btservice.storage;

import androidx.room.Entity;

@Entity
class CustomizedMetadataEntity {
    public byte[] manufacturer_name;
    public byte[] model_name;
    public byte[] software_version;
    public byte[] hardware_version;
    public byte[] companion_app;
    public byte[] main_icon;
    public byte[] is_untethered_headset;
    public byte[] untethered_left_icon;
    public byte[] untethered_right_icon;
    public byte[] untethered_case_icon;
    public byte[] untethered_left_battery;
    public byte[] untethered_right_battery;
    public byte[] untethered_case_battery;
    public byte[] untethered_left_charging;
    public byte[] untethered_right_charging;
    public byte[] untethered_case_charging;
    public byte[] enhanced_settings_ui_uri;

    public String toString() {
        StringBuilder builder = new StringBuilder();
        builder.append("manufacturer_name=")
                .append(metadataToString(manufacturer_name))
                .append("|model_name=")
                .append(metadataToString(model_name))
                .append("|software_version=")
                .append(metadataToString(software_version))
                .append("|hardware_version=")
                .append(metadataToString(hardware_version))
                .append("|companion_app=")
                .append(metadataToString(companion_app))
                .append("|main_icon=")
                .append(metadataToString(main_icon))
                .append("|is_untethered_headset=")
                .append(metadataToString(is_untethered_headset))
                .append("|untethered_left_icon=")
                .append(metadataToString(untethered_left_icon))
                .append("|untethered_right_icon=")
                .append(metadataToString(untethered_right_icon))
                .append("|untethered_case_icon=")
                .append(metadataToString(untethered_case_icon))
                .append("|untethered_left_battery=")
                .append(metadataToString(untethered_left_battery))
                .append("|untethered_right_battery=")
                .append(metadataToString(untethered_right_battery))
                .append("|untethered_case_battery=")
                .append(metadataToString(untethered_case_battery))
                .append("|untethered_left_charging=")
                .append(metadataToString(untethered_left_charging))
                .append("|untethered_right_charging=")
                .append(metadataToString(untethered_right_charging))
                .append("|untethered_case_charging=")
                .append(metadataToString(untethered_case_charging))
                .append("|enhanced_settings_ui_uri=")
                .append(metadataToString(enhanced_settings_ui_uri));

        return builder.toString();
    }

    private String metadataToString(byte[] metadata) {
        if (metadata == null) {
            return null;
        }
        return new String(metadata);
    }
}
