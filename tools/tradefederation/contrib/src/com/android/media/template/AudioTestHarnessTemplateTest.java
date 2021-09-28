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

package com.android.media.template;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.Option.Importance;
import com.android.ddmlib.testrunner.IRemoteAndroidTestRunner;
import com.android.ddmlib.testrunner.RemoteAndroidTestRunner;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IRemoteTest;

import org.junit.Assert;

import java.util.concurrent.TimeUnit;

/**
 * Template test for Audio Test Harness.
 *
 * This template test involves iteraction with devices connected host and interfaces to invoke
 * Android APIs for testing.
 */
public class AudioTestHarnessTemplateTest implements IDeviceTest, IRemoteTest {
  // These params can be passed in from config files and will pass to
  // AudioTestHarnessTemplateAndroidTest class.
    @Option(name = "audio-file", description = "Audio file to be played by device",
            importance = Importance.ALWAYS)
    String mAudioFile = "/system/product/media/audio/ringtones/Lollipop.ogg";

    @Option(name = "audio-play-duration", description = "Milliseconds for the audio to be played", importance = Importance.ALWAYS)
    String mAudioPlayDuration = "5000";

    ITestDevice mTestDevice = null;

    //Max test timeout - 2 hrs
    private static final int MAX_TEST_TIMEOUT = 2 * 60 * 60 * 1000;

    // Constants for running the tests
    private static final String TEST_CLASS_NAME =
            "com.android.mediaframeworktest.template.AudioTestHarnessTemplateAndroidTest";
    private static final String TEST_PACKAGE_NAME = "com.android.mediaframeworktest";
    private static final String TEST_RUNNER_NAME = ".AudioTestHarnessTemplateRunner";
    private static final String AUDIO_FILE_KEY  = "audioFile";
    private static final String AUDIO_PLAY_DURATION_KEY  = "audioPlayDuration";


    @Override
    public void run(ITestInvocationListener listener) throws DeviceNotAvailableException {
        Assert.assertNotNull(mTestDevice);
        CLog.i("Starting test with Audio Test Harness Template.");
        IRemoteAndroidTestRunner runner = new RemoteAndroidTestRunner(TEST_PACKAGE_NAME,
                TEST_RUNNER_NAME, mTestDevice.getIDevice());
        runner.addInstrumentationArg(AUDIO_FILE_KEY, mAudioFile);
        runner.addInstrumentationArg(AUDIO_PLAY_DURATION_KEY, mAudioPlayDuration);
        CLog.i("Playing audio file %s for %s milliseconds", mAudioFile, mAudioPlayDuration);
        runner.setClassName(TEST_CLASS_NAME);
        runner.setMaxTimeToOutputResponse(MAX_TEST_TIMEOUT, TimeUnit.MILLISECONDS);

        mTestDevice.runInstrumentationTests(runner, listener);
    }


    @Override
    public void setDevice(ITestDevice device) {
        mTestDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mTestDevice;
    }
}
