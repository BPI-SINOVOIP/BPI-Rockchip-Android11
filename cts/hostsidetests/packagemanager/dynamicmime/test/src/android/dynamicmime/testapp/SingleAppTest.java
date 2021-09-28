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

package android.dynamicmime.testapp;

import static android.dynamicmime.common.Constants.GROUP_FIRST;
import static android.dynamicmime.common.Constants.GROUP_SECOND;
import static android.dynamicmime.common.Constants.MIME_ANY;
import static android.dynamicmime.common.Constants.MIME_IMAGE_ANY;
import static android.dynamicmime.common.Constants.MIME_IMAGE_JPEG;
import static android.dynamicmime.common.Constants.MIME_IMAGE_PNG;
import static android.dynamicmime.common.Constants.MIME_TEXT_ANY;
import static android.dynamicmime.common.Constants.MIME_TEXT_PLAIN;
import static android.dynamicmime.common.Constants.MIME_TEXT_XML;

import android.dynamicmime.testapp.assertions.MimeGroupAssertions;
import android.dynamicmime.testapp.commands.MimeGroupCommands;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test cases for single app and one MIME group per intent filter scenario
 */
@RunWith(AndroidJUnit4.class)
public class SingleAppTest extends BaseDynamicMimeTest {
    public SingleAppTest() {
        super(MimeGroupCommands.testApp(context()), MimeGroupAssertions.testApp(context()));
    }

