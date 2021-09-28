/*
 * Copyright 2020 The Android Open Source Project
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

package android.hardware.camera2.cts;

import static android.hardware.camera2.cts.CameraTestUtils.*;
import static android.hardware.camera2.cts.RobustnessTest.MaxStreamSizes.*;

import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.CaptureFailure;
import android.hardware.camera2.cts.helpers.StaticMetadata;
import android.hardware.camera2.cts.testcases.Camera2ConcurrentAndroidTestCase;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.MandatoryStreamCombination;
import android.hardware.camera2.params.MandatoryStreamCombination.MandatoryStreamInformation;
import android.hardware.camera2.params.SessionConfiguration;
import android.media.ImageReader;
import android.util.Log;
import android.view.Surface;

import com.android.ex.camera2.blocking.BlockingSessionCallback;

import java.util.Arrays;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;

import org.junit.runners.Parameterized;
import org.junit.runner.RunWith;
import org.junit.Test;

import static junit.framework.Assert.assertTrue;
import static org.mockito.Mockito.*;

/**
 * Tests exercising concurrent camera streaming mandatory stream combinations.
 */

@RunWith(Parameterized.class)
public class ConcurrentCameraTest extends Camera2ConcurrentAndroidTestCase {
    private static final String TAG = "ConcurrentCameraTest";
    private static final boolean VERBOSE = Log.isLoggable(TAG, Log.VERBOSE);

    /**
     * Class representing a per camera id test sample.
     */
    private static class TestSample {
        public String cameraId;
        public StaticMetadata staticInfo;
        public MandatoryStreamCombination combination;
        public boolean haveSession = false;
        public boolean substituteY8;
        public List<OutputConfiguration> outputConfigs = new ArrayList<OutputConfiguration>();
        public List<SurfaceTexture> privTargets = new ArrayList<SurfaceTexture>();
        public List<ImageReader> jpegTargets = new ArrayList<ImageReader>();
        public List<ImageReader> yuvTargets = new ArrayList<ImageReader>();
        public List<ImageReader> y8Targets = new ArrayList<ImageReader>();
        public List<ImageReader> rawTargets = new ArrayList<ImageReader>();
        public List<ImageReader> heicTargets = new ArrayList<ImageReader>();
        public List<ImageReader> depth16Targets = new ArrayList<ImageReader>();
        public TestSample(String cameraId, StaticMetadata staticInfo,
                MandatoryStreamCombination combination, boolean subY8) {
            this.cameraId = cameraId;
            this.staticInfo = staticInfo;
            this.combination = combination;
            this.substituteY8 = subY8;
        }
    }

    /**
     * Class representing the information needed to generate a unique combination per camera id
     */
    private static class GeneratedEntry {
        public String cameraId;
        public boolean substituteY8;
        MandatoryStreamCombination combination;
        //Note: We don't have physical camera ids here since physical camera ids do not have
        //      guaranteed concurrent stream combinations.
        GeneratedEntry(String cameraId, boolean substituteY8,
                MandatoryStreamCombination combination) {
            this.cameraId = cameraId;
            this.substituteY8 = substituteY8;
            this.combination = combination;
        }
    };

    @Test
    public void testMandatoryConcurrentStreamCombination() throws Exception {
        for (Set<String> cameraIdCombinations : mConcurrentCameraIdCombinations) {
            if (cameraIdCombinations.size() == 0) {
                continue;
            }
            List<HashMap<String, GeneratedEntry>> streamCombinationPermutations =
                    generateStreamSelections(cameraIdCombinations);

            for (HashMap<String, GeneratedEntry> streamCombinationPermutation :
                    streamCombinationPermutations) {
                ArrayList<TestSample> testSamples = new ArrayList<TestSample>();
                for (Map.Entry<String, GeneratedEntry> deviceSample :
                        streamCombinationPermutation.entrySet()) {
                    CameraTestInfo info = mCameraTestInfos.get(deviceSample.getKey());
                    assertTrue("CameraTestInfo not found for camera id " + deviceSample.getKey(),
                            info != null);
                    MandatoryStreamCombination chosenCombination =
                            deviceSample.getValue().combination;
                    boolean substituteY8 = deviceSample.getValue().substituteY8;
                    TestSample testSample = new TestSample(deviceSample.getKey(), info.mStaticInfo,
                            chosenCombination, substituteY8);
                    testSamples.add(testSample);
                    openDevice(deviceSample.getKey());
                }
                try {
                    testMandatoryConcurrentStreamCombination(testSamples);
                } finally {
                    for (TestSample testSample : testSamples) {
                        closeDevice(testSample.cameraId);
                    }
                }
            }
        }
    }

