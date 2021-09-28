/*
 * Copyright 2019 The Android Open Source Project
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

import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test CaptureRequest/Result/CameraCharacteristics.Key objects.
 */
@RunWith(AndroidJUnit4.class)
@SuppressWarnings("EqualsIncompatibleType")
public class SimpleObjectsTest {

    @Test
    public void CameraKeysTest() throws Exception {
        String keyName = "android.testing.Key";
        String keyName2 = "android.testing.Key2";

        CaptureRequest.Key<Integer> testRequestKey =
                new CaptureRequest.Key<>(keyName, Integer.class);
        Assert.assertEquals("Request key name not correct",
                testRequestKey.getName(), keyName);

        CaptureResult.Key<Integer> testResultKey =
                new CaptureResult.Key<>(keyName, Integer.class);
        Assert.assertEquals("Result key name not correct",
                testResultKey.getName(), keyName);

        CameraCharacteristics.Key<Integer> testCharacteristicsKey =
                new CameraCharacteristics.Key<>(keyName, Integer.class);
        Assert.assertEquals("Characteristics key name not correct",
                testCharacteristicsKey.getName(), keyName);

        CaptureRequest.Key<Integer> testRequestKey2 =
                new CaptureRequest.Key<>(keyName, Integer.class);
        Assert.assertEquals("Two request keys with same name/type should be equal",
                testRequestKey, testRequestKey2);
        CaptureRequest.Key<Byte> testRequestKey3 =
                new CaptureRequest.Key<>(keyName, Byte.class);
        Assert.assertTrue("Two request keys with different types should not be equal",
                !testRequestKey.equals(testRequestKey3));
        CaptureRequest.Key<Integer> testRequestKey4 =
                new CaptureRequest.Key<>(keyName2, Integer.class);
        Assert.assertTrue("Two request keys with different names should not be equal",
                !testRequestKey.equals(testRequestKey4));
        CaptureRequest.Key<Byte> testRequestKey5 =
                new CaptureRequest.Key<>(keyName2, Byte.class);
        Assert.assertTrue("Two request keys with different types and names should not be equal",
                !testRequestKey.equals(testRequestKey5));

        CaptureResult.Key<Integer> testResultKey2 =
                new CaptureResult.Key<>(keyName, Integer.class);
        Assert.assertEquals("Two result keys with same name/type should be equal",
                testResultKey, testResultKey2);
        CaptureResult.Key<Byte> testResultKey3 =
                new CaptureResult.Key<>(keyName, Byte.class);
        Assert.assertTrue("Two result keys with different types should not be equal",
                !testResultKey.equals(testResultKey3));
        CaptureResult.Key<Integer> testResultKey4 =
                new CaptureResult.Key<>(keyName2, Integer.class);
        Assert.assertTrue("Two result keys with different names should not be equal",
                !testResultKey.equals(testResultKey4));
        CaptureResult.Key<Byte> testResultKey5 =
                new CaptureResult.Key<>(keyName2, Byte.class);
        Assert.assertTrue("Two result keys with different types and names should not be equal",
                !testResultKey.equals(testResultKey5));

        CameraCharacteristics.Key<Integer> testCharacteristicsKey2 =
                new CameraCharacteristics.Key<>(keyName, Integer.class);
        Assert.assertEquals("Two characteristics keys with same name/type should be equal",
                testCharacteristicsKey, testCharacteristicsKey2);
        CameraCharacteristics.Key<Byte> testCharacteristicsKey3 =
                new CameraCharacteristics.Key<>(keyName, Byte.class);
        Assert.assertTrue("Two characteristics keys with different types should not be equal",
                !testCharacteristicsKey.equals(testCharacteristicsKey3));
        CameraCharacteristics.Key<Integer> testCharacteristicsKey4 =
                new CameraCharacteristics.Key<>(keyName2, Integer.class);
        Assert.assertTrue("Two characteristics keys with different names should not be equal",
                !testCharacteristicsKey.equals(testCharacteristicsKey4));
        CameraCharacteristics.Key<Byte> testCharacteristicsKey5 =
                new CameraCharacteristics.Key<>(keyName2, Byte.class);
        Assert.assertTrue(
                "Two characteristics keys with different types and names should not be equal",
                !testCharacteristicsKey.equals(testCharacteristicsKey5));

    }

}
