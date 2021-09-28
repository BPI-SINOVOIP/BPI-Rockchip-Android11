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
package android.app.cts;

import static org.junit.Assert.assertEquals;

import android.app.DownloadManager;
import android.content.IntentFilter;
import android.net.Uri;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

@RunWith(AndroidJUnit4.class)
public class DownloadManagerInstallerTest extends DownloadManagerTestBase {

    @Test
    public void testSetDestinationUri_otherAppObbDir() throws Exception {
        File obbDir = mContext.getObbDir();

        String otherAppObbPath = obbDir.getPath().replace(mContext.getPackageName(),
                "android.app.cts.some_random_package");
        File destPath = new File(otherAppObbPath);
        destPath.mkdirs();

        File destFile = new File(destPath, "test.obb");
        deleteFromShell(destFile);

        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            DownloadManager.Request requestPublic = new DownloadManager.Request(getGoodUrl());
            requestPublic.setDestinationUri(Uri.fromFile(destFile));
            long id = mDownloadManager.enqueue(requestPublic);

            int allDownloads = getTotalNumberDownloads();
            assertEquals(1, allDownloads);

            receiver.waitForDownloadComplete(SHORT_TIMEOUT, id);
            assertSuccessfulDownload(id, destFile);

            assertRemoveDownload(id, 0);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }
}
