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
import android.os.Environment;
import android.server.wm.intent.Persistence.IntentFlag;
import android.server.wm.intent.Persistence.TestCase;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Ignore;
import org.junit.Test;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * The {@link IntentGenerationTests#generate()} method runs a very long generation process that
 * takes over 10 minutes.
 *
 * The generated tests and results observed on a reference device can then be used in CTS for
 * verification.
 *
 * It writes a bunch of files out to the directory obtained from
 * {@link Environment#getExternalStoragePublicDirectory(String)} which is currently
 * /storage/emulated/0/Documents/.
 *
 * These can then be pulled out via adb and put into the intent_tests asset directory.
 * These are the tests that are checked by atest CtsWindowManagerDeviceTestCases:IntentTests
 *
 * Because this process takes so much time, certain timeouts in the AndroidTest.xml need to be
 * raised.
 *
 * <option name="run-command" value="settings put system screen_off_timeout 1200000000" /> in the
 * target preparer and <option name="test-timeout" value="3600000" /> in the <test class=></test>
 *
 * Since this test only exists to trigger this process using the test infrastructure
 * it is ignored by default.
 *
 * Build/Install/Run:
 * atest CtsWindowManagerDeviceTestCases:IntentGenerationTests
 *
 *
 * <p>NOTE: It is also possible to use this to manually verify a single test (useful for local
 * debugging). To do that:
 * 1. Put the correct test name in {@link #verifySingle()} and remove the @Ignore annotation
 * 2. Push the file under test to device, e.g.
 *    adb shell mkdir /storage/emulated/0/Documents/relinquishTaskIdentity/
 *    adb push cts/tests/framework/base/windowmanager/intent_tests/relinquishTaskIdentity/test-0.json /storage/emulated/0/Documents/relinquishTaskIdentity/
 * 3. Run the test
 *    atest CtsWindowManagerDeviceTestCases:IntentGenerationTests#verifySingle
 * </p>
 */
public class IntentGenerationTests extends IntentTestBase {
    private static final Cases CASES = new Cases();

    /**
     * The flag parsing table used to parse the json files.
     */
    private static final Map<String, IntentFlag> TABLE = CASES.createFlagParsingTable();

    /**
     * Runner used to record testCases.
     */
    private LaunchRunner mLaunchRunner;

    /**
     * The target context we can launch the first activity from.
     */
    private Context mTargetContext = getInstrumentation().getTargetContext();

    //20 minute timeout.
    @Test(timeout = 1_200_000)
    @Ignore
    public void generate() throws Exception {
        mLaunchRunner.runAndWrite(mTargetContext, "forResult", CASES.forResultCases());
        mLaunchRunner.runAndWrite(mTargetContext, "newTask", CASES.newTaskCases());
        mLaunchRunner.runAndWrite(mTargetContext, "newDocumentCases", CASES.newDocumentCases());
        mLaunchRunner.runAndWrite(mTargetContext, "resetTaskIfNeeded", CASES.resetTaskIfNeeded());
        mLaunchRunner.runAndWrite(mTargetContext, "clearCases", CASES.clearCases());
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mLaunchRunner = new LaunchRunner(this);
    }

    /**
     * Debugging utility to verify a single test in the assets folder.
     *
     * @throws IOException   if writing to storage fails
     * @throws JSONException if the file has invalid json in it.
     */
    @Test
    // Comment the following line to test locally
    @Ignore
    public void verifySingle() throws IOException, JSONException {
        // Replace this line with the test you need
        String test = "relinquishTaskIdentity/test-0.json";
        TestCase testCase = readFromStorage(test);
        mLaunchRunner.verify(mTargetContext, testCase);
    }

    private TestCase readFromStorage(String fileName) throws IOException, JSONException {
        File documentsDirectory = Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DOCUMENTS);
        Path testsFilePath = documentsDirectory.toPath().resolve(fileName);

        JSONObject jsonInTestFile = new JSONObject(
                new String(Files.readAllBytes(testsFilePath), StandardCharsets.UTF_8));
        return TestCase.fromJson(jsonInTestFile, TABLE, fileName);
    }

    /**
     * This class runs multiple {@link TestCase}-s in a single test.
     * Therefore the list of componentNames that occur in the test is not known here.
     *
     * Instead {@link IntentTestBase#cleanUp(List)} is called from {@link
     * LaunchRunner#runAndWrite(Context, String, List)} instead.
     *
     * @return an emptyList
     */
    @Override
    List<ComponentName> activitiesUsedInTest() {
        return Collections.emptyList();
    }
}
