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

package android.inputmethodservice.cts.common.test;

/**
 * Constants of CtsInputMethodServiceDeviceTests.apk that contains tests on device side and
 * related activities for test.
 */
public final class DeviceTestConstants {

    // This is constants holding class, can't instantiate.
    private DeviceTestConstants() {}

    /** Package name of the APK. */
    public static final String PACKAGE = "android.inputmethodservice.cts.devicetest";

    /** APK file name. */
    public static final String APK = "CtsInputMethodServiceDeviceTests.apk";

    /**
     * Device test class: InputMethodServiceDeviceTest.
     */
    private static final String SERVICE_TEST =
            "android.inputmethodservice.cts.devicetest.InputMethodServiceDeviceTest";

    public static final TestInfo TEST_CREATE_IME1 =
            new TestInfo(PACKAGE, SERVICE_TEST, "testCreateIme1");
    public static final TestInfo TEST_SWITCH_IME1_TO_IME2 =
            new TestInfo(PACKAGE, SERVICE_TEST, "testSwitchIme1ToIme2");
    public static final TestInfo TEST_SWITCH_INPUTMETHOD =
            new TestInfo(PACKAGE, SERVICE_TEST, "testSwitchInputMethod");
    public static final TestInfo TEST_SWITCH_NEXT_INPUT =
            new TestInfo(PACKAGE, SERVICE_TEST, "testSwitchToNextInputMethod");
    public static final TestInfo TEST_SWITCH_PREVIOUS_INPUT =
            new TestInfo(PACKAGE, SERVICE_TEST, "switchToPreviousInputMethod");
    public static final TestInfo TEST_INPUT_UNBINDS_ON_IME_STOPPED =
            new TestInfo(PACKAGE, SERVICE_TEST, "testInputUnbindsOnImeStopped");
    public static final TestInfo TEST_INPUT_UNBINDS_ON_APP_STOPPED =
            new TestInfo(PACKAGE, SERVICE_TEST, "testInputUnbindsOnAppStopped");
    public static final TestInfo TEST_IME_VISIBILITY_AFTER_IME_SWITCHING =
            new TestInfo(PACKAGE, SERVICE_TEST, "testImeVisibilityAfterImeSwitching");

    /**
     * Device test class: ShellCommandDeviceTest.
     */
    private static final String SHELL_TEST =
            "android.inputmethodservice.cts.devicetest.ShellCommandDeviceTest";

    public static final TestInfo TEST_SHELL_COMMAND =
            new TestInfo(PACKAGE, SHELL_TEST, "testShellCommand");
    public static final TestInfo TEST_SHELL_COMMAND_IME =
            new TestInfo(PACKAGE, SHELL_TEST, "testShellCommandIme");
    public static final TestInfo TEST_SHELL_COMMAND_IME_LIST =
            new TestInfo(PACKAGE, SHELL_TEST, "testShellCommandImeList");
    public static final TestInfo TEST_SHELL_COMMAND_DUMP =
            new TestInfo(PACKAGE, SHELL_TEST, "testShellCommandDump");
    public static final TestInfo TEST_SHELL_COMMAND_HELP =
            new TestInfo(PACKAGE, SHELL_TEST, "testShellCommandHelp");

    /**
     * Device test class: InputMethodManagerDeviceTest.
     */
    private static final String MANAGER_TEST =
            "android.inputmethodservice.cts.devicetest.InputMethodManagerDeviceTest";

    public static final TestInfo TEST_IME1_IN_INPUT_METHOD_LIST =
            new TestInfo(PACKAGE, MANAGER_TEST, "testIme1InInputMethodList");
    public static final TestInfo TEST_IME1_NOT_IN_INPUT_METHOD_LIST =
            new TestInfo(PACKAGE, MANAGER_TEST, "testIme1NotInInputMethodList");
    public static final TestInfo TEST_IME1_IN_ENABLED_INPUT_METHOD_LIST =
            new TestInfo(PACKAGE, MANAGER_TEST, "testIme1InEnabledInputMethodList");
    public static final TestInfo TEST_IME1_NOT_IN_ENABLED_INPUT_METHOD_LIST =
            new TestInfo(PACKAGE, MANAGER_TEST, "testIme1NotInEnabledInputMethodList");

    public static final TestInfo TEST_IME2_IN_INPUT_METHOD_LIST =
            new TestInfo(PACKAGE, MANAGER_TEST, "testIme2InInputMethodList");
    public static final TestInfo TEST_IME2_NOT_IN_INPUT_METHOD_LIST =
            new TestInfo(PACKAGE, MANAGER_TEST, "testIme2NotInInputMethodList");
    public static final TestInfo TEST_IME2_IN_ENABLED_INPUT_METHOD_LIST =
            new TestInfo(PACKAGE, MANAGER_TEST, "testIme2InEnabledInputMethodList");
    public static final TestInfo TEST_IME2_NOT_IN_ENABLED_INPUT_METHOD_LIST =
            new TestInfo(PACKAGE, MANAGER_TEST, "testIme2NotInEnabledInputMethodList");

    public static final TestInfo TEST_IME1_IMPLICITLY_ENABLED_SUBTYPE_EXISTS =
            new TestInfo(PACKAGE, MANAGER_TEST, "testIme1ImplicitlyEnabledSubtypeExists");
    public static final TestInfo TEST_IME1_IMPLICITLY_ENABLED_SUBTYPE_NOT_EXIST =
            new TestInfo(PACKAGE, MANAGER_TEST, "testIme1ImplicitlyEnabledSubtypeNotExist");

    /**
     * Device test class: MultiUserDeviceTest.
     */
    private static final String MULTI_USER_TEST =
            "android.inputmethodservice.cts.devicetest.MultiUserDeviceTest";

    public static final TestInfo TEST_CONNECTING_TO_THE_SAME_USER_IME =
            new TestInfo(PACKAGE, MULTI_USER_TEST, "testConnectingToTheSameUserIme");

    /**
     * Device test class: NoOpTest.
     */
    private static final String NO_OP_TEST =
            "android.inputmethodservice.cts.devicetest.NoOpDeviceTest";

    public static final TestInfo TEST_WAIT_15SEC =
            new TestInfo(PACKAGE, NO_OP_TEST, "testWait15Sec");
}
