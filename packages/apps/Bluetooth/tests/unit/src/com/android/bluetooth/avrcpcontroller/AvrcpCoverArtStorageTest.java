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

package com.android.bluetooth.avrcpcontroller;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.InputStream;

/**
 * A test suite for the AvrcpCoverArtStorage class.
 */
@RunWith(AndroidJUnit4.class)
public final class AvrcpCoverArtStorageTest {
    private Context mTargetContext;
    private Resources mTestResources;
    private BluetoothDevice mDevice1;
    private BluetoothDevice mDevice2;
    private Bitmap mImage1;
    private Bitmap mImage2;
    private final String mHandle1 = "1";
    private final String mHandle2 = "2";
    private AvrcpCoverArtStorage mAvrcpCoverArtStorage;

    @Before
    public void setUp() {
        mTargetContext = InstrumentationRegistry.getTargetContext();
        try {
            mTestResources = mTargetContext.getPackageManager()
                    .getResourcesForApplication("com.android.bluetooth.tests");
        } catch (PackageManager.NameNotFoundException e) {
            Assert.fail("Setup Failure Unable to get resources" + e.toString());
        }
        mDevice1 = BluetoothAdapter.getDefaultAdapter().getRemoteDevice("AA:BB:CC:DD:EE:FF");
        mDevice2 = BluetoothAdapter.getDefaultAdapter().getRemoteDevice("BB:CC:DD:EE:FF:AA");
        InputStream is = mTestResources.openRawResource(
                com.android.bluetooth.tests.R.raw.image_200_200);
        mImage1 = BitmapFactory.decodeStream(is);
        InputStream is2 = mTestResources.openRawResource(
                com.android.bluetooth.tests.R.raw.image_600_600);
        mImage2 = BitmapFactory.decodeStream(is2);

        mAvrcpCoverArtStorage = new AvrcpCoverArtStorage(mTargetContext);
    }

    @After
    public void tearDown() {
        if (mAvrcpCoverArtStorage != null) {
            mAvrcpCoverArtStorage.removeImagesForDevice(mDevice1);
            mAvrcpCoverArtStorage.removeImagesForDevice(mDevice2);
            mAvrcpCoverArtStorage = null;
        }
        mImage1 = null;
        mImage2 = null;
        mDevice1 = null;
        mDevice2 = null;
        mTestResources = null;
        mTargetContext = null;
    }

    private void assertImageSame(Bitmap expected, BluetoothDevice device, String handle) {
        Bitmap image = mAvrcpCoverArtStorage.getImage(device, handle);
        Assert.assertTrue(expected.sameAs(image));
    }

