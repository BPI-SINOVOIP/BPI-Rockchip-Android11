/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.vms;

import android.car.vms.VmsAssociatedLayer;
import android.car.vms.VmsLayer;
import android.car.vms.VmsLayerDependency;
import android.car.vms.VmsLayersOffering;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

@SmallTest
public class VmsLayerAvailabilityTest extends AndroidTestCase {

    private static final VmsLayer LAYER_X = new VmsLayer(1, 1, 2);
    private static final VmsLayer LAYER_Y = new VmsLayer(3, 2, 4);
    private static final VmsLayer LAYER_Z = new VmsLayer(5, 3, 6);

    private static final int PUBLISHER_ID_1 = 19;
    private static final int PUBLISHER_ID_2 = 28;

    private static final Set<Integer> PUBLISHERS_1 = Collections.singleton(PUBLISHER_ID_1);
    private static final Set<Integer> PUBLISHERS_2 = Collections.singleton(PUBLISHER_ID_2);
    private static final Set<Integer> PUBLISHERS_1_AND_2 =
            new HashSet<>(Arrays.asList(PUBLISHER_ID_1, PUBLISHER_ID_2));

    private static final VmsLayerDependency X_DEPENDS_ON_Y =
            new VmsLayerDependency(LAYER_X, Collections.singleton(LAYER_Y));

    private static final VmsLayerDependency X_DEPENDS_ON_Z =
            new VmsLayerDependency(LAYER_X, Collections.singleton(LAYER_Z));

    private static final VmsLayerDependency Y_DEPENDS_ON_Z =
            new VmsLayerDependency(LAYER_Y, Collections.singleton(LAYER_Z));

    private static final VmsLayerDependency Y_DEPENDS_ON_X =
            new VmsLayerDependency(LAYER_Y, Collections.singleton(LAYER_X));

    private static final VmsLayerDependency Z_DEPENDS_ON_X =
            new VmsLayerDependency(LAYER_Z, Collections.singleton(LAYER_X));

    private static final VmsLayerDependency Z_DEPENDS_ON_NOTHING =
            new VmsLayerDependency(LAYER_Z);

    private static final VmsLayerDependency X_DEPENDS_ON_SELF =
            new VmsLayerDependency(LAYER_X, Collections.singleton(LAYER_X));

    private Set<VmsLayersOffering> mOfferings;
    private VmsLayerAvailability mLayersAvailability;

    @Override
    protected void setUp() throws Exception {
        mLayersAvailability = new VmsLayerAvailability();
        mOfferings = new HashSet<>();
        super.setUp();
    }

    public void testNoOffering() {
        assertTrue(mLayersAvailability.getAvailableLayers().getAssociatedLayers().isEmpty());
    }

    public void testEmptyOffering() {
        mLayersAvailability.setPublishersOffering(Collections.emptyList());
        assertTrue(mLayersAvailability.getAvailableLayers().getAssociatedLayers().isEmpty());
    }

