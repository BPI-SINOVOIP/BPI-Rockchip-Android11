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

package android.soundtrigger.cts;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import android.hardware.soundtrigger.SoundTrigger;
import android.media.AudioFormat;
import android.os.Parcel;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Locale;
import java.util.Random;
import java.util.UUID;

@RunWith(AndroidJUnit4.class)
public class SoundTriggerTest {
    private static final int TEST_KEYPHRASE_ID = 200;
    private static final String TEST_KEYPHRASE_TEXT = "test_keyphrase";
    private static final int[] TEST_SUPPORTED_USERS = new int[] {1, 2, 3};
    private static final int TEST_RECOGNITION_MODES = SoundTrigger.RECOGNITION_MODE_GENERIC
            | SoundTrigger.RECOGNITION_MODE_USER_AUTHENTICATION
            | SoundTrigger.RECOGNITION_MODE_USER_IDENTIFICATION
            | SoundTrigger.RECOGNITION_MODE_VOICE_TRIGGER;

    private static final UUID TEST_MODEL_UUID = UUID.randomUUID();
    private static final UUID TEST_VENDOR_UUID = UUID.randomUUID();
    private static final int TEST_MODEL_VERSION = 123456;

    private static final int TEST_MODULE_ID = 1;
    private static final String TEST_IMPLEMENTOR = "test implementor";
    private static final String TEST_DESCRIPTION = "test description";
    private static final UUID TEST_MODULE_UUID = UUID.randomUUID();
    private static final int TEST_MODULE_VERSION = 45678;
    private static final String TEST_SUPPORTED_MODEL_ARCH = UUID.randomUUID().toString();
    private static final int TEST_MAX_SOUND_MODELS = 10;
    private static final int TEST_MAX_KEYPHRASES = 2;
    private static final int TEST_MAX_USERS = 3;
    private static final boolean TEST_SUPPORT_CAPTURE_TRANSITION = true;
    private static final int TEST_MAX_BUFFER_SIZE = 2048;
    private static final boolean TEST_SUPPORTS_CONCURRENT_CAPTURE = true;
    private static final int TEST_POWER_CONSUMPTION_MW = 50;
    private static final boolean TEST_RETURNES_TRIGGER_IN_EVENT = false;
    private static final int TEST_AUDIO_CAPABILITIES =
            SoundTrigger.ModuleProperties.AUDIO_CAPABILITY_ECHO_CANCELLATION
                    | SoundTrigger.ModuleProperties.AUDIO_CAPABILITY_NOISE_SUPPRESSION;
    private static final byte[] TEST_MODEL_DATA = new byte[1024];

    @BeforeClass
    public static void setUpClass() {
        new Random().nextBytes(TEST_MODEL_DATA);
    }

    private static SoundTrigger.Keyphrase createTestKeyphrase() {
        return new SoundTrigger.Keyphrase(TEST_KEYPHRASE_ID, TEST_RECOGNITION_MODES,
                Locale.forLanguageTag("en-US"), TEST_KEYPHRASE_TEXT, TEST_SUPPORTED_USERS);
    }

    private static void verifyKeyphraseMatchesTestParams(SoundTrigger.Keyphrase keyphrase) {
        assertEquals(keyphrase.getId(), TEST_KEYPHRASE_ID);
        assertEquals(keyphrase.getRecognitionModes(), TEST_RECOGNITION_MODES);
        assertEquals(keyphrase.getLocale(), Locale.forLanguageTag("en-US"));
        assertEquals(keyphrase.getText(), TEST_KEYPHRASE_TEXT);
        assertArrayEquals(keyphrase.getUsers(), TEST_SUPPORTED_USERS);
    }

    private static SoundTrigger.KeyphraseSoundModel createTestKeyphraseSoundModel() {
        return new SoundTrigger.KeyphraseSoundModel(TEST_MODEL_UUID, TEST_VENDOR_UUID,
                SoundTriggerTest.TEST_MODEL_DATA,
                new SoundTrigger.Keyphrase[] {createTestKeyphrase()}, TEST_MODEL_VERSION);
    }

