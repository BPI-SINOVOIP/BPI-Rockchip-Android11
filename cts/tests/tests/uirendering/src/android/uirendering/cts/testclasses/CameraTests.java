/*
 * Copyright (C) 2018 The Android Open Source Project
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

import android.graphics.Camera;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.uirendering.cts.bitmapverifiers.RectVerifier;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class CameraTests extends ActivityTestBase {
    @Test
    public void testBasicTranslate() {
        createTest()
                .addCanvasClient((canvas, width, height) -> {
                    Paint paint = new Paint();
                    paint.setAntiAlias(false);
                    paint.setColor(Color.BLUE);
                    Camera camera = new Camera();
                    camera.translate(0, 50, 0);
                    camera.applyToCanvas(canvas);
                    canvas.drawRect(0, 50, 100, 100, paint);
                })
                .runWithVerifier(new RectVerifier(Color.WHITE, Color.BLUE,
                        new Rect(0, 0, 100, 50)));
    }
}
