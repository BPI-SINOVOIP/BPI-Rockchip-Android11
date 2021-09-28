package com.android.nn.benchmark.imageprocessors;

import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import com.android.nn.benchmark.core.ImageProcessorInterface;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;

/**
 * Inception image prepreocessor. Crops image to a centered square, 85% of smallest original
 * dimensions. Scales to target size and quantizes if needed.
 */

public class Inception implements ImageProcessorInterface {
    public void preprocess(int datasize, float quantScale, float quantZeroPoint,
                           int imageDimension, AssetManager assetManager,
                           String imageFileName, File cacheDir, ByteBuffer outputBuffer)
            throws IOException {
        Bitmap origBitmap = BitmapFactory.decodeStream(assetManager.open(imageFileName));

        int croppedSize = (int)(0.875f * Math.min(origBitmap.getWidth(), origBitmap.getHeight()));
        int x = origBitmap.getWidth() / 2 - croppedSize / 2;
        int y = origBitmap.getHeight() / 2 - croppedSize / 2;

        Bitmap centeredBitmap = Bitmap.createBitmap(origBitmap, x, y, croppedSize, croppedSize);
        // FileOutputStream cb = new FileOutputStream(cacheDir.getAbsolutePath() +  "/c.jpeg");
        // centeredBitmap.compress(Bitmap.CompressFormat.JPEG, 90, cb);
        Bitmap scaledBitmap = Bitmap.createScaledBitmap(centeredBitmap, imageDimension,
                imageDimension, true);
        // FileOutputStream sb = new FileOutputStream(cacheDir.getAbsolutePath() + "/s.jpeg");
        // scaledBitmap.compress(Bitmap.CompressFormat.JPEG, 90, sb);

        int[] pixels = new int[imageDimension * imageDimension];
        scaledBitmap.getPixels(pixels, 0, imageDimension, 0, 0, imageDimension, imageDimension);

        // Some of the bitmap operations may return the same underlying bitmap, recycle only
        // at the end.
        scaledBitmap.recycle();
        centeredBitmap.recycle();
        origBitmap.recycle();

        outputBuffer.clear();
        outputBuffer.order(ByteOrder.LITTLE_ENDIAN);
        for (int i = 0; i < imageDimension * imageDimension; i++) {
            // Needs to use more bits in intermediates since bytes are signed.
            // Results of bitwise ops are ints.
            //
            int redOrig = (pixels[i] >> 16) & 0xFF;
            int greenOrig = (pixels[i] >> 8) & 0xFF;
            int blueOrig = pixels[i] & 0xFF;

            float redFloat = (redOrig * 2.0f / 255.0f) - 1.0f;
            float greenFloat = (greenOrig * 2.0f / 255.0f) - 1.0f;
            float blueFloat = (blueOrig * 2.0f / 255.0f) - 1.0f;

            if (datasize == 4) {
                outputBuffer.putFloat(redFloat);
                outputBuffer.putFloat(greenFloat);
                outputBuffer.putFloat(blueFloat);
            } else {
                int redByte = (int)(redFloat / quantScale + quantZeroPoint);
                int greenByte = (int)(greenFloat / quantScale + quantZeroPoint);
                int blueByte = (int)(blueFloat / quantScale + quantZeroPoint);
                outputBuffer.put((byte)(redByte & 0xff));
                outputBuffer.put((byte)(greenByte & 0xff));
                outputBuffer.put((byte)(blueByte & 0xff));
            }
        }
    }
}
