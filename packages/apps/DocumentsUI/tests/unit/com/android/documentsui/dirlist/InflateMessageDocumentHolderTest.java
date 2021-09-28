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

package com.android.documentsui.dirlist;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.view.View;
import android.widget.Button;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.documentsui.CrossProfileQuietModeException;
import com.android.documentsui.Model;
import com.android.documentsui.R;
import com.android.documentsui.base.State;
import com.android.documentsui.testing.TestActionHandler;
import com.android.documentsui.testing.TestEnv;
import com.android.documentsui.testing.TestProvidersAccess;

import org.junit.Before;
import org.junit.Test;

@SmallTest
public final class InflateMessageDocumentHolderTest {

    private Context mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
    private Runnable mDefaultCallback = () -> {
    };
    private Message mInflateMessage;
    private TestActionHandler mTestActionHandler = new TestActionHandler();
    private InflateMessageDocumentHolder mHolder;

    @Before
    public void setUp() {
        DocumentsAdapter.Environment env =
                new TestEnvironment(mContext, TestEnv.create(), mTestActionHandler);
        env.getDisplayState().action = State.ACTION_GET_CONTENT;
        env.getDisplayState().canShareAcrossProfile = true;
        env.getDisplayState().supportsCrossProfile = true;
        mInflateMessage = new Message.InflateMessage(env, mDefaultCallback);
        mContext.setTheme(R.style.DocumentsTheme);
        mContext.getTheme().applyStyle(R.style.DocumentsDefaultTheme,  /* force= */false);

        mHolder = new InflateMessageDocumentHolder(mContext, /* parent= */null);
    }

    @Test
    public void testClickingButtonShouldShowProgressBar() {
        Model.Update error = new Model.Update(
                new CrossProfileQuietModeException(TestProvidersAccess.OtherUser.USER_ID),
                /* remoteActionsEnabled= */ true);
        mInflateMessage.update(error);

        mHolder.bind(mInflateMessage);

        View content = mHolder.itemView.findViewById(R.id.content);
        View crossProfile = mHolder.itemView.findViewById(R.id.cross_profile);
        View crossProfileContent = mHolder.itemView.findViewById(R.id.cross_profile_content);
        View progress = mHolder.itemView.findViewById(R.id.cross_profile_progress);
        Button button = mHolder.itemView.findViewById(R.id.button);

        assertThat(content.getVisibility()).isEqualTo(View.GONE);
        assertThat(crossProfile.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(crossProfileContent.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(progress.getVisibility()).isEqualTo(View.GONE);

        if (button.getVisibility() == View.VISIBLE) {
            // The button is visible when docsUI has the permission to modify quiet mode.
            assertThat(button.callOnClick()).isTrue();
            assertThat(crossProfile.getVisibility()).isEqualTo(View.VISIBLE);
            assertThat(crossProfileContent.getVisibility()).isEqualTo(View.GONE);
            assertThat(progress.getVisibility()).isEqualTo(View.VISIBLE);
        }
    }
}
