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

import static android.dynamicmime.common.Constants.ALIAS_BOTH_GROUPS;
import static android.dynamicmime.common.Constants.ALIAS_BOTH_GROUPS_AND_TYPE;
import static android.dynamicmime.common.Constants.GROUP_FIRST;
import static android.dynamicmime.common.Constants.GROUP_SECOND;
import static android.dynamicmime.common.Constants.MIME_IMAGE_PNG;
import static android.dynamicmime.common.Constants.MIME_TEXT_ANY;
import static android.dynamicmime.common.Constants.MIME_TEXT_PLAIN;

import android.dynamicmime.testapp.assertions.AssertionsByIntentResolution;
import android.dynamicmime.testapp.commands.MimeGroupCommands;

import org.junit.Test;

/**
 * Test cases for:
 *  - multiple MIME groups per intent filter
 *  - MIME groups and android:mimeType attribute in one filter
 */
public class ComplexFilterTest extends BaseDynamicMimeTest {
    public ComplexFilterTest() {
        super(MimeGroupCommands.testApp(context()),
                AssertionsByIntentResolution.testApp(context()));
    }

    @Test
    public void testMimeGroupsNotIntersect() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_PNG);
        setMimeGroup(GROUP_SECOND, MIME_TEXT_ANY);

        assertBothGroups(MIME_IMAGE_PNG, MIME_TEXT_ANY);
    }

    @Test
    public void testMimeGroupsIntersect() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_IMAGE_PNG);
        setMimeGroup(GROUP_SECOND, MIME_IMAGE_PNG, MIME_TEXT_ANY);

        assertBothGroups(MIME_TEXT_PLAIN, MIME_TEXT_ANY, MIME_IMAGE_PNG);
    }

    @Test
    public void testRemoveTypeFromIntersection() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_IMAGE_PNG);
        setMimeGroup(GROUP_SECOND, MIME_IMAGE_PNG, MIME_TEXT_ANY);
        removeMimeTypeFromGroup(GROUP_FIRST, MIME_IMAGE_PNG);

        assertBothGroups(MIME_TEXT_PLAIN, MIME_TEXT_ANY, MIME_IMAGE_PNG);
    }

    @Test
    public void testRemoveIntersectionFromBothGroups() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_IMAGE_PNG);
        setMimeGroup(GROUP_SECOND, MIME_IMAGE_PNG, MIME_TEXT_ANY);
        removeMimeTypeFromGroup(GROUP_FIRST, MIME_IMAGE_PNG);
        removeMimeTypeFromGroup(GROUP_SECOND, MIME_IMAGE_PNG);

        assertBothGroups(MIME_TEXT_PLAIN, MIME_TEXT_ANY);
    }

    @Test
    public void testClearGroupWithoutIntersection() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);
        setMimeGroup(GROUP_SECOND, MIME_IMAGE_PNG);
        clearMimeGroup(GROUP_FIRST);

        assertBothGroups(MIME_IMAGE_PNG);
    }

    @Test
    public void testClearGroupWithIntersection() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN, MIME_IMAGE_PNG);
        setMimeGroup(GROUP_SECOND, MIME_IMAGE_PNG, MIME_TEXT_ANY);
        clearMimeGroup(GROUP_FIRST);

        assertBothGroups(MIME_IMAGE_PNG, MIME_TEXT_ANY);
    }

    @Test
    public void testMimeGroupNotIntersectWithStaticType() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_PNG);

        assertGroupsAndType(MIME_IMAGE_PNG, MIME_TEXT_PLAIN);
    }

    @Test
    public void testMimeGroupIntersectWithStaticType() {
        setMimeGroup(GROUP_FIRST, MIME_TEXT_PLAIN);

        assertGroupsAndType(MIME_TEXT_PLAIN);
    }

    @Test
    public void testRemoveStaticTypeFromMimeGroup() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_PNG, MIME_TEXT_PLAIN);
        removeMimeTypeFromGroup(GROUP_FIRST, MIME_TEXT_PLAIN);

        assertGroupsAndType(MIME_IMAGE_PNG, MIME_TEXT_PLAIN);
    }

    @Test
    public void testClearGroupContainsStaticType() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_PNG, MIME_TEXT_PLAIN);
        clearMimeGroup(GROUP_FIRST);

        assertGroupsAndType(MIME_TEXT_PLAIN);
    }

    @Test
    public void testClearGroupNotContainStaticType() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_PNG, MIME_TEXT_ANY);
        clearMimeGroup(GROUP_FIRST);

        assertGroupsAndType(MIME_TEXT_PLAIN);
    }

    private void assertGroupsAndType(String... expectedTypes) {
        assertMimeGroup(ALIAS_BOTH_GROUPS_AND_TYPE, expectedTypes);
    }

    private void assertBothGroups(String... expectedTypes) {
        assertMimeGroup(ALIAS_BOTH_GROUPS, expectedTypes);
    }
}
