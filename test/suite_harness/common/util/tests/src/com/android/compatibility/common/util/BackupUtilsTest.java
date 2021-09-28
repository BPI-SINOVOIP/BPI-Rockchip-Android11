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
 * limitations under the License
 */

package com.android.compatibility.common.util;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.util.RunUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Unit tests for {@link BackupUtils}
 */
@RunWith(JUnit4.class)
public class BackupUtilsTest {
    private static final int BACKUP_SERVICE_INIT_TIMEOUT_SECS = 1;
    private static final int TEST_USER_ID = 10;

    private boolean mIsDumpsysCommandCalled;
    private boolean mIsEnableCommandCalled;
    private boolean mIsActivateCommandCalled;

    @Before
    public void setUp() {
        mIsDumpsysCommandCalled = false;
        mIsEnableCommandCalled = false;
    }

    @Test
    public void testEnableBackup_whenEnableTrueAndEnabled_returnsTrue() throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("bmgr enabled")) {
                    output = "Backup Manager currently enabled";
                } else if (command.equals("bmgr enable true")) {
                    output = "Backup Manager now enabled";
                    mIsEnableCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };
        assertTrue(backupUtils.enableBackup(true));
        assertTrue(mIsEnableCommandCalled);
    }

    @Test
    public void testEnableBackup_whenEnableTrueAndDisabled_returnsFalse() throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("bmgr enabled")) {
                    output = "Backup Manager currently disabled";
                } else if (command.equals("bmgr enable true")) {
                    output = "Backup Manager now enabled";
                    mIsEnableCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };
        assertFalse(backupUtils.enableBackup(true));
        assertTrue(mIsEnableCommandCalled);
    }

    @Test
    public void testEnableBackup_whenEnableFalseAndEnabled_returnsTrue() throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("bmgr enabled")) {
                    output = "Backup Manager currently enabled";
                } else if (command.equals("bmgr enable false")) {
                    output = "Backup Manager now disabled";
                    mIsEnableCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };
        assertTrue(backupUtils.enableBackup(false));
        assertTrue(mIsEnableCommandCalled);
    }

    @Test
    public void testEnableBackup_whenEnableFalseAndDisabled_returnsFalse() throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("bmgr enabled")) {
                    output = "Backup Manager currently disabled";
                } else if (command.equals("bmgr enable false")) {
                    output = "Backup Manager now disabled";
                    mIsEnableCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };
        assertFalse(backupUtils.enableBackup(false));
        assertTrue(mIsEnableCommandCalled);
    }

    @Test
    public void testEnableBackup_whenEnableTrueAndEnabledAndCommandsReturnMultipleLines()
            throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("bmgr enabled")) {
                    output = "Backup Manager currently enabled" + "\n...";
                } else if (command.equals("bmgr enable true")) {
                    output = "Backup Manager now enabled" + "\n...";
                    mIsEnableCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };
        assertTrue(backupUtils.enableBackup(true));
        assertTrue(mIsEnableCommandCalled);
    }

    @Test
    public void testEnableBackup_whenQueryCommandThrows_propagatesException() throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("bmgr enabled")) {
                    throw new IOException(String.format(
                            "enableBackup: Failed to run command: %s", command));
                } else if (command.equals("bmgr enable true")) {
                    output = "Backup Manager now enabled";
                    mIsEnableCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };

        boolean isExceptionHappened = false;
        try {
            backupUtils.enableBackup(true);
        } catch (IOException e) {
            // enableBackup: Failed to run command: bmgr enabled
            isExceptionHappened = true;
        }
        assertTrue(isExceptionHappened);
        assertFalse(mIsEnableCommandCalled);
    }

    @Test
    public void testEnableBackup_whenSetCommandThrows_propagatesException() throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("bmgr enabled")) {
                    output = "Backup Manager currently enabled";
                } else if (command.equals("bmgr enable true")) {
                    mIsEnableCommandCalled = true;
                    throw new IOException(String.format(
                            "enableBackup: Failed to run command: %s", command));
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };

        boolean isExceptionHappened = false;
        try {
            backupUtils.enableBackup(true);
        } catch (IOException e) {
            // enableBackup: Failed to run command: bmgr enable true
            isExceptionHappened = true;
        }
        assertTrue(isExceptionHappened);
        assertTrue(mIsEnableCommandCalled);
    }

    @Test
    public void testEnableBackup_whenQueryCommandReturnsInvalidString_throwsException()
            throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("bmgr enabled")) {
                    output = "Backup Manager ???";
                } else if (command.equals("bmgr enable true")) {
                    output = "Backup Manager now enabled";
                    mIsEnableCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };

        boolean isExceptionHappened = false;
        try {
            backupUtils.enableBackup(true);
        } catch (RuntimeException e) {
            // non-parsable output setting bmgr enabled: Backup Manager ???
            isExceptionHappened = true;
        }
        assertTrue(isExceptionHappened);
        assertFalse(mIsEnableCommandCalled);
    }

    @Test
    public void testEnableBackup_whenQueryCommandReturnsEmptyString_throwsException()
            throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("bmgr enabled")) {
                    // output is empty already
                } else if (command.equals("bmgr enable true")) {
                    output = "Backup Manager now enabled";
                    mIsEnableCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };

        boolean isExceptionHappened = false;
        try {
            backupUtils.enableBackup(true);
        } catch (NullPointerException e) {
            // null output by running command, bmgr enabled
            isExceptionHappened = true;
        }
        assertTrue(isExceptionHappened);
        assertFalse(mIsEnableCommandCalled);
    }

    @Test
    public void testWaitForBackupInitialization_whenEnabled() throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("dumpsys backup")) {
                    output = "Backup Manager is enabled / provisioned / not pending init";
                    mIsDumpsysCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };
        backupUtils.waitForBackupInitialization();
        assertTrue(mIsDumpsysCommandCalled);
    }

    @Test
    public void testWaitForBackupInitialization_whenDisabled() throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("dumpsys backup")) {
                    output = "Backup Manager is disabled / provisioned / not pending init";
                    mIsDumpsysCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };
        backupUtils.waitForBackupInitialization();
        assertTrue(mIsDumpsysCommandCalled);
    }

    @Test
    public void testWaitUntilBackupServiceIsRunning_whenRunning_doesntThrow() throws Exception {
        BackupUtils backupUtils = constructDumpsysForBackupUsers(TEST_USER_ID);

        try {
            backupUtils.waitUntilBackupServiceIsRunning(
                    TEST_USER_ID, BACKUP_SERVICE_INIT_TIMEOUT_SECS);
        } catch (AssertionError e) {
            fail("BackupUtils#waitUntilBackupServiceIsRunning threw an exception");
        }
        assertTrue(mIsDumpsysCommandCalled);
    }

    @Test
    public void testWaitUntilBackupServiceIsRunning_whenNotRunning_throws() throws Exception {
        // Pass in a different userId to not have the current one among running ids.
        BackupUtils backupUtils = constructDumpsysForBackupUsers(TEST_USER_ID + 1);

        boolean wasExceptionThrown = false;
        try {
            backupUtils.waitUntilBackupServiceIsRunning(
                    TEST_USER_ID, BACKUP_SERVICE_INIT_TIMEOUT_SECS);
        } catch (AssertionError e) {
            wasExceptionThrown = true;
        }

        assertTrue(mIsDumpsysCommandCalled);
        assertTrue(wasExceptionThrown);
    }

    private BackupUtils constructDumpsysForBackupUsers(int runningUserId) {
        return new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("dumpsys backup users")) {
                    output = "Backup Manager is running for users: " + runningUserId;
                    mIsDumpsysCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };
    }

    @Test
    public void testWaitForBackupInitialization_whenEnabledAndCommandReturnsMultipleLines()
            throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("dumpsys backup")) {
                    output = "Backup Manager is enabled / provisioned / not pending init" + "\n...";
                    mIsDumpsysCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };
        backupUtils.waitForBackupInitialization();
        assertTrue(mIsDumpsysCommandCalled);
    }

    @Test
    public void testWaitForBackupInitialization_whenCommandThrows_propagatesException()
            throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("dumpsys backup")) {
                    mIsDumpsysCommandCalled = true;
                    throw new IOException(String.format(
                            "waitForBackupInitialization: Failed to run command: %s", command));
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };

        boolean isExceptionHappened = false;
        try {
            backupUtils.waitForBackupInitialization();
        } catch (IOException e) {
            // waitForBackupInitialization: Failed to run command: dumpsys backup
            isExceptionHappened = true;
        }
        assertTrue(isExceptionHappened);
        assertTrue(mIsDumpsysCommandCalled);
    }

    @Test
    public void testWaitForBackupInitialization_whenCommandReturnsInvalidString()
            throws Exception {
        class TestRunnable implements Runnable {
            @Override
            public void run() {
                try {
                    BackupUtils backupUtils = new BackupUtils() {
                        @Override
                        protected InputStream executeShellCommand(String command)
                                throws IOException {
                            String output = "";
                            if (command.equals("dumpsys backup")) {
                                output = "Backup Manager ???";
                                mIsDumpsysCommandCalled = true;
                            }
                            return new ByteArrayInputStream(output.getBytes("UTF-8"));
                        }
                    };
                    backupUtils.waitForBackupInitialization();
                } catch (IOException e) {
                    // ignore
                }
            }
        }

        TestRunnable testRunnable = new TestRunnable();
        Thread testThread = new Thread(testRunnable);

        try {
            testThread.start();
            RunUtil.getDefault().sleep(100);
            assertTrue(mIsDumpsysCommandCalled);
            assertTrue(testThread.isAlive());
        } catch (Exception e) {
            // ignore
        } finally {
            testThread.interrupt();
        }
    }

    @Test
    public void testWaitForBackupInitialization_whenCommandReturnsEmptyString_throwsException()
            throws Exception {
        BackupUtils backupUtils = new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                String output = "";
                if (command.equals("dumpsys backup")) {
                    // output is empty already
                    mIsDumpsysCommandCalled = true;
                }
                return new ByteArrayInputStream(output.getBytes("UTF-8"));
            }
        };

        boolean isExceptionHappened = false;
        try {
            backupUtils.waitForBackupInitialization();
        } catch (NullPointerException e) {
            // null output by running command, dumpsys backup
            isExceptionHappened = true;
        }
        assertTrue(isExceptionHappened);
        assertTrue(mIsDumpsysCommandCalled);
    }

    @Test
    public void testActivateBackup_whenEnableTrueAndEnabled_returnsTrue() throws Exception {
        BackupUtils backupUtils =
                new BackupUtils() {
                    @Override
                    protected InputStream executeShellCommand(String command) throws IOException {
                        String output = "";
                        if (command.equals(getBmgrCommand("activated", TEST_USER_ID))) {
                            output = "Backup Manager currently activated";
                        } else if (command.equals(getBmgrCommand("activate true", TEST_USER_ID))) {
                            output = "Backup Manager now activated";
                            mIsActivateCommandCalled = true;
                        }
                        return new ByteArrayInputStream(output.getBytes("UTF-8"));
                    }
                };
        assertTrue(backupUtils.activateBackupForUser(true, TEST_USER_ID));
        assertTrue(mIsActivateCommandCalled);
    }

    @Test
    public void testActivateBackup_whenEnableTrueAndDisabled_returnsFalse() throws Exception {
        BackupUtils backupUtils =
                new BackupUtils() {
                    @Override
                    protected InputStream executeShellCommand(String command) throws IOException {
                        String output = "";
                        if (command.equals(getBmgrCommand("activated", TEST_USER_ID))) {
                            output = "Backup Manager currently deactivated";
                        } else if (command.equals(getBmgrCommand("activate true", TEST_USER_ID))) {
                            output = "Backup Manager now activated";
                            mIsActivateCommandCalled = true;
                        }
                        return new ByteArrayInputStream(output.getBytes("UTF-8"));
                    }
                };
        assertFalse(backupUtils.activateBackupForUser(true, TEST_USER_ID));
        assertTrue(mIsActivateCommandCalled);
    }

    @Test
    public void testActivateBackup_whenEnableFalseAndEnabled_returnsTrue() throws Exception {
        BackupUtils backupUtils =
                new BackupUtils() {
                    @Override
                    protected InputStream executeShellCommand(String command) throws IOException {
                        String output = "";
                        if (command.equals(getBmgrCommand("activated", TEST_USER_ID))) {
                            output = "Backup Manager currently activated";
                        } else if (command.equals(getBmgrCommand("activate false", TEST_USER_ID))) {
                            output = "Backup Manager now deactivated";
                            mIsActivateCommandCalled = true;
                        }
                        return new ByteArrayInputStream(output.getBytes("UTF-8"));
                    }
                };
        assertTrue(backupUtils.activateBackupForUser(false, TEST_USER_ID));
        assertTrue(mIsActivateCommandCalled);
    }

    @Test
    public void testActivateBackup_whenEnableFalseAndDisabled_returnsFalse() throws Exception {
        BackupUtils backupUtils =
                new BackupUtils() {
                    @Override
                    protected InputStream executeShellCommand(String command) throws IOException {
                        String output = "";
                        if (command.equals(getBmgrCommand("activated", TEST_USER_ID))) {
                            output = "Backup Manager currently deactivated";
                        } else if (command.equals(getBmgrCommand("activate false", TEST_USER_ID))) {
                            output = "Backup Manager now deactivated";
                            mIsActivateCommandCalled = true;
                        }
                        return new ByteArrayInputStream(output.getBytes("UTF-8"));
                    }
                };
        assertFalse(backupUtils.activateBackupForUser(false, TEST_USER_ID));
        assertTrue(mIsActivateCommandCalled);
    }

    private String getBmgrCommand(String command, int userId) {
        return "bmgr --user " + userId + " " + command;
    }
}