    @Test
    public void testGroupWithExactType() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);

        assertMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
        assertMatchedByMimeGroup(GROUP_FIRST, MIME_TEXT_ANY, MIME_ANY);
        assertNotMatchedByMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY);
    }

    @Test
    public void testGroupWithWildcardType() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);
        assertMatchedByMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_ANY);
        assertNotMatchedByMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY);
    }

    @Test
    public void testGroupWithAnyType() {
        setMimeGroup(GROUP_FIRST, MIME_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_ANY);
        assertMatchedByMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_TEXT_ANY, MIME_IMAGE_ANY);
    }

    @Test
    public void testAddSimilarTypes() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
        addMimeTypeToGroup(GROUP_FIRST, MIME_TEXT_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_TEXT_ANY, MIME_TEXT_PLAIN);
    }

    @Test
    public void testAddDifferentTypes() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
        addMimeTypeToGroup(GROUP_FIRST, MIME_IMAGE_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY, MIME_TEXT_PLAIN);
    }

    @Test
    public void testRemoveExactType() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
        removeMimeTypeFromGroup(GROUP_FIRST, MIME_TEXT_PLAIN);

        assertMimeGroupIsEmpty(GROUP_FIRST);
        assertNotMatchedByMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
    }

    @Test
    public void testRemoveDifferentType() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
        removeMimeTypeFromGroup(GROUP_FIRST, MIME_TEXT_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
    }

    @Test
    public void testRemoveSameBaseType_keepSpecific() {
        testKeepAndRemoveSimilarTypes(MIME_TEXT_PLAIN, MIME_TEXT_ANY);
    }

    @Test
    public void testRemoveSameBaseType_keepWildcard() {
        testKeepAndRemoveSimilarTypes(MIME_TEXT_ANY, MIME_TEXT_PLAIN);
    }

    @Test
    public void testRemoveAnyType_keepSpecific() {
        testKeepAndRemoveSimilarTypes(MIME_TEXT_PLAIN, MIME_ANY);
    }

    @Test
    public void testRemoveAnyType_keepWildcard() {
        testKeepAndRemoveSimilarTypes(MIME_ANY, MIME_TEXT_PLAIN);
    }

    @Test
    public void testResetWithoutIntersection() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_TEXT_XML);
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_JPEG, MIME_IMAGE_PNG);

        assertMimeGroup(GROUP_FIRST, MIME_IMAGE_JPEG, MIME_IMAGE_PNG);
    }

    @Test
    public void testResetWithIntersection() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_TEXT_XML);
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_IMAGE_PNG);

        assertMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_IMAGE_PNG);
    }

    @Test
    public void testClear() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_IMAGE_PNG);
        clearMimeGroup(GROUP_FIRST);

        assertMimeGroupIsEmpty(GROUP_FIRST);
    }

    @Test
    public void testDefinedGroupNotNullAfterRemove() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
        removeMimeTypeFromGroup(GROUP_FIRST, MIME_TEXT_PLAIN);

        assertMimeGroupIsEmpty(GROUP_FIRST);
    }

    @Test
    public void testDefinedGroupNotNullAfterClear() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
        clearMimeGroup(GROUP_FIRST);

        assertMimeGroupIsEmpty(GROUP_FIRST);
    }

    @Test
    public void testMimeGroupsIndependentAdd() {
        addMimeTypeToGroup(GROUP_SECOND, MIME_TEXT_ANY);

        assertMimeGroup(GROUP_SECOND, MIME_TEXT_ANY);
        assertMimeGroupIsEmpty(GROUP_FIRST);
    }

    @Test
    public void testMimeGroupsIndependentAddToBoth() {
        addMimeTypeToGroup(GROUP_SECOND, MIME_TEXT_ANY);
        addMimeTypeToGroup(GROUP_FIRST, MIME_IMAGE_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY);
        assertMimeGroup(GROUP_SECOND, MIME_TEXT_ANY);
    }

    @Test
    public void testMimeGroupsIndependentRemove() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY);
        setMimeGroup(GROUP_SECOND, MIME_TEXT_ANY);

        removeMimeTypeFromGroup(GROUP_FIRST, MIME_TEXT_ANY);
        removeMimeTypeFromGroup(GROUP_SECOND, MIME_IMAGE_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY);
        assertMimeGroup(GROUP_SECOND, MIME_TEXT_ANY);
    }

    @Test
    public void testMimeGroupsIndependentClear() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY, MIME_TEXT_ANY);
        setMimeGroup(GROUP_SECOND, MIME_TEXT_ANY);

        clearMimeGroup(GROUP_FIRST);

        assertMimeGroupIsEmpty(GROUP_FIRST);
        assertMimeGroup(GROUP_SECOND, MIME_TEXT_ANY);
    }

    @Test
    public void testMimeGroupsIndependentClearBoth() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY, MIME_TEXT_ANY);
        setMimeGroup(GROUP_SECOND, MIME_TEXT_ANY);
        clearMimeGroup(GROUP_FIRST);
        setMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);
        clearMimeGroup(GROUP_SECOND);

        assertMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);
        assertMimeGroupIsEmpty(GROUP_SECOND);
    }

    @Test
    public void testMimeGroupsIndependentSet() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_PNG, MIME_IMAGE_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_IMAGE_PNG, MIME_IMAGE_ANY);
        assertMimeGroupIsEmpty(GROUP_SECOND);
    }

    @Test
    public void testMimeGroupsIndependentSetBoth() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_PNG, MIME_IMAGE_ANY);
        setMimeGroup(GROUP_SECOND, MIME_TEXT_PLAIN, MIME_TEXT_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_IMAGE_PNG, MIME_IMAGE_ANY);
        assertMimeGroup(GROUP_SECOND, MIME_TEXT_PLAIN, MIME_TEXT_ANY);
    }

    private void testKeepAndRemoveSimilarTypes(String mimeTypeToKeep, String mimeTypeToRemove) {
        addMimeTypeToGroup(GROUP_FIRST, mimeTypeToKeep, mimeTypeToRemove);
        removeMimeTypeFromGroup(GROUP_FIRST, mimeTypeToRemove);

        assertMimeGroup(GROUP_FIRST, mimeTypeToKeep);
        assertMatchedByMimeGroup(GROUP_FIRST, mimeTypeToRemove);
    }
}
