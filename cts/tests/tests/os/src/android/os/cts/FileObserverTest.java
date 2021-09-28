/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.os.cts;

import android.os.Environment;
import android.os.FileObserver;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;
import android.test.AndroidTestCase;
import android.util.Pair;

import androidx.test.InstrumentationRegistry;

import java.io.File;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class FileObserverTest extends AndroidTestCase {
    private static final String PATH = "/PATH";
    private static final String TEST_FILE = "file_observer_test.txt";
    private static final String TEST_DIR = "fileobserver_dir";
    private static final int FILE_DATA = 0x20;
    private static final int UNDEFINED = 0x8000;
    private static final long DELAY_MSECOND = 2000;

    private void helpSetUp(File dir) throws Exception {
        File testFile = new File(dir, TEST_FILE);
        testFile.createNewFile();
        File testDir = new File(dir, TEST_DIR);
        testDir.mkdirs();
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        File dir = getContext().getFilesDir();
        helpSetUp(dir);

        dir = getContext().getCacheDir();
        helpSetUp(dir);

        // Instant apps cannot access external storage
        if (!InstrumentationRegistry.getTargetContext().getPackageManager().isInstantApp()) {
            dir = getContext().getExternalFilesDir(null);
            helpSetUp(dir);

            dir = Environment.getExternalStorageDirectory();
            helpSetUp(dir);
        }
    }

    private void helpTearDown(File dir) throws Exception {
        File testFile = new File(dir, TEST_FILE);
        File testDir = new File(dir, TEST_DIR);
        File moveDestFile = new File(testDir, TEST_FILE);

        if (testFile.exists()) {
            testFile.delete();
        }

        if (moveDestFile.exists()) {
            moveDestFile.delete();
        }

        if (testDir.exists()) {
            testDir.delete();
        }
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();

        File dir = getContext().getFilesDir();
        helpTearDown(dir);

        dir = getContext().getCacheDir();
        helpTearDown(dir);

        dir = getContext().getExternalFilesDir(null);
        helpTearDown(dir);

        dir = Environment.getExternalStorageDirectory();
        helpTearDown(dir);
    }

    public void testConstructor() {
        // new the instance
        new MockFileObserver(new File(PATH));
        // new the instance
        new MockFileObserver(new File(PATH), FileObserver.ACCESS);
    }

    /*
     * Test point
     * 1. Observe a dir, when it's child file have been written and closed,
     * observer should get modify open-child modify-child and closed-write events.
     * 2. While stop observer a dir, observer should't get any event while delete it's child file.
     * 3. Observer a dir, when create delete a child file and delete self,
     * observer should get create-child close-nowrite delete-child delete-self events.
     * 4. Observer a file, the file moved from dir and the file moved to dir, move the file,
     * file observer should get move-self event,
     * moved from dir observer should get moved-from event,
     * moved to dir observer should get moved-to event.
     *
     * On emulated storage, there may be additional operations related to case insensitivity, so
     * we just check that the expected ones are present.
     */
    public void helpTestFileObserver(File dir, boolean isEmulated) throws Exception {
        MockFileObserver fileObserver = null;
        int[] expected = null;
        FileEvent[] moveEvents = null;
        File testFile = new File(dir, TEST_FILE);
        File testDir = new File(dir, TEST_DIR);
        File moveDestFile;
        FileOutputStream out = null;

        fileObserver = new MockFileObserver(testFile.getParentFile());
        try {
            fileObserver.startWatching();

            verifyTriggeredEventsOnFile(fileObserver, testFile, isEmulated);

            fileObserver.stopWatching();

            // action after observer stop watching
            testFile.delete(); // delete

            // should not get any event
            expected = new int[] {UNDEFINED};
            moveEvents = waitForEvent(fileObserver);
            if (isEmulated)
                assertEventsContains(testFile, expected, moveEvents);
            else
                assertEventsEquals(testFile, expected, moveEvents);
        } finally {
            fileObserver.stopWatching();
            if (out != null)
                out.close();
            out = null;
        }
        fileObserver = new MockFileObserver(testDir);
        try {
            fileObserver.startWatching();
            verifyTriggeredEventsOnDir(fileObserver, testDir, isEmulated);
        } finally {
            fileObserver.stopWatching();
        }
        dir = getContext().getFilesDir();
        testFile = new File(dir, TEST_FILE);
        testFile.createNewFile();
        testDir = new File(dir, TEST_DIR);
        testDir.mkdirs();
        moveDestFile = new File(testDir, TEST_FILE);
        final MockFileObserver movedFileObserver = new MockFileObserver(Arrays.asList(
                dir,
                testDir,
                testFile
        ));
        try {
            movedFileObserver.startWatching();

            testFile.renameTo(moveDestFile);

            expected = new int[] {
                    FileObserver.MOVED_FROM,
                    FileObserver.MOVED_TO,
                    FileObserver.MOVE_SELF,
            };
            moveEvents = waitForEvent(movedFileObserver);
            if (isEmulated) {
                assertEventsContains(testFile, expected, moveEvents);
            } else {
                assertEventsEquals(testFile, expected, moveEvents);
            }
        } finally {
            movedFileObserver.stopWatching();
        }

        // Because Javadoc didn't specify when should a event happened,
        // here ACCESS ATTRIB we found no way to test.
    }

    private void verifyTriggeredEventsOnFile(MockFileObserver fileObserver,
            File testFile, boolean isEmulated) throws Exception {
        final FileOutputStream out = new FileOutputStream(testFile);

        out.write(FILE_DATA); // modify, open, write, modify
        out.close(); // close_write

        final int[] expected = {
                FileObserver.MODIFY,
                FileObserver.OPEN,
                FileObserver.MODIFY,
                FileObserver.CLOSE_WRITE
        };

        final FileEvent[] moveEvents = waitForEvent(fileObserver);
        if (isEmulated) {
            assertEventsContains(testFile, expected, moveEvents);
        } else {
            assertEventsEquals(testFile, expected, moveEvents);
        }
    }

    private void verifyTriggeredEventsOnDir(MockFileObserver fileObserver,
            File testDir, boolean isEmulated) throws Exception {
        final File testFile = new File(testDir, TEST_FILE);
        assertTrue(testFile.createNewFile());
        assertTrue(testFile.exists());
        testFile.delete();
        testDir.delete();

        final int[] expected = {
                FileObserver.CREATE,
                FileObserver.OPEN,
                FileObserver.CLOSE_WRITE,
                FileObserver.DELETE,
                FileObserver.DELETE_SELF,
                UNDEFINED
        };

        final FileEvent[] moveEvents = waitForEvent(fileObserver);
        if (isEmulated) {
            assertEventsContains(testFile, expected, moveEvents);
        } else {
            assertEventsEquals(testFile, expected, moveEvents);
        }
    }

    public void testFileObserver() throws Exception {
        helpTestFileObserver(getContext().getFilesDir(), false);
    }

    @AppModeFull(reason = "Instant apps cannot access external storage")
    public void testFileObserverExternal() throws Exception {
        helpTestFileObserver(getContext().getExternalFilesDir(null), true);
    }

    @AppModeFull(reason = "Instant apps cannot access external storage")
    public void testFileObserverExternalStorageDirectory() throws Exception {
        helpTestFileObserver(Environment.getExternalStorageDirectory(), true);
    }

    @AppModeFull(reason = "Instant apps cannot access external storage")
    public void testFileObserver_multipleFilesFull() throws Exception {
        verifyMultipleFiles(
                Pair.create(getContext().getCacheDir(), false),
                Pair.create(getContext().getFilesDir(), false),
                Pair.create(getContext().getExternalFilesDir(null), true),
                Pair.create(Environment.getExternalStorageDirectory(), true)
        );
    }

    @AppModeInstant(reason = "Instant specific variant excluding disallowed external storage")
    public void testFileObserver_multipleFilesInstant() throws Exception {
        verifyMultipleFiles(
                Pair.create(getContext().getCacheDir(), false),
                Pair.create(getContext().getFilesDir(), false)
        );
    }

    @SafeVarargs
    private final void verifyMultipleFiles(Pair<File, Boolean>... dirsAndIsEmulated) throws Exception {
        List<File> directories = new ArrayList<>(dirsAndIsEmulated.length);
        for (Pair<File, Boolean> pair : dirsAndIsEmulated) {
            directories.add(pair.first);
        }

        final MockFileObserver fileObserver1 = new MockFileObserver(directories);
        try {
            fileObserver1.startWatching();
            for (Pair<File, Boolean> pair : dirsAndIsEmulated) {
                verifyTriggeredEventsOnFile(fileObserver1,
                        new File(pair.first, TEST_FILE), pair.second);
            }
        } finally {
            fileObserver1.stopWatching();
        }

        directories = new ArrayList<>(dirsAndIsEmulated.length);
        for (Pair<File, Boolean> pair : dirsAndIsEmulated) {
            directories.add(new File(pair.first, TEST_DIR));
        }

        final MockFileObserver fileObserver2 = new MockFileObserver(directories);
        try {
            fileObserver2.startWatching();
            for (Pair<File, Boolean> pair : dirsAndIsEmulated) {
                verifyTriggeredEventsOnDir(fileObserver2,
                        new File(pair.first, TEST_DIR), pair.second);
            }
        } finally {
            fileObserver2.stopWatching();
        }
    }

    private void assertEventsEquals(
            File testFile, final int[] expected, final FileEvent[] moveEvents) {
        List<Integer> expectedEvents = new ArrayList<Integer>();
        for (int i = 0; i < expected.length; i++) {
            expectedEvents.add(expected[i]);
        }
        List<FileEvent> actualEvents = Arrays.asList(moveEvents);
        String message = "For test file [" + testFile.getAbsolutePath()
                + "] expected: " + expectedEvents + " Actual: " + actualEvents;
        assertEquals(message, expected.length, moveEvents.length);
        for (int i = 0; i < expected.length; i++) {
            assertEquals(message, expected[i], moveEvents[i].event);
        }
    }

    private void assertEventsContains(
            File testFile, final int[] expected, final FileEvent[] moveEvents) {
        List<Integer> expectedEvents = new ArrayList<Integer>();
        for (int i = 0; i < expected.length; i++) {
            expectedEvents.add(expected[i]);
        }
        List<FileEvent> actualEvents = Arrays.asList(moveEvents);
        String message = "For test file [" + testFile.getAbsolutePath()
                + "] expected: " + expectedEvents + " Actual: " + actualEvents;
        int j = 0;
        for (int i = 0; i < expected.length; i++) {
            while (expected[i] != moveEvents[j].event) {
                j++;
                if (j >= moveEvents.length)
                    fail(message);
            }
            j++;
        }
    }

    private FileEvent[] waitForEvent(MockFileObserver fileObserver)
            throws InterruptedException {
        Thread.sleep(DELAY_MSECOND);
        synchronized (fileObserver) {
            return fileObserver.getEvents();
       }
    }

    private static class FileEvent {
        public int event = UNDEFINED;
        public String path;

        public FileEvent(final int event, final String path) {
            this.event = event;
            this.path = path;
        }

        @Override
        public String toString() {
            return Integer.toString(event);
        }
    }

    /*
     * MockFileObserver
     */
    private static class MockFileObserver extends FileObserver {

        private List<FileEvent> mEvents = new ArrayList<FileEvent>();

        public MockFileObserver(File file) {
            super(file);
        }

        public MockFileObserver(File file, int mask) {
            super(file, mask);
        }

        public MockFileObserver(List<File> files) {
            super(files);
        }

        public MockFileObserver(List<File> files, int mask) {
            super(files, mask);
        }

        @Override
        public synchronized void onEvent(int event, String path) {
            mEvents.add(new FileEvent(event, path));
        }

        public synchronized FileEvent[] getEvents() {
            final FileEvent[] events = new FileEvent[mEvents.size()];
            mEvents.toArray(events);
            mEvents.clear();
            return events;
        }
    }
}
