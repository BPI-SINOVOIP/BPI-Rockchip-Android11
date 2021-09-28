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

package com.android.car.settings.storage;

import android.content.Context;
import android.os.AsyncTask;
import android.os.storage.StorageManager;
import android.os.storage.VolumeInfo;
import android.widget.Toast;

import androidx.annotation.Nullable;

import com.android.car.settings.R;
import com.android.car.settings.common.Logger;

/** Task used to unmount a {@link android.os.storage.StorageVolume}. */
public class UnmountTask extends AsyncTask<Void, Void, Exception> {

    private static final Logger LOG = new Logger(UnmountTask.class);

    private final Context mContext;
    private final StorageManager mStorageManager;
    private final String mVolumeId;
    private final String mDescription;

    public UnmountTask(Context context, VolumeInfo volume) {
        mContext = context.getApplicationContext();
        mStorageManager = mContext.getSystemService(StorageManager.class);
        mVolumeId = volume.getId();
        mDescription = mStorageManager.getBestVolumeDescription(volume);
    }

    @Override
    protected Exception doInBackground(Void... params) {
        try {
            mStorageManager.unmount(mVolumeId);
            return null;
        } catch (Exception e) {
            return e;
        }
    }

    @Override
    protected void onPostExecute(@Nullable Exception e) {
        if (e == null) {
            Toast.makeText(mContext, mContext.getString(R.string.storage_unmount_success,
                    mDescription), Toast.LENGTH_SHORT).show();
        } else {
            LOG.e("Failed to unmount " + mVolumeId, e);
            Toast.makeText(mContext, mContext.getString(R.string.storage_unmount_failure,
                    mDescription), Toast.LENGTH_SHORT).show();
        }
    }
}