    // @return Generates a List of HashMaps<String, GeneratedEntry> which is a map:
    //         cameraId -> GeneratedEntry (represents a camera device's test configuration)
    // @param  perIdGeneratedEntries a per-camera id set of combinations to recursively generate
    //         GeneratedEntries for.
    private List<HashMap<String, GeneratedEntry>> generateStreamSelectionsInternal(
            HashMap<String, List<GeneratedEntry>> perIdGeneratedEntries) {
        Set<String> cameraIds = perIdGeneratedEntries.keySet();
        // Base condition for recursion.
        if (cameraIds.size() == 1) {
            String cameraId = cameraIds.toArray(new String[1])[0];
            List<HashMap<String, GeneratedEntry>> ret =
                    new ArrayList<HashMap<String, GeneratedEntry>> ();
            for (GeneratedEntry entry : perIdGeneratedEntries.get(cameraId)) {
                HashMap<String, GeneratedEntry> retPut = new HashMap<String, GeneratedEntry>();
                retPut.put(cameraId, entry);
                ret.add(retPut);
            }
            return ret;
        }
        // Choose one camera id, create all combinations for the remaining set and then add each of
        // the camera id's combinations to the returned list of maps.
        String keyChosen = null;
        for (String cameraId: cameraIds) {
            keyChosen = cameraId;
            break;
        }
        List<GeneratedEntry> selfGeneratedEntries = perIdGeneratedEntries.get(keyChosen);
        perIdGeneratedEntries.remove(keyChosen);
        List<HashMap<String, GeneratedEntry>> recResult =
                generateStreamSelectionsInternal(perIdGeneratedEntries);
        List<HashMap<String, GeneratedEntry>> res =
                new ArrayList<HashMap<String, GeneratedEntry>>();
        for (GeneratedEntry gen : selfGeneratedEntries) {
            for (HashMap<String, GeneratedEntry> entryMap : recResult) {
                // Make a copy of the HashMap, add the generated entry to it and add it to the final
                // result (since we want to use the original for the other 'gen' entries)
                HashMap<String, GeneratedEntry> copy = (HashMap)entryMap.clone();
                copy.put(keyChosen, gen);
                res.add(copy);
            }
        }
        return res;
    }

    /**
     * Generates a list of combinations used for mandatory stream combination testing.
     * Each combination(GeneratedEntry) corresponds to a camera id advertised by
     * getConcurrentCameraIds().
     */
    private List<HashMap<String, GeneratedEntry>> generateStreamSelections(
            Set<String> cameraIdCombination) {
        // First artificially create lists of GeneratedEntries for each camera id in the passed
        // set.
        HashMap<String, List<GeneratedEntry>> perIdGeneratedEntries =
                new HashMap<String, List<GeneratedEntry>>();
        for (String cameraId : cameraIdCombination) {
            List<GeneratedEntry> genEntries = getGeneratedEntriesFor(cameraId);
            perIdGeneratedEntries.put(cameraId, genEntries);
        }
        return generateStreamSelectionsInternal(perIdGeneratedEntries);
    }

    // get GeneratedEntries for a particular camera id.
    List<GeneratedEntry> getGeneratedEntriesFor(String cameraId) {
        CameraTestInfo info = mCameraTestInfos.get(cameraId);
        assertTrue("CameraTestInfo not found for camera id " + cameraId, info != null);
        MandatoryStreamCombination[] combinations = info.mMandatoryStreamCombinations;
        List<GeneratedEntry> generatedEntries = new ArrayList<GeneratedEntry>();

        // Now generate entries on the camera's mandatory streams
        for (MandatoryStreamCombination combination : combinations) {
            generatedEntries.add(new GeneratedEntry(cameraId,/*substituteY8*/false, combination));
            // Check whether substituting YUV_888 format with Y8 format
            boolean substituteY8 = false;
            if (info.mStaticInfo.isMonochromeWithY8()) {
                List<MandatoryStreamInformation> streamsInfo = combination.getStreamsInformation();
                for (MandatoryStreamInformation streamInfo : streamsInfo) {
                    if (streamInfo.getFormat() == ImageFormat.YUV_420_888) {
                        substituteY8 = true;
                        break;
                    }
                }
            }

            if (substituteY8) {
                generatedEntries.add(new GeneratedEntry(cameraId, /*substituteY8*/true,
                          combination));
            }
        }
        return generatedEntries;
    }

