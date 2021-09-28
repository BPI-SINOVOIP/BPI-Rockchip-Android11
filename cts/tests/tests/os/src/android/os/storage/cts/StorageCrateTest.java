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

package android.os.storage.cts;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.IOException;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;

@RunWith(AndroidJUnit4.class)
public class StorageCrateTest {
    private static final String CRATES_ROOT = "crates";
    @Rule
    public TestName mTestName = new TestName();

    private Context mContext;
    private Path mCratesRoot;
    private String mCrateId;
    private Path mCratePath;

    private void cleanAllOfCrates() throws IOException {
        if (!mCratesRoot.toFile().exists()) {
            return;
        }

        Files.walkFileTree(mCratesRoot, new SimpleFileVisitor<>() {
            @Override
            public FileVisitResult visitFile(Path file, BasicFileAttributes attrs)
                    throws IOException {
                Files.deleteIfExists(file);
                return super.visitFile(file, attrs);
            }

            @Override
            public FileVisitResult postVisitDirectory(Path dir, IOException exc)
                    throws IOException {
                Files.deleteIfExists(dir);
                return super.postVisitDirectory(dir, exc);
            }
        });

        Files.deleteIfExists(mCratesRoot);
    }

    /**
     * Setup the necessary member field used by test methods.
     * <p>It needs to remove all of directories in {@link Context#getCrateDir(String crateId)} that
     * include {@link Context#getCrateDir(String crateId)} itself.
     * Why it needs to run cleanAllOfCrates, the process may crashed before tearDown. Running
     * cleanAllOfCrates to make sure the test environment is clean.</p>
     */
    @Before
    public void setUp() throws IOException {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mCratesRoot = mContext.getDataDir().toPath().resolve(CRATES_ROOT);
        mCrateId = mTestName.getMethodName();
        mCratePath = mCratesRoot.resolve(mTestName.getMethodName());

        cleanAllOfCrates();
    }

    @Test
    public void getCrateDir_notInvoke_cratesRootShouldNotExist() {
        final File cratesRootDir = mCratesRoot.toFile();

        assertThat(cratesRootDir.exists()).isFalse();
    }

    @Test
    public void getCrateDir_withCrateId_cratesRootExist() {
        final File cratesRootDir = mCratesRoot.toFile();

        mContext.getCrateDir(mCrateId);

        assertThat(cratesRootDir.exists()).isTrue();
    }

    @Test
    public void getCrateDir_withCrateId_shouldReturnNonNullDir() {
        final File crateDir = mContext.getCrateDir(mCrateId);

        assertThat(crateDir).isNotNull();
    }

    @Test
    public void getCrateDir_withCrateId_cratePathBeDirectory() {
        final File crateDir = mContext.getCrateDir(mCrateId);

        assertThat(crateDir.isDirectory()).isTrue();
    }

    @Test
    public void getCrateDir_withCrateId_cratePathShouldExist() {
        final File crateDir = mContext.getCrateDir(mCrateId);

        assertThat(crateDir.exists()).isTrue();
    }

    @Test
    public void getCrateDir_withCrateId_cratesRootShouldUnderDataDir() {
        final File cratesRootDir = mCratesRoot.toFile();

        mContext.getCrateDir(mCrateId);

        assertThat(cratesRootDir.getParentFile().getName())
                .isEqualTo(mContext.getDataDir().getName());
    }

    @Test
    public void getCrateDir_withCrateId_crateDirShouldUnderCratesRootDir() {
        final File cratesRootDir = mCratesRoot.toFile();
        final File crateDir = mCratePath.toFile();

        mContext.getCrateDir(mCrateId);

        assertThat(crateDir.getParentFile().getName()).isEqualTo(cratesRootDir.getName());
    }

    @Test
    public void getCrateDir_anyExceptionHappened_shouldNotCreateAnyDir() {
        File crateDir = null;

        try {
            crateDir = mContext.getCrateDir(null);
        } catch (Exception e) {
        }

        assertThat(crateDir).isNull();
    }

