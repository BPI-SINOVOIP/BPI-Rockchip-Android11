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

package android.transition.cts;

import android.app.SharedElementCallback;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.ColorSpace;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Parcelable;
import android.view.View;
import android.widget.ImageView;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class SharedElementCallbackTest {
    private static class Callback extends SharedElementCallback {}

    @Test
    public void testSnapshot() {
        final int SIZE = 10;
        ColorSpace cs = ColorSpace.get(ColorSpace.Named.DISPLAY_P3);
        Bitmap bitmap = Bitmap.createBitmap(SIZE, SIZE, Bitmap.Config.ARGB_8888, true, cs);
        bitmap.eraseColor(Color.BLUE);
        bitmap = bitmap.copy(Bitmap.Config.HARDWARE, false);
        Context context = InstrumentationRegistry.getContext();
        ImageView originalView = new ImageView(context);
        originalView.setImageBitmap(bitmap);

        Callback cb = new Callback();
        Matrix matrix = new Matrix();
        RectF screenBounds = new RectF(0, 0, SIZE, SIZE);
        Parcelable snapshot = cb.onCaptureSharedElementSnapshot(originalView, matrix, screenBounds);
        assertNotNull(snapshot);

        View view = cb.onCreateSnapshotView(context, snapshot);
        assertNotNull(view);
        assertTrue(view instanceof ImageView);

        ImageView finalView = (ImageView) view;
        Drawable drawable = finalView.getDrawable();
        assertTrue(drawable instanceof BitmapDrawable);
        BitmapDrawable bitmapDrawable = (BitmapDrawable) drawable;
        Bitmap finalBitmap = bitmapDrawable.getBitmap();
        assertNotNull(finalView);

        assertSame(Bitmap.Config.HARDWARE, finalBitmap.getConfig());
        assertSame(cs, finalBitmap.getColorSpace());
    }
}
