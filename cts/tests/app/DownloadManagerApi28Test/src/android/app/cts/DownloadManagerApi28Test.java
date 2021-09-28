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
package android.app.cts;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.DownloadManager;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.os.FileUtils;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

@RunWith(AndroidJUnit4.class)
public class DownloadManagerApi28Test extends DownloadManagerTestBase {

    @Test
    public void testSetDestinationUri_publicDir() throws Exception {
        File publicLocation = new File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS),
                "publicFile.bin");
        deleteFromShell(publicLocation);

        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            DownloadManager.Request requestPublic = new DownloadManager.Request(getGoodUrl());
            requestPublic.setDestinationUri(Uri.fromFile(publicLocation));
            long id = mDownloadManager.enqueue(requestPublic);

            int allDownloads = getTotalNumberDownloads();
            assertEquals(1, allDownloads);

            receiver.waitForDownloadComplete(SHORT_TIMEOUT, id);
            assertSuccessfulDownload(id, publicLocation);

            assertRemoveDownload(id, 0);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    @Test
    public void testSetDestinationUri_sdcardPath() throws Exception {
        final File path = new File("/sdcard/publicFile.bin");
        deleteFromShell(path);

        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            DownloadManager.Request requestPublic = new DownloadManager.Request(getGoodUrl());
            requestPublic.setDestinationUri(Uri.fromFile(path));
            long id = mDownloadManager.enqueue(requestPublic);

            int allDownloads = getTotalNumberDownloads();
            assertEquals(1, allDownloads);

            receiver.waitForDownloadComplete(SHORT_TIMEOUT, id);
            assertSuccessfulDownload(id, path);

            assertRemoveDownload(id, 0);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    @Test
    public void testDestinationInExternalPublicDir() throws Exception {
        File publicLocation = new File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS),
                "publicFile.bin");
        deleteFromShell(publicLocation);

        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            DownloadManager.Request requestPublic = new DownloadManager.Request(getGoodUrl());
            requestPublic.setDestinationInExternalPublicDir(Environment.DIRECTORY_DOCUMENTS,
                    "publicFile.bin");
            long id = mDownloadManager.enqueue(requestPublic);

            int allDownloads = getTotalNumberDownloads();
            assertEquals(1, allDownloads);

            receiver.waitForDownloadComplete(SHORT_TIMEOUT, id);
            assertSuccessfulDownload(id, publicLocation);

            assertRemoveDownload(id, 0);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    @Test
    public void testAddCompletedDownload_publicDirs() throws Exception {
        final String[] filePaths = new String[] {
                createFile(Environment.getExternalStoragePublicDirectory(
                        Environment.DIRECTORY_DOWNLOADS), "file1.txt").getPath(),
                createFile(Environment.getExternalStoragePublicDirectory(
                        Environment.DIRECTORY_DOCUMENTS), "file2.txt").getPath(),
                "/sdcard/Download/file3.txt",
        };

        for (String path : filePaths) {
            final String fileContents = "Test content:" + path + "_" + System.nanoTime();

            final File file = new File(path);
            writeToFile(new File(path), fileContents);

            final long id = mDownloadManager.addCompletedDownload(file.getName(), "Test desc", true,
                    "text/plain", path, fileContents.getBytes().length, true);
            final String actualContents = readFromFile(mDownloadManager.openDownloadedFile(id));
            assertEquals(fileContents, actualContents);

            final Uri downloadUri = mDownloadManager.getUriForDownloadedFile(id);
            mContext.grantUriPermission("com.android.shell", downloadUri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION);
            final String rawFilePath = getRawFilePath(downloadUri);
            final String rawFileContents = readFromRawFile(rawFilePath);
            assertEquals(fileContents, rawFileContents);
            assertRemoveDownload(id, 0);
        }
    }


    @Test
    public void testDownloadManager_mediaStoreEntry() throws Exception {
        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            mContext.registerReceiver(receiver,
                    new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE));

            final String fileName = "noiseandchirps.mp3";
            final DownloadManager.Request request
                    = new DownloadManager.Request(getAssetUrl(fileName));
            final Uri[] downloadPathUris = new Uri[] {
                    Uri.fromFile(new File(Environment.getExternalStoragePublicDirectory(
                            Environment.DIRECTORY_DOWNLOADS), "testfile1.mp3")),
                    Uri.parse("file:///sdcard/testfile2.mp3"),
            };
            for (Uri downloadLocation : downloadPathUris) {
                request.setDestinationUri(downloadLocation);
                final long downloadId = mDownloadManager.enqueue(request);
                receiver.waitForDownloadComplete(SHORT_TIMEOUT, downloadId);
                assertSuccessfulDownload(downloadId, new File(downloadLocation.getPath()));
                final Uri downloadUri = mDownloadManager.getUriForDownloadedFile(downloadId);
                final Uri mediaStoreUri = getMediaStoreUri(downloadUri);
                final ContentResolver contentResolver = mContext.getContentResolver();
                assertArrayEquals(hash(contentResolver.openInputStream(downloadUri)),
                        hash(contentResolver.openInputStream(mediaStoreUri)));

                // Delete entry in DownloadProvider and verify it's deleted from MediaProvider
                // as well.
                assertRemoveDownload(downloadId, 0);
                try (Cursor cursor = mContext.getContentResolver().query(
                        mediaStoreUri, null, null, null)) {
                    assertEquals(0, cursor.getCount());
                }
            }
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    /**
     * Add a file using DownloadManager.addCompleted and verify that the file has been added
     * to MediaStore as well.
     */
    @Test
    public void testAddCompletedDownload_mediaStoreEntry() throws Exception {
        final String assetName = "noiseandchirps.mp3";
        final String[] downloadPath = new String[] {
                new File(Environment.getExternalStoragePublicDirectory(
                        Environment.DIRECTORY_DOWNLOADS), "file1.mp3").getPath(),
                new File(Environment.getExternalStoragePublicDirectory(
                        Environment.DIRECTORY_MUSIC), "file2.mp3").getPath(),
                "/sdcard/file3.mp3",
        };
        for (String downloadLocation : downloadPath) {
            final File file = new File(Uri.parse(downloadLocation).getPath());
            try (InputStream in = mContext.getAssets().open(assetName);
                 OutputStream out = new FileOutputStream(file)) {
                FileUtils.copy(in, out);
            }

            final long downloadId = mDownloadManager.addCompletedDownload(file.getName(),
                    "Test desc", true,
                    "audio/mp3", downloadLocation, file.length(), true);
            assertTrue(downloadId >= 0);
            final Uri downloadUri = mDownloadManager.getUriForDownloadedFile(downloadId);
            final Uri mediaStoreUri = getMediaStoreUri(downloadUri);
            assertArrayEquals(hash(new FileInputStream(file)),
                    hash(mContext.getContentResolver().openInputStream(mediaStoreUri)));

            // Delete entry in DownloadProvider and verify it's deleted from MediaProvider as well.
            assertRemoveDownload(downloadId, 0);
            try (Cursor cursor = mContext.getContentResolver().query(
                    mediaStoreUri, null, null, null)) {
                assertEquals(0, cursor.getCount());
            }
        }
    }
}