    public void testSingleLayerNoDeps() {
        Set<VmsAssociatedLayer> expectedAvailableAssociatedLayers = new HashSet<>();
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_X, PUBLISHERS_2));

        VmsLayersOffering offering =
                new VmsLayersOffering(Collections.singleton(new VmsLayerDependency(LAYER_X)),
                        PUBLISHER_ID_2);

        mOfferings.add(offering);
        mLayersAvailability.setPublishersOffering(mOfferings);

        assertEquals(expectedAvailableAssociatedLayers,
                mLayersAvailability.getAvailableLayers().getAssociatedLayers());
    }

    public void testChainOfDependenciesSatisfied() {
        Set<VmsAssociatedLayer> expectedAvailableAssociatedLayers = new HashSet<>();
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_X, PUBLISHERS_1));
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_Y, PUBLISHERS_1));
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_Z, PUBLISHERS_1));

        VmsLayersOffering offering =
                new VmsLayersOffering(
                        new HashSet<>(Arrays.asList(
                                X_DEPENDS_ON_Y, Y_DEPENDS_ON_Z, Z_DEPENDS_ON_NOTHING)),
                        PUBLISHER_ID_1);

        mOfferings.add(offering);
        mLayersAvailability.setPublishersOffering(mOfferings);

        assertEquals(expectedAvailableAssociatedLayers,
                new HashSet<>(
                        mLayersAvailability.getAvailableLayers().getAssociatedLayers()));
    }

    public void testChainOfDependenciesSatisfiedTwoOfferings() {
        Set<VmsAssociatedLayer> expectedAvailableAssociatedLayers = new HashSet<>();
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_X, PUBLISHERS_1));
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_Y, PUBLISHERS_1));
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_Z, PUBLISHERS_1));

        VmsLayersOffering offering1 =
                new VmsLayersOffering(
                        new HashSet<>(Arrays.asList(X_DEPENDS_ON_Y, Y_DEPENDS_ON_Z)),
                        PUBLISHER_ID_1);

        VmsLayersOffering offering2 =
                new VmsLayersOffering(Collections.singleton(Z_DEPENDS_ON_NOTHING),
                        PUBLISHER_ID_1);

        mOfferings.add(offering1);
        mOfferings.add(offering2);
        mLayersAvailability.setPublishersOffering(mOfferings);

        assertEquals(expectedAvailableAssociatedLayers,
                new HashSet<>(
                        mLayersAvailability.getAvailableLayers().getAssociatedLayers()));
    }

    public void testChainOfDependenciesNotSatisfied() {
        Set<VmsAssociatedLayer> expectedAvailableAssociatedLayers = new HashSet<>();
        VmsLayersOffering offering =
                new VmsLayersOffering(new HashSet<>(Arrays.asList(X_DEPENDS_ON_Y, Y_DEPENDS_ON_Z)),
                        PUBLISHER_ID_1);

        mOfferings.add(offering);
        mLayersAvailability.setPublishersOffering(mOfferings);

        assertEquals(expectedAvailableAssociatedLayers,
                new HashSet<>(
                        mLayersAvailability.getAvailableLayers().getAssociatedLayers()));
    }

    public void testOneOfMultipleDependencySatisfied() {
        Set<VmsAssociatedLayer> expectedAvailableAssociatedLayers = new HashSet<>();
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_X, PUBLISHERS_1));
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_Z, PUBLISHERS_1));


        VmsLayersOffering offering =
                new VmsLayersOffering(
                        new HashSet<>(Arrays.asList(
                                X_DEPENDS_ON_Y, X_DEPENDS_ON_Z, Z_DEPENDS_ON_NOTHING)),
                        PUBLISHER_ID_1);

        mOfferings.add(offering);
        mLayersAvailability.setPublishersOffering(mOfferings);

        assertEquals(expectedAvailableAssociatedLayers,
                new HashSet<>(
                        mLayersAvailability.getAvailableLayers().getAssociatedLayers()));
    }

    public void testCyclicDependency() {
        Set<VmsAssociatedLayer> expectedAvailableAssociatedLayers = new HashSet<>();

        VmsLayersOffering offering =
                new VmsLayersOffering(
                        new HashSet<>(
                                Arrays.asList(X_DEPENDS_ON_Y, Y_DEPENDS_ON_Z, Z_DEPENDS_ON_X)),
                        PUBLISHER_ID_1);

        mOfferings.add(offering);
        mLayersAvailability.setPublishersOffering(mOfferings);

        assertEquals(expectedAvailableAssociatedLayers,
                new HashSet<>(
                        mLayersAvailability.getAvailableLayers().getAssociatedLayers()));
    }

    public void testAlmostCyclicDependency() {
        Set<VmsAssociatedLayer> expectedAvailableAssociatedLayers = new HashSet<>();
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_Z, PUBLISHERS_1_AND_2));
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_X, PUBLISHERS_1));
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_Y, PUBLISHERS_2));

        VmsLayersOffering offering1 =
                new VmsLayersOffering(
                        new HashSet<>(Arrays.asList(X_DEPENDS_ON_Y, Z_DEPENDS_ON_NOTHING)),
                        PUBLISHER_ID_1);

        VmsLayersOffering offering2 =
                new VmsLayersOffering(new HashSet<>(Arrays.asList(Y_DEPENDS_ON_Z, Z_DEPENDS_ON_X)),
                        PUBLISHER_ID_2);

        mOfferings.add(offering1);
        mOfferings.add(offering2);
        mLayersAvailability.setPublishersOffering(mOfferings);

        assertEquals(expectedAvailableAssociatedLayers,
                mLayersAvailability.getAvailableLayers().getAssociatedLayers());
    }

    public void testCyclicDependencyAndLayerWithoutDependency() {
        Set<VmsAssociatedLayer> expectedAvailableAssociatedLayers = new HashSet<>();
        expectedAvailableAssociatedLayers.add(new VmsAssociatedLayer(LAYER_Z, PUBLISHERS_1));

        VmsLayersOffering offering1 =
                new VmsLayersOffering(
                        new HashSet<>(
                                Arrays.asList(X_DEPENDS_ON_Y, Z_DEPENDS_ON_NOTHING)),
                        PUBLISHER_ID_1);

        VmsLayersOffering offering2 =
                new VmsLayersOffering(Collections.singleton(Y_DEPENDS_ON_X), PUBLISHER_ID_2);

        mOfferings.add(offering1);
        mOfferings.add(offering2);
        mLayersAvailability.setPublishersOffering(mOfferings);

        assertEquals(expectedAvailableAssociatedLayers,
                new HashSet<>(
                        mLayersAvailability.getAvailableLayers().getAssociatedLayers()));
    }

    public void testSelfDependency() {
        Set<VmsAssociatedLayer> expectedAvailableAssociatedLayers = new HashSet<>();

        VmsLayersOffering offering =
                new VmsLayersOffering(Collections.singleton(X_DEPENDS_ON_SELF),
                        PUBLISHER_ID_1);

        mOfferings.add(offering);
        mLayersAvailability.setPublishersOffering(mOfferings);

        assertEquals(expectedAvailableAssociatedLayers,
                new HashSet<>(
                        mLayersAvailability.getAvailableLayers().getAssociatedLayers()));
    }
}
