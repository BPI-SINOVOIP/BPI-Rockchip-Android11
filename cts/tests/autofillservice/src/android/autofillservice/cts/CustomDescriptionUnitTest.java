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

package android.autofillservice.cts;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.testng.Assert.assertThrows;

import android.platform.test.annotations.AppModeFull;
import android.service.autofill.BatchUpdates;
import android.service.autofill.CustomDescription;
import android.service.autofill.InternalOnClickAction;
import android.service.autofill.InternalTransformation;
import android.service.autofill.InternalValidator;
import android.service.autofill.OnClickAction;
import android.service.autofill.Transformation;
import android.service.autofill.Validator;
import android.util.SparseArray;
import android.widget.RemoteViews;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@AppModeFull(reason = "Unit test")
public class CustomDescriptionUnitTest {

    private final CustomDescription.Builder mBuilder =
            new CustomDescription.Builder(mock(RemoteViews.class));
    private final BatchUpdates mValidUpdate =
            new BatchUpdates.Builder().updateTemplate(mock(RemoteViews.class)).build();
    private final Transformation mValidTransformation = mock(InternalTransformation.class);
    private final Validator mValidCondition = mock(InternalValidator.class);
    private final OnClickAction mValidAction = mock(InternalOnClickAction.class);

    @Test
    public void testNullConstructor() {
        assertThrows(NullPointerException.class, () ->  new CustomDescription.Builder(null));
    }

    @Test
    public void testAddChild_null() {
        assertThrows(IllegalArgumentException.class, () ->  mBuilder.addChild(42, null));
    }

    @Test
    public void testAddChild_invalidImplementation() {
        assertThrows(IllegalArgumentException.class,
                () ->  mBuilder.addChild(42, mock(Transformation.class)));
    }

    @Test
    public void testBatchUpdate_nullCondition() {
        assertThrows(IllegalArgumentException.class,
                () ->  mBuilder.batchUpdate(null, mValidUpdate));
    }

    @Test
    public void testBatchUpdate_invalidImplementation() {
        assertThrows(IllegalArgumentException.class,
                () ->  mBuilder.batchUpdate(mock(Validator.class), mValidUpdate));
    }

    @Test
    public void testBatchUpdate_nullUpdates() {
        assertThrows(NullPointerException.class,
                () ->  mBuilder.batchUpdate(mValidCondition, null));
    }

    @Test
    public void testSetOnClickAction_null() {
        assertThrows(IllegalArgumentException.class, () ->  mBuilder.addOnClickAction(42, null));
    }

    @Test
    public void testSetOnClickAction_invalidImplementation() {
        assertThrows(IllegalArgumentException.class,
                () -> mBuilder.addOnClickAction(42, mock(OnClickAction.class)));
    }

    @Test
    public void testSetOnClickAction_thereCanBeOnlyOne() {
        final CustomDescription customDescription = mBuilder
                .addOnClickAction(42, mock(InternalOnClickAction.class))
                .addOnClickAction(42, mValidAction)
                .build();
        final SparseArray<InternalOnClickAction> actions = customDescription.getActions();
        assertThat(actions.size()).isEqualTo(1);
        assertThat(actions.keyAt(0)).isEqualTo(42);
        assertThat(actions.valueAt(0)).isSameAs(mValidAction);
    }

    @Test
    public void testBuild_valid() {
        new CustomDescription.Builder(mock(RemoteViews.class)).build();
        new CustomDescription.Builder(mock(RemoteViews.class))
            .addChild(108, mValidTransformation)
            .batchUpdate(mValidCondition, mValidUpdate)
            .addOnClickAction(42, mValidAction)
            .build();
    }

    @Test
    public void testNoMoreInteractionsAfterBuild() {
        mBuilder.build();

        assertThrows(IllegalStateException.class, () -> mBuilder.build());
        assertThrows(IllegalStateException.class,
                () -> mBuilder.addChild(108, mValidTransformation));
        assertThrows(IllegalStateException.class,
                () -> mBuilder.batchUpdate(mValidCondition, mValidUpdate));
        assertThrows(IllegalStateException.class,
                () -> mBuilder.addOnClickAction(42, mValidAction));
    }
}
