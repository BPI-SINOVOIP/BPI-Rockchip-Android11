/*
 * Copyright (C) 2021 The Android Open Source Project
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

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.platform.test.annotations.AppModeFull;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.RemoteViews;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@AppModeFull
@SmallTest
@RunWith(AndroidJUnit4.class)
public class FontResourceTest {

    private static final int REMOTE_FONT_TEXT_WIDTH = 900;
    private static final String RESOURCE_PACKAGE = "android.text.cts.resources";

    private int getLayoutId() {
        Context context = InstrumentationRegistry.getTargetContext();
        int resID = 0;
        try {
            resID = context.createPackageContext(RESOURCE_PACKAGE, 0)
                    .getResources().getIdentifier(
                            "textview_layout",
                            "layout",
                            RESOURCE_PACKAGE);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        assertThat(resID).isNotEqualTo(0);
        return resID;
    }

    private TextView inflateWithRemoteViews(Context ctx) {
        RemoteViews remoteViews = new RemoteViews(RESOURCE_PACKAGE, getLayoutId());
        return (TextView) remoteViews.apply(ctx, null);
    }

    private TextView inflateWithInflator(Context ctx) {
        LayoutInflater inflater = LayoutInflater.from(ctx);
        return (TextView) inflater.inflate(getLayoutId(), null);
    }

    private int measureText(TextView textView) {
        textView.setTextSize(TypedValue.COMPLEX_UNIT_PX, 100f);  // make 1em = 100px
        textView.measure(
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED),
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
        return textView.getLayout().getWidth();
    }

    @Test
    public void testRemoteResource() throws Exception {
        Context context = InstrumentationRegistry.getTargetContext();

        Context freeContext = context.createPackageContext(
                RESOURCE_PACKAGE, Context.CONTEXT_IGNORE_SECURITY);
        Context restrictContext = context.createPackageContext(
                RESOURCE_PACKAGE, Context.CONTEXT_RESTRICTED);

        // This expectation is for verifying the precondition of the test case. If the context
        // ignores the security, loads the custom font and TextView gives the width with it. If the
        // context is restricted, the custom font should not be loaded and TextView gives the width
        // different from the one with the custom font.
        // The custom font has 3em for "a" character. The text is "aaa", then 9em = 900px is the
        // expected width.
        assertThat(measureText(inflateWithInflator(freeContext)))
                .isEqualTo(REMOTE_FONT_TEXT_WIDTH);
        assertThat(measureText(inflateWithInflator(restrictContext)))
                .isNotEqualTo(REMOTE_FONT_TEXT_WIDTH);

        // The RemoteView should ignore the custom font files.
        assertThat(measureText(inflateWithRemoteViews(context)))
                .isNotEqualTo(REMOTE_FONT_TEXT_WIDTH);
    }
}
