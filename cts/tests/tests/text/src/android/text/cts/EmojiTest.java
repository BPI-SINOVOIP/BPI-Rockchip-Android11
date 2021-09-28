/*
 * Copyright (C) 2013 The Android Open Source Project
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

package android.text.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Picture;
import android.util.TypedValue;
import android.view.KeyEvent;
import android.view.View;
import android.webkit.WebView.VisualStateCallback;
import android.webkit.cts.WebViewOnUiThread;
import android.widget.EditText;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.NullWebViewUtils;

import com.google.common.util.concurrent.SettableFuture;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeUnit;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class EmojiTest {
    private static final long TEST_TIMEOUT_MS = 20000L; // 20s
    private Context mContext;
    private EditText mEditText;

    @Rule
    public ActivityTestRule<EmojiCtsActivity> mActivityRule =
            new ActivityTestRule<>(EmojiCtsActivity.class);

    @Before
    public void setup() {
        mContext = mActivityRule.getActivity();
    }

    /**
     * Tests all Emoji are defined in Character class
     */
    @Test
    public void testEmojiCodePoints() {
        for (int i = 0; i < EmojiConstants.ALL_EMOJI.length; i++) {
            assertTrue(Character.isDefined(EmojiConstants.ALL_EMOJI[i]));
        }
    }

    private String describeBitmap(final Bitmap bmp) {
        StringBuilder sb = new StringBuilder();
        sb.append("[ID:0x" + Integer.toHexString(System.identityHashCode(bmp)));
        sb.append(" " + Integer.toString(bmp.getWidth()) + "x" + Integer.toString(bmp.getHeight()));
        sb.append(" Config:");
        if (bmp.getConfig() == Bitmap.Config.ALPHA_8) {
            sb.append("ALPHA_8");
        } else if (bmp.getConfig() == Bitmap.Config.RGB_565) {
            sb.append("RGB_565");
        } else if (bmp.getConfig() == Bitmap.Config.ARGB_4444) {
            sb.append("ARGB_4444");
        } else if (bmp.getConfig() == Bitmap.Config.ARGB_8888) {
            sb.append("ARGB_8888");
        } else {
            sb.append("UNKNOWN");
        }
        sb.append("]");
        return sb.toString();
    }

    // These emojis should have different characters
    private static final int sComparedCodePoints[][] = {
        {0x1F436, 0x1F435},      // Dog(U+1F436) and Monkey(U+1F435)
        {0x26BD, 0x26BE},        // Soccer ball(U+26BD) and Baseball(U+26BE)
        {0x1F47B, 0x1F381},      // Ghost(U+1F47B) and wrapped present(U+1F381)
        {0x2764, 0x1F494},       // Heavy black heart(U+2764) and broken heart(U+1F494)
        {0x1F603, 0x1F33B}       // Smiling face with open mouth(U+1F603) and sunflower(U+1F33B)
    };

    /**
     * Tests Emoji has different glyph for different meaning characters.
     * Test on Canvas, TextView and EditText
     */
    @UiThreadTest
    @Test
    public void testEmojiGlyph() {
        CaptureCanvas ccanvas = new CaptureCanvas(mContext);

        Bitmap bitmapA, bitmapB;  // Emoji displayed Bitmaps to compare

        for (int i = 0; i < sComparedCodePoints.length; i++) {
            String baseMessage = "Glyph for U+" + Integer.toHexString(sComparedCodePoints[i][0])
                    + " should be different from glyph for U+"
                    + Integer.toHexString(sComparedCodePoints[i][1]) + ". ";

            bitmapA = ccanvas.capture(Character.toChars(sComparedCodePoints[i][0]));
            bitmapB = ccanvas.capture(Character.toChars(sComparedCodePoints[i][1]));

            String bmpDiffMessage = describeBitmap(bitmapA) + "vs" + describeBitmap(bitmapB);
            assertFalse(baseMessage + bmpDiffMessage, bitmapA.sameAs(bitmapB));

            // cannot reuse CaptureTextView as 2nd setText call throws NullPointerException
            CaptureTextView cviewA = new CaptureTextView(mContext);
            bitmapA = cviewA.capture(Character.toChars(sComparedCodePoints[i][0]));
            CaptureTextView cviewB = new CaptureTextView(mContext);
            bitmapB = cviewB.capture(Character.toChars(sComparedCodePoints[i][1]));

            bmpDiffMessage = describeBitmap(bitmapA) + "vs" + describeBitmap(bitmapB);
            assertFalse(baseMessage + bmpDiffMessage, bitmapA.sameAs(bitmapB));

            CaptureEditText cedittextA = new CaptureEditText(mContext);
            bitmapA = cedittextA.capture(Character.toChars(sComparedCodePoints[i][0]));
            CaptureEditText cedittextB = new CaptureEditText(mContext);
            bitmapB = cedittextB.capture(Character.toChars(sComparedCodePoints[i][1]));

            bmpDiffMessage = describeBitmap(bitmapA) + "vs" + describeBitmap(bitmapB);
            assertFalse(baseMessage + bmpDiffMessage, bitmapA.sameAs(bitmapB));
        }
    }

    /**
     * Tests Emoji has different glyph for different meaning characters.
     * Test on WebView
     */
    @Test
    public void testEmojiGlyphWebView() {
        if (!NullWebViewUtils.isWebViewAvailable()) {
            return;
        }

        Bitmap bitmapA, bitmapB;  // Emoji displayed Bitmaps to compare

        CaptureWebView cwebview = new CaptureWebView();
        for (int i = 0; i < sComparedCodePoints.length; i++) {
            String baseMessage = "Glyph for U+" + Integer.toHexString(sComparedCodePoints[i][0])
                    + " should be different from glyph for U+"
                    + Integer.toHexString(sComparedCodePoints[i][1]) + ". ";

            bitmapA = cwebview.capture(Character.toChars(sComparedCodePoints[i][0]));
            bitmapB = cwebview.capture(Character.toChars(sComparedCodePoints[i][1]));

            String bmpDiffMessage = describeBitmap(bitmapA) + "vs" + describeBitmap(bitmapB);
            assertFalse(baseMessage + bmpDiffMessage, bitmapA.sameAs(bitmapB));
        }
    }

    /**
     * Tests EditText handles Emoji
     */
    @LargeTest
    @Test
    public void testEmojiEditable() throws Throwable {
        int testedCodePoints[] = {
            0xAE,    // registered mark
            0x2764,    // heavy black heart
            0x1F353    // strawberry - surrogate pair sample. Count as two characters.
        };

        String origStr, newStr;

        // delete Emoji by sending KEYCODE_DEL
        for (int i = 0; i < testedCodePoints.length; i++) {
            origStr = "Test character  ";
            // cannot reuse CaptureTextView as 2nd setText call throws NullPointerException
            mActivityRule.runOnUiThread(() -> mEditText = new EditText(mContext));
            mEditText.setText(origStr + String.valueOf(Character.toChars(testedCodePoints[i])));

            // confirm the emoji is added.
            newStr = mEditText.getText().toString();
            assertEquals(newStr.codePointCount(0, newStr.length()), origStr.length() + 1);

            // Delete added character by sending KEYCODE_DEL event
            mActivityRule.runOnUiThread(() -> mEditText.dispatchKeyEvent(
                    new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL)));
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();

            newStr = mEditText.getText().toString();
            assertEquals(newStr.codePointCount(0, newStr.length()), origStr.length() + 1);
        }
    }

    private static class CaptureCanvas extends View {

        String mTestStr;
        Paint paint = new Paint();

        CaptureCanvas(Context context) {
            super(context);
        }

        public void onDraw(Canvas canvas) {
            if (mTestStr != null) {
                canvas.drawText(mTestStr, 50, 50, paint);
            }
            return;
        }

        Bitmap capture(char c[]) {
            mTestStr = String.valueOf(c);
            invalidate();

            setDrawingCacheEnabled(true);
            measure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                    MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
            layout(0, 0, 200,200);

            Bitmap bitmap = Bitmap.createBitmap(getDrawingCache());
            setDrawingCacheEnabled(false);
            return bitmap;
        }

    }

    private static class CaptureTextView extends TextView {

        CaptureTextView(Context context) {
            super(context);
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        }

        Bitmap capture(char c[]) {
            setText(String.valueOf(c));

            invalidate();

            setDrawingCacheEnabled(true);
            measure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                    MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
            layout(0, 0, 200,200);

            Bitmap bitmap = Bitmap.createBitmap(getDrawingCache());
            setDrawingCacheEnabled(false);
            return bitmap;
        }

    }

    private static class CaptureEditText extends EditText {

        CaptureEditText(Context context) {
            super(context);
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        }

        Bitmap capture(char c[]) {
            setText(String.valueOf(c));

            invalidate();

            setDrawingCacheEnabled(true);
            measure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                    MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
            layout(0, 0, 200,200);

            Bitmap bitmap = Bitmap.createBitmap(getDrawingCache());
            setDrawingCacheEnabled(false);
            return bitmap;
        }

    }


    private static long sRequestId = 0;

    private class CaptureWebView {
        WebViewOnUiThread webViewOnUiThread;

        CaptureWebView() {
            webViewOnUiThread = new WebViewOnUiThread(mActivityRule.getActivity().getWebView());
            // Offscreen pre-raster ensures that visibile region of the WebView is not used to
            // determine which tiles to render, and instead the full WebView size is treated
            // as the visible region.
            webViewOnUiThread.getSettings().setOffscreenPreRaster(true);
        }

        Bitmap capture(char c[]) {
            webViewOnUiThread.loadDataAndWaitForCompletion(
                    "<html><body>" + String.valueOf(c) + "</body></html>",
                    "text/html; charset=utf-8", "utf-8");

            // Wait for the loaded DOM state to be ready to draw.
            final SettableFuture<Void> future = SettableFuture.create();
            webViewOnUiThread.postVisualStateCallback(sRequestId++, new VisualStateCallback() {
                @Override
                public void onComplete(long requestId) {
                    future.set(null);
                }
            });
            try {
                future.get(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS);
            } catch (Exception e) {
                return null;
            }

            Picture picture = webViewOnUiThread.capturePicture();
            if (picture == null || picture.getHeight() <= 0 || picture.getWidth() <= 0) {
                return null;
            }
            Bitmap bitmap = Bitmap.createBitmap(picture.getWidth(), picture.getHeight(),
                    Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            picture.draw(canvas);

            return bitmap;
        }

    }

}

