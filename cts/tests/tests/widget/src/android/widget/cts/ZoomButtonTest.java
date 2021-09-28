/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.widget.cts;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.Instrumentation;
import android.util.AttributeSet;
import android.util.Xml;
import android.view.View;
import android.view.ViewConfiguration;
import android.widget.ListView;
import android.widget.ZoomButton;

import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CtsTouchUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.xmlpull.v1.XmlPullParser;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ZoomButtonTest {
    private static long NANOS_IN_MILLI = 1000000;

    private Instrumentation mInstrumentation;
    private ZoomButton mZoomButton;
    private Activity mActivity;

    @Rule
    public ActivityTestRule<ZoomButtonCtsActivity> mActivityRule =
            new ActivityTestRule<>(ZoomButtonCtsActivity.class);

    @Before
    public void setup() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mActivity = mActivityRule.getActivity();
        mZoomButton = (ZoomButton) mActivity.findViewById(R.id.zoombutton_test);
    }

    @UiThreadTest
    @Test
    public void testConstructor() {
        new ZoomButton(mActivity);

        new ZoomButton(mActivity, null);

        new ZoomButton(mActivity, null, android.R.attr.imageButtonStyle);

        new ZoomButton(mActivity, null, 0, android.R.style.Widget_Material_Light_ImageButton);

        XmlPullParser parser = mActivity.getResources().getXml(R.layout.zoombutton_layout);
        AttributeSet attrs = Xml.asAttributeSet(parser);
        assertNotNull(attrs);
        new ZoomButton(mActivity, attrs);
        new ZoomButton(mActivity, attrs, 0);
    }

    @Test(expected=NullPointerException.class)
    public void testConstructorWithNullContext1() {
        new ZoomButton(null);
    }

    @Test(expected=NullPointerException.class)
    public void testConstructorWithNullContext2() {
        new ZoomButton(null, null);
    }

    @Test(expected=NullPointerException.class)
    public void testConstructorWithNullContext3() {
        new ZoomButton(null, null, 0);
    }

    @UiThreadTest
    @Test
    public void testSetEnabled() {
        assertFalse(mZoomButton.isPressed());
        mZoomButton.setEnabled(true);
        assertTrue(mZoomButton.isEnabled());
        assertFalse(mZoomButton.isPressed());

        mZoomButton.setPressed(true);
        assertTrue(mZoomButton.isPressed());
        mZoomButton.setEnabled(true);
        assertTrue(mZoomButton.isEnabled());
        assertTrue(mZoomButton.isPressed());

        mZoomButton.setEnabled(false);
        assertFalse(mZoomButton.isEnabled());
        assertFalse(mZoomButton.isPressed());
    }

    @UiThreadTest
    @Test
    public void testDispatchUnhandledMove() {
        assertFalse(mZoomButton.dispatchUnhandledMove(new ListView(mActivity), View.FOCUS_DOWN));

        assertFalse(mZoomButton.dispatchUnhandledMove(null, View.FOCUS_DOWN));
    }

    @LargeTest
    @Test
    public void testSetZoomSpeed() {
        final long[] zoomSpeeds = { 0, 100 };
        mZoomButton.setEnabled(true);
        ZoomClickListener zoomClickListener = new ZoomClickListener();
        mZoomButton.setOnClickListener(zoomClickListener);

        for (long zoomSpeed : zoomSpeeds) {
            // Reset the tracking state of our listener, but continue using it for testing
            // various zoom speeds on the same ZoomButton
            zoomClickListener.reset();

            mZoomButton.setZoomSpeed(zoomSpeed);

            final long startTime = System.nanoTime();
            // Emulate long click
            long longPressWait = ViewConfiguration.getLongPressTimeout() + zoomSpeed + 200;
            CtsTouchUtils.emulateLongPressOnViewCenter(mInstrumentation, mActivityRule, mZoomButton,
                    longPressWait);

            final Long callbackFirstInvocationTime = zoomClickListener.getTimeOfFirstClick();
            assertNotNull("Expecting at least one callback", callbackFirstInvocationTime);

            // Verify that the first callback is fired after the system-level long press timeout.
            final long minTimeUntilFirstInvocationMs = ViewConfiguration.getLongPressTimeout();
            final long actualTimeUntilFirstInvocationNs = callbackFirstInvocationTime - startTime;
            assertTrue("First callback not during long press timeout was "
                            + actualTimeUntilFirstInvocationNs / NANOS_IN_MILLI
                            + " while long press timeout is " + minTimeUntilFirstInvocationMs,
                    actualTimeUntilFirstInvocationNs
                            > minTimeUntilFirstInvocationMs * NANOS_IN_MILLI);
            assertTrue("First callback should have happened sooner than "
                            + actualTimeUntilFirstInvocationNs / NANOS_IN_MILLI,
                    actualTimeUntilFirstInvocationNs
                            <= (minTimeUntilFirstInvocationMs + 200) * NANOS_IN_MILLI);
        }
    }

    private static class ZoomClickListener implements View.OnClickListener {
        private Long mTimeOfFirstClick = null;

        public void reset() {
            mTimeOfFirstClick = null;
        }

        public Long getTimeOfFirstClick() {
            return mTimeOfFirstClick;
        }

        public void onClick(View v) {
            if (mTimeOfFirstClick == null) {
                // Mark the current system time as the time of first click
                mTimeOfFirstClick = System.nanoTime();
            }
        }
    }
}
