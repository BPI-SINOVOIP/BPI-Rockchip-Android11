/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.cts.verifier.managedprovisioning;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import androidx.core.content.FileProvider;

import java.io.File;

/** Cts Verifier helper activity that runs only in the primary profile.*/
public class ByodPrimaryHelperActivity extends Activity {

    private static final String TAG = "ByodPrimaryHlprActivity";
    private static final int REQUEST_INSTALL_PACKAGE = 1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (ByodHelperActivity.ACTION_INSTALL_APK_IN_PRIMARY.equals(getIntent().getAction())) {
            final Uri uri = FileProvider.getUriForFile(this, Utils.FILE_PROVIDER_AUTHORITY,
                    new File(ByodFlowTestActivity.HELPER_APP_PATH));
            final Intent installIntent = new Intent(Intent.ACTION_INSTALL_PACKAGE)
                    .setData(uri)
                    .putExtra(Intent.EXTRA_NOT_UNKNOWN_SOURCE, true)
                    .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    .putExtra(Intent.EXTRA_RETURN_RESULT, true)
                    .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
            startActivityForResult(installIntent, REQUEST_INSTALL_PACKAGE);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQUEST_INSTALL_PACKAGE) {
            Log.w(TAG, "Receiving installer result " + resultCode + ".");
            finish();
        }
    }
}