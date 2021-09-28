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

package com.android.server.wm.flicker;

import static com.android.server.wm.flicker.LayersTraceSubject.assertThat;
import static com.android.server.wm.flicker.TestFileUtils.readTestFile;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.fail;

import android.graphics.Rect;

import androidx.test.runner.AndroidJUnit4;

import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

import java.nio.file.Paths;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Contains {@link LayersTraceSubject} tests. To run this test: {@code atest
 * FlickerLibTest:LayersTraceSubjectTest}
 */
@RunWith(AndroidJUnit4.class)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class LayersTraceSubjectTest {
    private static final Rect displayRect = new Rect(0, 0, 1440, 2880);

    private static LayersTrace readLayerTraceFromFile(String relativePath) {
        try {
            return LayersTrace.parseFrom(readTestFile(relativePath), Paths.get(relativePath));
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Test
    public void testCanDetectEmptyRegionFromLayerTrace() {
        LayersTrace layersTraceEntries = readLayerTraceFromFile("layers_trace_emptyregion.pb");
        try {
            assertThat(layersTraceEntries).coversRegion(displayRect).forAllEntries();
            fail("Assertion passed");
        } catch (AssertionError e) {
            assertWithMessage("Contains path to trace")
                    .that(e.getMessage())
                    .contains("layers_trace_emptyregion.pb");
            assertWithMessage("Contains timestamp").that(e.getMessage()).contains("0h38m28s8ms");
            assertWithMessage("Contains assertion function")
                    .that(e.getMessage())
                    .contains("coversRegion");
            assertWithMessage("Contains debug info")
                    .that(e.getMessage())
                    .contains("Region to test: " + displayRect);
            assertWithMessage("Contains debug info")
                    .that(e.getMessage())
                    .contains("first empty point: 0, 99");
        }
    }

    @Test
    public void testCanDetectIncorrectVisibilityFromLayerTrace() {
        LayersTrace layersTraceEntries =
                readLayerTraceFromFile("layers_trace_invalid_layer_visibility.pb");
        try {
            assertThat(layersTraceEntries)
                    .showsLayer("com.android.server.wm.flicker.testapp")
                    .then()
                    .hidesLayer("com.android.server.wm.flicker.testapp")
                    .forAllEntries();
            fail("Assertion passed");
        } catch (AssertionError e) {
            assertWithMessage("Contains path to trace")
                    .that(e.getMessage())
                    .contains("layers_trace_invalid_layer_visibility.pb");
            assertWithMessage("Contains timestamp")
                    .that(e.getMessage())
                    .contains("2d22h13m14s303ms");
            assertWithMessage("Contains assertion function")
                    .that(e.getMessage())
                    .contains("!isVisible");
            assertWithMessage("Contains debug info")
                    .that(e.getMessage())
                    .contains(
                            "com.android.server.wm.flicker.testapp/com.android.server.wm.flicker.testapp"
                                    + ".SimpleActivity#0 is visible");
        }
    }

    private void detectRootLayer(String fileName) {
        LayersTrace layersTrace = readLayerTraceFromFile(fileName);

        for (LayersTrace.Entry entry : layersTrace.getEntries()) {
            List<LayersTrace.Layer> flattened = entry.asFlattenedLayers();
            List<LayersTrace.Layer> rootLayers =
                    flattened
                            .stream()
                            .filter(LayersTrace.Layer::isRootLayer)
                            .collect(Collectors.toList());

            assertWithMessage("Does not have any root layer")
                    .that(rootLayers.size())
                    .isGreaterThan(0);

            int firstParentId = rootLayers.get(0).getParentId();

            assertWithMessage("Has multiple root layers")
                    .that(rootLayers.stream().allMatch(p -> p.getParentId() == firstParentId))
                    .isTrue();
        }
    }

    @Test
    public void testCanDetectRootLayer() {
        detectRootLayer("layers_trace_root.pb");
    }

    @Test
    public void testCanDetectRootLayerAOSP() {
        detectRootLayer("layers_trace_root_aosp.pb");
    }
}
