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

package com.android.layoutlib.bridge.intensive;

import com.android.ide.common.rendering.api.SessionParams;
import com.android.layoutlib.bridge.intensive.setup.ConfigGenerator;
import com.android.layoutlib.bridge.intensive.setup.LayoutLibTestCallback;
import com.android.layoutlib.bridge.intensive.setup.LayoutPullParser;

import org.junit.After;
import org.junit.Test;

public class HighQualityShadowsRenderTests extends RenderTestBase {
    @After
    public void afterTestCase() {
        com.android.layoutlib.bridge.test.widgets.HookWidget.reset();
    }

    @Test
    public void testHighQualityRectangleShadow() throws Exception {
        LayoutPullParser parser = createParserFromPath("shadows_test.xml");
        LayoutLibTestCallback layoutLibCallback =
                new LayoutLibTestCallback(getLogger(), mDefaultClassLoader);
        layoutLibCallback.initResources();
        SessionParams params = getSessionParamsBuilder()
                .setParser(parser)
                .setConfigGenerator(ConfigGenerator.NEXUS_5)
                .setCallback(layoutLibCallback)
                .disableDecoration()
                .build();

        renderAndVerify(params, "shadows_test_high_quality.png");
        // We expect fidelity warnings for Path.isConvex. Fail for anything else.
        sRenderMessages.removeIf(message -> message.equals("Path.isConvex is not supported."));
        sRenderMessages.removeIf(message -> message.equals("Font$Builder.nAddAxis is not supported."));
    }

    @Test
    public void testRoundedEdgeRectangle() throws Exception {
        LayoutPullParser parser = createParserFromPath("shadows_rounded_edge_test.xml");
        LayoutLibTestCallback layoutLibCallback =
                new LayoutLibTestCallback(getLogger(), mDefaultClassLoader);
        layoutLibCallback.initResources();
        SessionParams params = getSessionParamsBuilder()
                .setParser(parser)
                .setConfigGenerator(ConfigGenerator.NEXUS_5)
                .setCallback(layoutLibCallback)
                .build();

        renderAndVerify(params, "shadows_test_high_quality_rounded_edge.png");
        // We expect fidelity warnings for Path.isConvex. Fail for anything else.
        sRenderMessages.removeIf(message -> message.equals("Path.isConvex is not supported."));
        sRenderMessages.removeIf(message -> message.equals("Font$Builder.nAddAxis is not supported."));
    }

    @Test
    public void testLargeView() throws Exception {
        LayoutPullParser parser = createParserFromPath("large_view_shadows_test.xml");
        LayoutLibTestCallback layoutLibCallback =
                new LayoutLibTestCallback(getLogger(), mDefaultClassLoader);
        layoutLibCallback.initResources();
        SessionParams params = getSessionParamsBuilder()
                .setParser(parser)
                .setConfigGenerator(ConfigGenerator.NEXUS_5)
                .setCallback(layoutLibCallback)
                .disableDecoration()
                .build();

        renderAndVerify(params, "large_shadows_test_high_quality.png");
        // We expect fidelity warnings for Path.isConvex. Fail for anything else.
        sRenderMessages.removeIf(message -> message.equals("Path.isConvex is not supported."));
    }

    @Test
    public void testShadowSizes() throws Exception {
        LayoutPullParser parser = createParserFromPath("shadow_sizes_test.xml");
        LayoutLibTestCallback layoutLibCallback =
                new LayoutLibTestCallback(getLogger(), mDefaultClassLoader);
        layoutLibCallback.initResources();
        SessionParams params = getSessionParamsBuilder()
                .setParser(parser)
                .setConfigGenerator(ConfigGenerator.NEXUS_5)
                .setCallback(layoutLibCallback)
                .build();

        renderAndVerify(params, "shadow_sizes_test_high_quality.png");
        // We expect fidelity warnings for Path.isConvex. Fail for anything else.
        sRenderMessages.removeIf(message -> message.equals("Path.isConvex is not supported."));
    }

    @Test
    public void testWidgetWithScroll() throws Exception {
        LayoutPullParser parser = createParserFromPath("shadows_scrollview.xml");
        LayoutLibTestCallback layoutLibCallback =
                new LayoutLibTestCallback(getLogger(), mDefaultClassLoader);
        layoutLibCallback.initResources();
        SessionParams params = getSessionParamsBuilder()
                .setParser(parser)
                .setCallback(layoutLibCallback)
                .build();

        renderAndVerify(params, "shadow_scrollview_test_high_quality.png");
        // We expect fidelity warnings for Path.isConvex. Fail for anything else.
        sRenderMessages.removeIf(message -> message.equals("Path.isConvex is not supported."));
        sRenderMessages.removeIf(message -> message.equals("Font$Builder.nAddAxis is not supported."));
    }
}
