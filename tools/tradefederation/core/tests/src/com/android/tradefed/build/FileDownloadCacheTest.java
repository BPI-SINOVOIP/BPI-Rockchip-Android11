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
package com.android.tradefed.build;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.StreamUtil;

import org.easymock.EasyMock;
import org.easymock.IAnswer;
import org.easymock.IExpectationSetters;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link FileDownloadCache}. */
@RunWith(JUnit4.class)
public class FileDownloadCacheTest {

    private static final String REMOTE_PATH = "foo/path";
    private static final String DOWNLOADED_CONTENTS = "downloaded contents";


    private IFileDownloader mMockDownloader;

    private File mCacheDir;
    private FileDownloadCache mCache;
    private boolean mFailCopy = false;

    @Before
    public void setUp() throws Exception {
        mMockDownloader = EasyMock.createMock(IFileDownloader.class);
        mCacheDir = FileUtil.createTempDir("unittest");
        mCache = new FileDownloadCache(mCacheDir);
    }

    @After
    public void tearDown() throws Exception {
        mCache.empty();
        FileUtil.recursiveDelete(mCacheDir);
    }

    /** Test basic case for {@link FileDownloadCache#fetchRemoteFile(IFileDownloader, String)}. */
    @Test
    public void testFetchRemoteFile() throws Exception {
        setDownloadExpections();
        EasyMock.replay(mMockDownloader);
        assertFetchRemoteFile();
        EasyMock.verify(mMockDownloader);
    }

    /**
     * Test basic case for {@link FileDownloadCache#fetchRemoteFile(IFileDownloader, String, File)}.
     */
    @Test
    public void testFetchRemoteFile_destFile() throws Exception {
        setDownloadExpections();
        EasyMock.replay(mMockDownloader);
        File destFile = FileUtil.createTempFile("test-download-cache", "txt");
        assertFetchRemoteFile(REMOTE_PATH, null, destFile);
        EasyMock.verify(mMockDownloader);
    }

    @Test
    public void testFetchRemoteFile_destFile_nullPath() throws Exception {
        EasyMock.replay(mMockDownloader);
        try {
            assertFetchRemoteFile(null, null, null);
            fail("Should have thrown an exception.");
        } catch (BuildRetrievalError expected) {
            assertEquals("remote path was null.", expected.getMessage());
        }
        EasyMock.verify(mMockDownloader);
    }

    /**
     * Test {@link FileDownloadCache#fetchRemoteFile(IFileDownloader, String)} when file can be
     * retrieved from cache.
     */
    @Test
    public void testFetchRemoteFile_cacheHit() throws Exception {
        setDownloadExpections();
        EasyMock.replay(mMockDownloader);
        assertFetchRemoteFile();
        EasyMock.verify(mMockDownloader);

        // now retrieve file again
        EasyMock.reset(mMockDownloader);
        setFreshnessExpections(true);
        EasyMock.replay(mMockDownloader);
        assertFetchRemoteFile();
        // verify only one download call occurred. It is not called at the second time.
        EasyMock.verify(mMockDownloader);
    }

    /**
     * Test {@link FileDownloadCache#fetchRemoteFile(IFileDownloader, String)} when file can be
     * retrieved from cache but cache is not fresh.
     */
    @Test
    public void testFetchRemoteFile_cacheHit_notFresh() throws Exception {
        setDownloadExpections();
        EasyMock.replay(mMockDownloader);
        assertFetchRemoteFile();
        EasyMock.verify(mMockDownloader);

        // now retrieve file again
        EasyMock.reset(mMockDownloader);
        setFreshnessExpections(false);
        setDownloadExpections();
        EasyMock.replay(mMockDownloader);
        assertFetchRemoteFile();
        // Assert the download is called again.
        EasyMock.verify(mMockDownloader);
    }

    /**
     * Test {@link FileDownloadCache#fetchRemoteFile(IFileDownloader, String)} when cache grows
     * larger than max
     */
    @Test
    public void testFetchRemoteFile_cacheSizeExceeded() throws Exception {
        final String remotePath2 = "anotherpath";
        // set cache size to be small
        mCache.setMaxCacheSize(DOWNLOADED_CONTENTS.length() + 1);
        setDownloadExpections(remotePath2);
        setDownloadExpections();
        EasyMock.replay(mMockDownloader);
        assertFetchRemoteFile(remotePath2, null);
        // now retrieve another file, which will exceed size of cache
        assertFetchRemoteFile();
        assertNotNull(mCache.getCachedFile(REMOTE_PATH));
        assertNull(mCache.getCachedFile(remotePath2));
        EasyMock.verify(mMockDownloader);
    }

