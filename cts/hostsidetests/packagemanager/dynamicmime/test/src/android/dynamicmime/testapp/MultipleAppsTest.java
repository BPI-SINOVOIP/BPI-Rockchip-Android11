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
import static android.dynamicmime.common.Constants.MIME_IMAGE_ANY;
import static android.dynamicmime.common.Constants.MIME_TEXT_ANY;

import android.dynamicmime.testapp.assertions.MimeGroupAssertions;
import android.dynamicmime.testapp.commands.MimeGroupCommands;
import android.dynamicmime.testapp.util.MimeGroupOperations;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test cases that verify independence of MIME groups from different apps
 */
@RunWith(AndroidJUnit4.class)
public class MultipleAppsTest extends BaseDynamicMimeTest {
    public MultipleAppsTest() {
        super(MimeGroupCommands.testApp(context()), MimeGroupAssertions.testApp(context()));
    }

    @After
    @Override
    public void tearDown() {
        helperApp().clearGroups();
        super.tearDown();
    }

    @Test
    public void testMimeGroupsIndependentSet() {
        helperApp().setMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);

        assertMimeGroupIsEmpty(GROUP_FIRST);
        helperApp().assertMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);

        setMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY);
        helperApp().assertMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);
    }

    @Test
    public void testMimeGroupsIndependentReset() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY);
        helperApp().setMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);

        removeMimeTypeFromGroup(GROUP_FIRST, MIME_TEXT_ANY);
        helperApp().removeMimeTypeFromGroup(GROUP_FIRST, MIME_IMAGE_ANY);

        assertMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY);
        helperApp().assertMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);
    }

    @Test
    public void testMimeGroupsIndependentClear() {
        setMimeGroup(GROUP_FIRST, MIME_IMAGE_ANY, MIME_TEXT_ANY);
        helperApp().setMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);

        clearMimeGroup(GROUP_FIRST);

        assertMimeGroupIsEmpty(GROUP_FIRST);
        helperApp().assertMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);

        setMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);
        helperApp().clearMimeGroup(GROUP_FIRST);

        assertMimeGroup(GROUP_FIRST, MIME_TEXT_ANY);
        helperApp().assertMimeGroupIsEmpty(GROUP_FIRST);
    }

    /**
     * {@link MimeGroupOperations} implementation to interact with sample app MIME groups
     */
    private MimeGroupOperations helperApp() {
        return new MimeGroupOperations(MimeGroupCommands.helperApp(context()),
                MimeGroupAssertions.helperApp(context()));
    }
}
