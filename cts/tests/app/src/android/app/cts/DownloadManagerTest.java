/*
 * Copyright (C) 2011 The Android Open Source Project
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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.DownloadManager;
import android.app.DownloadManager.Query;
import android.app.DownloadManager.Request;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore;
import android.util.Log;
import android.util.Pair;

import androidx.test.filters.FlakyTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CddTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

@RunWith(AndroidJUnit4.class)
public class DownloadManagerTest extends DownloadManagerTestBase {

    @FlakyTest
    @Test
    public void testDownloadManager() throws Exception {
        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            long goodId = mDownloadManager.enqueue(new Request(getGoodUrl()));
            long badId = mDownloadManager.enqueue(new Request(getBadUrl()));

            int allDownloads = getTotalNumberDownloads();
            assertEquals(2, allDownloads);

            assertDownloadQueryableById(goodId);
            assertDownloadQueryableById(badId);

            receiver.waitForDownloadComplete(SHORT_TIMEOUT, goodId, badId);

            assertDownloadQueryableByStatus(DownloadManager.STATUS_SUCCESSFUL);
            assertDownloadQueryableByStatus(DownloadManager.STATUS_FAILED);

            assertRemoveDownload(goodId, allDownloads - 1);
            assertRemoveDownload(badId, allDownloads - 2);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    @FlakyTest
    @Test
    public void testDownloadManagerSupportsHttp() throws Exception {
        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            long id = mDownloadManager.enqueue(new Request(getGoodUrl()));

            assertEquals(1, getTotalNumberDownloads());

            assertDownloadQueryableById(id);

            receiver.waitForDownloadComplete(SHORT_TIMEOUT, id);

            assertDownloadQueryableByStatus(DownloadManager.STATUS_SUCCESSFUL);

            assertRemoveDownload(id, 0);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    @FlakyTest
    @Test
    public void testDownloadManagerSupportsHttpWithExternalWebServer() throws Exception {
        if (!hasInternetConnection()) {
            Log.i(TAG, "testDownloadManagerSupportsHttpWithExternalWebServer() ignored on device without Internet");
            return;
        }

        // As a result of testDownloadManagerSupportsHttpsWithExternalWebServer relying on an
        // external resource https://www.example.com this test uses http://www.example.com to help
        // disambiguate errors from testDownloadManagerSupportsHttpsWithExternalWebServer.

        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            long id = mDownloadManager.enqueue(new Request(Uri.parse("http://www.example.com")));

            assertEquals(1, getTotalNumberDownloads());

            assertDownloadQueryableById(id);

            receiver.waitForDownloadComplete(LONG_TIMEOUT, id);

            assertDownloadQueryableByStatus(DownloadManager.STATUS_SUCCESSFUL);

            assertRemoveDownload(id, 0);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    @Test
    public void testDownloadManagerSupportsHttpsWithExternalWebServer() throws Exception {
        if (!hasInternetConnection()) {
            Log.i(TAG, "testDownloadManagerSupportsHttpsWithExternalWebServer() ignored on device without Internet");
            return;
        }

        // For HTTPS, DownloadManager trusts only SSL server certs issued by CAs trusted by the
        // system. Unfortunately, this means that it cannot trust the mock web server's SSL cert.
        // Until this is resolved (e.g., by making it possible to specify additional CA certs to
        // trust for a particular download), this test relies on https://www.example.com being
        // operational and reachable from the Android under test.

        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            long id = mDownloadManager.enqueue(new Request(Uri.parse("https://www.example.com")));

            assertEquals(1, getTotalNumberDownloads());

            assertDownloadQueryableById(id);

            receiver.waitForDownloadComplete(LONG_TIMEOUT, id);

            assertDownloadQueryableByStatus(DownloadManager.STATUS_SUCCESSFUL);

            assertRemoveDownload(id, 0);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    @CddTest(requirement="7.6.1")
    @Test
    public void testMinimumDownload() throws Exception {
        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            long id = mDownloadManager.enqueue(new Request(getMinimumDownloadUrl()));
            receiver.waitForDownloadComplete(LONG_TIMEOUT, id);

            ParcelFileDescriptor fileDescriptor = mDownloadManager.openDownloadedFile(id);
            assertEquals(MINIMUM_DOWNLOAD_BYTES, fileDescriptor.getStatSize());

            Cursor cursor = null;
            try {
                cursor = mDownloadManager.query(new Query().setFilterById(id));
                assertTrue(cursor.moveToNext());
                assertEquals(DownloadManager.STATUS_SUCCESSFUL, cursor.getInt(
                        cursor.getColumnIndex(DownloadManager.COLUMN_STATUS)));
                assertEquals(MINIMUM_DOWNLOAD_BYTES, cursor.getInt(
                        cursor.getColumnIndex(DownloadManager.COLUMN_TOTAL_SIZE_BYTES)));
                assertFalse(cursor.moveToNext());
            } finally {
                if (cursor != null) {
                    cursor.close();
                }
            }

            assertRemoveDownload(id, 0);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    /**
     * Set download locations and verify that file is downloaded to correct location.
     *
     * Checks three different methods of setting location: directly via setDestinationUri, and
     * indirectly through setDestinationInExternalFilesDir and setDestinationinExternalPublicDir.
     */
    @FlakyTest
    @Test
    public void testDownloadManagerDestination() throws Exception {
        File uriLocation = new File(mContext.getExternalFilesDir(null), "uriFile.bin");
        deleteFromShell(uriLocation);

        File extFileLocation = new File(mContext.getExternalFilesDir(null), "extFile.bin");
        deleteFromShell(extFileLocation);

        File publicLocation = new File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
                "publicFile.bin");
        deleteFromShell(publicLocation);

        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            Request requestUri = new Request(getGoodUrl());
            requestUri.setDestinationUri(Uri.fromFile(uriLocation));
            long uriId = mDownloadManager.enqueue(requestUri);

            Request requestExtFile = new Request(getGoodUrl());
            requestExtFile.setDestinationInExternalFilesDir(mContext, null, "extFile.bin");
            long extFileId = mDownloadManager.enqueue(requestExtFile);

            Request requestPublic = new Request(getGoodUrl());
            requestPublic.setDestinationInExternalPublicDir(Environment.DIRECTORY_DOWNLOADS,
                    "publicFile.bin");
            long publicId = mDownloadManager.enqueue(requestPublic);

            int allDownloads = getTotalNumberDownloads();
            assertEquals(3, allDownloads);

            receiver.waitForDownloadComplete(SHORT_TIMEOUT, uriId, extFileId, publicId);

            assertSuccessfulDownload(uriId, uriLocation);
            assertSuccessfulDownload(extFileId, extFileLocation);
            assertSuccessfulDownload(publicId, publicLocation);

            assertRemoveDownload(uriId, allDownloads - 1);
            assertRemoveDownload(extFileId, allDownloads - 2);
            assertRemoveDownload(publicId, allDownloads - 3);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    /**
     * Set the download location and verify that the extension of the file name is left unchanged.
     */
    @Test
    public void testDownloadManagerDestinationExtension() throws Exception {
        String noExt = "noiseandchirps";
        File noExtLocation = new File(mContext.getExternalFilesDir(null), noExt);
        deleteFromShell(noExtLocation);

        String wrongExt = "noiseandchirps.wrong";
        File wrongExtLocation = new File(mContext.getExternalFilesDir(null), wrongExt);
        deleteFromShell(wrongExtLocation);

        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            Request requestNoExt = new Request(getAssetUrl(noExt));
            requestNoExt.setDestinationUri(Uri.fromFile(noExtLocation));
            long noExtId = mDownloadManager.enqueue(requestNoExt);

            Request requestWrongExt = new Request(getAssetUrl(wrongExt));
            requestWrongExt.setDestinationUri(Uri.fromFile(wrongExtLocation));
            long wrongExtId = mDownloadManager.enqueue(requestWrongExt);

            int allDownloads = getTotalNumberDownloads();
            assertEquals(2, allDownloads);

            receiver.waitForDownloadComplete(SHORT_TIMEOUT, noExtId, wrongExtId);

            assertSuccessfulDownload(noExtId, noExtLocation);
            assertSuccessfulDownload(wrongExtId, wrongExtLocation);

            assertRemoveDownload(noExtId, allDownloads - 1);
            assertRemoveDownload(wrongExtId, allDownloads - 2);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    @Test
    public void testSetDestinationUri_invalidRequests() throws Exception {
        final File documentsFile = new File(
                Environment.getExternalStoragePublicDirectory("TestDir"),
                "uriFile.bin");
        deleteFromShell(documentsFile);

        final Request badRequest = new Request(getGoodUrl());
        badRequest.setDestinationUri(Uri.fromFile(documentsFile));
        try {
            mDownloadManager.enqueue(badRequest);
            fail(documentsFile + " is not valid for setDestinationUri()");
        } catch (Exception e) {
            // Expected
        }

        final Uri badUri = Uri.parse("file:///sdcard/Android/data/"
                + mContext.getPackageName() + "/../uriFile.bin");
        final Request badRequest2 = new Request(getGoodUrl());
        badRequest2.setDestinationUri(badUri);
        try {
            mDownloadManager.enqueue(badRequest2);
            fail(badUri + " is not valid for setDestinationUri()");
        } catch (Exception e) {
            // Expected
        }

        final File sdcardPath = new File("/sdcard/uriFile.bin");
        final Request badRequest3 = new Request(getGoodUrl());
        badRequest3.setDestinationUri(Uri.fromFile(sdcardPath));
        try {
            mDownloadManager.enqueue(badRequest2);
            fail(sdcardPath + " is not valid for setDestinationUri()");
        } catch (Exception e) {
            // Expected
        }
    }

    @Test
    public void testSetDestinationUri() throws Exception {
        final File[] destinationFiles = {
                new File(Environment.getExternalStoragePublicDirectory(
                        Environment.DIRECTORY_DOWNLOADS), System.nanoTime() + "_uriFile.bin"),
                new File(Environment.getExternalStoragePublicDirectory(
                        Environment.DIRECTORY_DOWNLOADS), System.nanoTime() + "_file.jpg"),
        };

        for (File downloadsFile : destinationFiles) {
            final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
            try {
                IntentFilter intentFilter = new IntentFilter(
                        DownloadManager.ACTION_DOWNLOAD_COMPLETE);
                mContext.registerReceiver(receiver, intentFilter);

                DownloadManager.Request request = new DownloadManager.Request(getGoodUrl());
                request.setDestinationUri(Uri.fromFile(downloadsFile));
                long id = mDownloadManager.enqueue(request);

                int allDownloads = getTotalNumberDownloads();
                assertEquals(1, allDownloads);

                receiver.waitForDownloadComplete(SHORT_TIMEOUT, id);
                assertSuccessfulDownload(id, downloadsFile);

                assertRemoveDownload(id, 0);
            } finally {
                mContext.unregisterReceiver(receiver);
            }
        }
    }

    @Test
    public void testSetDestinationInExternalPublicDownloadDir() throws Exception {
        final String[] destinations = {
                Environment.DIRECTORY_DOWNLOADS,
                Environment.DIRECTORY_DCIM,
        };
        final String[] subPaths = {
                "testing/" + System.nanoTime() + "_publicFile.bin",
                System.nanoTime() + "_file.jpg",
        };

        for (int i = 0; i < destinations.length; ++i) {
            final String destination = destinations[i];
            final String subPath = subPaths[i];

            final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
            try {
                IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
                mContext.registerReceiver(receiver, intentFilter);

                DownloadManager.Request requestPublic = new DownloadManager.Request(getGoodUrl());
                requestPublic.setDestinationInExternalPublicDir(destination, subPath);
                long id = mDownloadManager.enqueue(requestPublic);

                int allDownloads = getTotalNumberDownloads();
                assertEquals(1, allDownloads);

                receiver.waitForDownloadComplete(SHORT_TIMEOUT, id);
                assertSuccessfulDownload(id, new File(
                        Environment.getExternalStoragePublicDirectory(destination), subPath));

                assertRemoveDownload(id, 0);
            } finally {
                mContext.unregisterReceiver(receiver);
            }
        }
    }

    @Test
    public void testSetDestinationInExternalPublicDir_invalidRequests() {
        final Request badRequest2 = new Request(getGoodUrl());
        try {
            badRequest2.setDestinationInExternalPublicDir("TestDir", "uriFile.bin");
            mDownloadManager.enqueue(badRequest2);
            fail("/sdcard/TestDir/uriFile.bin"
                    + " is not valid for setDestinationInExternalPublicDir()");
        } catch (Exception e) {
            // Expected
        }
    }

    private String cannonicalizeProcessName(ApplicationInfo ai) {
        return cannonicalizeProcessName(ai.processName, ai);
    }

    private String cannonicalizeProcessName(String process, ApplicationInfo ai) {
        if (process == null) {
            return null;
        }
        // Handle private scoped process names.
        if (process.startsWith(":")) {
            return ai.packageName + process;
        }
        return process;
    }

    @FlakyTest
    @Test
    public void testProviderAcceptsCleartext() throws Exception {
        // Check that all the applications that share an android:process with the DownloadProvider
        // accept cleartext traffic. Otherwise process loading races can lead to inconsistent flags.
        final PackageManager pm = mContext.getPackageManager();
        ProviderInfo downloadInfo = pm.resolveContentProvider("downloads", 0);
        assertNotNull(downloadInfo);
        String downloadProcess
                = cannonicalizeProcessName(downloadInfo.processName, downloadInfo.applicationInfo);

        for (PackageInfo pi : mContext.getPackageManager().getInstalledPackages(0)) {
            if (downloadProcess.equals(cannonicalizeProcessName(pi.applicationInfo))) {
                assertTrue("package: " + pi.applicationInfo.packageName
                        + " must set android:usesCleartextTraffic=true"
                        ,(pi.applicationInfo.flags & ApplicationInfo.FLAG_USES_CLEARTEXT_TRAFFIC)
                        != 0);
            }
        }
    }

    /**
     * Test that a download marked as not visible in Downloads ui can be successfully downloaded.
     */
    @Test
    public void testDownloadNotVisibleInUi() throws Exception {
        File uriLocation = new File(mContext.getExternalFilesDir(null), "uriFile.bin");
        deleteFromShell(uriLocation);

        final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
        try {
            IntentFilter intentFilter = new IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE);
            mContext.registerReceiver(receiver, intentFilter);

            final Request request = new Request(getGoodUrl());
            request.setDestinationUri(Uri.fromFile(uriLocation))
                    .setVisibleInDownloadsUi(false);
            long uriId = mDownloadManager.enqueue(request);

            int allDownloads = getTotalNumberDownloads();
            assertEquals(1, allDownloads);

            receiver.waitForDownloadComplete(SHORT_TIMEOUT, uriId);

            assertSuccessfulDownload(uriId, uriLocation);

            assertRemoveDownload(uriId, 0);
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }

    //TODO(b/130797842): Remove FlakyTest annotation after that bug is fixed.
    @FlakyTest
    @Test
    public void testAddCompletedDownload() throws Exception {
        final String fileContents = "RED;GREEN;BLUE";
        final File file = createFile(mContext.getExternalFilesDir(null), "colors.txt");

        writeToFile(file, fileContents);

        final long id = mDownloadManager.addCompletedDownload(file.getName(), "Test desc", true,
                "text/plain", file.getPath(), fileContents.getBytes().length, true);
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

    @Test
    public void testAddCompletedDownload_downloadDir() throws Exception {
        final String path = createFile(Environment.getExternalStoragePublicDirectory(
                        Environment.DIRECTORY_DOWNLOADS), "file1.txt").getPath();

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

    @Test
    public void testAddCompletedDownload_invalidPaths() throws Exception {
        final String fileContents = "RED;GREEN;BLUE";

        // Try adding internal path
        File file = createFile(mContext.getFilesDir(), "colors.txt");
        writeToFile(file, fileContents);
        try {
            mDownloadManager.addCompletedDownload("Test title", "Test desc", true,
                    "text/plain", file.getPath(), fileContents.getBytes().length, true);
            fail("addCompletedDownload should have failed for adding internal path");
        } catch (Exception e) {
            // expected
        }

        // Try adding path in top-level documents dir
        file = new File(
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS),
                "colors.txt");
        writeToFileWithDelegator(file, fileContents);
        try {
            mDownloadManager.addCompletedDownload("Test title", "Test desc", true,
                    "text/plain", file.getPath(), fileContents.getBytes().length, true);
            fail("addCompletedDownload should have failed for top-level download dir");
        } catch (SecurityException e) {
            // expected
        }

        // Try adding top-level sdcard path
        final String path = "/sdcard/test-download.txt";
        writeToFileWithDelegator(new File(path), fileContents);
        try {
            mDownloadManager.addCompletedDownload("Test title", "Test desc", true,
                    "text/plain", path, fileContents.getBytes().length, true);
            fail("addCompletedDownload should have failed for top-level sdcard path");
        } catch (SecurityException e) {
            // expected
        }


        final String path2 = "/sdcard/Android/data/" + mContext.getPackageName()
                + "/../uriFile.bin";
        try {
            mDownloadManager.addCompletedDownload("Test title", "Test desc", true,
                    "text/plain", path2, fileContents.getBytes().length, true);
            fail(path2 + " is not valid for addCompleteDownload()");
        } catch (Exception e) {
            // Expected
        }

        // Try adding non-existent path
        try {
            mDownloadManager.addCompletedDownload("Test title", "Test desc", true, "text/plain",
                    new File(mContext.getExternalFilesDir(null), "test_file.mp4").getPath(),
                    fileContents.getBytes().length, true);
            fail("addCompletedDownload should have failed for adding non-existent path");
        } catch (Exception e) {
            // expected
        }

        // Try adding random string
        try {
            mDownloadManager.addCompletedDownload("Test title", "Test desc", true,
                    "text/plain", "RANDOM", fileContents.getBytes().length, true);
            fail("addCompletedDownload should have failed for adding random string");
        } catch (Exception e) {
            // expected
        }
    }

    @Test
    public void testDownload_mediaScanned() throws Exception {
        final String[] destinations = {
                Environment.DIRECTORY_MUSIC,
                Environment.DIRECTORY_DOWNLOADS,
        };
        final String[] subPaths = {
                "testmp3.mp3",
                "testvideo.3gp",
        };
        final Pair<String, String>[] expectedMediaAttributes = new Pair[] {
                Pair.create(MediaStore.Audio.AudioColumns.IS_MUSIC, "1"),
                Pair.create(MediaStore.Video.VideoColumns.DURATION, "11047"),
        };

        for (int i = 0; i < destinations.length; ++i) {
            final String destination = destinations[i];
            final String subPath = subPaths[i];

            final DownloadCompleteReceiver receiver = new DownloadCompleteReceiver();
            try {
                IntentFilter intentFilter = new IntentFilter(
                        DownloadManager.ACTION_DOWNLOAD_COMPLETE);
                mContext.registerReceiver(receiver, intentFilter);

                DownloadManager.Request requestPublic = new DownloadManager.Request(
                        getAssetUrl(subPath));
                requestPublic.setDestinationInExternalPublicDir(destination, subPath);
                long id = mDownloadManager.enqueue(requestPublic);

                int allDownloads = getTotalNumberDownloads();
                assertEquals(1, allDownloads);

                receiver.waitForDownloadComplete(SHORT_TIMEOUT, id);
                assertSuccessfulDownload(id, new File(
                        Environment.getExternalStoragePublicDirectory(destination), subPath));

                final Uri downloadUri = mDownloadManager.getUriForDownloadedFile(id);
                mContext.grantUriPermission("com.android.shell", downloadUri,
                        Intent.FLAG_GRANT_READ_URI_PERMISSION);
                final Uri mediaStoreUri = getMediaStoreUri(downloadUri);
                assertEquals(expectedMediaAttributes[i].second,
                        getMediaStoreColumnValue(mediaStoreUri, expectedMediaAttributes[i].first));
                final int expectedSize = getTotalBytes(
                        mContext.getContentResolver().openInputStream(downloadUri));
                assertEquals(expectedSize, Integer.parseInt(getMediaStoreColumnValue(
                        mediaStoreUri, MediaStore.MediaColumns.SIZE)));

                assertRemoveDownload(id, 0);
            } finally {
                mContext.unregisterReceiver(receiver);
            }
        }
    }
}
