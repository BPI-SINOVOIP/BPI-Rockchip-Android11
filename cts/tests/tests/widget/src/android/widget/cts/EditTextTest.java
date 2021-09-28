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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Point;
import android.text.Editable;
import android.text.Layout;
import android.text.TextUtils;
import android.text.method.ArrowKeyMovementMethod;
import android.text.method.MovementMethod;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.SparseArray;
import android.util.TypedValue;
import android.util.Xml;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.TextView.BufferType;

import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CtsKeyEventUtil;
import com.android.compatibility.common.util.CtsTouchUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.xmlpull.v1.XmlPullParser;

import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class EditTextTest {
    private Activity mActivity;
    private EditText mEditText1;
    private EditText mEditText2;
    private AttributeSet mAttributeSet;
    private Instrumentation mInstrumentation;

    @Rule
    public ActivityTestRule<EditTextCtsActivity> mActivityRule =
            new ActivityTestRule<>(EditTextCtsActivity.class);

    @Before
    public void setup() {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
        mActivity = mActivityRule.getActivity();
        mEditText1 = (EditText) mActivity.findViewById(R.id.edittext_simple1);
        mEditText2 = (EditText) mActivity.findViewById(R.id.edittext_simple2);

        XmlPullParser parser = mActivity.getResources().getXml(R.layout.edittext_layout);
        mAttributeSet = Xml.asAttributeSet(parser);
    }

    @After
    public void teardown() throws Throwable {
        mActivityRule.runOnUiThread(() -> mEditText1.setSingleLine(false));
    }

    @Test
    public void testConstructor() {
        new EditText(mActivity);

        new EditText(mActivity, null);

        new EditText(mActivity, null, 0);

        new EditText(mActivity, mAttributeSet);

        new EditText(mActivity, mAttributeSet, 0);
    }

    @Test(expected=NullPointerException.class)
    public void testConstructorNullContext1() {
        new EditText(null);
    }

    @Test(expected=NullPointerException.class)
    public void testConstructorNullContext2() {
        new EditText(null, null);
    }

    @UiThreadTest
    @Test
    public void testAccessText() {
        mEditText1.setText("android", BufferType.NORMAL);
        assertTrue(TextUtils.equals("android", mEditText1.getText()));

        mEditText1.setText("", BufferType.SPANNABLE);
        assertEquals(0, mEditText1.getText().length());

        mEditText1.setText(null, BufferType.EDITABLE);
        assertEquals(0, mEditText1.getText().length());
    }

    @UiThreadTest
    @Test
    public void testSetSelectionIndex() {
        mEditText1.setText("android", BufferType.EDITABLE);
        int position = 4;
        mEditText1.setSelection(position);
        assertEquals(position, mEditText1.getSelectionStart());
        assertEquals(position, mEditText1.getSelectionEnd());

        position = 0;
        mEditText1.setSelection(position);
        assertEquals(position, mEditText1.getSelectionStart());
        assertEquals(position, mEditText1.getSelectionEnd());
    }

    @UiThreadTest
    @Test(expected=IndexOutOfBoundsException.class)
    public void testSetSelectionIndexBeforeFirst() {
        mEditText1.setText("android", BufferType.EDITABLE);
        mEditText1.setSelection(-1);
    }

    @UiThreadTest
    @Test(expected=IndexOutOfBoundsException.class)
    public void testSetSelectionIndexAfterLast() {
        mEditText1.setText("android", BufferType.EDITABLE);
        mEditText1.setSelection(mEditText1.getText().length() + 1);
    }

    @UiThreadTest
    @Test
    public void testSetSelectionStartEnd() {
        mEditText1.setText("android", BufferType.EDITABLE);
        int start = 1;
        int end = 2;
        mEditText1.setSelection(start, end);
        assertEquals(start, mEditText1.getSelectionStart());
        assertEquals(end, mEditText1.getSelectionEnd());

        start = 0;
        end = 0;
        mEditText1.setSelection(start, end);
        assertEquals(start, mEditText1.getSelectionStart());
        assertEquals(end, mEditText1.getSelectionEnd());

        start = 7;
        end = 1;
        mEditText1.setSelection(start, end);
        assertEquals(start, mEditText1.getSelectionStart());
        assertEquals(end, mEditText1.getSelectionEnd());
    }

    @UiThreadTest
    @Test(expected=IndexOutOfBoundsException.class)
    public void testSetSelectionStartEndBeforeFirst() {
        mEditText1.setText("android", BufferType.EDITABLE);
        mEditText1.setSelection(-5, -1);
    }

    @UiThreadTest
    @Test(expected=IndexOutOfBoundsException.class)
    public void testSetSelectionStartEndAfterLast() {
        mEditText1.setText("android", BufferType.EDITABLE);
        mEditText1.setSelection(5, mEditText1.getText().length() + 1);
    }

    @UiThreadTest
    @Test
    public void testSelectAll() {
        String string = "android";
        mEditText1.setText(string, BufferType.EDITABLE);
        mEditText1.selectAll();
        assertEquals(0, mEditText1.getSelectionStart());
        assertEquals(string.length(), mEditText1.getSelectionEnd());

        mEditText1.setText("", BufferType.EDITABLE);
        mEditText1.selectAll();
        assertEquals(0, mEditText1.getSelectionStart());
        assertEquals(0, mEditText1.getSelectionEnd());

        mEditText1.setText(null, BufferType.EDITABLE);
        mEditText1.selectAll();
        assertEquals(0, mEditText1.getSelectionStart());
        assertEquals(0, mEditText1.getSelectionEnd());
    }

    @UiThreadTest
    @Test
    public void testExtendSelection() {
        mEditText1.setText("android", BufferType.EDITABLE);
        int start = 0;
        int end = 0;
        mEditText1.setSelection(start, end);
        assertEquals(start, mEditText1.getSelectionStart());
        assertEquals(end, mEditText1.getSelectionEnd());

        end = 6;
        mEditText1.extendSelection(end);
        assertEquals(start, mEditText1.getSelectionStart());
        assertEquals(end, mEditText1.getSelectionEnd());

        start = 0;
        end = 0;
        mEditText1.setSelection(start);
        mEditText1.extendSelection(end);
        assertEquals(start, mEditText1.getSelectionStart());
        assertEquals(end, mEditText1.getSelectionEnd());
    }

    @UiThreadTest
    @Test(expected=IndexOutOfBoundsException.class)
    public void testExtendSelectionBeyondLast() {
        mEditText1.setText("android", BufferType.EDITABLE);
        mEditText1.setSelection(0, 4);
        mEditText1.extendSelection(10);
    }

    @Test
    public void testGetDefaultEditable() {
        MockEditText mockEditText = new MockEditText(mActivity, mAttributeSet);

        assertTrue(mockEditText.getDefaultEditable());
    }

    @Test
    public void testAutoSizeNotSupported() {
        DisplayMetrics metrics = mActivity.getResources().getDisplayMetrics();
        EditText autoSizeEditText = (EditText) mActivity.findViewById(R.id.edittext_autosize);

        // If auto-size would work then the text size would be less then 50dp (the value set in the
        // layout file).
        final int sizeSetInPixels = (int) (0.5f + TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, 50f, metrics));
        assertEquals(sizeSetInPixels, (int) autoSizeEditText.getTextSize());
    }

    @Test
    public void testGetDefaultMovementMethod() {
        MockEditText mockEditText = new MockEditText(mActivity, mAttributeSet);
        MovementMethod method1 = mockEditText.getDefaultMovementMethod();
        MovementMethod method2 = mockEditText.getDefaultMovementMethod();

        assertNotNull(method1);
        assertTrue(method1 instanceof ArrowKeyMovementMethod);

        assertSame(method1, method2);
    }

    @UiThreadTest
    @Test
    public void testSetEllipsize() {
        assertNull(mEditText1.getEllipsize());

        mEditText1.setEllipsize(TextUtils.TruncateAt.START);
        assertSame(TextUtils.TruncateAt.START, mEditText1.getEllipsize());
    }

    @UiThreadTest
    @Test(expected=IllegalArgumentException.class)
    public void testSetEllipsizeMarquee() {
        mEditText1.setEllipsize(TextUtils.TruncateAt.MARQUEE);
    }

    @UiThreadTest
    @Test
    public void testOnSaveInstanceState_savesTextStateWhenFreezesTextIsTrue() {
        // prepare EditText for before saveInstanceState
        final String testStr = "This is a test str";
        mEditText1.setFreezesText(true);
        mEditText1.setText(testStr);

        // prepare EditText for after saveInstanceState
        mEditText2.setFreezesText(true);

        mEditText2.onRestoreInstanceState(mEditText1.onSaveInstanceState());

        assertTrue(TextUtils.equals(mEditText1.getText(), mEditText2.getText()));
    }

    @UiThreadTest
    @Test
    public void testOnSaveInstanceState_savesTextStateWhenFreezesTextIfFalse() {
        // prepare EditText for before saveInstanceState
        final String testStr = "This is a test str";
        mEditText1.setFreezesText(false);
        mEditText1.setText(testStr);

        // prepare EditText for after saveInstanceState
        mEditText2.setFreezesText(false);

        mEditText2.onRestoreInstanceState(mEditText1.onSaveInstanceState());

        assertTrue(TextUtils.equals(mEditText1.getText(), mEditText2.getText()));
    }

    @UiThreadTest
    @Test
    public void testOnSaveInstanceState_savesSelectionStateWhenFreezesTextIsFalse() {
        // prepare EditText for before saveInstanceState
        final String testStr = "This is a test str";
        mEditText1.setFreezesText(false);
        mEditText1.setText(testStr);
        mEditText1.setSelection(2, testStr.length() - 2);

        // prepare EditText for after saveInstanceState
        mEditText2.setFreezesText(false);

        mEditText2.onRestoreInstanceState(mEditText1.onSaveInstanceState());

        assertEquals(mEditText1.getSelectionStart(), mEditText2.getSelectionStart());
        assertEquals(mEditText1.getSelectionEnd(), mEditText2.getSelectionEnd());
    }

    @UiThreadTest
    @Test
    public void testOnSaveInstanceState_savesSelectionStateWhenFreezesTextIsTrue() {
        // prepare EditText for before saveInstanceState
        final String testStr = "This is a test str";
        mEditText1.setFreezesText(true);
        mEditText1.setText(testStr);
        mEditText1.setSelection(2, testStr.length() - 2);

        // prepare EditText for after saveInstanceState
        mEditText2.setFreezesText(true);

        mEditText2.onRestoreInstanceState(mEditText1.onSaveInstanceState());

        assertEquals(mEditText1.getSelectionStart(), mEditText2.getSelectionStart());
        assertEquals(mEditText1.getSelectionEnd(), mEditText2.getSelectionEnd());
    }

    private boolean isWatch() {
        return (mActivity.getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_TYPE_WATCH) == Configuration.UI_MODE_TYPE_WATCH;
    }

    @Test
    public void testHyphenationFrequencyDefaultValue() {
        final Context context = InstrumentationRegistry.getTargetContext();
        final EditText editText = new EditText(context);

        // Hypenation is enabled by default on watches to fit more text on their tiny screens.
        if (isWatch()) {
            assertEquals(Layout.HYPHENATION_FREQUENCY_NORMAL, editText.getHyphenationFrequency());
        } else {
            assertEquals(Layout.HYPHENATION_FREQUENCY_NONE, editText.getHyphenationFrequency());
        }
    }

    @Test
    public void testBreakStrategyDefaultValue() {
        final Context context = InstrumentationRegistry.getTargetContext();
        final EditText editText = new EditText(context);
        assertEquals(Layout.BREAK_STRATEGY_SIMPLE, editText.getBreakStrategy());
    }

    @UiThreadTest
    @Test
    public void testOnInitializeA11yNodeInfo_hasAccessibilityActions() {
        mEditText1.setText("android");
        final AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mEditText1.onInitializeAccessibilityNodeInfo(info);
        List<AccessibilityNodeInfo.AccessibilityAction> actionList = info.getActionList();
        assertTrue("info's isLongClickable should be true",
                info.isLongClickable());
        assertTrue("info should have ACTION_LONG_CLICK",
                actionList.contains(AccessibilityNodeInfo.AccessibilityAction.ACTION_LONG_CLICK));
        assertTrue("info should have ACTION_SET_TEXT",
                actionList.contains(AccessibilityNodeInfo.AccessibilityAction.ACTION_SET_TEXT));

    }

    private class MockEditText extends EditText {
        public MockEditText(Context context) {
            super(context);
        }

        public MockEditText(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        public MockEditText(Context context, AttributeSet attrs, int defStyle) {
            super(context, attrs, defStyle);
        }

        @Override
        protected boolean getDefaultEditable() {
            return super.getDefaultEditable();
        }

        @Override
        protected MovementMethod getDefaultMovementMethod() {
            return super.getDefaultMovementMethod();
        }
    }

    @Test
    public void testGetTextNonEditable() {
        // This subclass calls getText before the object is fully constructed. This should not cause
        // a null pointer exception.
        GetTextEditText editText = new GetTextEditText(mActivity);
    }

    private class GetTextEditText extends EditText {

        GetTextEditText(Context context) {
            super(context);
        }

        @Override
        public void setText(CharSequence text, BufferType type) {
            Editable currentText = getText();
            super.setText(text, type);
        }
    }

    @Test
    public void testGetTextBeforeConstructor() {
        // This subclass calls getText before the TextView constructor. This should not cause
        // a null pointer exception.
        GetTextEditText2 editText = new GetTextEditText2(mActivity);
    }

    private class GetTextEditText2 extends EditText {

        GetTextEditText2(Context context) {
            super(context);
        }

        @Override
        public void setOverScrollMode(int overScrollMode) {
            // This method is called by the View constructor before the TextView/EditText
            // constructors.
            Editable text = getText();
        }
    }

    @Test
    public void testCursorDrag() throws Exception {
        AtomicReference<SparseArray<Point>> dragStartEnd = new AtomicReference<>();
        String text = "Hello world, how are you doing today?";
        mInstrumentation.runOnMainSync(() -> {
            mEditText1.setText(text);
            mEditText1.requestFocus();
            mEditText1.setSelection(text.length());
            dragStartEnd.set(getScreenCoords(mEditText1, text.indexOf("y?"), text.indexOf("el")));
        });
        assertCursorPosition(mEditText1, text.length());
        assertTrue(mEditText1.hasFocus());

        // Simulate a drag gesture. The cursor should end up at the position where the finger is
        // lifted.
        CtsTouchUtils.emulateDragGesture(mInstrumentation, mActivityRule, dragStartEnd.get());
        assertCursorPosition(mEditText1, text.indexOf("el"));
    }

    private static void assertCursorPosition(TextView textView, int expectedOffset) {
        assertEquals(expectedOffset, textView.getSelectionStart());
        assertEquals(expectedOffset, textView.getSelectionEnd());
    }

    private static SparseArray<Point> getScreenCoords(TextView textView, int ... offsets) {
        SparseArray<Point> result  = new SparseArray<>(offsets.length);
        for (int i = 0; i < offsets.length; i++) {
            result.append(i, getScreenCoords(textView, offsets[i]));
        }
        return result;
    }

    private static Point getScreenCoords(TextView textView, int offset) {
        // Get the x,y coordinates for the given offset in the text. These are relative to the view.
        int x = (int) textView.getLayout().getPrimaryHorizontal(offset);
        int line = textView.getLayout().getLineForOffset(offset);
        int yTop = textView.getLayout().getLineTop(line);
        int yBottom = textView.getLayout().getLineBottom(line);
        int y = (yTop + yBottom) / 2;

        // Get the x,y coordinates of the view.
        final int[] viewOnScreenXY = new int[2];
        textView.getLocationOnScreen(viewOnScreenXY);

        // Return the absolute screen coordinates for the given offset in the text.
        return new Point(viewOnScreenXY[0] + x, viewOnScreenXY[1] + y);
    }

    @Test
    public void testEnterKey() throws Throwable {
        mActivityRule.runOnUiThread(() -> {
            mEditText1.setSingleLine(true);
            mEditText1.requestFocus();
        });

        CtsKeyEventUtil.sendKeyDownUp(mInstrumentation, mEditText1, KeyEvent.KEYCODE_ENTER);
        mInstrumentation.waitForIdleSync();
        assertTrue(mEditText2.hasFocus());

        mActivityRule.runOnUiThread(() -> mEditText1.requestFocus());
        CtsKeyEventUtil.sendKeyDownUp(mInstrumentation, mEditText1, KeyEvent.KEYCODE_NUMPAD_ENTER);
        assertTrue(mEditText2.hasFocus());
    }
}
