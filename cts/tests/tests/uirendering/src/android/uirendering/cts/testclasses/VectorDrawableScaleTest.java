/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.uirendering.cts.testclasses;

import android.content.Context;
import android.uirendering.cts.R;
import android.uirendering.cts.bitmapcomparers.MSSIMComparer;
import android.uirendering.cts.bitmapverifiers.GoldenImageVerifier;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;
import android.view.View;
import android.widget.ImageView;

import androidx.test.filters.MediumTest;

import org.junit.Test;

@MediumTest
public class VectorDrawableScaleTest extends ActivityTestBase {
    @Test
    public void testVectorDrawableInImageView() {
        final Context context = getInstrumentation().getTargetContext();
        createTest()
                .addLayout(R.layout.vector_drawable_scale_layout, view-> {
                    setupImageViews(view);
                }, false /* not HW only*/)
                .runWithVerifier(new GoldenImageVerifier(context,
                      R.drawable.vector_drawable_scale_golden,
                      new MSSIMComparer(.87f)));
    }

    // Setup 2 imageviews, one big and one small. The purpose of this test is to make sure that the
    // imageview with smaller scale will not affect the appearance in the imageview with larger
    // scale.
    private static void setupImageViews(View view) {
        ImageView imageView = (ImageView) view.findViewById(R.id.imageview1);
        imageView.setImageResource(R.drawable.vector_icon_create);
        imageView = (ImageView) view.findViewById(R.id.imageview2);
        imageView.setImageResource(R.drawable.vector_icon_create);
    }
}
