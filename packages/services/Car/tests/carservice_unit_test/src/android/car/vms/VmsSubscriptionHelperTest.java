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

package android.car.vms;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import static java.util.Collections.emptySet;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class VmsSubscriptionHelperTest {
    private static final VmsLayer LAYER1 = new VmsLayer(1, 1, 1);
    private static final VmsLayer LAYER2 = new VmsLayer(2, 1, 1);
    private static final VmsLayer LAYER3 = new VmsLayer(3, 1, 1);

    private static final int PROVIDER_ID1 = 12345;
    private static final int PROVIDER_ID2 = 54321;
    private static final int PROVIDER_ID3 = 99999;

    private final VmsSubscriptionHelper mSubscriptionHelper =
            new VmsSubscriptionHelper(this::handleUpdate);

    private Set<VmsAssociatedLayer> mSubscriptionUpdate;
    private int mSubscriptionUpdateCount;
    private boolean mUpdateThrowsException;

    @Test
    public void testSubscribe_SingleLayer() {
        mSubscriptionHelper.subscribe(LAYER1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(1);
        assertSubscriptions(new VmsAssociatedLayer(LAYER1, emptySet()));
    }

    @Test
    public void testSubscribe_SingleLayer_IgnoreDuplicates() {
        mSubscriptionHelper.subscribe(LAYER1);
        mSubscriptionHelper.subscribe(LAYER1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(1);
        assertSubscriptions(new VmsAssociatedLayer(LAYER1, emptySet()));
    }

    @Test
    public void testSubscribe_SingleLayer_RetryAfterException() {
        mUpdateThrowsException = true;
        assertThrows(
                UpdateHandlerException.class,
                () -> mSubscriptionHelper.subscribe(LAYER1));

        mUpdateThrowsException = false;
        mSubscriptionHelper.subscribe(LAYER1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(2);
        assertSubscriptions(new VmsAssociatedLayer(LAYER1, emptySet()));
    }

    @Test
    public void testUnsubscribe_SingleLayer() {
        mSubscriptionHelper.subscribe(LAYER1);
        mSubscriptionHelper.unsubscribe(LAYER1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(2);
        assertEmptySubscriptions();
    }

    @Test
    public void testUnsubscribe_SingleLayer_IgnoreUnknown() {
        mSubscriptionHelper.subscribe(LAYER1);
        mSubscriptionHelper.unsubscribe(LAYER2);

        assertThat(mSubscriptionUpdateCount).isEqualTo(1);
        assertSubscriptions(new VmsAssociatedLayer(LAYER1, emptySet()));
    }

    @Test
    public void testUnsubscribe_SingleLayer_RetryAfterException() {
        mSubscriptionHelper.subscribe(LAYER1);
        mUpdateThrowsException = true;
        assertThrows(
                UpdateHandlerException.class,
                () -> mSubscriptionHelper.unsubscribe(LAYER1));

        mUpdateThrowsException = false;
        mSubscriptionHelper.unsubscribe(LAYER1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(3);
        assertEmptySubscriptions();
    }

    @Test
    public void testSubscribe_MultipleLayers() {
        mSubscriptionHelper.subscribe(LAYER1);
        mSubscriptionHelper.subscribe(LAYER2);
        mSubscriptionHelper.subscribe(LAYER3);

        assertThat(mSubscriptionUpdateCount).isEqualTo(3);
        assertSubscriptions(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER3, emptySet()));
    }

    @Test
    public void testUnsubscribe_MultipleLayers() {
        mSubscriptionHelper.subscribe(LAYER1);
        mSubscriptionHelper.subscribe(LAYER2);
        mSubscriptionHelper.subscribe(LAYER3);
        mSubscriptionHelper.unsubscribe(LAYER2);

        assertThat(mSubscriptionUpdateCount).isEqualTo(4);
        assertSubscriptions(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER3, emptySet()));
    }

    @Test
    public void testSubscribe_SingleLayerAndProvider() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(1);
        assertSubscriptions(new VmsAssociatedLayer(LAYER1, asSet(PROVIDER_ID1)));
    }

    @Test
    public void testSubscribe_SingleLayerAndProvider_IgnoreDuplicates() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(1);
        assertSubscriptions(new VmsAssociatedLayer(LAYER1, asSet(PROVIDER_ID1)));
    }

    @Test
    public void testSubscribe_SingleLayerAndProvider_RetryAfterException() {
        mUpdateThrowsException = true;
        assertThrows(
                UpdateHandlerException.class,
                () -> mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1));

        mUpdateThrowsException = false;
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(2);
        assertSubscriptions(new VmsAssociatedLayer(LAYER1, asSet(PROVIDER_ID1)));
    }

    @Test
    public void testUnsubscribe_SingleLayerAndProvider() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);
        mSubscriptionHelper.unsubscribe(LAYER1, PROVIDER_ID1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(2);
        assertEmptySubscriptions();
    }

    @Test
    public void testUnsubscribe_SingleLayerAndProvider_IgnoreUnknown() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);
        mSubscriptionHelper.unsubscribe(LAYER1, PROVIDER_ID2);

        assertThat(mSubscriptionUpdateCount).isEqualTo(1);
        assertSubscriptions(new VmsAssociatedLayer(LAYER1, asSet(PROVIDER_ID1)));
    }

    @Test
    public void testUnubscribe_SingleLayerAndProvider_RetryAfterException() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);
        mUpdateThrowsException = true;
        assertThrows(
                UpdateHandlerException.class,
                () -> mSubscriptionHelper.unsubscribe(LAYER1, PROVIDER_ID1));

        mUpdateThrowsException = false;
        mSubscriptionHelper.unsubscribe(LAYER1, PROVIDER_ID1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(3);
        assertEmptySubscriptions();
    }

    @Test
    public void testSubscribe_SingleLayerAndMultipleProviders() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID2);
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID3);

        assertThat(mSubscriptionUpdateCount).isEqualTo(3);
        assertSubscriptions(
                new VmsAssociatedLayer(LAYER1, asSet(PROVIDER_ID1, PROVIDER_ID2, PROVIDER_ID3)));
    }

    @Test
    public void testUnsubscribe_SingleLayerAndMultipleProviders() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID2);
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID3);
        mSubscriptionHelper.unsubscribe(LAYER1, PROVIDER_ID2);

        assertThat(mSubscriptionUpdateCount).isEqualTo(4);
        assertSubscriptions(new VmsAssociatedLayer(LAYER1, asSet(PROVIDER_ID1, PROVIDER_ID3)));
    }

    @Test
    public void testSubscribe_MultipleLayersAndProvider() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);
        mSubscriptionHelper.subscribe(LAYER2, PROVIDER_ID1);
        mSubscriptionHelper.subscribe(LAYER3, PROVIDER_ID1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(3);
        assertSubscriptions(
                new VmsAssociatedLayer(LAYER1, asSet(PROVIDER_ID1)),
                new VmsAssociatedLayer(LAYER2, asSet(PROVIDER_ID1)),
                new VmsAssociatedLayer(LAYER3, asSet(PROVIDER_ID1)));
    }

    @Test
    public void testUnsubscribe_MultipleLayersAndProvider() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);
        mSubscriptionHelper.subscribe(LAYER2, PROVIDER_ID1);
        mSubscriptionHelper.subscribe(LAYER3, PROVIDER_ID1);
        mSubscriptionHelper.unsubscribe(LAYER2, PROVIDER_ID1);

        assertThat(mSubscriptionUpdateCount).isEqualTo(4);
        assertSubscriptions(
                new VmsAssociatedLayer(LAYER1, asSet(PROVIDER_ID1)),
                new VmsAssociatedLayer(LAYER3, asSet(PROVIDER_ID1)));
    }

    @Test
    public void testSubscribe_MultipleLayersAndMultipleProviders() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID2);
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID3);
        mSubscriptionHelper.subscribe(LAYER2, PROVIDER_ID2);
        mSubscriptionHelper.subscribe(LAYER3, PROVIDER_ID3);

        assertThat(mSubscriptionUpdateCount).isEqualTo(5);
        assertSubscriptions(
                new VmsAssociatedLayer(LAYER1, asSet(PROVIDER_ID1, PROVIDER_ID2, PROVIDER_ID3)),
                new VmsAssociatedLayer(LAYER2, asSet(PROVIDER_ID2)),
                new VmsAssociatedLayer(LAYER3, asSet(PROVIDER_ID3))
        );
    }

    @Test
    public void testUnsubscribe_MultipleLayersAndMultipleProviders() {
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID1);
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID2);
        mSubscriptionHelper.subscribe(LAYER1, PROVIDER_ID3);
        mSubscriptionHelper.subscribe(LAYER2, PROVIDER_ID2);
        mSubscriptionHelper.subscribe(LAYER3, PROVIDER_ID3);
        mSubscriptionHelper.unsubscribe(LAYER1, PROVIDER_ID2);
        mSubscriptionHelper.unsubscribe(LAYER3, PROVIDER_ID3);

        assertThat(mSubscriptionUpdateCount).isEqualTo(7);
        assertSubscriptions(
                new VmsAssociatedLayer(LAYER1, asSet(PROVIDER_ID1, PROVIDER_ID3)),
                new VmsAssociatedLayer(LAYER2, asSet(PROVIDER_ID2)));
    }

    private void handleUpdate(Set<VmsAssociatedLayer> subscriptionUpdate) {
        mSubscriptionUpdate = subscriptionUpdate;
        mSubscriptionUpdateCount++;
        if (mUpdateThrowsException) {
            throw new UpdateHandlerException();
        }
    }

    private void assertEmptySubscriptions() {
        assertSubscriptions();
    }

    private void assertSubscriptions(VmsAssociatedLayer... associatedLayers) {
        Set<VmsAssociatedLayer> subscriptions = asSet(associatedLayers);
        assertThat(mSubscriptionUpdate).isEqualTo(subscriptions);
        assertThat(mSubscriptionHelper.getSubscriptions()).isEqualTo(subscriptions);
    }

    @SafeVarargs
    private static <T> Set<T> asSet(T... values) {
        return new HashSet<>(Arrays.asList(values));
    }

    private static class UpdateHandlerException extends RuntimeException {}
}