    private static void verifyKeyphraseSoundModelMatchesTestParams(
            SoundTrigger.KeyphraseSoundModel keyphraseSoundModel) {
        assertEquals(keyphraseSoundModel.getUuid(), TEST_MODEL_UUID);
        assertEquals(keyphraseSoundModel.getVendorUuid(), TEST_VENDOR_UUID);
        assertArrayEquals(keyphraseSoundModel.getData(), SoundTriggerTest.TEST_MODEL_DATA);
        assertArrayEquals(keyphraseSoundModel.getKeyphrases(),
                new SoundTrigger.Keyphrase[] {createTestKeyphrase()});
        assertEquals(keyphraseSoundModel.getVersion(), TEST_MODEL_VERSION);
        assertEquals(keyphraseSoundModel.getType(), SoundTrigger.SoundModel.TYPE_KEYPHRASE);
    }

    private SoundTrigger.ModuleProperties createTestModuleProperties() {
        return new SoundTrigger.ModuleProperties(TEST_MODULE_ID, TEST_IMPLEMENTOR, TEST_DESCRIPTION,
                TEST_MODULE_UUID.toString(), TEST_MODULE_VERSION, TEST_SUPPORTED_MODEL_ARCH,
                TEST_MAX_SOUND_MODELS, TEST_MAX_KEYPHRASES, TEST_MAX_USERS, TEST_RECOGNITION_MODES,
                TEST_SUPPORT_CAPTURE_TRANSITION, TEST_MAX_BUFFER_SIZE,
                TEST_SUPPORTS_CONCURRENT_CAPTURE, TEST_POWER_CONSUMPTION_MW,
                TEST_RETURNES_TRIGGER_IN_EVENT, TEST_AUDIO_CAPABILITIES);
    }

    private static void verifyModulePropertiesMatchesTestParams(
            SoundTrigger.ModuleProperties moduleProperties) {
        assertEquals(moduleProperties.getId(), TEST_MODULE_ID);
        assertEquals(moduleProperties.getImplementor(), TEST_IMPLEMENTOR);
        assertEquals(moduleProperties.getDescription(), TEST_DESCRIPTION);
        assertEquals(moduleProperties.getUuid(), TEST_MODULE_UUID);
        assertEquals(moduleProperties.getVersion(), TEST_MODULE_VERSION);
        assertEquals(moduleProperties.getSupportedModelArch(), TEST_SUPPORTED_MODEL_ARCH);
        assertEquals(moduleProperties.getMaxSoundModels(), TEST_MAX_SOUND_MODELS);
        assertEquals(moduleProperties.getMaxKeyphrases(), TEST_MAX_KEYPHRASES);
        assertEquals(moduleProperties.getMaxUsers(), TEST_MAX_USERS);
        assertEquals(moduleProperties.getRecognitionModes(), TEST_RECOGNITION_MODES);
        assertEquals(moduleProperties.isCaptureTransitionSupported(),
                TEST_SUPPORT_CAPTURE_TRANSITION);
        assertEquals(moduleProperties.getMaxBufferMillis(), TEST_MAX_BUFFER_SIZE);
        assertEquals(moduleProperties.isConcurrentCaptureSupported(),
                TEST_SUPPORTS_CONCURRENT_CAPTURE);
        assertEquals(moduleProperties.getPowerConsumptionMw(), TEST_POWER_CONSUMPTION_MW);
        assertEquals(moduleProperties.isTriggerReturnedInEvent(), TEST_RETURNES_TRIGGER_IN_EVENT);
        assertEquals(moduleProperties.getAudioCapabilities(), TEST_AUDIO_CAPABILITIES);
        assertEquals(moduleProperties.describeContents(), 0);
    }