    /**
     * Test {@link FileDownloadCache#fetchRemoteFile(IFileDownloader, String)} when download fails
     */
    @Test
    public void testFetchRemoteFile_downloadFailed() throws Exception {
        mMockDownloader.downloadFile(EasyMock.eq(REMOTE_PATH),
                (File)EasyMock.anyObject());
        EasyMock.expectLastCall()
                .andThrow(
                        new BuildRetrievalError(
                                "download error", InfraErrorIdentifier.ARTIFACT_DOWNLOAD_ERROR));
        EasyMock.replay(mMockDownloader);
        try {
            mCache.fetchRemoteFile(mMockDownloader, REMOTE_PATH);
            fail("BuildRetrievalError not thrown");
        } catch (BuildRetrievalError e) {
            // expected
        }
        assertNull(mCache.getCachedFile(REMOTE_PATH));
        EasyMock.verify(mMockDownloader);
    }

    /**
     * Test {@link FileDownloadCache#fetchRemoteFile(IFileDownloader, String)} when download fails
     * with RuntimeException.
     */
    @Test
    public void testFetchRemoteFile_downloadFailed_Runtime() throws Exception {
        mMockDownloader.downloadFile(EasyMock.eq(REMOTE_PATH), (File) EasyMock.anyObject());
        EasyMock.expectLastCall().andThrow(new RuntimeException("download error"));
        EasyMock.replay(mMockDownloader);
        try {
            mCache.fetchRemoteFile(mMockDownloader, REMOTE_PATH);
            fail("RuntimeException not thrown");
        } catch (RuntimeException e) {
            // expected
        }
        assertNull(mCache.getCachedFile(REMOTE_PATH));
        EasyMock.verify(mMockDownloader);
    }

    /**
     * Test {@link FileDownloadCache#fetchRemoteFile(IFileDownloader, String)} when copy of a cached
     * file is missing
     */
    @Test
    public void testFetchRemoteFile_cacheMissing() throws Exception {
        // perform successful download
        setDownloadExpections(REMOTE_PATH);
        EasyMock.replay(mMockDownloader);
        assertFetchRemoteFile();
        EasyMock.verify(mMockDownloader);
        // now be sneaky and delete the cachedFile, so copy will fail
        File cachedFile = mCache.getCachedFile(REMOTE_PATH);
        assertNotNull(cachedFile);
        boolean res = cachedFile.delete();
        assertTrue(res);
        File file = null;
        try {
            EasyMock.reset(mMockDownloader);
            setDownloadExpections(REMOTE_PATH);
            EasyMock.replay(mMockDownloader);
            file = mCache.fetchRemoteFile(mMockDownloader, REMOTE_PATH);
            // file should have been updated in cache.
            assertNotNull(file);
            assertNotNull(mCache.getCachedFile(REMOTE_PATH));
            EasyMock.verify(mMockDownloader);
        } finally {
            FileUtil.deleteFile(file);
        }
    }

    /**
     * Test {@link FileDownloadCache#fetchRemoteFile(IFileDownloader, String)} when copy of a cached
     * file fails
     */
    @Test
    public void testFetchRemoteFile_copyFailed() throws Exception {
        mCache =
                new FileDownloadCache(mCacheDir) {
                    @Override
                    File copyFile(String remotePath, File cachedFile, File desFile)
                            throws BuildRetrievalError {
                        if (mFailCopy) {
                            FileUtil.deleteFile(cachedFile);
                        }
                        return super.copyFile(remotePath, cachedFile, desFile);
                    }
                };
        // perform successful download
        setDownloadExpections(REMOTE_PATH);
        EasyMock.replay(mMockDownloader);
        assertFetchRemoteFile();
        EasyMock.verify(mMockDownloader);

        mFailCopy = true;
        try {
            EasyMock.reset(mMockDownloader);
            setFreshnessExpections(true);
            EasyMock.replay(mMockDownloader);
            mCache.fetchRemoteFile(mMockDownloader, REMOTE_PATH);
            fail("BuildRetrievalError not thrown");
        } catch (BuildRetrievalError e) {
            // expected
        }
        // file should be removed from cache
        assertNull(mCache.getCachedFile(REMOTE_PATH));
        EasyMock.verify(mMockDownloader);
    }

    /**
     * Test {@link FileDownloadCache#fetchRemoteFile(IFileDownloader, String)} when remote is a
     * folder.
     */
    @Test
    public void testFetchRemoteFile_folder() throws Exception {
        List<String> relativePaths = new ArrayList<String>();
        relativePaths.add("file.txt");
        relativePaths.add("folder1/file1.txt");
        relativePaths.add("folder1/folder2/file2.txt");
        setDownloadExpections(REMOTE_PATH, relativePaths);
        EasyMock.replay(mMockDownloader);
        assertFetchRemoteFile(REMOTE_PATH, relativePaths);
        EasyMock.verify(mMockDownloader);
    }

    /** Test that when the cache is rebuilt we can find the file without a new download. */
    @Test
    public void testCacheRebuild() throws Exception {
        File cacheDir = FileUtil.createTempDir("cache-unittest");
        File subDir = FileUtil.createTempDir("subdir", cacheDir);
        File file = FileUtil.createTempFile("test-cache-file", ".txt", subDir);
        FileUtil.writeToFile("test", file);
        File cacheFile = null;
        try {
            mCache = new FileDownloadCache(cacheDir);
            setFreshnessExpections(true);

            EasyMock.replay(mMockDownloader);
            cacheFile =
                    mCache.fetchRemoteFile(
                            mMockDownloader, subDir.getName() + "/" + file.getName());
            assertNotNull(cacheFile);
            EasyMock.verify(mMockDownloader);
        } finally {
            FileUtil.recursiveDelete(cacheDir);
            FileUtil.deleteFile(cacheFile);
        }
    }

