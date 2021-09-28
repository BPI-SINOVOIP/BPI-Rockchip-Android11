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
package com.android.tradefed.device.cloud;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;

import com.google.common.net.HostAndPort;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.util.Arrays;

/** Unit tests for {@link RemoteFileUtil}. */
@RunWith(JUnit4.class)
public class RemoteFileUtilTest {

    private TestDeviceOptions mOptions;
    private IRunUtil mMockRunUtil;

    @Before
    public void setUp() {
        mMockRunUtil = EasyMock.createMock(IRunUtil.class);
        mOptions = new TestDeviceOptions();
    }

    /** Test fetching a remote file via scp. */
    @Test
    public void testFetchRemoteFile() throws Exception {
        GceAvdInfo fakeInfo = new GceAvdInfo("ins-gce", HostAndPort.fromHost("127.0.0.1"));
        String remotePath = "/home/vsoc-01/cuttlefish_runtime/kernel.log";
        CommandResult res = new CommandResult(CommandStatus.SUCCESS);
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq("scp"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("root@127.0.0.1:" + remotePath),
                                EasyMock.anyObject()))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil);
        File resFile = null;
        try {
            resFile =
                    RemoteFileUtil.fetchRemoteFile(
                            fakeInfo, mOptions, mMockRunUtil, 500L, remotePath);
            // The original remote name is used.
            assertTrue(resFile.getName().startsWith("kernel"));
        } finally {
            FileUtil.deleteFile(resFile);
        }
        EasyMock.verify(mMockRunUtil);
    }

    /** Test when fetching a remote file fails. */
    @Test
    public void testFetchRemoteFile_fail() throws Exception {
        GceAvdInfo fakeInfo = new GceAvdInfo("ins-gce", HostAndPort.fromHost("127.0.0.1"));
        String remotePath = "/home/vsoc-01/cuttlefish_runtime/kernel.log";
        CommandResult res = new CommandResult(CommandStatus.FAILED);
        res.setStderr("Failed to fetch file.");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq("scp"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("root@127.0.0.1:" + remotePath),
                                EasyMock.anyObject()))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil);
        File resFile =
                RemoteFileUtil.fetchRemoteFile(fakeInfo, mOptions, mMockRunUtil, 500L, remotePath);
        assertNull(resFile);
        EasyMock.verify(mMockRunUtil);
    }

    /** Test pulling a directory from the remote hosts. */
    @Test
    public void testFetchRemoteDir() throws Exception {
        GceAvdInfo fakeInfo = new GceAvdInfo("ins-gce", HostAndPort.fromHost("127.0.0.1"));
        String remotePath = "/home/vsoc-01/cuttlefish_runtime/tombstones";
        CommandResult res = new CommandResult(CommandStatus.SUCCESS);
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq("scp"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("-r"),
                                EasyMock.eq("root@127.0.0.1:" + remotePath),
                                EasyMock.anyObject()))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil);
        File resDir = null;
        try {
            resDir =
                    RemoteFileUtil.fetchRemoteDir(
                            fakeInfo, mOptions, mMockRunUtil, 500L, remotePath);
            // The original remote name is used.
            assertTrue(resDir.isDirectory());
        } finally {
            FileUtil.recursiveDelete(resDir);
        }
        EasyMock.verify(mMockRunUtil);
    }

    /** Test pushing a file to a remote instance via scp. */
    @Test
    public void testPushFileToRemote() throws Exception {
        GceAvdInfo fakeInfo = new GceAvdInfo("ins-gce", HostAndPort.fromHost("127.0.0.1"));
        String remotePath = "/home/vsoc-01/cuttlefish_runtime/kernel.log";
        CommandResult res = new CommandResult(CommandStatus.SUCCESS);
        File localFile = FileUtil.createTempDir("test-remote-push-dir");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq("scp"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("-R"),
                                EasyMock.eq(localFile.getAbsolutePath()),
                                EasyMock.eq("root@127.0.0.1:" + remotePath)))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil);

        try {
            boolean result =
                    RemoteFileUtil.pushFileToRemote(
                            fakeInfo,
                            mOptions,
                            Arrays.asList("-R"),
                            mMockRunUtil,
                            500L,
                            remotePath,
                            localFile);
            assertTrue(result);
        } finally {
            FileUtil.recursiveDelete(localFile);
        }
        EasyMock.verify(mMockRunUtil);
    }

    /** Test pushing a file to a remote instance via scp when it fails */
    @Test
    public void testPushFileToRemote_fail() throws Exception {
        GceAvdInfo fakeInfo = new GceAvdInfo("ins-gce", HostAndPort.fromHost("127.0.0.1"));
        String remotePath = "/home/vsoc-01/cuttlefish_runtime/kernel.log";
        CommandResult res = new CommandResult(CommandStatus.FAILED);
        res.setStderr("failed to push to remote.");
        File localFile = FileUtil.createTempDir("test-remote-push-dir");
        EasyMock.expect(
                        mMockRunUtil.runTimedCmd(
                                EasyMock.anyLong(),
                                EasyMock.eq("scp"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("UserKnownHostsFile=/dev/null"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("StrictHostKeyChecking=no"),
                                EasyMock.eq("-o"),
                                EasyMock.eq("ServerAliveInterval=10"),
                                EasyMock.eq("-i"),
                                EasyMock.anyObject(),
                                EasyMock.eq("-R"),
                                EasyMock.eq(localFile.getAbsolutePath()),
                                EasyMock.eq("root@127.0.0.1:" + remotePath)))
                .andReturn(res);
        EasyMock.replay(mMockRunUtil);

        try {
            boolean result =
                    RemoteFileUtil.pushFileToRemote(
                            fakeInfo,
                            mOptions,
                            Arrays.asList("-R"),
                            mMockRunUtil,
                            500L,
                            remotePath,
                            localFile);
            assertFalse(result);
        } finally {
            FileUtil.recursiveDelete(localFile);
        }
        EasyMock.verify(mMockRunUtil);
    }
}
