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

package com.android.cts.verifier.security;

import android.content.Context;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.res.AssetManager;
import android.net.Uri;
import android.provider.MediaStore;

import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.InputStream;

public class CACertWriter {
    static final String TAG = CACertWriter.class.getSimpleName();

    private static final String CERT_ASSET_NAME = "myCA.cer";

    public static boolean extractCertToDownloads(
            Context applicationContext, AssetManager assetManager) {
        // Use MediaStore API to write the CA cert to the Download folder
        final ContentValues downloadValues = new ContentValues();
        downloadValues.put(MediaStore.MediaColumns.DISPLAY_NAME, CERT_ASSET_NAME);

        Uri targetCollection = MediaStore.Downloads.EXTERNAL_CONTENT_URI;
        ContentResolver resolver = applicationContext.getContentResolver();
        Uri toOpen = resolver.insert(targetCollection, downloadValues);
        Log.i(TAG, String.format("Writing CA cert to %s", toOpen));

        InputStream is = null;
        OutputStream os = null;
        try {
            try {
                is = assetManager.open(CERT_ASSET_NAME);
                os = resolver.openOutputStream(toOpen);
                byte[] buffer = new byte[1024];
                int length;
                while ((length = is.read(buffer)) > 0) {
                    os.write(buffer, 0, length);
                }
            } finally {
                if (is != null) is.close();
                if (os != null) os.close();
            }
        } catch (IOException ioe) {
            Log.w(TAG, String.format("Problem moving cert file to %s", toOpen), ioe);
            return false;
        }
        return true;
    }
}
