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

package com.android.tradefed.result;

import static com.google.common.truth.Truth.assertThat;

import com.android.tradefed.util.FileUtil;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;
import java.util.List;

@RunWith(JUnit4.class)
public class TestDescriptionsFileTest {

    @Test
    public void getFile_empty() throws IOException {
        TestDescriptionsFile tdf = new TestDescriptionsFile();
        File file = tdf.getFile();
        assertThat(file).isNotNull();
        assertThat(file.exists()).isTrue();
        assertThat(FileUtil.readStringFromFile(file)).isEmpty();
    }

    @Test
    public void getFile_oneEntry() throws IOException {
        TestDescriptionsFile tdf = new TestDescriptionsFile();
        TestDescription td = new TestDescription("classname", "testname");
        tdf.add(td);

        File file = tdf.getFile();
        assertThat(file).isNotNull();
        assertThat(file.exists()).isTrue();
        assertThat(FileUtil.readStringFromFile(file)).contains(td.toString());
    }

    @Test
    public void getFile_caching() throws IOException {
        TestDescriptionsFile tdf = new TestDescriptionsFile();
        TestDescription td = new TestDescription("classname", "testname");
        tdf.add(td);

        File file = tdf.getFile();
        File cachedFile = tdf.getFile();

        assertThat(cachedFile).isEqualTo(file);

        TestDescription td2 = new TestDescription("classname", "test2");
        tdf.add(td2);
        File changedFile = tdf.getFile();
        assertThat(changedFile).isNotEqualTo(file);

        TestDescriptionsFile tdf1 = new TestDescriptionsFile(changedFile);
        TestDescriptionsFile fromFile = tdf1;
        assertThat(fromFile.getTests()).containsExactly(td, td2);
    }

    @Test
    public void getTests_empty() throws IOException {
        TestDescriptionsFile tdf = new TestDescriptionsFile();

        assertThat(tdf.getTests()).isEmpty();
    }

    @Test
    public void fromFile_empty() throws IOException {
        TestDescriptionsFile tdf = new TestDescriptionsFile();
        File file = tdf.getFile();

        TestDescriptionsFile tdf1 = new TestDescriptionsFile(file);
        TestDescriptionsFile tdfFromFile = tdf1;
        List<TestDescription> tests = tdfFromFile.getTests();

        assertThat(tests).isEmpty();
    }

    @Test
    public void fromFile() throws IOException {
        TestDescriptionsFile tdf = new TestDescriptionsFile();
        TestDescription td = new TestDescription("classname", "testname");
        tdf.add(td);

        File file = tdf.getFile();

        TestDescriptionsFile tdf1 = new TestDescriptionsFile(file);
        TestDescriptionsFile tdfFromFile = tdf1;
        List<TestDescription> tests = tdfFromFile.getTests();

        assertThat(tests).containsExactly(td);
    }
}
