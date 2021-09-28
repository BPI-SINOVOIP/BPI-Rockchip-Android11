/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.inputmethodservice.cts.devicetest;

import static android.content.Intent.ACTION_CLOSE_SYSTEM_DIALOGS;
import static android.content.Intent.FLAG_RECEIVER_FOREGROUND;
import static android.inputmethodservice.cts.DeviceEvent.isFrom;
import static android.inputmethodservice.cts.DeviceEvent.isNewerThan;
import static android.inputmethodservice.cts.DeviceEvent.isType;
import static android.inputmethodservice.cts.common.BusyWaitUtils.pollingCheck;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_BIND_INPUT;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_CREATE;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_DESTROY;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_START_INPUT;
import static android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType.ON_UNBIND_INPUT;
import static android.inputmethodservice.cts.common.ImeCommandConstants.ACTION_IME_COMMAND;
import static android.inputmethodservice.cts.common.ImeCommandConstants.COMMAND_SWITCH_INPUT_METHOD;
import static android.inputmethodservice.cts.common.ImeCommandConstants.COMMAND_SWITCH_INPUT_METHOD_WITH_SUBTYPE;
import static android.inputmethodservice.cts.common.ImeCommandConstants.COMMAND_SWITCH_TO_NEXT_INPUT;
import static android.inputmethodservice.cts.common.ImeCommandConstants.COMMAND_SWITCH_TO_PREVIOUS_INPUT;
import static android.inputmethodservice.cts.common.ImeCommandConstants.EXTRA_ARG_STRING1;
import static android.inputmethodservice.cts.common.ImeCommandConstants.EXTRA_COMMAND;
import static android.inputmethodservice.cts.devicetest.MoreCollectors.startingFrom;

import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.inputmethodservice.cts.DeviceEvent;
import android.inputmethodservice.cts.common.DeviceEventConstants.DeviceEventType;
import android.inputmethodservice.cts.common.EditTextAppConstants;
import android.inputmethodservice.cts.common.Ime1Constants;
import android.inputmethodservice.cts.common.Ime2Constants;
import android.inputmethodservice.cts.common.test.ShellCommandUtils;
import android.inputmethodservice.cts.devicetest.SequenceMatcher.MatchResult;
import android.os.PowerManager;
import android.os.SystemClock;
import android.support.test.uiautomator.UiObject2;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InputMethodSubtype;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import java.util.function.IntFunction;
import java.util.function.Predicate;
import java.util.stream.Collector;

/**
 * Test general lifecycle events around InputMethodService.
 */
@RunWith(AndroidJUnit4.class)
public class InputMethodServiceDeviceTest {

    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(7);

