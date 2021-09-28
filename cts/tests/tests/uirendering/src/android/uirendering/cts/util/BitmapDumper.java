/*
 * Copyright (C) 2014 The Android Open Source Project
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
package android.uirendering.cts.util;

import android.app.Instrumentation;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.uirendering.cts.differencevisualizers.DifferenceVisualizer;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;

/**
 * A utility class that will allow the user to save bitmaps to the sdcard on the device.
 */
public final class BitmapDumper {
    private static final String TAG = "BitmapDumper";
    private static final String KEY_PREFIX = "uirendering_";
    private static final String TYPE_IDEAL_RENDERING = "idealCapture";
    private static final String TYPE_TESTED_RENDERING = "testedCapture";
    private static final String TYPE_VISUALIZER_RENDERING = "visualizer";
    private static final String TYPE_SINGULAR = "capture";

    // Magic number for an in-progress status report
    private static final int INST_STATUS_IN_PROGRESS = 2;
    private static File sDumpDirectory;
    private static Instrumentation sInstrumentation;

    private BitmapDumper() {}

    public static void initialize(Instrumentation instrumentation) {
        sInstrumentation = instrumentation;
        sDumpDirectory = instrumentation.getContext().getExternalCacheDir();

        // Cleanup old tests
        // These are removed on uninstall anyway but just in case...
        File[] toRemove = sDumpDirectory.listFiles();
        if (toRemove != null && toRemove.length > 0) {
            for (File file : toRemove) {
                deleteContentsAndDir(file);
            }
        }
    }

    private static boolean deleteContentsAndDir(File dir) {
        if (deleteContents(dir)) {
            return dir.delete();
        } else {
            return false;
        }
    }

    private static boolean deleteContents(File dir) {
        File[] files = dir.listFiles();
        boolean success = true;
        if (files != null) {
            for (File file : files) {
                if (file.isDirectory()) {
                    success &= deleteContents(file);
                }
                if (!file.delete()) {
                    Log.w(TAG, "Failed to delete " + file);
                    success = false;
                }
            }
        }
        return success;
    }

    private static File getFile(String className, String testName, String type) {
        File testDirectory = new File(sDumpDirectory, className);
        testDirectory.mkdirs();
        return new File(testDirectory, testName + "_" + type + ".png");
    }

    /**
     * Saves two files, one the capture of an ideal drawing, and one the capture of the tested
     * drawing. The third file saved is a bitmap that is returned from the given visualizer's
     * method.
     * The files are saved to the sdcard directory
     */
    public static void dumpBitmaps(Bitmap idealBitmap, Bitmap testedBitmap, String testName,
            String className, DifferenceVisualizer differenceVisualizer) {
        Bitmap visualizerBitmap;

        int width = idealBitmap.getWidth();
        int height = idealBitmap.getHeight();
        int[] testedArray = new int[width * height];
        int[] idealArray = new int[width * height];
        idealBitmap.getPixels(testedArray, 0, width, 0, 0, width, height);
        testedBitmap.getPixels(idealArray, 0, width, 0, 0, width, height);
        int[] visualizerArray = differenceVisualizer.getDifferences(idealArray, testedArray);
        visualizerBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        visualizerBitmap.setPixels(visualizerArray, 0, width, 0, 0, width, height);
        Bitmap croppedBitmap = Bitmap.createBitmap(testedBitmap, 0, 0, width, height);

        File idealFile = getFile(className, testName, TYPE_IDEAL_RENDERING);
        File testedFile = getFile(className, testName, TYPE_TESTED_RENDERING);
        File visualizerFile = getFile(className, testName, TYPE_VISUALIZER_RENDERING);
        saveBitmap(idealBitmap, idealFile);
        saveBitmap(croppedBitmap, testedFile);
        saveBitmap(visualizerBitmap, visualizerFile);

        Bundle report = new Bundle();
        report.putString(KEY_PREFIX + TYPE_IDEAL_RENDERING, idealFile.getAbsolutePath());
        report.putString(KEY_PREFIX + TYPE_TESTED_RENDERING, testedFile.getAbsolutePath());
        report.putString(KEY_PREFIX + TYPE_VISUALIZER_RENDERING, visualizerFile.getAbsolutePath());
        sInstrumentation.sendStatus(INST_STATUS_IN_PROGRESS, report);
    }

    public static void dumpBitmap(Bitmap bitmap, String testName, String className) {
        if (bitmap == null) {
            Log.d(TAG, "File not saved, bitmap was null for test : " + testName);
            return;
        }
        File capture = getFile(className, testName, TYPE_SINGULAR);
        saveBitmap(bitmap, capture);
        Bundle report = new Bundle();
        report.putString(KEY_PREFIX + TYPE_SINGULAR, capture.getAbsolutePath());
        sInstrumentation.sendStatus(INST_STATUS_IN_PROGRESS, report);
    }

    private static void logIfBitmapSolidColor(String fileName, Bitmap bitmap) {
        int firstColor = bitmap.getPixel(0, 0);
        for (int x = 0; x < bitmap.getWidth(); x++) {
            for (int y = 0; y < bitmap.getHeight(); y++) {
                if (bitmap.getPixel(x, y) != firstColor) {
                    return;
                }
            }
        }

        Log.w(TAG, String.format("%s entire bitmap color is %x", fileName, firstColor));
    }

    private static void saveBitmap(Bitmap bitmap, File file) {
        if (bitmap == null) {
            Log.d(TAG, "File not saved, bitmap was null");
            return;
        }

        logIfBitmapSolidColor(file.getName(), bitmap);

        try (FileOutputStream fileStream = new FileOutputStream(file)) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 0 /* ignored for PNG */, fileStream);
            fileStream.flush();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
