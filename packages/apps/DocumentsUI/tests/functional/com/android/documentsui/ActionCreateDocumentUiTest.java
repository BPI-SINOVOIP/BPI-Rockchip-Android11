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

package com.android.documentsui;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.android.documentsui.base.Providers.AUTHORITY_STORAGE;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.support.test.uiautomator.UiDevice;

import androidx.test.filters.LargeTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.bots.Bots;
import com.android.documentsui.picker.PickActivity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.UUID;


@LargeTest
@RunWith(AndroidJUnit4.class)
public class ActionCreateDocumentUiTest {

    @Rule
    public final ActivityTestRule<PickActivity> mRule =
            new ActivityTestRule<>(PickActivity.class, false, false);

    private Context mTargetContext;
    private Context mContext;
    private Bots mBots;
    private UiDevice mDevice;

    @Before
    public void setup() {
        UiAutomation automation = getInstrumentation().getUiAutomation();

        mDevice = UiDevice.getInstance(getInstrumentation());
        mTargetContext = getInstrumentation().getTargetContext();
        mContext = getInstrumentation().getContext();
        mBots = new Bots(mDevice, automation, mTargetContext, 5000);
    }

    @Test
    public void testActionCreate_TextFile() throws Exception {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");

        Uri hintUri = DocumentsContract.buildRootUri(AUTHORITY_STORAGE, "primary");
        intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, hintUri);

        mRule.launchActivity(intent);

        String fileName = UUID.randomUUID().toString() + ".txt";

        mBots.main.setDialogText(fileName);
        mBots.main.clickSaveButton();
        mDevice.waitForIdle();

        Instrumentation.ActivityResult activityResult = mRule.getActivityResult();

        Intent result = activityResult.getResultData();
        Uri uri = result.getData();
        int flags = result.getFlags();

        assertThat(activityResult.getResultCode()).isEqualTo(Activity.RESULT_OK);
        assertThat(uri.getAuthority()).isEqualTo(AUTHORITY_STORAGE);
        assertThat(uri.getPath()).contains(fileName);

        int expectedFlags =
                Intent.FLAG_GRANT_READ_URI_PERMISSION
                        | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                        | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION;

        assertThat(flags).isEqualTo(expectedFlags);
        assertThat(DocumentsContract.deleteDocument(mContext.getContentResolver(), uri)).isTrue();
    }

}