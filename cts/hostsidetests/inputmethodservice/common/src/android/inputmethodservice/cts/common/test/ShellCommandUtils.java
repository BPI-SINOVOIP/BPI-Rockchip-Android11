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

import java.util.Arrays;

/**
 * Utility class for preparing "adb shell" command.
 */
public final class ShellCommandUtils {

    // This is utility class, can't instantiate.
    private ShellCommandUtils() {}

    // Copied from android.content.pm.PackageManager#FEATURE_INPUT_METHODS.
    public static final String FEATURE_INPUT_METHODS = "android.software.input_methods";

    private static final String SETTING_DEFAULT_IME = "secure default_input_method";

    /** Command to get ID of current IME. */
    public static String getCurrentIme() {
        return "settings get " + SETTING_DEFAULT_IME;
    }

    /** Command to get ID of current IME. */
    public static String getCurrentIme(int userId) {
        return String.format("settings --user %d get %s", userId, SETTING_DEFAULT_IME);
    }

    /** Command to set current IME to {@code imeId} synchronously */
    public static String setCurrentImeSync(String imeId) {
        return "ime set " + imeId;
    }

    /** Command to set current IME to {@code imeId} synchronously for the specified {@code user}*/
    public static String setCurrentImeSync(String imeId, int userId) {
        return String.format("ime set --user %d %s", userId, imeId);
    }

    public static String getEnabledImes() {
        return "ime list -s";
    }

    public static String getEnabledImes(int userId) {
        return String.format("ime list -s --user %d", userId);
    }

    public static String getAvailableImes() {
        return "ime list -s -a";
    }

    public static String getAvailableImes(int userId) {
        return String.format("ime list -s -a --user %d", userId);
    }

    public static String listPackage(String packageName) {
        return "pm list package " + packageName;
    }

    /** Command to enable IME of {@code imeId}. */
    public static String enableIme(String imeId) {
        return "ime enable " + imeId;
    }

    /** Command to enable IME of {@code imeId} for the specified {@code userId}. */
    public static String enableIme(String imeId, int userId) {
        return String.format("ime enable --user %d %s", userId, imeId);
    }

    /** Command to disable IME of {@code imeId}. */
    public static String disableIme(String imeId) {
        return "ime disable " + imeId;
    }

    /** Command to disable IME of {@code imeId} for the specified {@code userId}. */
    public static String disableIme(String imeId, int userId) {
        return String.format("ime disable --user %d %s", userId, imeId);
    }

    /** Command to reset currently selected/enabled IMEs to the default ones. */
    public static String resetImes() {
        return "ime reset";
    }

    /** Command to reset currently selected/enabled IMEs to the default ones for the specified
     * {@code userId} */
    public static String resetImes(int userId) {
        return String.format("ime reset --user %d", userId);
    }

    /** Command to reset currently selected/enabled IMEs to the default ones for all the users. */
    public static String resetImesForAllUsers() {
        return "ime reset --user all";
    }

    /** Command to delete all records of IME event provider. */
    public static String deleteContent(String contentUri) {
        return "content delete --uri " + contentUri;
    }

    public static String uninstallPackage(String packageName) {
        return "pm uninstall " + packageName;
    }

    /**
     * Command to uninstall {@code packageName} only for {@code userId}.
     *
     * @param packageName package name of the package to be uninstalled.
     * @param userId user ID to specify the user.
     */
    public static String uninstallPackage(String packageName, int userId) {
        return "pm uninstall --user " + userId + " " + packageName;
    }

    /**
     * Command to get the last user ID that is specified to
     * InputMethodManagerService.Lifecycle#onSwitchUser().
     *
     * @return the command to be passed to shell command.
     */
    public static String getLastSwitchUserId() {
        return "cmd input_method get-last-switch-user-id";
    }

    /**
     * Command to create a new profile user.
     *
     * @param parentUserId parent user to whom the new profile user should belong
     * @param userName name of the new profile user.
     * @return the command to be passed to shell command.
     */
    public static String createManagedProfileUser(int parentUserId, String userName) {
        return "pm create-user --profileOf " + parentUserId + " --managed " + userName;
    }

    /** Command to turn on the display (if it's sleeping). */
    public static String wakeUp() {
        return "input keyevent KEYCODE_WAKEUP";
    }

    /** Command to turn off the display */
    public static String sleepDevice() {
        return "input keyevent KEYCODE_SLEEP";
    }

    /** Command to dismiss Keyguard (if it's shown) */
    public static String dismissKeyguard() {
        return "wm dismiss-keyguard";
    }

    /** Command to close system dialogs (if shown) */
    public static String closeSystemDialog() {
        return "am broadcast -a android.intent.action.CLOSE_SYSTEM_DIALOGS";
    }

    /**
     * Command to unlock screen.
     *
     * Note that this command is originated from
     * {@code android.server.wm.UiDeviceUtils#pressUnlockButton()}, which is only valid for
     * unlocking insecure keyguard for test automation.
     */
    public static String unlockScreen() {
        return "input keyevent KEYCODE_MENU";
    }

    /**
     * Command to show IME picker popup window.
     *
     * Note that {@code android.view.inputmethod.InputMethodManager#dispatchInputEvent} will handle
     * KEYCODE_SYM to show IME picker when any input method enabled.
     */
    public static String showImePicker() {
        return "input keyevent KEYCODE_SYM";
    }

    /**
     * Command to send broadcast {@code Intent}.
     *
     * @param action action of intent.
     * @param targetComponent target of intent.
     * @param extras extra of intent, must be specified as triplet of option flag, key, and value.
     * @return shell command to send broadcast intent.
     */
    public static String broadcastIntent(String action, String targetComponent, String... extras) {
        if (extras.length % 3 != 0) {
            throw new IllegalArgumentException(
                    "extras must be triplets: " + Arrays.toString(extras));
        }
        final StringBuilder sb = new StringBuilder("am broadcast -a ")
                .append(action);
        for (int index = 0; index < extras.length; index += 3) {
            final String optionFlag = extras[index];
            final String extraKey = extras[index + 1];
            final String extraValue = extras[index + 2];
            sb.append(" ").append(optionFlag)
                    .append(" ").append(extraKey)
                    .append(" ").append(extraValue);
        }
        return sb.append(" ").append(targetComponent).toString();
    }
}