    private void testMandatoryConcurrentStreamCombination(ArrayList<TestSample> testSamples)
            throws Exception {

        final int TIMEOUT_FOR_RESULT_MS = 1000;
        final int MIN_RESULT_COUNT = 3;
        HashMap<String, SessionConfiguration> testSessionMap =
                new HashMap<String, SessionConfiguration>();
        for (TestSample testSample : testSamples) {
            CameraTestInfo info = mCameraTestInfos.get(testSample.cameraId);
            assertTrue("CameraTestInfo not found for camera id " + testSample.cameraId,
                    info != null);
            CameraTestUtils.setupConfigurationTargets(
                testSample.combination.getStreamsInformation(), testSample.privTargets,
                testSample.jpegTargets, testSample.yuvTargets, testSample.y8Targets,
                testSample.rawTargets, testSample.heicTargets, testSample.depth16Targets,
                testSample.outputConfigs, MIN_RESULT_COUNT, testSample.substituteY8,
                /*substituteHEIC*/false, /*physicalCameraId*/null, mHandler);

            try {
                checkSessionConfigurationSupported(info.mCamera, mHandler, testSample.outputConfigs,
                        /*inputConfig*/ null, SessionConfiguration.SESSION_REGULAR,
                        true/*defaultSupport*/, String.format(
                        "Session configuration query from combination: %s failed",
                        testSample.combination.getDescription()));
                testSessionMap.put(testSample.cameraId, new SessionConfiguration(
                      SessionConfiguration.SESSION_REGULAR,testSample.outputConfigs,
                      new HandlerExecutor(mHandler), new BlockingSessionCallback()));
            } catch (Throwable e) {
                mCollector.addMessage(String.format(
                        "Mandatory stream combination: %s for camera id %s failed due: %s",
                        testSample.combination.getDescription(), testSample.cameraId,
                        e.getMessage()));
            }
        }

        // Mandatory stream combinations must be reported as supported.
        assertTrue("Concurrent session configs not supported",
                mCameraManager.isConcurrentSessionConfigurationSupported(testSessionMap));

        for (TestSample testSample  : testSamples) {
            try {
                CameraTestInfo info = mCameraTestInfos.get(testSample.cameraId);
                assertTrue("CameraTestInfo not found for camera id " + testSample.cameraId,
                        info != null);
                createSessionByConfigs(testSample.cameraId, testSample.outputConfigs);
                testSample.haveSession = true;
                CaptureRequest.Builder requestBuilder =
                        info.mCamera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);

                for (OutputConfiguration c : testSample.outputConfigs) {
                    requestBuilder.addTarget(c.getSurface());
                }
                CaptureRequest request = requestBuilder.build();
                CameraCaptureSession.CaptureCallback mockCaptureCallback =
                        mock(CameraCaptureSession.CaptureCallback.class);
                info.mCameraSession.setRepeatingRequest(request, mockCaptureCallback, mHandler);

                verify(mockCaptureCallback,
                        timeout(TIMEOUT_FOR_RESULT_MS * MIN_RESULT_COUNT).atLeast(MIN_RESULT_COUNT))
                        .onCaptureCompleted(
                            eq(info.mCameraSession),
                            eq(request),
                            isA(TotalCaptureResult.class));
                verify(mockCaptureCallback, never()).
                        onCaptureFailed(
                            eq(info.mCameraSession),
                            eq(request),
                            isA(CaptureFailure.class));
            } catch (Throwable e) {
                mCollector.addMessage(String.format(
                        "Mandatory stream combination : %s for camera id %s failed due: %s",
                        testSample.combination.getDescription(),
                        testSample.cameraId, e.getMessage()));
            }

        }

        for (TestSample testSample : testSamples) {
            if (testSample.haveSession) {
                try {
                    Log.i(TAG,
                            String.format("Done with camera %s, combination: %s, closing session",
                            testSample.cameraId, testSample.combination.getDescription()));
                    stopCapture(testSample.cameraId, /*fast*/false);
                } catch (Throwable e) {
                    String closingDownFormat =
                            "Closing down for combination: %s  for camera id %s failed due to: %s";
                    mCollector.addMessage(
                            String.format(closingDownFormat,
                            testSample.combination.getDescription(), testSample.cameraId,
                            e.getMessage()));
                }
            }
            for (SurfaceTexture target : testSample.privTargets) {
                target.release();
            }
            for (ImageReader target : testSample.jpegTargets) {
                target.close();
            }
            for (ImageReader target : testSample.yuvTargets) {
                target.close();
            }
            for (ImageReader target : testSample.y8Targets) {
                target.close();
            }
            for (ImageReader target : testSample.rawTargets) {
                target.close();
            }
            for (ImageReader target : testSample.heicTargets) {
                target.close();
            }
            for (ImageReader target : testSample.depth16Targets) {
                target.close();
            }
        }
    }
}
