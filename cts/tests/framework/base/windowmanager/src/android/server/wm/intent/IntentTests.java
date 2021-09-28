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

package android.server.wm.intent;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import android.content.ComponentName;
import android.content.Context;
import android.content.res.AssetManager;
import android.os.Environment;
import android.platform.test.annotations.Presubmit;
import android.server.wm.intent.Persistence.IntentFlag;
import android.server.wm.intent.Persistence.TestCase;

import androidx.test.filters.FlakyTest;
import androidx.test.filters.LargeTest;

import com.google.common.collect.Lists;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

/**
 * This test will verify every intent_test json file in a separately run test, through JUnits
 * Parameterized runner.
 *
 * Running a single test using this class is not supported.
 * For this use case look at {@link IntentGenerationTests#verifySingle()}
 *
 * Note: atest CtsWindowManagerDeviceTestCases:IntentTests#verify does not work because the
 * Parameterized runner won't expose the test that way to atest.
 *
 * Build/Install/Run:
 * atest CtsWindowManagerDeviceTestCases:IntentTests
 */
@FlakyTest(detail = "Promote once confirmed non-flaky")
@Presubmit
@LargeTest
@RunWith(Parameterized.class)
public class IntentTests extends IntentTestBase {
    private static final Cases CASES = new Cases();

    /**
     * The flag parsing table used to parse the json files.
     */
    private static final Map<String, IntentFlag> TABLE = CASES.createFlagParsingTable();
    private static Context TARGET_CONTEXT = getInstrumentation().getTargetContext();

    private static final int JSON_INDENTATION_LEVEL = 4;

    /**
     * The runner used to verify the recorded test cases.
     */
    private LaunchRunner mLaunchRunner;

    /**
     * The Test case we are currently verifying.
     */
    private TestCase mTestCase;

    public IntentTests(TestCase testCase, String name) {
        mTestCase = testCase;
    }

    /**
     * Read all the tests in the assets of the application and create a separate test to run for
     * each of them using the {@link org.junit.runners.Parameterized}
     */
    @Parameterized.Parameters(name = "{1}")
    public static Collection<Object[]> data() throws IOException, JSONException {
        return readAllFromAssets().stream().map(
                test -> new Object[]{test, test.getName()}).collect(
                Collectors.toList());
    }

    @Test
    public void verify() {
        mLaunchRunner.verify(TARGET_CONTEXT, mTestCase);
    }


    @Override
    public void setUp() throws Exception {
        super.setUp();
        mLaunchRunner = new LaunchRunner(this);
    }

    @Override
    List<ComponentName> activitiesUsedInTest() {
        return mTestCase.getSetup().componentsInCase();
    }

    public static List<TestCase> readAllFromAssets() throws IOException, JSONException {
        List<TestCase> testCases = Lists.newArrayList();
        AssetManager assets = TARGET_CONTEXT.getAssets();

        List<String> testFiles = Lists.newArrayList();
        for (String dir : assets.list("")) {
            for (String file : assets.list(dir)) {
                if (file.endsWith(".json")) {
                    testFiles.add(dir + "/" + file);
                }
            }
        }

        for (String testFile : testFiles) {
            try (BufferedReader bufferedReader = new BufferedReader(new InputStreamReader(
                    new BufferedInputStream(assets.open(testFile))))) {
                String jsonData = bufferedReader.lines().collect(
                        Collectors.joining("\n"));

                TestCase testCase = TestCase.fromJson(
                        new JSONObject(jsonData), TABLE, testFile);

                testCases.add(testCase);
            }
        }

        return testCases;
    }

    static void writeToDocumentsStorage(TestCase testCase, int number, String dirName)
            throws JSONException, IOException {
        File documentsDirectory = Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DOCUMENTS);
        Path testsFilePath = documentsDirectory.toPath().resolve(dirName);

        if (!Files.exists(testsFilePath)) {
            Files.createDirectories(testsFilePath);
        }
        Files.write(testsFilePath.resolve("test-" + number + ".json"),
                Lists.newArrayList(testCase.toJson().toString(JSON_INDENTATION_LEVEL)));
    }
}

