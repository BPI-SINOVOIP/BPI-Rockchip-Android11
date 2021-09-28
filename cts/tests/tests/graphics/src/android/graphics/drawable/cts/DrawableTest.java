/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.graphics.drawable.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.content.res.Resources.Theme;
import android.graphics.BitmapFactory;
import android.graphics.BlendMode;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Insets;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.cts.R;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Drawable.Callback;
import android.net.Uri;
import android.util.AttributeSet;
import android.util.StateSet;
import android.util.TypedValue;
import android.util.Xml;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class DrawableTest {
    private Context mContext;
    private Resources mResources;

    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getTargetContext();
        mResources = mContext.getResources();
    }

    @Test
    public void testClearColorFilter() {
        Drawable mockDrawable = new MockDrawable();
        mockDrawable.clearColorFilter();
        assertNull(mockDrawable.getColorFilter());

        ColorFilter cf = new ColorFilter();
        mockDrawable.setColorFilter(cf);
        assertEquals(cf, mockDrawable.getColorFilter());

        mockDrawable.clearColorFilter();
        assertNull(mockDrawable.getColorFilter());
    }

    @Test
    public void testCopyBounds() {
        Drawable mockDrawable = new MockDrawable();
        Rect rect1 = mockDrawable.copyBounds();
        Rect r1 = new Rect();
        mockDrawable.copyBounds(r1);
        assertEquals(0, rect1.bottom);
        assertEquals(0, rect1.left);
        assertEquals(0, rect1.right);
        assertEquals(0, rect1.top);
        assertEquals(0, r1.bottom);
        assertEquals(0, r1.left);
        assertEquals(0, r1.right);
        assertEquals(0, r1.top);

        mockDrawable.setBounds(10, 10, 100, 100);
        Rect rect2 = mockDrawable.copyBounds();
        Rect r2 = new Rect();
        mockDrawable.copyBounds(r2);
        assertEquals(100, rect2.bottom);
        assertEquals(10, rect2.left);
        assertEquals(100, rect2.right);
        assertEquals(10, rect2.top);
        assertEquals(100, r2.bottom);
        assertEquals(10, r2.left);
        assertEquals(100, r2.right);
        assertEquals(10, r2.top);

        mockDrawable.setBounds(new Rect(50, 50, 500, 500));
        Rect rect3 = mockDrawable.copyBounds();
        Rect r3 = new Rect();
        mockDrawable.copyBounds(r3);
        assertEquals(500, rect3.bottom);
        assertEquals(50, rect3.left);
        assertEquals(500, rect3.right);
        assertEquals(50, rect3.top);
        assertEquals(500, r3.bottom);
        assertEquals(50, r3.left);
        assertEquals(500, r3.right);
        assertEquals(50, r3.top);

        try {
            mockDrawable.copyBounds(null);
            fail("should throw NullPointerException.");
        } catch (NullPointerException e) {
        }
    }

    @Test
    public void testCreateFromPath() throws IOException {
        assertNull(Drawable.createFromPath(null));

        Uri uri = Uri.parse(ContentResolver.SCHEME_ANDROID_RESOURCE + "://" +
                mContext.getPackageName() + R.raw.testimage);
        assertNull(Drawable.createFromPath(uri.getPath()));

        File imageFile = new File(mContext.getFilesDir(), "tempimage.jpg");
        assertTrue(imageFile.createNewFile());
        assertTrue(imageFile.exists());
        writeSampleImage(imageFile);

        final String path = imageFile.getPath();
        Uri u = Uri.parse(path);
        assertNotNull(Drawable.createFromPath(u.toString()));
        assertTrue(imageFile.delete());
    }

    @Test
    public void testCreateFromIncomplete() throws IOException {
        // Create a truncated image file.
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        byte[] buffer = new byte[1024];
        try (InputStream source = mResources.openRawResource(R.raw.testimage)) {
            for (int len = source.read(buffer); len >= 0; len = source.read(buffer)) {
                bytes.write(buffer, 0, len);
            }
        }

        File imageFile = new File(mContext.getFilesDir(), "tempimage.jpg");
        assertTrue(imageFile.createNewFile());
        assertTrue(imageFile.exists());
        try (OutputStream target = new FileOutputStream(imageFile)) {
            byte[] byteArray = bytes.toByteArray();
            target.write(byteArray, 0, byteArray.length / 2);
        }

        // Now test various Drawable APIs that should succeed.
        final String path = imageFile.getPath();
        Uri u = Uri.parse(path);
        assertNotNull(Drawable.createFromPath(u.toString()));

        try (FileInputStream input = new FileInputStream(imageFile)) {
            assertNotNull(Drawable.createFromStream(input, ""));
        }

        try (FileInputStream input = new FileInputStream(imageFile)) {
            assertNotNull(Drawable.createFromResourceStream(mResources, new TypedValue(),
                        input, ""));
        }

        try (FileInputStream input = new FileInputStream(imageFile)) {
            assertNotNull(Drawable.createFromResourceStream(mResources, new TypedValue(),
                        input, "", new BitmapFactory.Options()));
        }

        assertTrue(imageFile.delete());
    }

    private void writeSampleImage(File imagefile) throws IOException {
        try (InputStream source = mResources.openRawResource(R.raw.testimage);
             OutputStream target = new FileOutputStream(imagefile)) {
            byte[] buffer = new byte[1024];
            for (int len = source.read(buffer); len >= 0; len = source.read(buffer)) {
                target.write(buffer, 0, len);
            }
        }
    }

    @Test
    public void testCreateFromStream() throws IOException {
        FileInputStream inputEmptyStream = null;
        FileInputStream inputStream = null;
        File imageFile = null;
        OutputStream outputEmptyStream = null;

        assertNull(Drawable.createFromStream(null, "test.bmp"));

        File emptyFile = new File(mContext.getFilesDir(), "tempemptyimage.jpg");

        // write some random data.
        try {
            outputEmptyStream = new FileOutputStream(emptyFile);
            outputEmptyStream.write(10);

            inputEmptyStream = new FileInputStream(emptyFile);
            assertNull(Drawable.createFromStream(inputEmptyStream, "Sample"));

            imageFile = new File(mContext.getFilesDir(), "tempimage.jpg");

            writeSampleImage(imageFile);

            inputStream = new FileInputStream(imageFile);
            assertNotNull(Drawable.createFromStream(inputStream, "Sample"));
        } finally {
            if (null != outputEmptyStream) {
                outputEmptyStream.close();
            }
            if (null != inputEmptyStream) {
                inputEmptyStream.close();
            }
            if (null != inputStream) {
                inputStream.close();
            }
            if (emptyFile.exists()) {
                assertTrue(emptyFile.delete());
            }
            if (imageFile.exists()) {
                assertTrue(imageFile.delete());
            }
        }
    }

    private Boolean isClosed = new Boolean(false);

    @Test
    public void testCreateFromStream2() throws IOException {
        FileInputStream inputStream = null;
        File imageFile = null;
        synchronized (isClosed) {
            isClosed = false;
            try {
                imageFile = new File(mContext.getFilesDir(), "tempimage.jpg");
                writeSampleImage(imageFile);

                inputStream = new FileInputStream(imageFile) {
                    @Override
                    public void close() throws IOException {
                        super.close();
                        isClosed = true;
                    }
                };
                assertNotNull(Drawable.createFromStream(inputStream, "Sample"));
            } finally {
                if (null != inputStream) {
		    // verify that the stream was not closed
                    assertFalse(isClosed);
                    inputStream.close();
                }
                if (imageFile.exists()) {
                    assertTrue(imageFile.delete());
                }
            }
        }
    }

    @Test
    public void testCreateFromResourceStream1() throws IOException {
        FileInputStream inputEmptyStream = null;
        FileInputStream inputStream = null;
        File imageFile = null;
        OutputStream outputEmptyStream = null;

        assertNull(Drawable.createFromResourceStream(null, null, null, "test.bmp"));

        File emptyFile = new File(mContext.getFilesDir(), "tempemptyimage.jpg");

        // write some random data.
        try {
            outputEmptyStream = new FileOutputStream(emptyFile);
            outputEmptyStream.write(10);

            inputEmptyStream = new FileInputStream(emptyFile);
            assertNull(Drawable.createFromResourceStream(mResources, null, inputEmptyStream,
                    "Sample"));

            imageFile = new File(mContext.getFilesDir(), "tempimage.jpg");

            writeSampleImage(imageFile);

            inputStream = new FileInputStream(imageFile);
            final TypedValue value = new TypedValue();
            assertNotNull(Drawable.createFromResourceStream(mResources, value, inputStream,
                    "Sample"));
        } finally {
            if (null != outputEmptyStream) {
                outputEmptyStream.close();
            }
            if (null != inputEmptyStream) {
                inputEmptyStream.close();
            }
            if (null != inputStream) {
                inputStream.close();
            }
            if (emptyFile.exists()) {
                assertTrue(emptyFile.delete());
            }
            if (imageFile.exists()) {
                assertTrue(imageFile.delete());
            }
        }
    }

    @Test
    public void testCreateFromResourceStream2() throws IOException {
        FileInputStream inputEmptyStream = null;
        FileInputStream inputStream = null;
        File imageFile = null;
        OutputStream outputEmptyStream = null;

        BitmapFactory.Options opt = new BitmapFactory.Options();
        opt.inScaled = false;

        assertNull(Drawable.createFromResourceStream(null, null, null, "test.bmp", opt));

        File emptyFile = new File(mContext.getFilesDir(), "tempemptyimage.jpg");

        // write some random data.
        try {
            outputEmptyStream = new FileOutputStream(emptyFile);
            outputEmptyStream.write(10);

            inputEmptyStream = new FileInputStream(emptyFile);
            assertNull(Drawable.createFromResourceStream(mResources, null, inputEmptyStream,
                    "Sample", opt));

            imageFile = new File(mContext.getFilesDir(), "tempimage.jpg");

            writeSampleImage(imageFile);

            inputStream = new FileInputStream(imageFile);
            final TypedValue value = new TypedValue();
            assertNotNull(Drawable.createFromResourceStream(mResources, value, inputStream,
                    "Sample", opt));
        } finally {
            if (null != outputEmptyStream) {
                outputEmptyStream.close();
            }
            if (null != inputEmptyStream) {
                inputEmptyStream.close();
            }
            if (null != inputStream) {
                inputStream.close();
            }
            if (emptyFile.exists()) {
                assertTrue(emptyFile.delete());
            }
            if (imageFile.exists()) {
                assertTrue(imageFile.delete());
            }
        }
    }

    @Test
    public void testImageIntrinsicScaledForDensity() throws IOException {
        try (InputStream is = mContext.getAssets().open("green-p3.png")) {
            Drawable drawable = Drawable.createFromStream(is, null);
            assertNotNull(drawable);

            float density = mContext.getResources().getDisplayMetrics().density;
            int densityAdjustedSize = Math.round(64 / density);
            assertEquals(densityAdjustedSize, drawable.getIntrinsicWidth());
            assertEquals(densityAdjustedSize, drawable.getIntrinsicHeight());
        }
    }

    @Test
    public void testImageIntrinsicScaledForDensityWithBitmapOptions() throws IOException {
        try (InputStream is = mContext.getAssets().open("green-p3.png")) {
            // Verify that providing BitmapFactory Options provides the same result
            Drawable drawable = Drawable.createFromResourceStream(
                    null, null, is, null, new BitmapFactory.Options());
            assertNotNull(drawable);

            float density = mContext.getResources().getDisplayMetrics().density;
            int densityAdjustedSize = Math.round(64 / density);
            assertEquals(densityAdjustedSize, drawable.getIntrinsicWidth());
            assertEquals(densityAdjustedSize, drawable.getIntrinsicHeight());
        }
    }

    @Test
    public void testCreateFromXml() throws XmlPullParserException, IOException {
        XmlPullParser parser = mResources.getXml(R.drawable.gradientdrawable);
        Drawable drawable = Drawable.createFromXml(mResources, parser);
        assertNotNull(drawable);

        Drawable expected = mResources.getDrawable(R.drawable.gradientdrawable, null);
        assertEquals(expected.getIntrinsicWidth(), drawable.getIntrinsicWidth());
        assertEquals(expected.getIntrinsicHeight(), drawable.getIntrinsicHeight());
    }

    @Test
    public void testCreateFromXmlThemed() throws XmlPullParserException, IOException {
        XmlPullParser parser = mResources.getXml(R.drawable.gradientdrawable_theme);
        Theme theme = mResources.newTheme();
        theme.applyStyle(R.style.Theme_ThemedDrawableTest, true);
        Drawable drawable = Drawable.createFromXml(mResources, parser, theme);
        assertNotNull(drawable);

        Drawable expected = mResources.getDrawable(R.drawable.gradientdrawable_theme, theme);
        assertEquals(expected.getIntrinsicWidth(), drawable.getIntrinsicWidth());
        assertEquals(expected.getIntrinsicHeight(), drawable.getIntrinsicHeight());
    }

    @Test
    public void testCreateFromXmlInner() throws XmlPullParserException, IOException {
        XmlPullParser parser = mResources.getXml(R.drawable.gradientdrawable);
        while (parser.next() != XmlPullParser.START_TAG) {
            // ignore event, just seek to first tag
        }
        AttributeSet attrs = Xml.asAttributeSet(parser);
        Drawable drawable = Drawable.createFromXmlInner(mResources, parser, attrs);
        assertNotNull(drawable);

        Drawable expected = mResources.getDrawable(R.drawable.gradientdrawable, null);
        assertEquals(expected.getIntrinsicWidth(), drawable.getIntrinsicWidth());
        assertEquals(expected.getIntrinsicHeight(), drawable.getIntrinsicHeight());
    }

    @Test
    public void testCreateFromXmlInnerThemed() throws XmlPullParserException, IOException {
        XmlPullParser parser = mResources.getXml(R.drawable.gradientdrawable_theme);
        while (parser.next() != XmlPullParser.START_TAG) {
            // ignore event, just seek to first tag
        }
        AttributeSet attrs = Xml.asAttributeSet(parser);
        Theme theme = mResources.newTheme();
        theme.applyStyle(R.style.Theme_ThemedDrawableTest, true);
        Drawable drawable = Drawable.createFromXmlInner(mResources, parser, attrs, theme);
        assertNotNull(drawable);

        Drawable expected = mResources.getDrawable(R.drawable.gradientdrawable_theme, theme);
        assertEquals(expected.getIntrinsicWidth(), drawable.getIntrinsicWidth());
        assertEquals(expected.getIntrinsicHeight(), drawable.getIntrinsicHeight());
    }

    @Test
    public void testAccessBounds() {
        Drawable mockDrawable = new MockDrawable();
        mockDrawable.setBounds(0, 0, 100, 100);
        Rect r = mockDrawable.getBounds();
        assertEquals(0, r.left);
        assertEquals(0, r.top);
        assertEquals(100, r.bottom);
        assertEquals(100, r.right);

        mockDrawable.setBounds(new Rect(10, 10, 150, 150));
        r = mockDrawable.getBounds();
        assertEquals(10, r.left);
        assertEquals(10, r.top);
        assertEquals(150, r.bottom);
        assertEquals(150, r.right);

        try {
            mockDrawable.setBounds(null);
            fail("should throw NullPointerException.");
        } catch (NullPointerException e) {
        }
    }

    @Test
    public void testAccessChangingConfigurations() {
        Drawable mockDrawable = new MockDrawable();
        assertEquals(0, mockDrawable.getChangingConfigurations());

        mockDrawable.setChangingConfigurations(1);
        assertEquals(1, mockDrawable.getChangingConfigurations());

        mockDrawable.setChangingConfigurations(Integer.MAX_VALUE);
        assertEquals(Integer.MAX_VALUE, mockDrawable.getChangingConfigurations());

        mockDrawable.setChangingConfigurations(Integer.MIN_VALUE);
        assertEquals(Integer.MIN_VALUE, mockDrawable.getChangingConfigurations());
    }

    @Test
    public void testGetConstantState() {
        Drawable mockDrawable = new MockDrawable();
        assertNull(mockDrawable.getConstantState());
    }

    @Test
    public void testGetCurrent() {
        Drawable mockDrawable = new MockDrawable();
        assertSame(mockDrawable, mockDrawable.getCurrent());
    }

    @Test
    public void testGetIntrinsicHeight() {
        Drawable mockDrawable = new MockDrawable();
        assertEquals(-1, mockDrawable.getIntrinsicHeight());
    }

    @Test
    public void testGetIntrinsicWidth() {
        Drawable mockDrawable = new MockDrawable();
        assertEquals(-1, mockDrawable.getIntrinsicWidth());
    }

    @Test
    public void testAccessLevel() {
        Drawable mockDrawable = new MockDrawable();
        assertEquals(0, mockDrawable.getLevel());

        assertFalse(mockDrawable.setLevel(10));
        assertEquals(10, mockDrawable.getLevel());

        assertFalse(mockDrawable.setLevel(20));
        assertEquals(20, mockDrawable.getLevel());

        assertFalse(mockDrawable.setLevel(0));
        assertEquals(0, mockDrawable.getLevel());

        assertFalse(mockDrawable.setLevel(10000));
        assertEquals(10000, mockDrawable.getLevel());
    }

    @Test
    public void testGetMinimumHeight() {
        Drawable mockDrawable = new MockDrawable();
        assertEquals(0, mockDrawable.getMinimumHeight());
    }

    @Test
    public void testGetMinimumWidth() {
        Drawable mockDrawable = new MockDrawable();
        assertEquals(0, mockDrawable.getMinimumWidth());
    }

    @Test
    public void testGetPadding() {
        Drawable mockDrawable = new MockDrawable();
        Rect r = new Rect(10, 10, 20, 20);
        assertFalse(mockDrawable.getPadding(r));
        assertEquals(0, r.bottom);
        assertEquals(0, r.top);
        assertEquals(0, r.left);
        assertEquals(0, r.right);

        try {
            mockDrawable.getPadding(null);
            fail("should throw NullPointerException.");
        } catch (NullPointerException e) {
        }
    }

    @Test
    public void testAccessState() {
        Drawable mockDrawable = new MockDrawable();
        assertEquals(StateSet.WILD_CARD, mockDrawable.getState());

        int[] states = new int[] {1, 2, 3};
        assertFalse(mockDrawable.setState(states));
        assertEquals(states, mockDrawable.getState());

        mockDrawable.setState(null);
    }

    @Test
    public void testGetTransparentRegion() {
        Drawable mockDrawable = new MockDrawable();
        assertNull(mockDrawable.getTransparentRegion());
    }

    @Test
    public void testInflate() throws XmlPullParserException, IOException {
        Drawable mockDrawable = new MockDrawable();

        XmlPullParser parser = mResources.getXml(R.xml.drawable_test);
        while (parser.next() != XmlPullParser.START_TAG) {
            // ignore event, just seek to first tag
        }
        AttributeSet attrs = Xml.asAttributeSet(parser);

        mockDrawable.inflate(mResources, parser, attrs);
        // visibility set to false in resource
        assertFalse(mockDrawable.isVisible());
    }

    @Test
    public void testInvalidateSelf() {
        Drawable mockDrawable = new MockDrawable();
        // if setCallback() is not called, invalidateSelf() would do nothing,
        // so just call it to check whether it throws exceptions.
        mockDrawable.invalidateSelf();

        Drawable.Callback mockCallback = mock(Drawable.Callback.class);
        mockDrawable.setCallback(mockCallback);
        mockDrawable.invalidateSelf();
        verify(mockCallback, times(1)).invalidateDrawable(mockDrawable);
    }

    @Test
    public void testIsStateful() {
        Drawable mockDrawable = new MockDrawable();
        assertFalse(mockDrawable.isStateful());
    }

    @Test
    public void testVisible() {
        Drawable mockDrawable = new MockDrawable();
        assertTrue(mockDrawable.isVisible());

        assertTrue(mockDrawable.setVisible(false, false));
        assertFalse(mockDrawable.isVisible());

        assertFalse(mockDrawable.setVisible(false, false));
        assertFalse(mockDrawable.isVisible());

        assertTrue(mockDrawable.setVisible(true, false));
        assertTrue(mockDrawable.isVisible());
    }

    @Test
    public void testOnBoundsChange() {
        MockDrawable mockDrawable = new MockDrawable();

        // No-op in the Drawable superclass.
        mockDrawable.onBoundsChange(new Rect(0, 0, 10, 10));
    }

    @Test
    public void testOnLevelChange() {
        MockDrawable mockDrawable = new MockDrawable();
        assertFalse(mockDrawable.onLevelChange(0));
    }

    @Test
    public void testOnStateChange() {
        MockDrawable mockDrawable = new MockDrawable();
        assertFalse(mockDrawable.onStateChange(null));
    }

    @Test
    public void testResolveOpacity() {
        assertEquals(PixelFormat.TRANSLUCENT,
                Drawable.resolveOpacity(PixelFormat.TRANSLUCENT, PixelFormat.TRANSLUCENT));
        assertEquals(PixelFormat.UNKNOWN,
                Drawable.resolveOpacity(PixelFormat.UNKNOWN, PixelFormat.TRANSLUCENT));
        assertEquals(PixelFormat.TRANSLUCENT,
                Drawable.resolveOpacity(PixelFormat.OPAQUE, PixelFormat.TRANSLUCENT));
        assertEquals(PixelFormat.TRANSPARENT,
                Drawable.resolveOpacity(PixelFormat.OPAQUE, PixelFormat.TRANSPARENT));
        assertEquals(PixelFormat.OPAQUE,
                Drawable.resolveOpacity(PixelFormat.RGB_888, PixelFormat.RGB_565));
    }

    @Test
    public void testScheduleSelf() {
        Drawable mockDrawable = new MockDrawable();
        mockDrawable.scheduleSelf(null, 1000L);

        Runnable runnable = mock(Runnable.class);
        mockDrawable.scheduleSelf(runnable, 1000L);

        Callback mockCallback = mock(Callback.class);
        mockDrawable.setCallback(mockCallback);
        mockDrawable.scheduleSelf(runnable, 1000L);
        verify(mockCallback).scheduleDrawable(eq(mockDrawable), eq(runnable), eq(1000L));

        mockDrawable.scheduleSelf(runnable, 0L);
        verify(mockCallback).scheduleDrawable(eq(mockDrawable), eq(runnable), eq(0L));

        mockDrawable.scheduleSelf(runnable, -1000L);
        verify(mockCallback).scheduleDrawable(eq(mockDrawable), eq(runnable), eq(-1000L));
    }

    @Test
    public void testAccessCallback() {
        Drawable mockDrawable = new MockDrawable();
        Callback mockCallback = mock(Callback.class);

        mockDrawable.setCallback(mockCallback);
        assertEquals(mockCallback, mockDrawable.getCallback());

        mockDrawable.setCallback(null);
        assertEquals(null, mockDrawable.getCallback());
    }

    @Test
    public void testSetColorFilter() {
        Drawable mockDrawable = new MockDrawable();
        mockDrawable.setColorFilter(5, PorterDuff.Mode.CLEAR);
    }

    @Test
    public void testSetDither() {
        Drawable mockDrawable = new MockDrawable();

        // No-op in the Drawable superclass.
        mockDrawable.setDither(false);
    }

    @Test
    public void testSetHotspotBounds() {
        Drawable mockDrawable = new MockDrawable();

        // No-op in the Drawable superclass.
        mockDrawable.setHotspotBounds(10, 15, 100, 150);
    }

    @Test
    public void testGetHotspotBounds() {
        Drawable mockDrawable = new MockDrawable();

        // No-op in the Drawable superclass.
        mockDrawable.getHotspotBounds(new Rect());
    }

    @Test
    public void testAccessLayoutDirection() {
        Drawable mockDrawable = new MockDrawable();

        mockDrawable.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        assertEquals(View.LAYOUT_DIRECTION_LTR, mockDrawable.getLayoutDirection());

        mockDrawable.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        assertEquals(View.LAYOUT_DIRECTION_RTL, mockDrawable.getLayoutDirection());
    }

    @Test
    public void testOnLayoutDirectionChanged() {
        Drawable mockDrawable = new MockDrawable();

        // No-op in the Drawable superclass.
        mockDrawable.onLayoutDirectionChanged(View.LAYOUT_DIRECTION_LTR);
    }

    @Test
    public void testSetFilterBitmap() {
        Drawable mockDrawable = new MockDrawable();

        // No-op in the Drawable superclass.
        mockDrawable.setFilterBitmap(false);
    }

    @Test
    public void testIsFilterBitmap() {
        Drawable mockDrawable = new MockDrawable();

        // No-op in the Drawable superclass.
        mockDrawable.isFilterBitmap();
    }

    @Test
    public void testUnscheduleSelf() {
        Drawable mockDrawable = new MockDrawable();
        Drawable.Callback mockCallback = mock(Drawable.Callback.class);
        mockDrawable.setCallback(mockCallback);
        mockDrawable.unscheduleSelf(null);
        verify(mockCallback, times(1)).unscheduleDrawable(mockDrawable, null);
    }

    @Test
    public void testMutate() {
        Drawable mockDrawable = new MockDrawable();
        assertSame(mockDrawable, mockDrawable.mutate());
    }

    @Test
    public void testDefaultOpticalInsetsIsNone() {
        Drawable mockDrawable = new MockDrawable();
        assertEquals(Insets.NONE, mockDrawable.getOpticalInsets());
    }

    @Test
    public void testNoSetTintModeInfiniteLoop() {
        // Setting a PorterDuff.Mode should delegate to the BlendMode API
        TestTintDrawable testTintDrawable = new TestTintDrawable();

        testTintDrawable.setTintMode(PorterDuff.Mode.OVERLAY);
        assertEquals(2, testTintDrawable.getNumTimesTintModeInvoked());
        assertEquals(1, testTintDrawable.getNumTimesBlendModeInvoked());
    }

    @Test
    public void testPorterDuffTintWithUnsupportedBlendMode() {
        // Setting a BlendMode should delegate to the PorterDuff.Mode API
        TestTintDrawable testTintDrawable = new TestTintDrawable();
        testTintDrawable.setTintBlendMode(BlendMode.LUMINOSITY);
        // 1 time invoking setTintBlendMode because the default is applied if
        // there is no equivalent for the luminosity blend mode on older API levels
        assertEquals(1, testTintDrawable.getNumTimesTintModeInvoked());
        assertEquals(2, testTintDrawable.getNumTimesBlendModeInvoked());
    }

    @Test
    public void testPorterDuffTintWithSupportedBlendMode() {
        TestTintDrawable testTintDrawable = new TestTintDrawable();
        testTintDrawable.setTintBlendMode(BlendMode.SRC_OVER);
        assertEquals(1, testTintDrawable.getNumTimesTintModeInvoked());
        assertEquals(2, testTintDrawable.getNumTimesBlendModeInvoked());
    }

    @Test
    public void testBlendModeImplementationInvokedWithBlendMode() {
        TestBlendModeImplementedDrawable test = new TestBlendModeImplementedDrawable();

        test.setTintBlendMode(BlendMode.SRC);
        assertEquals(1, test.getNumTimesBlendModeInvoked());
        assertEquals(0, test.getNumTimesTintModeInvoked());
    }

    @Test
    public void testBlendModeImplementationInvokedWithPorterDuffMode() {
        TestBlendModeImplementedDrawable test = new TestBlendModeImplementedDrawable();
        test.setTintMode(PorterDuff.Mode.CLEAR);

        assertEquals(1, test.getNumTimesTintModeInvoked());
        assertEquals(1, test.getNumTimesBlendModeInvoked());
    }

    @Test
    public void testPorterDuffImplementationInvokedWithBlendMode() {
        TestPorterDuffImplementedDrawable test = new TestPorterDuffImplementedDrawable();
        test.setTintBlendMode(BlendMode.CLEAR);

        assertEquals(1, test.getNumTimesBlendModeInvoked());
        assertEquals(1, test.getNumTimesTintModeInvoked());
    }

    @Test
    public void testPorterDuffImplementationInvokedWithPorterDuffMode() {
        TestPorterDuffImplementedDrawable test = new TestPorterDuffImplementedDrawable();
        test.setTintMode(PorterDuff.Mode.CLEAR);

        assertEquals(1, test.getNumTimesTintModeInvoked());
        assertEquals(0, test.getNumTimesBlendModeInvoked());
    }

    @Test
    public void testNullPorterDuffReturnsDefaultBlendMode() {
        TestNullBlendModeDrawable d = new TestNullBlendModeDrawable();
        d.setTintMode((PorterDuff.Mode) null);
        assertEquals(BlendMode.SRC_IN, d.mode);
    }

    @Test
    public void testNullBlendModeReturnsDefaultPorterDuffMode() {
        TestNullPorterDuffDrawable d = new TestNullPorterDuffDrawable();
        d.setTintBlendMode((BlendMode) null);
        assertEquals(PorterDuff.Mode.SRC_IN, d.mode);
    }

    // Since Mockito can't mock or spy on protected methods, we have a custom extension
    // of Drawable to track calls to protected methods. This class also has empty implementations
    // of the base abstract methods.
    private static class MockDrawable extends Drawable {
        private ColorFilter mColorFilter;

        @Override
        public void draw(Canvas canvas) {
        }

        @Override
        public void setAlpha(int alpha) {
        }

        @Override
        public void setColorFilter(ColorFilter cf) {
            mColorFilter = cf;
        }

        @Override
        public ColorFilter getColorFilter() {
            return mColorFilter;
        }

        @Override
        public int getOpacity() {
            return PixelFormat.OPAQUE;
        }

        protected void onBoundsChange(Rect bounds) {
            super.onBoundsChange(bounds);
        }

        protected boolean onLevelChange(int level) {
            return super.onLevelChange(level);
        }

        protected boolean onStateChange(int[] state) {
            return super.onStateChange(state);
        }
    }

    private static class TestTintDrawable extends Drawable {

        private int mSetTintModeInvoked = 0;
        private int mSetBlendModeInvoked = 0;

        @Override
        public void draw(Canvas canvas) {

        }

        @Override
        public void setAlpha(int alpha) {

        }

        @Override
        public void setColorFilter(ColorFilter colorFilter) {

        }

        @Override
        public int getOpacity() {
            return 0;
        }

        @Override
        public void setTintMode(PorterDuff.Mode tintMode) {
            mSetTintModeInvoked++;
            super.setTintMode(tintMode);
        }

        @Override
        public void setTintBlendMode(BlendMode blendMode) {
            mSetBlendModeInvoked++;
            super.setTintBlendMode(blendMode);
        }

        public int getNumTimesTintModeInvoked() {
            return mSetTintModeInvoked;
        }

        public int getNumTimesBlendModeInvoked() {
            return mSetBlendModeInvoked;
        }
    }

    private static class TestNullPorterDuffDrawable extends Drawable {

        public PorterDuff.Mode mode;

        @Override
        public void draw(Canvas canvas) {

        }

        @Override
        public void setAlpha(int alpha) {

        }

        @Override
        public void setColorFilter(ColorFilter colorFilter) {

        }

        @Override
        public void setTintMode(PorterDuff.Mode tintMode) {
            mode = tintMode;
        }

        @Override
        public int getOpacity() {
            return 0;
        }
    }

    private static class TestNullBlendModeDrawable extends Drawable {

        public BlendMode mode;

        @Override
        public void draw(Canvas canvas) {

        }

        @Override
        public void setAlpha(int alpha) {

        }

        @Override
        public void setColorFilter(ColorFilter colorFilter) {

        }

        @Override
        public void setTintBlendMode(BlendMode blendMode) {
            mode = blendMode;
        }

        @Override
        public int getOpacity() {
            return 0;
        }
    }

    private static class TestBlendModeImplementedDrawable extends Drawable {

        private int mSetTintModeInvoked = 0;
        private int mSetBlendModeInvoked = 0;

        @Override
        public void draw(Canvas canvas) {

        }

        @Override
        public void setAlpha(int alpha) {

        }

        @Override
        public void setColorFilter(ColorFilter colorFilter) {

        }

        @Override
        public int getOpacity() {
            return 0;
        }

        @Override
        public void setTintMode(PorterDuff.Mode tintMode) {
            mSetTintModeInvoked++;
            super.setTintMode(tintMode);
        }

        @Override
        public void setTintBlendMode(BlendMode blendMode) {
            // Intentionally not delegating to super class implementation
            mSetBlendModeInvoked++;
        }

        public int getNumTimesTintModeInvoked() {
            return mSetTintModeInvoked;
        }

        public int getNumTimesBlendModeInvoked() {
            return mSetBlendModeInvoked;
        }
    }

    private static class TestPorterDuffImplementedDrawable extends Drawable {

        private int mSetTintModeInvoked = 0;
        private int mSetBlendModeInvoked = 0;

        @Override
        public void draw(Canvas canvas) {

        }

        @Override
        public void setAlpha(int alpha) {

        }

        @Override
        public void setColorFilter(ColorFilter colorFilter) {

        }

        @Override
        public int getOpacity() {
            return 0;
        }

        @Override
        public void setTintMode(PorterDuff.Mode tintMode) {
            // Intentionally not delegating to super class implementation
            mSetTintModeInvoked++;
        }

        @Override
        public void setTintBlendMode(BlendMode blendMode) {
            mSetBlendModeInvoked++;
            super.setTintBlendMode(blendMode);
        }

        public int getNumTimesTintModeInvoked() {
            return mSetTintModeInvoked;
        }

        public int getNumTimesBlendModeInvoked() {
            return mSetBlendModeInvoked;
        }
    }
}
