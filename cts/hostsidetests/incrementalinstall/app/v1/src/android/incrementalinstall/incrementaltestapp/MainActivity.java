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
package android.incrementalinstall.incrementaltestapp;

import static android.incrementalinstall.common.Consts.COMPONENT_STATUS_KEY;
import static android.incrementalinstall.common.Consts.INCREMENTAL_TEST_APP_STATUS_RECEIVER_ACTION;
import static android.incrementalinstall.common.Consts.TARGET_COMPONENT_KEY;

import android.app.Activity;
import android.content.Intent;
import android.incrementalinstall.common.Consts;
import android.incrementalinstall.incrementaltestapp.dynamiccode.DynamicCodeShim;
import android.incrementalinstall.incrementaltestapp.nativelib.CompressedNativeLib;
import android.incrementalinstall.incrementaltestapp.nativelib.UncompressedNativeLib;
import android.os.Bundle;

import java.io.InputStream;

/**
 * A simple activity which broadcasts component loading status to
 * android.incrementalinstall.inrementaltestappvalidation.
 */
public class MainActivity extends Activity {

    private static String PACKAGE_NAME;

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        PACKAGE_NAME = getApplicationContext().getPackageName();

        broadcastStatus(Consts.SupportedComponents.ON_CREATE_COMPONENT, true);
        broadcastStatus(Consts.SupportedComponents.ON_CREATE_COMPONENT_2, false);
        loadDynamicAsset();
        loadDynamicCode();
        loadCompressedNativeLib();
        loadUncompressedNativeLib();
    }

    private void loadDynamicAsset() {
        boolean dynamicAssetStatus = false;
        String file = "dynamicasset.txt";
        try (InputStream is = this.createPackageContext(PACKAGE_NAME, 0)
                .getAssets().open(file)) {
            int size = is.available();
            if (size > 0) {
                dynamicAssetStatus = true;
            }
        } catch (Exception ignored) {
        }
        broadcastStatus(Consts.SupportedComponents.DYNAMIC_ASSET_COMPONENT, dynamicAssetStatus);
    }

    private void loadDynamicCode() {
        boolean dynamicCodeStatus = false;
        DynamicCodeShim shim = new DynamicCodeShim();
        try {
            if (shim.getString() != null) {
                dynamicCodeStatus = true;
            }
        } catch (Exception ignored) {
        }
        broadcastStatus(Consts.SupportedComponents.DYNAMIC_CODE_COMPONENT, dynamicCodeStatus);
    }

    private void loadCompressedNativeLib() {
        boolean compressedNativeLibStatus = false;
        CompressedNativeLib nativeLib = new CompressedNativeLib();
        try {
            if (nativeLib.getStringFromNative() != null) {
                compressedNativeLibStatus = true;
            }
        } catch (Throwable ignored) {
        }
        broadcastStatus(Consts.SupportedComponents.COMPRESSED_NATIVE_COMPONENT,
                compressedNativeLibStatus);
    }

    private void loadUncompressedNativeLib() {
        boolean unCompressedNativeLibStatus = false;
        UncompressedNativeLib nativeLib = new UncompressedNativeLib();
        try {
            if (nativeLib.getStringFromNative() != null) {
                unCompressedNativeLibStatus = true;
            }
        } catch (Throwable ignored) {
        }
        broadcastStatus(Consts.SupportedComponents.UNCOMPRESSED_NATIVE_COMPONENT,
                unCompressedNativeLibStatus);
    }

    private void broadcastStatus(String component, boolean status) {
        Intent intent = new Intent(INCREMENTAL_TEST_APP_STATUS_RECEIVER_ACTION);
        intent.putExtra(TARGET_COMPONENT_KEY, component);
        intent.putExtra(COMPONENT_STATUS_KEY, status);
        sendBroadcast(intent);
    }
}