    /** Test to check CtsInputMethod1 receives onCreate and onStartInput. */
    @Test
    public void testCreateIme1() throws Throwable {
        final TestHelper helper = new TestHelper();

        final long startActivityTime = SystemClock.uptimeMillis();
        helper.launchActivity(EditTextAppConstants.PACKAGE, EditTextAppConstants.CLASS,
                EditTextAppConstants.URI);

        pollingCheck(() -> helper.queryAllEvents()
                        .collect(startingFrom(helper.isStartOfTest()))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_CREATE))),
                TIMEOUT, "CtsInputMethod1.onCreate is called");
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(startActivityTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_START_INPUT))),
                TIMEOUT, "CtsInputMethod1.onStartInput is called");
    }

    /** Test to check IME is switched from CtsInputMethod1 to CtsInputMethod2. */
    @Test
    public void testSwitchIme1ToIme2() throws Throwable {
        final TestHelper helper = new TestHelper();

        final long startActivityTime = SystemClock.uptimeMillis();
        helper.launchActivity(EditTextAppConstants.PACKAGE, EditTextAppConstants.CLASS,
                EditTextAppConstants.URI);

        pollingCheck(() -> helper.queryAllEvents()
                        .collect(startingFrom(helper.isStartOfTest()))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_CREATE))),
                TIMEOUT, "CtsInputMethod1.onCreate is called");
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(startActivityTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_START_INPUT))),
                TIMEOUT, "CtsInputMethod1.onStartInput is called");

        helper.findUiObject(EditTextAppConstants.EDIT_TEXT_RES_NAME).click();

        // Switch IME from CtsInputMethod1 to CtsInputMethod2.
        final long switchImeTime = SystemClock.uptimeMillis();
        helper.shell(ShellCommandUtils.broadcastIntent(
                ACTION_IME_COMMAND, Ime1Constants.PACKAGE,
                "-e", EXTRA_COMMAND, COMMAND_SWITCH_INPUT_METHOD,
                "-e", EXTRA_ARG_STRING1, Ime2Constants.IME_ID));

        pollingCheck(() -> helper.shell(ShellCommandUtils.getCurrentIme())
                        .equals(Ime2Constants.IME_ID),
                TIMEOUT, "CtsInputMethod2 is current IME");
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(switchImeTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_DESTROY))),
                TIMEOUT, "CtsInputMethod1.onDestroy is called");
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(switchImeTime))
                        .filter(isFrom(Ime2Constants.CLASS))
                        .collect(sequenceOfTypes(ON_CREATE, ON_BIND_INPUT, ON_START_INPUT))
                        .matched(),
                TIMEOUT,
                "CtsInputMethod2.onCreate, onBindInput, and onStartInput are called"
                        + " in sequence");
    }

    /**
     * Test {@link android.inputmethodservice.InputMethodService#switchInputMethod(String,
     * InputMethodSubtype)}.
     */
    @Test
    public void testSwitchInputMethod() throws Throwable {
        final TestHelper helper = new TestHelper();
        final long startActivityTime = SystemClock.uptimeMillis();
        helper.launchActivity(EditTextAppConstants.PACKAGE, EditTextAppConstants.CLASS,
                EditTextAppConstants.URI);
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(startActivityTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_START_INPUT))),
                TIMEOUT, "CtsInputMethod1.onStartInput is called");
        helper.findUiObject(EditTextAppConstants.EDIT_TEXT_RES_NAME).click();

        final long setImeTime = SystemClock.uptimeMillis();
        // call setInputMethodAndSubtype(IME2, null)
        helper.shell(ShellCommandUtils.broadcastIntent(
                ACTION_IME_COMMAND, Ime1Constants.PACKAGE,
                "-e", EXTRA_COMMAND, COMMAND_SWITCH_INPUT_METHOD_WITH_SUBTYPE,
                "-e", EXTRA_ARG_STRING1, Ime2Constants.IME_ID));
        pollingCheck(() -> helper.shell(ShellCommandUtils.getCurrentIme())
                        .equals(Ime2Constants.IME_ID),
                TIMEOUT, "CtsInputMethod2 is current IME");
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(setImeTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_DESTROY))),
                TIMEOUT, "CtsInputMethod1.onDestroy is called");
    }

    /**
     * Test {@link android.inputmethodservice.InputMethodService#switchToNextInputMethod(boolean)}.
     */
    @Test
    public void testSwitchToNextInputMethod() throws Throwable {
        final TestHelper helper = new TestHelper();
        final long startActivityTime = SystemClock.uptimeMillis();
        helper.launchActivity(EditTextAppConstants.PACKAGE, EditTextAppConstants.CLASS,
                EditTextAppConstants.URI);
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(startActivityTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_START_INPUT))),
                TIMEOUT, "CtsInputMethod1.onStartInput is called");
        helper.findUiObject(EditTextAppConstants.EDIT_TEXT_RES_NAME).click();

        pollingCheck(() -> helper.shell(ShellCommandUtils.getCurrentIme())
                        .equals(Ime1Constants.IME_ID),
                TIMEOUT, "CtsInputMethod1 is current IME");
        helper.shell(ShellCommandUtils.broadcastIntent(
                ACTION_IME_COMMAND, Ime1Constants.PACKAGE,
                "-e", EXTRA_COMMAND, COMMAND_SWITCH_TO_NEXT_INPUT));
        pollingCheck(() -> !helper.shell(ShellCommandUtils.getCurrentIme())
                        .equals(Ime1Constants.IME_ID),
                TIMEOUT, "CtsInputMethod1 shouldn't be current IME");
    }

    /**
     * Test {@link android.inputmethodservice.InputMethodService#switchToPreviousInputMethod()}.
     */
    @Test
    public void switchToPreviousInputMethod() throws Throwable {
        final TestHelper helper = new TestHelper();
        final long startActivityTime = SystemClock.uptimeMillis();
        helper.launchActivity(EditTextAppConstants.PACKAGE, EditTextAppConstants.CLASS,
                EditTextAppConstants.URI);
        helper.findUiObject(EditTextAppConstants.EDIT_TEXT_RES_NAME).click();

        final String initialIme = helper.shell(ShellCommandUtils.getCurrentIme());
        helper.shell(ShellCommandUtils.setCurrentImeSync(Ime2Constants.IME_ID));
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(startActivityTime))
                        .anyMatch(isFrom(Ime2Constants.CLASS).and(isType(ON_START_INPUT))),
                TIMEOUT, "CtsInputMethod2.onStartInput is called");
        helper.shell(ShellCommandUtils.broadcastIntent(
                ACTION_IME_COMMAND, Ime2Constants.PACKAGE,
                "-e", EXTRA_COMMAND, COMMAND_SWITCH_TO_PREVIOUS_INPUT));
        pollingCheck(() -> helper.shell(ShellCommandUtils.getCurrentIme())
                        .equals(initialIme),
                TIMEOUT, initialIme + " is current IME");
    }

    /**
     * Test if uninstalling the currently selected IME then selecting another IME triggers standard
     * startInput/bindInput sequence.
     */
    @Test
    public void testInputUnbindsOnImeStopped() throws Throwable {
        final TestHelper helper = new TestHelper();
        final long startActivityTime = SystemClock.uptimeMillis();
        helper.launchActivity(EditTextAppConstants.PACKAGE, EditTextAppConstants.CLASS,
                EditTextAppConstants.URI);
        final UiObject2 editText = helper.findUiObject(EditTextAppConstants.EDIT_TEXT_RES_NAME);
        editText.click();

        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(startActivityTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_START_INPUT))),
                TIMEOUT, "CtsInputMethod1.onStartInput is called");
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(startActivityTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_BIND_INPUT))),
                TIMEOUT, "CtsInputMethod1.onBindInput is called");

        final long imeForceStopTime = SystemClock.uptimeMillis();
        helper.shell(ShellCommandUtils.uninstallPackage(Ime1Constants.PACKAGE));

        helper.shell(ShellCommandUtils.setCurrentImeSync(Ime2Constants.IME_ID));
        editText.click();
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(imeForceStopTime))
                        .anyMatch(isFrom(Ime2Constants.CLASS).and(isType(ON_START_INPUT))),
                TIMEOUT, "CtsInputMethod2.onStartInput is called");
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(imeForceStopTime))
                        .anyMatch(isFrom(Ime2Constants.CLASS).and(isType(ON_BIND_INPUT))),
                TIMEOUT, "CtsInputMethod2.onBindInput is called");
    }

    /**
     * Test if uninstalling the currently running IME client triggers
     * {@link android.inputmethodservice.InputMethodService#onUnbindInput()}.
     */
    @Test
    public void testInputUnbindsOnAppStopped() throws Throwable {
        final TestHelper helper = new TestHelper();
        final long startActivityTime = SystemClock.uptimeMillis();
        helper.launchActivity(EditTextAppConstants.PACKAGE, EditTextAppConstants.CLASS,
                EditTextAppConstants.URI);
        helper.findUiObject(EditTextAppConstants.EDIT_TEXT_RES_NAME).click();

        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(startActivityTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_START_INPUT))),
                TIMEOUT, "CtsInputMethod1.onStartInput is called");
        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(startActivityTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_BIND_INPUT))),
                TIMEOUT, "CtsInputMethod1.onBindInput is called");

        helper.shell(ShellCommandUtils.uninstallPackage(EditTextAppConstants.PACKAGE));

        pollingCheck(() -> helper.queryAllEvents()
                        .filter(isNewerThan(startActivityTime))
                        .anyMatch(isFrom(Ime1Constants.CLASS).and(isType(ON_UNBIND_INPUT))),
                TIMEOUT, "CtsInputMethod1.onUnBindInput is called");
    }

    /**
     * Test if IMEs remain to be visible after switching to other IMEs.
     *
     * <p>Regression test for Bug 152876819.</p>
     */
    @Test
    public void testImeVisibilityAfterImeSwitching() throws Throwable {
        final TestHelper helper = new TestHelper();

        helper.launchActivity(EditTextAppConstants.PACKAGE, EditTextAppConstants.CLASS,
                EditTextAppConstants.URI);

        helper.findUiObject(EditTextAppConstants.EDIT_TEXT_RES_NAME).click();

        InputMethodVisibilityVerifier.assertIme1Visible(TIMEOUT);

        // Switch IME from CtsInputMethod1 to CtsInputMethod2.
        helper.shell(ShellCommandUtils.broadcastIntent(
                ACTION_IME_COMMAND, Ime1Constants.PACKAGE,
                "-e", EXTRA_COMMAND, COMMAND_SWITCH_INPUT_METHOD,
                "-e", EXTRA_ARG_STRING1, Ime2Constants.IME_ID));

        InputMethodVisibilityVerifier.assertIme2Visible(TIMEOUT);

        // Switch IME from CtsInputMethod2 to CtsInputMethod1.
        helper.shell(ShellCommandUtils.broadcastIntent(
                ACTION_IME_COMMAND, Ime2Constants.PACKAGE,
                "-e", EXTRA_COMMAND, COMMAND_SWITCH_INPUT_METHOD,
                "-e", EXTRA_ARG_STRING1, Ime1Constants.IME_ID));

        InputMethodVisibilityVerifier.assertIme1Visible(TIMEOUT);

        // Switch IME from CtsInputMethod1 to CtsInputMethod2.
        helper.shell(ShellCommandUtils.broadcastIntent(
                ACTION_IME_COMMAND, Ime1Constants.PACKAGE,
                "-e", EXTRA_COMMAND, COMMAND_SWITCH_INPUT_METHOD,
                "-e", EXTRA_ARG_STRING1, Ime2Constants.IME_ID));

        InputMethodVisibilityVerifier.assertIme2Visible(TIMEOUT);

        // Make sure the IME switch UI still works after device screen off / on with focusing
        // same Editor.
        turnScreenOff(helper);
        turnScreenOn(helper);
        helper.shell(ShellCommandUtils.unlockScreen());
        assertTrue(helper.findUiObject(EditTextAppConstants.EDIT_TEXT_RES_NAME).isFocused());

        // Switch IME from CtsInputMethod2 to CtsInputMethod1.
        showInputMethodPicker(helper);
        helper.shell(ShellCommandUtils.broadcastIntent(
                ACTION_IME_COMMAND, Ime2Constants.PACKAGE,
                "-e", EXTRA_COMMAND, COMMAND_SWITCH_INPUT_METHOD,
                "-e", EXTRA_ARG_STRING1, Ime1Constants.IME_ID));

        InputMethodVisibilityVerifier.assertIme1Visible(TIMEOUT);

        // Switch IME from CtsInputMethod1 to CtsInputMethod2.
        showInputMethodPicker(helper);
        helper.shell(ShellCommandUtils.broadcastIntent(
                ACTION_IME_COMMAND, Ime1Constants.PACKAGE,
                "-e", EXTRA_COMMAND, COMMAND_SWITCH_INPUT_METHOD,
                "-e", EXTRA_ARG_STRING1, Ime2Constants.IME_ID));

        InputMethodVisibilityVerifier.assertIme2Visible(TIMEOUT);
    }

    /**
     * Build stream collector of {@link DeviceEvent} collecting sequence that elements have
     * specified types.
     *
     * @param types {@link DeviceEventType}s that elements of sequence should have.
     * @return {@link java.util.stream.Collector} that corrects the sequence.
     */
    private static Collector<DeviceEvent, ?, MatchResult<DeviceEvent>> sequenceOfTypes(
            final DeviceEventType... types) {
        final IntFunction<Predicate<DeviceEvent>[]> arraySupplier = Predicate[]::new;
        return SequenceMatcher.of(Arrays.stream(types)
                .map(DeviceEvent::isType)
                .toArray(arraySupplier));
    }

    /**
     * Call a command to turn screen On.
     *
     * This method will wait until the power state is interactive with {@link
     * PowerManager#isInteractive()}.
     */
    private static void turnScreenOn(TestHelper helper) throws Exception {
        final Context context = InstrumentationRegistry.getInstrumentation().getContext();
        final PowerManager pm = context.getSystemService(PowerManager.class);
        helper.shell(ShellCommandUtils.wakeUp());
        pollingCheck(() -> pm != null && pm.isInteractive(), TIMEOUT,
                "Device does not wake up within the timeout period");
    }

    /**
     * Call a command to turn screen off.
     *
     * This method will wait until the power state is *NOT* interactive with
     * {@link PowerManager#isInteractive()}.
     * Note that {@link PowerManager#isInteractive()} may not return {@code true} when the device
     * enables Aod mode, recommend to add (@link DisableScreenDozeRule} in the test to disable Aod
     * for making power state reliable.
     */
    private static void turnScreenOff(TestHelper helper) throws Exception {
        final Context context = InstrumentationRegistry.getInstrumentation().getContext();
        final PowerManager pm = context.getSystemService(PowerManager.class);
        helper.shell(ShellCommandUtils.sleepDevice());
        pollingCheck(() -> pm != null && !pm.isInteractive(), TIMEOUT,
                "Device does not sleep within the timeout period");
    }

    private static void showInputMethodPicker(TestHelper helper) throws Exception {
        // Test InputMethodManager#showInputMethodPicker() works as expected.
        final Context context = InstrumentationRegistry.getInstrumentation().getContext();
        final InputMethodManager imm = context.getSystemService(InputMethodManager.class);
        helper.shell(ShellCommandUtils.showImePicker());
        pollingCheck(() -> imm.isInputMethodPickerShown(), TIMEOUT,
                "InputMethod picker should be shown");
    }
}
