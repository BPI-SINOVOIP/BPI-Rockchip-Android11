/*
 * Copyright (C) 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package android.widget.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;

import android.app.Instrumentation;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.EditText;
import android.widget.NumberPicker;

import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.FlakyTest;
import androidx.test.filters.SmallTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CtsKeyEventUtil;
import com.android.compatibility.common.util.CtsTouchUtils;

import junit.framework.Assert;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@FlakyTest
@SmallTest
@RunWith(AndroidJUnit4.class)
public class NumberPickerTest {
    private static final String[] NUMBER_NAMES3 = {"One", "Two", "Three"};
    private static final String[] NUMBER_NAMES_ALT3 = {"Three", "Four", "Five"};
    private static final String[] NUMBER_NAMES5 = {"One", "Two", "Three", "Four", "Five"};

    private Instrumentation mInstrumentation;
    private NumberPickerCtsActivity mActivity;
    private NumberPicker mNumberPicker;
    @Mock
    private View.AccessibilityDelegate mMockA11yDelegate;

    @Rule
    public ActivityTestRule<NumberPickerCtsActivity> mActivityRule =
            new ActivityTestRule<>(NumberPickerCtsActivity.class);

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        // Create a UiAutomation, which will enable accessibility and allow us to test a11y events.
        mInstrumentation.getUiAutomation();
        mActivity = mActivityRule.getActivity();
        mNumberPicker = (NumberPicker) mActivity.findViewById(R.id.number_picker);
    }

    @UiThreadTest
    @Test
    public void testConstructor() {
        new NumberPicker(mActivity);

        new NumberPicker(mActivity, null);

        new NumberPicker(mActivity, null, android.R.attr.numberPickerStyle);

        new NumberPicker(mActivity, null, 0, android.R.style.Widget_Material_NumberPicker);

        new NumberPicker(mActivity, null, 0, android.R.style.Widget_Material_Light_NumberPicker);
    }

    private void verifyDisplayedValues(String[] expected) {
        final String[] displayedValues = mNumberPicker.getDisplayedValues();
        assertEquals(expected.length, displayedValues.length);
        for (int i = 0; i < expected.length; i++) {
            assertEquals(expected[i], displayedValues[i]);
        }
    }

    @UiThreadTest
    @Test
    public void testSetDisplayedValuesRangeMatch() {
        mNumberPicker.setMinValue(10);
        mNumberPicker.setMaxValue(12);
        mNumberPicker.setDisplayedValues(NUMBER_NAMES3);

        assertEquals(10, mNumberPicker.getMinValue());
        assertEquals(12, mNumberPicker.getMaxValue());
        verifyDisplayedValues(NUMBER_NAMES3);

        // Set a different displayed values array, but still matching the min/max range
        mNumberPicker.setDisplayedValues(NUMBER_NAMES_ALT3);

        assertEquals(10, mNumberPicker.getMinValue());
        assertEquals(12, mNumberPicker.getMaxValue());
        verifyDisplayedValues(NUMBER_NAMES_ALT3);

        mNumberPicker.setMinValue(24);
        mNumberPicker.setMaxValue(26);

        assertEquals(24, mNumberPicker.getMinValue());
        assertEquals(26, mNumberPicker.getMaxValue());
        verifyDisplayedValues(NUMBER_NAMES_ALT3);
    }

    @UiThreadTest
    @Test
    public void testSetDisplayedValuesRangeMismatch() {
        mNumberPicker.setMinValue(10);
        mNumberPicker.setMaxValue(14);
        assertEquals(10, mNumberPicker.getMinValue());
        assertEquals(14, mNumberPicker.getMaxValue());

        // Try setting too few displayed entries
        try {
            // This is expected to fail since the displayed values only has three entries,
            // while the min/max range has five.
            mNumberPicker.setDisplayedValues(NUMBER_NAMES3);
            fail("The size of the displayed values array must be equal to min/max range!");
        } catch (Exception e) {
            // We are expecting to catch an exception. Set displayed values to an array that
            // matches the min/max range.
            mNumberPicker.setDisplayedValues(NUMBER_NAMES5);
        }
    }

    @UiThreadTest
    @Test
    public void testSelectionDisplayedValueFromDisplayedValues() {
        mNumberPicker.setMinValue(1);
        mNumberPicker.setMaxValue(3);
        mNumberPicker.setDisplayedValues(NUMBER_NAMES3);

        mNumberPicker.setValue(1);
        assertTrue(TextUtils.equals(NUMBER_NAMES3[0],
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(2);
        assertTrue(TextUtils.equals(NUMBER_NAMES3[1],
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(3);
        assertTrue(TextUtils.equals(NUMBER_NAMES3[2],
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        // Switch to a different displayed values array
        mNumberPicker.setDisplayedValues(NUMBER_NAMES_ALT3);
        assertTrue(TextUtils.equals(NUMBER_NAMES_ALT3[2],
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(1);
        assertTrue(TextUtils.equals(NUMBER_NAMES_ALT3[0],
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(2);
        assertTrue(TextUtils.equals(NUMBER_NAMES_ALT3[1],
                mNumberPicker.getDisplayedValueForCurrentSelection()));
    }

    @UiThreadTest
    @Test
    public void testSelectionDisplayedValueFromFormatter() {
        mNumberPicker.setMinValue(0);
        mNumberPicker.setMaxValue(4);
        mNumberPicker.setFormatter((int value) -> "entry " + value);

        mNumberPicker.setValue(0);
        assertTrue(TextUtils.equals("entry 0",
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(1);
        assertTrue(TextUtils.equals("entry 1",
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(2);
        assertTrue(TextUtils.equals("entry 2",
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(3);
        assertTrue(TextUtils.equals("entry 3",
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(4);
        assertTrue(TextUtils.equals("entry 4",
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        // Switch to a different formatter
        mNumberPicker.setFormatter((int value) -> "row " + value);
        // Check that the currently selected value has new displayed value
        assertTrue(TextUtils.equals("row 4",
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        // and check a couple more values for the new formatting
        mNumberPicker.setValue(0);
        assertTrue(TextUtils.equals("row 0",
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(1);
        assertTrue(TextUtils.equals("row 1",
                mNumberPicker.getDisplayedValueForCurrentSelection()));
    }


    @UiThreadTest
    @Test
    public void testSelectionDisplayedValuePrecedence() {
        mNumberPicker.setMinValue(1);
        mNumberPicker.setMaxValue(3);
        mNumberPicker.setDisplayedValues(NUMBER_NAMES3);
        mNumberPicker.setFormatter((int value) -> "entry " + value);

        // According to the widget documentation, displayed values take precedence over formatter
        mNumberPicker.setValue(1);
        assertTrue(TextUtils.equals(NUMBER_NAMES3[0],
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(2);
        assertTrue(TextUtils.equals(NUMBER_NAMES3[1],
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(3);
        assertTrue(TextUtils.equals(NUMBER_NAMES3[2],
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        // Set displayed values to null and test that the widget is using the formatter
        mNumberPicker.setDisplayedValues(null);
        assertTrue(TextUtils.equals("entry 3",
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(1);
        assertTrue(TextUtils.equals("entry 1",
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(2);
        assertTrue(TextUtils.equals("entry 2",
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        // Set a different displayed values array and test that it's taking precedence
        mNumberPicker.setDisplayedValues(NUMBER_NAMES_ALT3);
        assertTrue(TextUtils.equals(NUMBER_NAMES_ALT3[1],
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(1);
        assertTrue(TextUtils.equals(NUMBER_NAMES_ALT3[0],
                mNumberPicker.getDisplayedValueForCurrentSelection()));

        mNumberPicker.setValue(3);
        assertTrue(TextUtils.equals(NUMBER_NAMES_ALT3[2],
                mNumberPicker.getDisplayedValueForCurrentSelection()));
    }

    @Test
    public void testAccessValue() throws Throwable {
        final NumberPicker.OnValueChangeListener mockValueChangeListener =
                mock(NumberPicker.OnValueChangeListener.class);

        mInstrumentation.runOnMainSync(() -> {
            mNumberPicker.setMinValue(20);
            mNumberPicker.setMaxValue(22);
            mNumberPicker.setDisplayedValues(NUMBER_NAMES3);

            mNumberPicker.setOnValueChangedListener(mockValueChangeListener);

            mNumberPicker.setValue(21);
            assertEquals(21, mNumberPicker.getValue());

            ((View) mNumberPicker.getParent()).setAccessibilityDelegate(mMockA11yDelegate);
            mNumberPicker.setValue(20);
            assertEquals(20, mNumberPicker.getValue());
            verify(mMockA11yDelegate, atLeastOnce()).onRequestSendAccessibilityEvent(
                    any(), eq(mNumberPicker), argThat(event -> event.getEventType()
                            == AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED));
            ((View) mNumberPicker.getParent()).setAccessibilityDelegate(null);

            mNumberPicker.setValue(22);
            assertEquals(22, mNumberPicker.getValue());

            // Check trying to set value out of min/max range
            mNumberPicker.setValue(10);
            assertEquals(20, mNumberPicker.getValue());

            mNumberPicker.setValue(100);
            assertEquals(22, mNumberPicker.getValue());
        });

        // Since all changes to value are via API calls, we should have no interactions /
        // callbacks on our listener.
        verifyZeroInteractions(mockValueChangeListener);
    }

    private boolean isWatch() {
        return (mActivity.getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_TYPE_MASK) == Configuration.UI_MODE_TYPE_WATCH;
    }

    @Test
    public void testInteractionWithSwipeDown() throws Throwable {
        mActivityRule.runOnUiThread(() -> {
            mNumberPicker.setMinValue(6);
            mNumberPicker.setMaxValue(8);
            mNumberPicker.setDisplayedValues(NUMBER_NAMES_ALT3);
        });

        final NumberPicker.OnValueChangeListener mockValueChangeListener =
                mock(NumberPicker.OnValueChangeListener.class);
        mNumberPicker.setOnValueChangedListener(mockValueChangeListener);

        final CountDownLatch latch = new CountDownLatch(1);
        final NumberPicker.OnScrollListener mockScrollListener =
                mock(NumberPicker.OnScrollListener.class);
        final NumberPicker.OnScrollListener waitForIdleListener =
                (NumberPicker numberPicker, int scrollState) -> {
                    mockScrollListener.onScrollStateChange(numberPicker, scrollState);
                    if (scrollState == NumberPicker.OnScrollListener.SCROLL_STATE_IDLE) {
                        mNumberPicker.post(latch::countDown);
                    }
                };
        mNumberPicker.setOnScrollListener(waitForIdleListener);

        mActivityRule.runOnUiThread(() -> mNumberPicker.setValue(7));
        assertEquals(7, mNumberPicker.getValue());

        // Swipe down across our number picker
        final int[] numberPickerLocationOnScreen = new int[2];
        mNumberPicker.getLocationOnScreen(numberPickerLocationOnScreen);

        try {
            int screenHeight = Resources.getSystem().getDisplayMetrics().heightPixels;
            int numberPickerMiddleX = numberPickerLocationOnScreen[0]
                    + mNumberPicker.getWidth() / 2;
            int numberPickerStartY = numberPickerLocationOnScreen[1] + 1;

            CtsTouchUtils.emulateDragGesture(mInstrumentation, mActivityRule,
                    numberPickerMiddleX,
                    numberPickerStartY,
                    0,
                    screenHeight - numberPickerStartY); // drag down to the bottom of the screen.

            Assert.assertTrue("Expected to get to IDLE state within 5 seconds",
                    latch.await(5, TimeUnit.SECONDS));
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }

        // At this point we expect that the drag-down gesture has selected the value
        // that was "above" the previously selected one, and that our value change listener
        // has been notified of that change exactly once.
        assertEquals(6, mNumberPicker.getValue());
        verify(mockValueChangeListener, times(1)).onValueChange(mNumberPicker, 7, 6);
        verifyNoMoreInteractions(mockValueChangeListener);

        // We expect that our scroll listener will be called with specific state changes.
        InOrder inOrder = inOrder(mockScrollListener);
        inOrder.verify(mockScrollListener).onScrollStateChange(mNumberPicker,
                NumberPicker.OnScrollListener.SCROLL_STATE_TOUCH_SCROLL);
        if (!isWatch()) {
            inOrder.verify(mockScrollListener).onScrollStateChange(mNumberPicker,
                    NumberPicker.OnScrollListener.SCROLL_STATE_FLING);
        }
        inOrder.verify(mockScrollListener).onScrollStateChange(mNumberPicker,
                NumberPicker.OnScrollListener.SCROLL_STATE_IDLE);
        verifyNoMoreInteractions(mockScrollListener);
    }

    @Test
    public void testInteractionWithSwipeUp() throws Throwable {
        mActivityRule.runOnUiThread(() -> {
            mNumberPicker.setMinValue(10);
            mNumberPicker.setMaxValue(12);
            mNumberPicker.setDisplayedValues(NUMBER_NAMES_ALT3);
        });

        final NumberPicker.OnValueChangeListener mockValueChangeListener =
                mock(NumberPicker.OnValueChangeListener.class);
        mNumberPicker.setOnValueChangedListener(mockValueChangeListener);

        final CountDownLatch latch = new CountDownLatch(1);
        final NumberPicker.OnScrollListener mockScrollListener =
                mock(NumberPicker.OnScrollListener.class);
        final NumberPicker.OnScrollListener waitForIdleListener =
                (NumberPicker numberPicker, int scrollState) -> {
                    mockScrollListener.onScrollStateChange(numberPicker, scrollState);
                    if (scrollState == NumberPicker.OnScrollListener.SCROLL_STATE_IDLE) {
                        mNumberPicker.post(latch::countDown);
                    }
                };
        mNumberPicker.setOnScrollListener(waitForIdleListener);

        mActivityRule.runOnUiThread(() -> mNumberPicker.setValue(11));
        assertEquals(11, mNumberPicker.getValue());

        // Swipe up across our number picker
        final int[] numberPickerLocationOnScreen = new int[2];
        mNumberPicker.getLocationOnScreen(numberPickerLocationOnScreen);

        ((View) mNumberPicker.getParent()).setAccessibilityDelegate(mMockA11yDelegate);
        try {
            int numberPickerMiddleX =
                    numberPickerLocationOnScreen[0] + mNumberPicker.getWidth() / 2;
            int numberPickerEndY = numberPickerLocationOnScreen[1] + mNumberPicker.getHeight() - 1;
            CtsTouchUtils.emulateDragGesture(mInstrumentation, mActivityRule,
                    numberPickerMiddleX,
                    numberPickerEndY,
                    0,
                    -(numberPickerEndY)); // drag up to the top of the screen.
            Assert.assertTrue("Expected to get to IDLE state within 5 seconds",
                    latch.await(5, TimeUnit.SECONDS));
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
        verify(mMockA11yDelegate, atLeastOnce()).onRequestSendAccessibilityEvent(
                any(), eq(mNumberPicker),
                argThat(event -> event.getEventType() == AccessibilityEvent.TYPE_VIEW_SCROLLED));
        ((View) mNumberPicker.getParent()).setAccessibilityDelegate(null);

        // At this point we expect that the drag-up gesture has selected the value
        // that was "below" the previously selected one, and that our value change listener
        // has been notified of that change exactly once.
        assertEquals(12, mNumberPicker.getValue());
        verify(mockValueChangeListener, times(1)).onValueChange(mNumberPicker, 11, 12);
        verifyNoMoreInteractions(mockValueChangeListener);

        // We expect that our scroll listener will be called with specific state changes.
        InOrder inOrder = inOrder(mockScrollListener);
        inOrder.verify(mockScrollListener).onScrollStateChange(mNumberPicker,
                NumberPicker.OnScrollListener.SCROLL_STATE_TOUCH_SCROLL);
        if (!isWatch()) {
            inOrder.verify(mockScrollListener).onScrollStateChange(mNumberPicker,
                    NumberPicker.OnScrollListener.SCROLL_STATE_FLING);
        }
        inOrder.verify(mockScrollListener).onScrollStateChange(mNumberPicker,
                NumberPicker.OnScrollListener.SCROLL_STATE_IDLE);
        verifyNoMoreInteractions(mockScrollListener);
    }

    @UiThreadTest
    @Test
    public void testAccessWrapSelectorValue() {
        mNumberPicker.setMinValue(100);
        mNumberPicker.setMaxValue(200);
        // As specified in the Javadocs of NumberPicker.setWrapSelectorWheel, when min/max
        // range is larger than what the widget is showing, the selector wheel is enabled.
        assertTrue(mNumberPicker.getWrapSelectorWheel());

        mNumberPicker.setWrapSelectorWheel(false);
        assertFalse(mNumberPicker.getWrapSelectorWheel());

        mNumberPicker.setWrapSelectorWheel(true);
        assertTrue(mNumberPicker.getWrapSelectorWheel());
    }

    @UiThreadTest
    @Test
    public void testSelectionDividerHeight() {
        final NumberPicker numberPicker =
                (NumberPicker) mActivity.findViewById(R.id.number_picker_divider_height);
        final int initialValue = numberPicker.getSelectionDividerHeight();
        assertEquals("Height set via XML", 4, initialValue);
        final int newValue = 8;
        numberPicker.setSelectionDividerHeight(newValue);
        assertEquals(newValue, numberPicker.getSelectionDividerHeight());
    }

    @UiThreadTest
    @Test
    public void testSetGetTextColor() {
        EditText inputText = (EditText) mNumberPicker.getChildAt(0);

        mNumberPicker.setTextColor(Color.RED);
        assertEquals(Color.RED, mNumberPicker.getTextColor());
        assertEquals(ColorStateList.valueOf(Color.RED), inputText.getTextColors());
    }

    @UiThreadTest
    @Test
    public void testSetGetTextSize() {
        EditText inputText = (EditText) mNumberPicker.getChildAt(0);

        mNumberPicker.setTextSize(20f);
        assertEquals(20f, mNumberPicker.getTextSize(), 0.01f);
        assertEquals(20f, inputText.getTextSize(), 0.01f);
    }

    @Test
    public void testEnterKey() throws Throwable {
        mActivityRule.runOnUiThread(() -> {
            mNumberPicker.setMinValue(10);
            mNumberPicker.setMaxValue(14);
            mNumberPicker.setDisplayedValues(NUMBER_NAMES5);
        });

        final NumberPicker.OnValueChangeListener mockValueChangeListener =
                mock(NumberPicker.OnValueChangeListener.class);
        mNumberPicker.setOnValueChangedListener(mockValueChangeListener);

        final int[] numberPickerLocationOnScreen = new int[2];
        mNumberPicker.getLocationOnScreen(numberPickerLocationOnScreen);
        int screenHeight = Resources.getSystem().getDisplayMetrics().heightPixels;
        int x = numberPickerLocationOnScreen[0] + mNumberPicker.getWidth() / 2;
        int y = numberPickerLocationOnScreen[1] + (int) mNumberPicker.getHeight() / 6;

        // Phase 1. Check enter key
        MotionEvent event = MotionEvent.obtain(System.currentTimeMillis(),
                System.currentTimeMillis(), MotionEvent.ACTION_DOWN, x, y, 0);
        mInstrumentation.sendPointerSync(event);

        // Send enter key to call removeAllCallbacks including longpressed
        CtsKeyEventUtil.sendKeyDownUp(mInstrumentation, mNumberPicker, KeyEvent.KEYCODE_ENTER);
        reset(mockValueChangeListener);

        // Wait a second to check if value is changed or not.
        SystemClock.sleep(1000);
        verifyZeroInteractions(mockValueChangeListener);

        event = MotionEvent.obtain(System.currentTimeMillis(), System.currentTimeMillis(),
                MotionEvent.ACTION_UP, x, y, 0);
        mInstrumentation.sendPointerSync(event);

        // Phase 2. Check numpad enter key
        event = MotionEvent.obtain(System.currentTimeMillis(), System.currentTimeMillis(),
                MotionEvent.ACTION_DOWN, x, y, 0);
        mInstrumentation.sendPointerSync(event);

        // Send numpad enter key. we expect it works like enter key.
        CtsKeyEventUtil.sendKeyDownUp(mInstrumentation, mNumberPicker,
                KeyEvent.KEYCODE_NUMPAD_ENTER);
        reset(mockValueChangeListener);

        // Wait a second to check if value is changed or not.
        SystemClock.sleep(1000);
        verifyZeroInteractions(mockValueChangeListener);

        event = MotionEvent.obtain(System.currentTimeMillis(), System.currentTimeMillis(),
                MotionEvent.ACTION_UP, x, y, 0);
        mInstrumentation.sendPointerSync(event);
    }
}
