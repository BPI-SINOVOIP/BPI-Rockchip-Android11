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

package android.cts.backup.profilefullbackupapp;

import static androidx.test.InstrumentationRegistry.getTargetContext;

import static com.google.common.truth.Truth.assertThat;

import android.platform.test.annotations.AppModeFull;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;

/** Device side test invoked by ProfileFullBackupRestoreHostSideTest. */
@RunWith(AndroidJUnit4.class)
@AppModeFull
public class ProfileFullBackupRestoreTest {
    private static final String FILE_ONE_CONTENT = "file1text";
    private static final String FILE_TWO_CONTENT = "file2text";

    private File mFile1;
    private File mFile2;

    @Before
    public void setUp() throws Exception {
        File dir = getTargetContext().getFilesDir();
        dir.mkdirs();

        mFile1 = new File(dir, "file1");
        mFile2 = new File(dir, "file2");
    }

    @Test
    public void assertFilesDontExist() throws Exception {
        assertThat(mFile1.exists()).isFalse();
        assertThat(mFile2.exists()).isFalse();
    }

    @Test
    public void writeFilesAndAssertSuccess() throws Exception {
        Files.write(mFile1.toPath(), FILE_ONE_CONTENT.getBytes());
        assertFileContains(mFile1, FILE_ONE_CONTENT);

        Files.write(mFile2.toPath(), FILE_TWO_CONTENT.getBytes());
        assertFileContains(mFile2, FILE_TWO_CONTENT);
    }

    @Test
    public void clearFilesAndAssertSuccess() throws Exception {
        mFile1.delete();
        mFile2.delete();
        assertFilesDontExist();
    }

    @Test
    public void assertFilesRestored() throws Exception {
        assertThat(mFile1.exists()).isTrue();
        assertFileContains(mFile1, FILE_ONE_CONTENT);

        assertThat(mFile2.exists()).isTrue();
        assertFileContains(mFile2, FILE_TWO_CONTENT);
    }

    private void assertFileContains(File file, String content) throws IOException {
        assertThat(Files.readAllBytes(file.toPath())).isEqualTo(content.getBytes());
    }
}
