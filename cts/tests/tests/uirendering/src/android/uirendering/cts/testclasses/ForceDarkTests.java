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

import android.app.UiModeManager;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.uirendering.cts.R;
import android.uirendering.cts.bitmapverifiers.RectVerifier;
import android.uirendering.cts.testinfrastructure.ActivityTestBase;
import android.uirendering.cts.testinfrastructure.CanvasClientView;
import android.uirendering.cts.testinfrastructure.ViewInitializer;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class ForceDarkTests extends ActivityTestBase {
    static int sPreviousUiMode = UiModeManager.MODE_NIGHT_NO;

    @BeforeClass
    public static void enableForceDark() {
        // Temporarily override the ui mode
        UiModeManager uiManager = (UiModeManager)
                InstrumentationRegistry.getContext().getSystemService(Context.UI_MODE_SERVICE);
        sPreviousUiMode = uiManager.getNightMode();
        if (sPreviousUiMode != UiModeManager.MODE_NIGHT_YES) {
            SystemUtil.runShellCommand("service call uimode 4 i32 2");
        }
    }

    @AfterClass
    public static void restoreForceDarkSetting() {
        if (sPreviousUiMode != UiModeManager.MODE_NIGHT_YES) {
            SystemUtil.runShellCommand("service call uimode 4 i32 " + sPreviousUiMode);
        }
    }

    @Override
    protected boolean useForceDark() {
        return true;
    }

    @Test
    public void testFgRect() {
        final Rect rect = new Rect(10, 10, 80, 80);
        createTest()
                .addLayout(R.layout.simple_force_dark, (ViewInitializer) view -> {
                    Assert.assertTrue(view.isForceDarkAllowed());
                    ((CanvasClientView) view).setCanvasClient((canvas, width, height) -> {
                        Paint p = new Paint();
                        p.setAntiAlias(false);
                        p.setColor(Color.BLACK);
                        canvas.drawRect(rect, p);
                    });
                }, true)
                .runWithVerifier(new RectVerifier(Color.BLACK, Color.WHITE, rect, 100));
    }

    @Test
    public void testFgRectDisable() {
        final Rect rect = new Rect(10, 10, 80, 80);
        createTest()
                .addLayout(R.layout.simple_force_dark, (ViewInitializer) view -> {
                    view.setForceDarkAllowed(false);
                    Assert.assertFalse(view.isForceDarkAllowed());
                    ((CanvasClientView) view).setCanvasClient((canvas, width, height) -> {
                        Paint p = new Paint();
                        p.setAntiAlias(false);
                        p.setColor(Color.BLACK);
                        canvas.drawRect(rect, p);
                    });
                }, true)
                .runWithVerifier(new RectVerifier(Color.WHITE, Color.BLACK, rect, 0));
    }

    @Test
    public void testSiblings() {
        final Rect fgRect = new Rect(10, 10, 80, 80);
        createTest()
                .addLayout(R.layout.force_dark_siblings, (ViewInitializer) view -> {

                    CanvasClientView bg = view.findViewById(R.id.bg_canvas);
                    CanvasClientView fg = view.findViewById(R.id.fg_canvas);

                    bg.setCanvasClient((canvas, width, height) -> {
                        canvas.drawColor(Color.WHITE);
                    });

                    fg.setCanvasClient((canvas, width, height) -> {
                        Paint p = new Paint();
                        p.setAntiAlias(false);
                        p.setColor(Color.BLACK);
                        canvas.drawRect(fgRect, p);
                    });
                }, true)
                .runWithVerifier(new RectVerifier(Color.BLACK, Color.WHITE, fgRect, 100));
    }
}