    @Test
    public void addNewImage_imageExists() {
        Uri expectedUri = AvrcpCoverArtProvider.getImageUri(mDevice1, mHandle1);
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));

        Uri uri = mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);

        Assert.assertEquals(expectedUri, uri);
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
    }

    @Test
    public void addExistingImage_imageUpdated() {
        Uri expectedUri = AvrcpCoverArtProvider.getImageUri(mDevice1, mHandle1);
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));

        Uri uri = mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertEquals(expectedUri, uri);
        assertImageSame(mImage1, mDevice1, mHandle1);

        uri = mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage2);
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertEquals(expectedUri, uri);
        assertImageSame(mImage2, mDevice1, mHandle1);
    }

    @Test
    public void addTwoImageSameDevice_bothExist() {
        Uri expectedUri1 = AvrcpCoverArtProvider.getImageUri(mDevice1, mHandle1);
        Uri expectedUri2 = AvrcpCoverArtProvider.getImageUri(mDevice1, mHandle2);
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle2));

        Uri uri1 = mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        Uri uri2 = mAvrcpCoverArtStorage.addImage(mDevice1, mHandle2, mImage2);

        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertEquals(expectedUri1, uri1);

        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle2));
        Assert.assertEquals(expectedUri2, uri2);
    }

    @Test
    public void addTwoImageDifferentDevices_bothExist() {
        Uri expectedUri1 = AvrcpCoverArtProvider.getImageUri(mDevice1, mHandle1);
        Uri expectedUri2 = AvrcpCoverArtProvider.getImageUri(mDevice2, mHandle1);
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice2, mHandle1));

        Uri uri1 = mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        Uri uri2 = mAvrcpCoverArtStorage.addImage(mDevice2, mHandle1, mImage1);

        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertEquals(expectedUri1, uri1);

        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertEquals(expectedUri2, uri2);
    }

    @Test
    public void addNullImage_imageNotAdded() {
        Uri uri = mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, null);
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertEquals(null, uri);
    }

    @Test
    public void addImageNullDevice_imageNotAdded() {
        Uri uri = mAvrcpCoverArtStorage.addImage(null, mHandle1, mImage1);
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertEquals(null, uri);
    }

    @Test
    public void addImageNullHandle_imageNotAdded() {
        Uri uri = mAvrcpCoverArtStorage.addImage(mDevice1, null, mImage1);
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertEquals(null, uri);
    }

    @Test
    public void addImageEmptyHandle_imageNotAdded() {
        Uri uri = mAvrcpCoverArtStorage.addImage(mDevice1, "", mImage1);
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertEquals(null, uri);
    }

    @Test
    public void getImage_canGetImageFromStorage() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        assertImageSame(mImage1, mDevice1, mHandle1);
    }

    @Test
    public void getImageSameHandleDifferentDevices_canGetImagesFromStorage() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice2, mHandle1, mImage2);
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice2, mHandle1));
        assertImageSame(mImage1, mDevice1, mHandle1);
        assertImageSame(mImage2, mDevice2, mHandle1);
    }

    @Test
    public void getImageThatDoesntExist_returnsNull() {
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Bitmap image = mAvrcpCoverArtStorage.getImage(mDevice1, mHandle1);
        Assert.assertEquals(null, image);
    }

    @Test
    public void getImageNullDevice_returnsNull() {
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Bitmap image = mAvrcpCoverArtStorage.getImage(null, mHandle1);
        Assert.assertEquals(null, image);
    }

    @Test
    public void getImageNullHandle_returnsNull() {
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Bitmap image = mAvrcpCoverArtStorage.getImage(mDevice1, null);
        Assert.assertEquals(null, image);
    }

    @Test
    public void getImageEmptyHandle_returnsNull() {
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Bitmap image = mAvrcpCoverArtStorage.getImage(mDevice1, "");
        Assert.assertEquals(null, image);
    }

    @Test
    public void removeExistingImage_imageDoesntExist() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle2, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice2, mHandle1, mImage1);
        mAvrcpCoverArtStorage.removeImage(mDevice1, mHandle1);
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle2));
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice2, mHandle1));
    }

    @Test
    public void removeNonExistentImage_nothingHappens() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.removeImage(mDevice1, mHandle2);
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
    }

    @Test
    public void removeImageNullDevice_nothingHappens() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.removeImage(null, mHandle1);
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
    }

    @Test
    public void removeImageNullHandle_nothingHappens() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.removeImage(mDevice1, null);
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
    }

    @Test
    public void removeImageEmptyHandle_nothingHappens() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.removeImage(mDevice1, "");
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
    }

    @Test
    public void removeImageNullInputs_nothingHappens() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.removeImage(null, null);
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
    }

    @Test
    public void removeAllImagesForDevice_onlyOneDeviceImagesGone() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle2, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice2, mHandle1, mImage1);

        mAvrcpCoverArtStorage.removeImagesForDevice(mDevice1);

        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle2));
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice2, mHandle1));
    }

    @Test
    public void removeAllImagesForDeviceDne_nothingHappens() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle2, mImage1);

        mAvrcpCoverArtStorage.removeImagesForDevice(mDevice2);

        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle2));
    }

    @Test
    public void removeAllImagesForNullDevice_nothingHappens() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle2, mImage1);

        mAvrcpCoverArtStorage.removeImagesForDevice(null);

        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertTrue(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle2));
    }

    @Test
    public void clearStorageOneDevice_allImagesRemoved() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle2, mImage1);

        mAvrcpCoverArtStorage.clear();

        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle2));
    }

    @Test
    public void clearStorageManyDevices_allImagesRemoved() {
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle1, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice1, mHandle2, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice2, mHandle1, mImage1);
        mAvrcpCoverArtStorage.addImage(mDevice2, mHandle2, mImage1);

        mAvrcpCoverArtStorage.clear();

        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle1));
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice1, mHandle2));
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice2, mHandle1));
        Assert.assertFalse(mAvrcpCoverArtStorage.doesImageExist(mDevice2, mHandle2));
    }
}
