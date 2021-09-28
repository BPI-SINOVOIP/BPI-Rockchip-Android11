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

package android.app.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.app.stubs.DialogStubActivity;
import android.app.stubs.R;
import android.content.DialogInterface;
import android.view.KeyEvent;
import android.widget.Button;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.PollingCheck;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/*
 * Test AlertDialog
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class AlertDialogTest {
    private Instrumentation mInstrumentation;
    private DialogStubActivity mActivity;
    private Button mPositiveButton;
    private Button mNegativeButton;
    private Button mNeutralButton;

    @Rule
    public ActivityTestRule<DialogStubActivity> mActivityRule =
            new ActivityTestRule<>(DialogStubActivity.class, true, false);

    @Before
    public void setUp() throws Exception {
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
    }

    protected void startDialogActivity(int dialogNumber) {
        mActivity = DialogStubActivity.startDialogActivity(mActivityRule, dialogNumber);

        PollingCheck.waitFor(mActivity.getDialog()::isShowing);
        PollingCheck.waitFor(mActivity.getDialog().getWindow().getDecorView()::hasWindowFocus);
    }

    @Test
    public void testAlertDialog() throws Throwable {
        doTestAlertDialog(DialogStubActivity.TEST_ALERTDIALOG);
    }

    private void doTestAlertDialog(int index) throws Throwable {
        startDialogActivity(index);
        assertTrue(mActivity.getDialog().isShowing());

        mPositiveButton = ((AlertDialog) (mActivity.getDialog())).getButton(
                DialogInterface.BUTTON_POSITIVE);
        assertNotNull(mPositiveButton);
        assertEquals(mActivity.getString(R.string.alert_dialog_positive),
                mPositiveButton.getText());
        mNeutralButton = ((AlertDialog) (mActivity.getDialog())).getButton(
                DialogInterface.BUTTON_NEUTRAL);
        assertNotNull(mNeutralButton);
        assertEquals(mActivity.getString(R.string.alert_dialog_neutral),
                mNeutralButton.getText());
        mNegativeButton = ((AlertDialog) (mActivity.getDialog())).getButton(
                DialogInterface.BUTTON_NEGATIVE);
        assertNotNull(mNegativeButton);
        assertEquals(mActivity.getString(R.string.alert_dialog_negative),
                mNegativeButton.getText());

        assertFalse(mActivity.isPositiveButtonClicked);
        performClick(mPositiveButton);
        PollingCheck.waitFor(() -> mActivity.isPositiveButtonClicked);

        assertFalse(mActivity.isNegativeButtonClicked);
        performClick(mNegativeButton);
        PollingCheck.waitFor(() -> mActivity.isNegativeButtonClicked);

        assertFalse(mActivity.isNeutralButtonClicked);
        performClick(mNeutralButton);
        PollingCheck.waitFor(() -> mActivity.isNeutralButtonClicked);
    }

    @Test
    public void testAlertDialogDeprecatedAPI() throws Throwable {
        doTestAlertDialog(DialogStubActivity.TEST_ALERTDIALOG_DEPRECATED);
    }

    private void testAlertDialogAPIWithMessage(final boolean useDeprecatedAPIs) throws Throwable {
        startDialogActivity(useDeprecatedAPIs
                ? DialogStubActivity.TEST_ALERTDIALOG_DEPRECATED_WITH_MESSAGE
                : DialogStubActivity.TEST_ALERTDIALOG_WITH_MESSAGE);

        assertTrue(mActivity.getDialog().isShowing());

        mPositiveButton = ((AlertDialog) (mActivity.getDialog())).getButton(
                DialogInterface.BUTTON_POSITIVE);
        assertNotNull(mPositiveButton);
        assertEquals(mActivity.getString(R.string.alert_dialog_positive),
                mPositiveButton.getText());
        mNegativeButton = ((AlertDialog) (mActivity.getDialog())).getButton(
                DialogInterface.BUTTON_NEGATIVE);
        assertNotNull(mNegativeButton);
        assertEquals(mActivity.getString(R.string.alert_dialog_negative),
                mNegativeButton.getText());
        mNeutralButton = ((AlertDialog) (mActivity.getDialog())).getButton(
                DialogInterface.BUTTON_NEUTRAL);
        assertNotNull(mNeutralButton);
        assertEquals(mActivity.getString(R.string.alert_dialog_neutral),
                mNeutralButton.getText());

        DialogStubActivity.buttonIndex = 0;
        performClick(mPositiveButton);
        PollingCheck.waitFor(() ->
                (DialogInterface.BUTTON_POSITIVE == DialogStubActivity.buttonIndex));

        DialogStubActivity.buttonIndex = 0;
        performClick(mNeutralButton);
        PollingCheck.waitFor(() ->
                (DialogInterface.BUTTON_NEUTRAL == DialogStubActivity.buttonIndex));

        DialogStubActivity.buttonIndex = 0;
        performClick(mNegativeButton);
        PollingCheck.waitFor(() ->
                (DialogInterface.BUTTON_NEGATIVE == DialogStubActivity.buttonIndex));
    }

    @Test
    public void testAlertDialogAPIWithMessageDeprecated() throws Throwable {
        testAlertDialogAPIWithMessage(true);
    }

    @Test
    public void testAlertDialogAPIWithMessageNotDeprecated() throws Throwable {
        testAlertDialogAPIWithMessage(false);
    }

    private void performClick(final Button button) throws Throwable {
        mActivityRule.runOnUiThread(() -> button.performClick());
    }

    @Test
    public void testCustomAlertDialog() {
        startDialogActivity(DialogStubActivity.TEST_CUSTOM_ALERTDIALOG);
        assertTrue(mActivity.getDialog().isShowing());
    }

    @Test
    public void testCustomAlertDialogView() {
        startDialogActivity(DialogStubActivity.TEST_CUSTOM_ALERTDIALOG_VIEW);
        assertTrue(mActivity.getDialog().isShowing());
    }

    @Test
    public void testCallback() {
        startDialogActivity(DialogStubActivity.TEST_ALERTDIALOG_CALLBACK);
        assertTrue(mActivity.onCreateCalled);

        mInstrumentation.sendKeySync(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_0));
        assertTrue(mActivity.onKeyDownCalled);
        mInstrumentation.sendKeySync(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_0));
        assertTrue(mActivity.onKeyUpCalled);
    }

    @Test
    public void testAlertDialogTheme() {
        startDialogActivity(DialogStubActivity.TEST_ALERTDIALOG_THEME);
        assertTrue(mActivity.getDialog().isShowing());
    }

    @Test
    public void testAlertDialogCancelable() {
        startDialogActivity(DialogStubActivity.TEST_ALERTDIALOG_CANCELABLE);
        assertTrue(mActivity.getDialog().isShowing());
        assertFalse(mActivity.onCancelCalled);
        mInstrumentation.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        PollingCheck.waitFor(() -> mActivity.onCancelCalled);
    }

    @Test
    public void testAlertDialogNotCancelable() {
        startDialogActivity(DialogStubActivity.TEST_ALERTDIALOG_NOT_CANCELABLE);
        assertTrue(mActivity.getDialog().isShowing());
        assertFalse(mActivity.onCancelCalled);
        mInstrumentation.sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        assertFalse(mActivity.onCancelCalled);
    }

    @Test
    public void testAlertDialogIconDrawable() {
        startDialogActivity(DialogStubActivity.TEST_ALERT_DIALOG_ICON_DRAWABLE);
        assertTrue(mActivity.getDialog().isShowing());
    }

    @Test
    public void testAlertDialogIconAttribute() {
        startDialogActivity(DialogStubActivity.TEST_ALERT_DIALOG_ICON_ATTRIBUTE);
        assertTrue(mActivity.getDialog().isShowing());
    }
}
