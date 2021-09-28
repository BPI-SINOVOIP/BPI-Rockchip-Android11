/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.compatibility.common.util;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import org.xmlpull.v1.XmlPullParserException;

import java.io.File;
import java.io.IOException;
import java.io.FileNotFoundException;

/**
 * Load dynamic config for device side test cases
 */
public class DynamicConfigDeviceSide extends DynamicConfig {

    public static final String CONTENT_PROVIDER =
            String.format("%s://android.tradefed.contentprovider", ContentResolver.SCHEME_CONTENT);

    public DynamicConfigDeviceSide(String moduleName) throws XmlPullParserException, IOException {
        this(moduleName, new File(CONFIG_FOLDER_ON_DEVICE));
    }

    public DynamicConfigDeviceSide(String moduleName, File configFolder)
            throws XmlPullParserException, IOException {
        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            throw new IOException("External storage is not mounted");
        }
        // Use the content provider to get the config:
        String uriPath = String.format("%s/%s/%s.dynamic", CONTENT_PROVIDER, configFolder.getAbsolutePath(), moduleName);
        Uri sdcardUri = Uri.parse(uriPath);
        Context appContext = InstrumentationRegistry.getTargetContext();
        try {
            ContentResolver resolver = appContext.getContentResolver();
            ParcelFileDescriptor descriptor = resolver.openFileDescriptor(sdcardUri,"r");

            initializeConfig(new ParcelFileDescriptor.AutoCloseInputStream(descriptor));
            return;
        } catch (FileNotFoundException e) {
            // Log the error and use the fallback too
            Log.e("DynamicConfigDeviceSide", "Error while using content provider for config", e);
        }
        // Fallback to the direct search
        File configFile = getConfigFile(configFolder, moduleName);
        initializeConfig(configFile);
    }
}
