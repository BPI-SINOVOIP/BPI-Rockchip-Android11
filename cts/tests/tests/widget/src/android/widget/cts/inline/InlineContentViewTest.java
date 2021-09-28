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

package android.widget.cts.inline;

import static com.google.common.truth.Truth.assertThat;

import android.app.Instrumentation;
import android.content.Context;
import android.util.Log;
import android.view.SurfaceControl;
import android.view.View;
import android.view.ViewGroup;
import android.widget.cts.R;
import android.widget.inline.InlineContentView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class InlineContentViewTest {
    private static final String LOG_TAG = "InlineContentViewTest";

    private Context mContext;
    private InlineContentView mInlineContentView;
    private InlineContentViewCtsActivity mActivity;
    private Instrumentation mInstrumentation;

    @Rule
    public ActivityTestRule<InlineContentViewCtsActivity> mActivityRule =
            new ActivityTestRule<>(InlineContentViewCtsActivity.class);

    @Before
    public void setup() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mContext = mInstrumentation.getTargetContext();
        mActivity = mActivityRule.getActivity();
        PollingCheck.waitFor(mActivity::hasWindowFocus);
        mInlineContentView = new InlineContentView(mContext);
    }

    @Test
    public void testConstructor() {
        new InlineContentView(mContext);

        new InlineContentView(mContext, /* attrs */ null);

        new InlineContentView(mContext, /* attrs */ null, /* defStyleAttr */ 0);

        new InlineContentView(mContext, /* attrs */ null, /* defStyleAttr */ 0,
                /* defStyleRes */ 0);
    }

    @Test(expected = NullPointerException.class)
    public void testConstructorNullContext() {
        new InlineContentView(/* context */ null);
    }

    @Test
    public void testGetSurfaceControl() {
        attachInlineContentView();

        final ViewGroup parent = (ViewGroup) mActivity.findViewById(R.id.inlinecontentview);

        assertThat(parent.getChildCount()).isEqualTo(1);

        final View view = parent.getChildAt(0);

        assertThat(view instanceof InlineContentView).isTrue();

        final SurfaceControl surfaceControl = ((InlineContentView) view).getSurfaceControl();

        assertThat(surfaceControl).isNotNull();
    }

    @Test
    public void testSetSurfaceControlCallback() throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        mInlineContentView.setSurfaceControlCallback(
                new InlineContentView.SurfaceControlCallback() {
                    @Override
                    public void onCreated(SurfaceControl surfaceControl) {
                        assertThat(surfaceControl).isNotNull();
                        latch.countDown();
                    }

                    @Override
                    public void onDestroyed(SurfaceControl surfaceControl) {
                        /* do nothing */
                    }
                });

        attachInlineContentView();

        latch.await();
    }

    @Test
    public void testSetZOrderedOnTop() {
        mInlineContentView.setZOrderedOnTop(false);

        assertThat(mInlineContentView.isZOrderedOnTop()).isFalse();

        mInlineContentView.setZOrderedOnTop(true);

        assertThat(mInlineContentView.isZOrderedOnTop()).isTrue();
    }

    private void attachInlineContentView() {
        final ViewGroup viewGroup = (ViewGroup) mActivity.findViewById(R.id.inlinecontentview);
        try {
            mActivityRule.runOnUiThread(
                    () -> viewGroup.addView(mInlineContentView, new ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT)));
            mInstrumentation.waitForIdleSync();
        } catch (Throwable e) {
            Log.e(LOG_TAG, "attachInlineContentView fail");
        }
    }
}