    /** Test that keys with multiple slashes are properly handled. */
    @Test
    public void testCacheRebuild_multiSlashPath() throws Exception {
        String gsPath = "foo//bar";
        // Perform successful download
        setDownloadExpections(gsPath);
        EasyMock.replay(mMockDownloader);
        assertFetchRemoteFile(gsPath, null);
        EasyMock.verify(mMockDownloader);

        File cachedFile = mCache.getCachedFile(gsPath);
        try {
            assertNotNull(cachedFile);

            // Now rebuild the cache and try to find our file
            mCache = new FileDownloadCache(mCacheDir);
            File cachedFileRebuilt = mCache.getCachedFile(gsPath);
            assertNotNull(cachedFileRebuilt);

            assertEquals(cachedFile, cachedFileRebuilt);
        } finally {
            FileUtil.deleteFile(cachedFile);
        }
    }

    /** Perform one fetchRemoteFile call and verify contents for default remote path */
    private void assertFetchRemoteFile() throws BuildRetrievalError, IOException {
        assertFetchRemoteFile(REMOTE_PATH, null);
    }

    private void assertFetchRemoteFile(String remotePath, List<String> relativePaths)
            throws BuildRetrievalError, IOException {
        assertFetchRemoteFile(remotePath, relativePaths, null);
    }

    /** Perform one fetchRemoteFile call and verify contents */
    private void assertFetchRemoteFile(String remotePath, List<String> relativePaths, File dest)
            throws BuildRetrievalError, IOException {
        // test downloading file not in cache
        File fileCopy = dest;
        if (dest != null) {
            mCache.fetchRemoteFile(mMockDownloader, remotePath, dest);
        } else {
            fileCopy = mCache.fetchRemoteFile(mMockDownloader, remotePath);
        }
        try {
            assertNotNull(mCache.getCachedFile(remotePath));
            if (relativePaths == null || relativePaths.size() == 0) {
                String contents = StreamUtil.getStringFromStream(new FileInputStream(fileCopy));
                assertEquals(DOWNLOADED_CONTENTS, contents);
                FileUtil.chmodGroupRWX(fileCopy);
                CommandResult res =
                        RunUtil.getDefault().runTimedCmd(60000, fileCopy.getAbsolutePath());
                assertNotEquals(
                        "File should not be busy.", CommandStatus.EXCEPTION, res.getStatus());
            } else {
                assertTrue(fileCopy.isDirectory());
                for (String relativePath : relativePaths) {
                    File file = Paths.get(fileCopy.getAbsolutePath(), relativePath).toFile();
                    assertEquals(DOWNLOADED_CONTENTS, FileUtil.readStringFromFile(file));
                }
            }

        } finally {
            FileUtil.recursiveDelete(fileCopy);
        }
    }

    /**
     * Set EasyMock expectations for a downloadFile call for default remote path
     */
    private void setDownloadExpections() throws BuildRetrievalError {
        setDownloadExpections(REMOTE_PATH, null);
    }

    /** Set EasyMock expectations for a downloadFile call. */
    private void setDownloadExpections(String remotePath) throws BuildRetrievalError {
        setDownloadExpections(remotePath, null);
    }

    /** Set EasyMock expectations for a downloadFile call */
    private IExpectationSetters<Object> setDownloadExpections(
            String remotePath, List<String> relativePaths) throws BuildRetrievalError {
        IAnswer<Object> downloadAnswer =
                new IAnswer<Object>() {
                    @Override
                    public Object answer() throws Throwable {
                        File fileArg = (File) EasyMock.getCurrentArguments()[1];
                        if (relativePaths == null || relativePaths.size() == 0) {
                            FileUtil.writeToFile(DOWNLOADED_CONTENTS, fileArg);
                        } else {
                            fileArg.mkdir();
                            for (String relativePath : relativePaths) {
                                File file =
                                        Paths.get(fileArg.getAbsolutePath(), relativePath).toFile();
                                file.getParentFile().mkdirs();
                                FileUtil.writeToFile(DOWNLOADED_CONTENTS, file);
                            }
                        }
                        return null;
                    }
                };

        mMockDownloader.downloadFile(EasyMock.eq(remotePath),
                EasyMock.<File>anyObject());
        return EasyMock.expectLastCall().andAnswer(downloadAnswer);
    }

    /** Set EasyMock expectations for a checkFreshness call */
    private void setFreshnessExpections(boolean freshness) throws BuildRetrievalError {
        EasyMock.expect(
                        mMockDownloader.isFresh(
                                EasyMock.<File>anyObject(), EasyMock.<String>anyObject()))
                .andReturn(freshness);
    }
}