    @Test
    public void getCrateDir_nullCrateId_crateDirShouldUnderCratesRootDir() {
        IllegalArgumentException illegalArgumentException = null;

        try {
            mContext.getCrateDir(null);
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_emptyCrateId_crateDirShouldUnderCratesRootDir() {
        IllegalArgumentException illegalArgumentException = null;

        try {
            mContext.getCrateDir("");
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_crateIdIsDot_crateDirShouldUnderCratesRootDir() {
        IllegalArgumentException illegalArgumentException = null;

        try {
            mContext.getCrateDir(".");
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_crateIdIsDotDot_crateDirShouldUnderCratesRootDir() {
        IllegalArgumentException illegalArgumentException = null;

        try {
            mContext.getCrateDir("..");
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_crateIdPrefixContainsDotDot_shouldTriggerIllegalArgumentException() {
        IllegalArgumentException illegalArgumentException = null;

        try {
            mContext.getCrateDir("../etc/password");
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_crateIdContainsDotDot_shouldTriggerIllegalArgumentException() {
        IllegalArgumentException illegalArgumentException = null;

        try {
            mContext.getCrateDir("normalCrate/../../../etc/password");
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_crateIdSuffixContainsDotDot_shouldTriggerIllegalArgumentException() {
        IllegalArgumentException illegalArgumentException = null;

        try {
            mContext.getCrateDir("normalCrate/etc/password/../../..");
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_crateIdStartWithSlashSlash_shouldTriggerIllegalArgumentException() {
        IllegalArgumentException illegalArgumentException = null;

        try {
            mContext.getCrateDir("/etc/password");
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_crateIdStartWithSlash_shouldTriggerIllegalArgumentException() {
        IllegalArgumentException illegalArgumentException = null;

        try {
            mContext.getCrateDir("//etc/password");
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_crateIdContainsSlashChar_shouldBeInvalidated() {
        IllegalArgumentException illegalArgumentException = null;

        try {
            mContext.getCrateDir("A/B");
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_superLongCrateId_shouldBeIllegalArgument() throws IOException {
        IllegalArgumentException illegalArgumentException = null;
        StringBuilder sb = new StringBuilder(1024);
        while (sb.length() > 1024) {
            sb.append(mCrateId);
        }

        try {
            mContext.getCrateDir(sb.toString());
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_callWithDifferentCrateId_shouldGenerateTheSameNumberOfCrates() {
        final String[] expectedCrates = new String[] {"A", "B", "C"};

        for (String crateId : expectedCrates) {
            mContext.getCrateDir(crateId);
        }

        String[] newChildDir = mCratesRoot.toFile().list();
        assertThat(newChildDir).asList().containsAllIn(expectedCrates);
    }

    @Test
    public void getCrateDir_withUtf8Characters_shouldCreateSuccess() {
        String utf8Characters = "æɛəɚʊʌɔ" + "宮商角止羽" + "あいうえお"
                + "ㅏㅓㅗㅜㅡㅣㅐㅔㅚㅟㅑㅕㅛㅠㅒㅖㅘㅙㅝㅞㅢ";

        File crateDir = mContext.getCrateDir(utf8Characters);

        assertThat(crateDir.getName()).isEqualTo(utf8Characters);
    }

    @Test
    public void getCrateDir_withNullCharacter_shouldBeFail() {
        String utf8Characters = "abcdefg\0hijklmnopqrstuvwxyz";

        IllegalArgumentException illegalArgumentException = null;
        try {
            mContext.getCrateDir(utf8Characters);
        } catch (IllegalArgumentException e) {
            illegalArgumentException = e;
        }

        assertThat(illegalArgumentException).isNotNull();
    }

    @Test
    public void getCrateDir_withLineFeedCharacter_shouldSuccess() {
        String utf8Characters = "abcdefg\nhijklmnopqrstuvwxyz";

        File crateDir = mContext.getCrateDir(utf8Characters);

        assertThat(crateDir.exists() && crateDir.isDirectory()).isTrue();
    }
}