    @Test
    public void testKeyphraseParcelUnparcel() {
        SoundTrigger.Keyphrase keyphraseSrc = createTestKeyphrase();
        verifyKeyphraseMatchesTestParams(keyphraseSrc);
        Parcel parcel = Parcel.obtain();
        keyphraseSrc.writeToParcel(parcel, 0);

        parcel.setDataPosition(0);
        SoundTrigger.Keyphrase keyphraseResult = SoundTrigger.Keyphrase.readFromParcel(parcel);
        assertEquals(keyphraseSrc, keyphraseResult);
        verifyKeyphraseMatchesTestParams(keyphraseResult);

        parcel.setDataPosition(0);
        keyphraseResult = SoundTrigger.Keyphrase.CREATOR.createFromParcel(parcel);
        assertEquals(keyphraseSrc, keyphraseResult);
        verifyKeyphraseMatchesTestParams(keyphraseResult);
    }

    @Test
    public void testKeyphraseSoundModelParcelUnparcel() {
        SoundTrigger.KeyphraseSoundModel keyphraseSoundModelSrc =
                createTestKeyphraseSoundModel();
        Parcel parcel = Parcel.obtain();
        keyphraseSoundModelSrc.writeToParcel(parcel, 0);

        parcel.setDataPosition(0);
        SoundTrigger.KeyphraseSoundModel keyphraseSoundModelResult =
                SoundTrigger.KeyphraseSoundModel.readFromParcel(parcel);
        assertEquals(keyphraseSoundModelSrc, keyphraseSoundModelResult);
        verifyKeyphraseSoundModelMatchesTestParams(keyphraseSoundModelResult);

        parcel.setDataPosition(0);
        keyphraseSoundModelResult = SoundTrigger.KeyphraseSoundModel.CREATOR.createFromParcel(
                parcel);
        assertEquals(keyphraseSoundModelSrc, keyphraseSoundModelResult);
        verifyKeyphraseSoundModelMatchesTestParams(keyphraseSoundModelResult);
    }

    @Test
    public void testModulePropertiesParcelUnparcel() {
        SoundTrigger.ModuleProperties modulePropertiesSrc = createTestModuleProperties();
        Parcel parcel = Parcel.obtain();
        modulePropertiesSrc.writeToParcel(parcel, 0);

        parcel.setDataPosition(0);
        SoundTrigger.ModuleProperties modulePropertiesResult =
                SoundTrigger.ModuleProperties.CREATOR.createFromParcel(parcel);
        assertEquals(modulePropertiesSrc, modulePropertiesResult);
        verifyModulePropertiesMatchesTestParams(modulePropertiesResult);
    }

    @Test
    public void testModelParamRangeParcelUnparcel() {
        SoundTrigger.ModelParamRange modelParamRangeSrc = new SoundTrigger.ModelParamRange(-1, 10);
        Parcel parcel = Parcel.obtain();
        modelParamRangeSrc.writeToParcel(parcel, 0);

        parcel.setDataPosition(0);
        SoundTrigger.ModelParamRange modelParamRangeResult =
                SoundTrigger.ModelParamRange.CREATOR.createFromParcel(parcel);
        assertEquals(modelParamRangeSrc, modelParamRangeResult);
        assertEquals(modelParamRangeResult.getStart(), -1);
        assertEquals(modelParamRangeResult.getEnd(), 10);
    }

    @Test
    public void testRecognitionEventBasicGetters() {
        AudioFormat audioFormat = new AudioFormat.Builder().build();
        SoundTrigger.RecognitionEvent recognitionEvent = new SoundTrigger.RecognitionEvent(
                0, 100, true, 101, 1000, 1001, true, audioFormat, TEST_MODEL_DATA);
        assertEquals(recognitionEvent.getCaptureFormat(), audioFormat);
        assertEquals(recognitionEvent.getCaptureSession(), 101);
        assertArrayEquals(recognitionEvent.getData(), TEST_MODEL_DATA);
    }
}
