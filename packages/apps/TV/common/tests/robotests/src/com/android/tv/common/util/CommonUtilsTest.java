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
package com.android.tv.common.util;

import static com.google.common.truth.Truth.assertThat;

import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.io.File;
import java.io.IOException;

/** Tests for {@link CommonUtils}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class CommonUtilsTest {

    @Test
    public void deleteDirOrFile_file() throws IOException {
        File file = new File(RuntimeEnvironment.application.getExternalFilesDir(null), "file");
        assertThat(file.createNewFile()).isTrue();
        assertThat(CommonUtils.deleteDirOrFile(file)).isTrue();
        assertThat(file.exists()).isFalse();
    }

    @Test
    public void deleteDirOrFile_Dir() throws IOException {
        File dir = new File(RuntimeEnvironment.application.getExternalFilesDir(null), "dir");
        assertThat(dir.mkdirs()).isTrue();
        assertThat(new File(dir, "file").createNewFile()).isTrue();
        assertThat(CommonUtils.deleteDirOrFile(dir)).isTrue();
        assertThat(dir.exists()).isFalse();
    }

    @Test
    public void deleteDirOrFile_unreadableDir() throws IOException {
        File dir = new File(RuntimeEnvironment.application.getExternalFilesDir(null), "dir");
        assertThat(dir.mkdirs()).isTrue();
        assertThat(new File(dir, "file").createNewFile()).isTrue();
        dir.setReadable(false);
        // Since dir is unreadable dir.listFiles() returns null and file is not deleted thus
        // dir is not actually deleted.
        assertThat(CommonUtils.deleteDirOrFile(dir)).isFalse();
        assertThat(dir.exists()).isTrue();
    }

    @Test
    public void deleteDirOrFile_unreadableSubDir() throws IOException {
        File dir = new File(RuntimeEnvironment.application.getExternalFilesDir(null), "dir");
        File subDir = new File(dir, "sub");
        assertThat(subDir.mkdirs()).isTrue();
        File file = new File(subDir, "file");
        assertThat(file.createNewFile()).isTrue();
        subDir.setReadable(false);
        // Since subDir is unreadable subDir.listFiles() returns null and file is not deleted thus
        // dir is not actually deleted.
        assertThat(CommonUtils.deleteDirOrFile(dir)).isFalse();
        assertThat(dir.exists()).isTrue();
    }
}
