/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.server.hdmi;

import android.util.Log;

import com.android.server.hdmi.HdmiUtils.CodecSad;
import com.android.server.hdmi.HdmiUtils.DeviceConfig;
import com.google.common.hash.HashCode;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.xmlpull.v1.XmlPullParserException;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

@RunWith(JUnit4.class)
/**
 * Reads short audio descriptors from a configuration file and outputs to a log
 * for use by host side tests.
 */
public class SadConfigurationReaderTest {

    private static final String TAG = "SadConfigurationReaderTest";

    // Variable should be copy of SHORT_AUDIO_DESCRIPTOR_CONFIG_PATH in
    // frameworks/base/services/core/java/com/android/server/hdmi/
    //  HdmiCecLocalDeviceAudioSystem.java
    private final String SHORT_AUDIO_DESCRIPTOR_CONFIG_PATH = "/vendor/etc/sadConfig.xml";

    @Test
    public void parseSadConfigXML() {
        List<DeviceConfig> deviceConfigs = null;
        File file = new File(SHORT_AUDIO_DESCRIPTOR_CONFIG_PATH);
        if (file.exists()) {
            try {
                InputStream in = new FileInputStream(file);
                deviceConfigs = HdmiUtils.ShortAudioDescriptorXmlParser.parse(in);
                in.close();
            } catch (IOException e) {
                Log.e(TAG, "Error reading file: " + file.getAbsolutePath(), e);
            } catch (XmlPullParserException e) {
                Log.e(TAG, "Unable to parse file: " + file.getAbsolutePath(), e);
            }
        } else {
            Log.e(TAG, "No config file present at " + file.getAbsolutePath());
            return;
        }
        DeviceConfig deviceConfigToUse = null;
        if (deviceConfigs != null && deviceConfigs.size() > 0) {
            for (DeviceConfig deviceConfig : deviceConfigs) {
                if (deviceConfig.name.equals("VX_AUDIO_DEVICE_IN_HDMI_ARC")) {
                    deviceConfigToUse = deviceConfig;
                    break;
                }
            }
            if (deviceConfigToUse == null) {
                Log.w(TAG, "sadConfig.xml does not have required device info for "
                                   + "VX_AUDIO_DEVICE_IN_HDMI_ARC");
                return;
            }
            List<Integer> audioCodecFormats = new ArrayList<>();
            List<String> shortAudioDescriptors = new ArrayList<>();
            for (CodecSad codecSad : deviceConfigToUse.supportedCodecs) {
                audioCodecFormats.add(codecSad.audioCodec);
                shortAudioDescriptors.add(HashCode.fromBytes(codecSad.sad).toString());
            }
            String audioCodecFormatsString = audioCodecFormats.toString();
            String shortAudioDescriptorsString = shortAudioDescriptors.toString();
            Log.i(TAG, "Supported Audio Formats");
            Log.i(TAG, audioCodecFormatsString.substring(1, audioCodecFormatsString.length() - 1));
            Log.i(TAG, shortAudioDescriptorsString
                               .substring(1, shortAudioDescriptorsString.length() - 1));
        }
    }
}
