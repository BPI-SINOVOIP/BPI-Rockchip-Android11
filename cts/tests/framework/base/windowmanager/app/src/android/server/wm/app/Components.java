/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.server.wm.app;

import android.content.ComponentName;
import android.server.wm.TestJournalProvider;
import android.server.wm.component.ComponentsBase;

public class Components extends ComponentsBase {
    public static final ComponentName ALT_LAUNCHING_ACTIVITY = component("AltLaunchingActivity");
    public static final ComponentName ALWAYS_FOCUSABLE_PIP_ACTIVITY =
            component("AlwaysFocusablePipActivity");
    public static final ComponentName ANIMATION_TEST_ACTIVITY = component("AnimationTestActivity");
    public static final ComponentName ASSISTANT_ACTIVITY = component("AssistantActivity");
    public static final ComponentName BOTTOM_ACTIVITY = component("BottomActivity");
    public static final ComponentName BOTTOM_LEFT_LAYOUT_ACTIVITY =
            component("BottomLeftLayoutActivity");
    public static final ComponentName BOTTOM_RIGHT_LAYOUT_ACTIVITY =
            component("BottomRightLayoutActivity");
    public static final ComponentName BROADCAST_RECEIVER_ACTIVITY =
            component("BroadcastReceiverActivity");
    public static final ComponentName DIALOG_WHEN_LARGE_ACTIVITY =
            component("DialogWhenLargeActivity");
    public static final ComponentName DISMISS_KEYGUARD_ACTIVITY =
            component("DismissKeyguardActivity");
    public static final ComponentName DISMISS_KEYGUARD_METHOD_ACTIVITY =
            component("DismissKeyguardMethodActivity");
    public static final ComponentName DOCKED_ACTIVITY = component("DockedActivity");
    /**
     * This activity is an alias activity pointing {@link TrampolineActivity}
     * in AndroidManifest.xml
     */
    public static final ComponentName ENTRY_POINT_ALIAS_ACTIVITY =
            component("EntryPointAliasActivity");
    public static final ComponentName FONT_SCALE_ACTIVITY = component("FontScaleActivity");
    public static final ComponentName FONT_SCALE_NO_RELAUNCH_ACTIVITY =
            component("FontScaleNoRelaunchActivity");
    public static final ComponentName FREEFORM_ACTIVITY = component("FreeformActivity");
    public static final ComponentName HOST_ACTIVITY = component("HostActivity");
    public static final ComponentName KEYGUARD_LOCK_ACTIVITY = component("KeyguardLockActivity");
    public static final ComponentName LANDSCAPE_ORIENTATION_ACTIVITY =
            component("LandscapeOrientationActivity");
    public static final ComponentName LAUNCH_ASSISTANT_ACTIVITY_FROM_SESSION =
            component("LaunchAssistantActivityFromSession");
    public static final ComponentName LAUNCH_ASSISTANT_ACTIVITY_INTO_STACK  =
            component("LaunchAssistantActivityIntoAssistantStack");
    public static final ComponentName LAUNCH_ENTER_PIP_ACTIVITY =
            component("LaunchEnterPipActivity");
    public static final ComponentName LAUNCH_INTO_PINNED_STACK_PIP_ACTIVITY =
            component("LaunchIntoPinnedStackPipActivity");
    public static final ComponentName LAUNCH_PIP_ON_PIP_ACTIVITY =
            component("LaunchPipOnPipActivity");
    public static final ComponentName LAUNCHING_ACTIVITY = component("LaunchingActivity");
    public static final ComponentName LOG_CONFIGURATION_ACTIVITY =
            component("LogConfigurationActivity");
    public static final ComponentName MOVE_TASK_TO_BACK_ACTIVITY =
            component("MoveTaskToBackActivity");
    public static final ComponentName NIGHT_MODE_ACTIVITY = component("NightModeActivity");
    public static final ComponentName NO_DISPLAY_ACTIVITY = component("NoDisplayActivity");
    public static final ComponentName NO_HISTORY_ACTIVITY = component("NoHistoryActivity");
    public static final ComponentName NO_RELAUNCH_ACTIVITY = component("NoRelaunchActivity");
    public static final ComponentName NON_RESIZEABLE_ACTIVITY = component("NonResizeableActivity");
    public static final ComponentName PRESENTATION_ACTIVITY = component("PresentationActivity");
    public static final ComponentName PIP_ACTIVITY = component("PipActivity");
    public static final ComponentName PIP_ACTIVITY2 = component("PipActivity2");
    public static final ComponentName PIP_ACTIVITY_WITH_MINIMAL_SIZE = component(
            "PipActivityWithMinimalSize");
    public static final ComponentName PIP_ACTIVITY_WITH_TINY_MINIMAL_SIZE = component(
            "PipActivityWithTinyMinimalSize");
    public static final ComponentName PIP_ACTIVITY_WITH_SAME_AFFINITY =
            component("PipActivityWithSameAffinity");
    public static final ComponentName PIP_ON_STOP_ACTIVITY = component("PipOnStopActivity");
    public static final ComponentName PORTRAIT_ORIENTATION_ACTIVITY =
            component("PortraitOrientationActivity");
    public static final ComponentName RECURSIVE_ACTIVITY = component("RecursiveActivity");
    public static final ComponentName REPORT_FULLY_DRAWN_ACTIVITY =
            component("ReportFullyDrawnActivity");
    public static final ComponentName RESIZEABLE_ACTIVITY = component("ResizeableActivity");
    public static final ComponentName RESUME_WHILE_PAUSING_ACTIVITY =
            component("ResumeWhilePausingActivity");
    public static final ComponentName SHOW_WHEN_LOCKED_ACTIVITY =
            component("ShowWhenLockedActivity");
    public static final ComponentName SHOW_WHEN_LOCKED_ATTR_IME_ACTIVITY =
            component("ShowWhenLockedAttrImeActivity");
    public static final ComponentName SHOW_WHEN_LOCKED_ATTR_ACTIVITY =
            component("ShowWhenLockedAttrActivity");
    public static final ComponentName SHOW_WHEN_LOCKED_ATTR_ROTATION_ACTIVITY =
            component("ShowWhenLockedAttrRotationActivity");
    public static final ComponentName SHOW_WHEN_LOCKED_ATTR_REMOVE_ATTR_ACTIVITY =
            component("ShowWhenLockedAttrRemoveAttrActivity");
    public static final ComponentName INHERIT_SHOW_WHEN_LOCKED_ADD_ACTIVITY =
            component("InheritShowWhenLockedAddActivity");
    public static final ComponentName INHERIT_SHOW_WHEN_LOCKED_ATTR_ACTIVITY =
            component("InheritShowWhenLockedAttrActivity");
    public static final ComponentName INHERIT_SHOW_WHEN_LOCKED_REMOVE_ACTIVITY =
            component("InheritShowWhenLockedRemoveActivity");
    public static final ComponentName NO_INHERIT_SHOW_WHEN_LOCKED_ATTR_ACTIVITY =
            component("NoInheritShowWhenLockedAttrActivity");
    public static final ComponentName SHOW_WHEN_LOCKED_DIALOG_ACTIVITY =
            component("ShowWhenLockedDialogActivity");
    public static final ComponentName SHOW_WHEN_LOCKED_TRANSLUCENT_ACTIVITY =
            component("ShowWhenLockedTranslucentActivity");
    public static final ComponentName SHOW_WHEN_LOCKED_WITH_DIALOG_ACTIVITY =
            component("ShowWhenLockedWithDialogActivity");
    public static final ComponentName SINGLE_INSTANCE_ACTIVITY =
            component("SingleInstanceActivity");
    public static final ComponentName HOME_ACTIVITY = component("HomeActivity");
    public static final ComponentName SECONDARY_HOME_ACTIVITY = component("SecondaryHomeActivity");
    public static final ComponentName SINGLE_HOME_ACTIVITY = component("SingleHomeActivity");
    public static final ComponentName SINGLE_SECONDARY_HOME_ACTIVITY =
            component("SingleSecondaryHomeActivity");
    public static final ComponentName SINGLE_TASK_ACTIVITY = component("SingleTaskActivity");
    public static final ComponentName SLOW_CREATE_ACTIVITY = component("SlowCreateActivity");
    public static final ComponentName SPLASHSCREEN_ACTIVITY = component("SplashscreenActivity");
    public static final ComponentName TEST_ACTIVITY = component("TestActivity");
    public static final ComponentName TOAST_ACTIVITY = component("ToastActivity");
    public static final ComponentName TOP_ACTIVITY = component("TopActivity");
    public static final ComponentName TEST_ACTIVITY_WITH_SAME_AFFINITY =
            component("TestActivityWithSameAffinity");
    public static final ComponentName TEST_LIVE_WALLPAPER_SERVICE = component("LiveWallpaper");
    public static final ComponentName TOP_LEFT_LAYOUT_ACTIVITY = component("TopLeftLayoutActivity");
    public static final ComponentName TOP_RIGHT_LAYOUT_ACTIVITY =
            component("TopRightLayoutActivity");
    public static final ComponentName TRANSLUCENT_ACTIVITY = component("TranslucentActivity");
    public static final ComponentName TRANSLUCENT_ASSISTANT_ACTIVITY =
            component("TranslucentAssistantActivity");
    public static final ComponentName TRANSLUCENT_TOP_ACTIVITY =
            component("TranslucentTopActivity");
    public static final ComponentName TRANSLUCENT_TEST_ACTIVITY =
            component("TranslucentTestActivity");
    public static final ComponentName TURN_SCREEN_ON_ACTIVITY = component("TurnScreenOnActivity");
    public static final ComponentName TURN_SCREEN_ON_ATTR_ACTIVITY =
            component("TurnScreenOnAttrActivity");
    public static final ComponentName TURN_SCREEN_ON_ATTR_DISMISS_KEYGUARD_ACTIVITY =
            component("TurnScreenOnAttrDismissKeyguardActivity");
    public static final ComponentName TURN_SCREEN_ON_ATTR_REMOVE_ATTR_ACTIVITY =
            component("TurnScreenOnAttrRemoveAttrActivity");
    public static final ComponentName TURN_SCREEN_ON_DISMISS_KEYGUARD_ACTIVITY =
            component("TurnScreenOnDismissKeyguardActivity");
    public static final ComponentName TURN_SCREEN_ON_SHOW_ON_LOCK_ACTIVITY =
            component("TurnScreenOnShowOnLockActivity");
    public static final ComponentName TURN_SCREEN_ON_SINGLE_TASK_ACTIVITY =
            component("TurnScreenOnSingleTaskActivity");
    public static final ComponentName TURN_SCREEN_ON_WITH_RELAYOUT_ACTIVITY =
            component("TurnScreenOnWithRelayoutActivity");
    public static final ComponentName UNRESPONSIVE_ACTIVITY = component("UnresponsiveActivity");
    public static final ComponentName VIRTUAL_DISPLAY_ACTIVITY =
            component("VirtualDisplayActivity");
    public static final ComponentName VR_TEST_ACTIVITY = component("VrTestActivity");
    public static final ComponentName WALLPAPAER_ACTIVITY = component("WallpaperActivity");
    public static final ComponentName LAUNCH_TEST_ON_DESTROY_ACTIVITY = component(
            "LaunchTestOnDestroyActivity");
    public static final ComponentName ALIAS_TEST_ACTIVITY = component("AliasTestActivity");
    public static final ComponentName DISPLAY_ACCESS_CHECK_EMBEDDING_ACTIVITY = component(
            "DisplayAccessCheckEmbeddingActivity");

    public static final ComponentName ASSISTANT_VOICE_INTERACTION_SERVICE =
            component("AssistantVoiceInteractionService");

    public static final ComponentName LAUNCH_BROADCAST_RECEIVER =
            component("LaunchBroadcastReceiver");

    public static final ComponentName CLICKABLE_TOAST_ACTIVITY =
            component("ClickableToastActivity");

    public static class LaunchBroadcastReceiver {
        public static final String LAUNCH_BROADCAST_ACTION =
                "android.server.wm.app.LAUNCH_BROADCAST_ACTION";

        public static final String ACTION_TEST_ACTIVITY_START =
                "android.server.wm.app.ACTION_TEST_ACTIVITY_START";
        public static final String EXTRA_COMPONENT_NAME = "component_name";
        public static final String EXTRA_TARGET_DISPLAY = "target_display";
    }

    public static final ComponentName SINGLE_TASK_INSTANCE_DISPLAY_ACTIVITY =
            component("SingleTaskInstanceDisplayActivity");
    public static final ComponentName SINGLE_TASK_INSTANCE_DISPLAY_ACTIVITY2 =
            component("SingleTaskInstanceDisplayActivity2");
    public static final ComponentName SINGLE_TASK_INSTANCE_DISPLAY_ACTIVITY3 =
            component("SingleTaskInstanceDisplayActivity3");

    public static final ComponentName INPUT_METHOD_TEST_ACTIVITY =
            component("InputMethodTestActivity");

    public static final ComponentName MPP_ACTIVITY =
            component("MinimalPostProcessingActivity");

    public static final ComponentName MPP_ACTIVITY2 =
            component("MinimalPostProcessingActivity2");

    public static final ComponentName MPP_ACTIVITY3 =
            component("MinimalPostProcessingManifestActivity");

    public static final ComponentName POPUP_MPP_ACTIVITY =
            component("PopupMinimalPostProcessingActivity");

    public static final ComponentName TEST_DREAM_SERVICE =
            component("TestDream");

    public static final ComponentName TEST_STUBBORN_DREAM_SERVICE =
            component("TestStubbornDream");

    /**
     * Action and extra key constants for {@link #INPUT_METHOD_TEST_ACTIVITY}.
     */
    public static class InputMethodTestActivity {
        public static final String EXTRA_PRIVATE_IME_OPTIONS = "private_ime_options";
        public static final String EXTRA_TEST_CURSOR_ANCHOR_INFO = "cursor_anchor_info";
    }

    /**
     * The keys are used for {@link TestJournalProvider} when testing wallpaper
     * component.
     */
    public static class TestLiveWallpaperKeys {
        public static final String COMPONENT = "LiveWallpaper";
        public static final String ENGINE_CREATED = "engine_created";
        public static final String ENGINE_DISPLAY_ID = "engine_display_Id";
    }

    /**
     * Action and extra key constants for {@link #TEST_ACTIVITY}.
     *
     * TODO(b/73346885): These constants should be in {@link android.server.wm.app.TestActivity}
     * once the activity is moved to test APK.
     */
    public static class TestActivity {
        // Finishes the activity
        public static final String TEST_ACTIVITY_ACTION_FINISH_SELF =
                "android.server.wm.app.TestActivity.finish_self";
        // Sets the fixed orientation (can be one of {@link ActivityInfo.ScreenOrientation}
        public static final String EXTRA_FIXED_ORIENTATION = "fixed_orientation";
        public static final String EXTRA_CONFIGURATION = "configuration";
        public static final String EXTRA_CONFIG_ASSETS_SEQ = "config_assets_seq";
        public static final String EXTRA_INTENT = "intent";
        public static final String EXTRA_INTENTS = "intents";
        public static final String EXTRA_NO_IDLE = "no_idle";
        public static final String COMMAND_NAVIGATE_UP_TO = "navigate_up_to";
        public static final String COMMAND_START_ACTIVITIES = "start_activities";
    }

    /**
     * Extra key constants for {@link #LAUNCH_ASSISTANT_ACTIVITY_INTO_STACK} and
     * {@link #LAUNCH_ASSISTANT_ACTIVITY_FROM_SESSION}.
     *
     * TODO(b/73346885): These constants should be in
     * {@link android.server.wm.app.AssistantActivity} once the activity is moved to test APK.
     */
    public static class AssistantActivity {
        // Launches the given activity in onResume
        public static final String EXTRA_ASSISTANT_LAUNCH_NEW_TASK = "launch_new_task";
        // Finishes this activity in onResume, this happens after EXTRA_ASSISTANT_LAUNCH_NEW_TASK
        public static final String EXTRA_ASSISTANT_FINISH_SELF = "finish_self";
        // Attempts to enter picture-in-picture in onResume
        public static final String EXTRA_ASSISTANT_ENTER_PIP = "enter_pip";
        // Display on which Assistant runs
        public static final String EXTRA_ASSISTANT_DISPLAY_ID = "assistant_display_id";
    }

    /**
     * Extra key constants for {@link android.server.wm.app.BottomActivity}.
     *
     * TODO(b/73346885): These constants should be in {@link android.server.wm.app.BottomActivity}
     * once the activity is moved to test APK.
     */
    public static class BottomActivity {
        public static final String EXTRA_BOTTOM_WALLPAPER = "USE_WALLPAPER";
        public static final String EXTRA_STOP_DELAY = "STOP_DELAY";
    }

    /**
     * Extra key constants for {@link android.server.wm.app.BroadcastReceiverActivity}.
     *
     * TODO(b/73346885): These constants should be in
     * {@link android.server.wm.app.BroadcastReceiverActivity} once the activity is moved to test APK.
     */
    public static class BroadcastReceiverActivity {
        public static final String ACTION_TRIGGER_BROADCAST = "trigger_broadcast";
        public static final String EXTRA_DISMISS_KEYGUARD = "dismissKeyguard";
        public static final String EXTRA_DISMISS_KEYGUARD_METHOD = "dismissKeyguardMethod";
        public static final String EXTRA_FINISH_BROADCAST = "finish";
        public static final String EXTRA_MOVE_BROADCAST_TO_BACK = "moveToBack";
        public static final String EXTRA_BROADCAST_ORIENTATION = "orientation";
        public static final String EXTRA_CUTOUT_EXISTS = "cutoutExists";
    }

    /** Extra key constants for {@link android.server.wm.app.LandscapeOrientationActivity}. */
    public static class LandscapeOrientationActivity {
        public static final String EXTRA_APP_CONFIG_INFO = "app_config_info";
        public static final String EXTRA_CONFIG_INFO_IN_ON_CREATE = "config_info_in_on_create";
        public static final String EXTRA_DISPLAY_REAL_SIZE = "display_real_size";
        public static final String EXTRA_SYSTEM_RESOURCES_CONFIG_INFO = "sys_config_info";
    }

    /** Extra key constants for {@link android.server.wm.app.FontScaleActivity}. */
    public static class FontScaleActivity {
        public static final String EXTRA_FONT_PIXEL_SIZE = "fontPixelSize";
        public static final String EXTRA_FONT_ACTIVITY_DPI = "fontActivityDpi";
    }

    /** Extra key constants for {@link android.server.wm.app.TurnScreenOnActivity}. */
    public static class TurnScreenOnActivity {
        // Turn on screen by window flags or APIs.
        public static final String EXTRA_USE_WINDOW_FLAGS = "useWindowFlags";
        public static final String EXTRA_SLEEP_MS_IN_ON_CREATE = "sleepMsInOnCreate";
    }

    /** Extra key constants for {@link android.server.wm.app.MinimalPostProcessingActivity}. */
    public static class MinimalPostProcessingActivity {
        // Turn on minimal post processing (if available).
        public static final String EXTRA_PREFER_MPP = "preferMinimalPostProcessing";
    }

    /**
     *  Logging constants for {@link android.server.wm.app.KeyguardDismissLoggerCallback}.
     *
     * TODO(b/73346885): These constants should be in
     * {@link android.server.wm.app.KeyguardDismissLoggerCallback} once the class is moved to test APK.
     */
    public static class KeyguardDismissLoggerCallback {
        public static final String KEYGUARD_DISMISS_LOG_TAG = "KeyguardDismissLoggerCallback";
        public static final String ENTRY_ON_DISMISS_CANCELLED = "onDismissCancelled";
        public static final String ENTRY_ON_DISMISS_ERROR = "onDismissError";
        public static final String ENTRY_ON_DISMISS_SUCCEEDED = "onDismissSucceeded";
    }

    /**
     * Extra key constants for {@link #LAUNCH_ASSISTANT_ACTIVITY_INTO_STACK}.
     *
     * TODO(b/73346885): These constants should be in
     * {@link android.server.wm.app.LaunchAssistantActivityIntoAssistantStack} once the activity is
     * moved to test APK.
     */
    public static class LaunchAssistantActivityIntoAssistantStack {
        // Launches the translucent assist activity
        public static final String EXTRA_ASSISTANT_IS_TRANSLUCENT = "is_translucent";
    }

    /**
     * Extra constants for {@link android.server.wm.app.MoveTaskToBackActivity}.
     *
     * TODO(b/73346885): These constants should be in
     * {@link android.server.wm.app.MoveTaskToBackActivity} once the activity is moved to test APK.
     */
    public static class MoveTaskToBackActivity {
        public static final String EXTRA_FINISH_POINT = "finish_point";
        public static final String FINISH_POINT_ON_PAUSE = "on_pause";
        public static final String FINISH_POINT_ON_STOP = "on_stop";
    }

    /**
     * Action and extra key constants for {@link android.server.wm.app.PipActivity}.
     *
     * TODO(b/73346885): These constants should be in {@link android.server.wm.app.PipActivity}
     * once the activity is moved to test APK.
     */
    public static class PipActivity {
        // Intent action that this activity dynamically registers to enter picture-in-picture
        public static final String ACTION_ENTER_PIP = "android.server.wm.app.PipActivity.enter_pip";
        // Intent action that this activity dynamically registers to move itself to the back
        public static final String ACTION_MOVE_TO_BACK =
                "android.server.wm.app.PipActivity.move_to_back";
        // Intent action that this activity dynamically registers to expand itself.
        // If EXTRA_SET_ASPECT_RATIO_WITH_DELAY is set, it will also attempt to apply the aspect
        // ratio after a short delay.
        public static final String ACTION_EXPAND_PIP =
                "android.server.wm.app/PipActivity.expand_pip";
        // Intent action that this activity dynamically registers to set requested orientation.
        // Will apply the oriention to the value set in the EXTRA_FIXED_ORIENTATION extra.
        public static final String ACTION_SET_REQUESTED_ORIENTATION =
                "android.server.wm.app.PipActivity.set_requested_orientation";
        // Intent action that will finish this activity
        public static final String ACTION_FINISH = "android.server.wm.app.PipActivity.finish";
        // Intent action that will request that the activity enters picture-in-picture.
        public static final String ACTION_ON_PIP_REQUESTED =
                "android.server.wm.app.PipActivity.on_pip_requested";

        // Adds an assertion that we do not ever get onStop() before we enter picture in picture
        public static final String EXTRA_ASSERT_NO_ON_STOP_BEFORE_PIP =
                "assert_no_on_stop_before_pip";
        // Calls enterPictureInPicture() on creation
        public static final String EXTRA_ENTER_PIP = "enter_pip";
        // Used with EXTRA_AUTO_ENTER_PIP, value specifies the aspect ratio to enter PIP with
        public static final String EXTRA_ENTER_PIP_ASPECT_RATIO_NUMERATOR =
                "enter_pip_aspect_ratio_numerator";
        // Used with EXTRA_AUTO_ENTER_PIP, value specifies the aspect ratio to enter PIP with
        public static final String EXTRA_ENTER_PIP_ASPECT_RATIO_DENOMINATOR =
                "enter_pip_aspect_ratio_denominator";
        // Calls requestAutoEnterPictureInPicture() with the value provided
        public static final String EXTRA_ENTER_PIP_ON_PAUSE = "enter_pip_on_pause";
        // Calls requestAutoEnterPictureInPicture() with the value provided
        public static final String EXTRA_ENTER_PIP_ON_USER_LEAVE_HINT =
                "enter_pip_on_user_leave_hint";
        // Calls requestAutoEnterPictureInPicture() with the value provided
        public static final String EXTRA_ENTER_PIP_ON_PIP_REQUESTED =
                "enter_pip_on_pip_requested";
        // Finishes the activity at the end of onResume (after EXTRA_START_ACTIVITY is handled)
        public static final String EXTRA_FINISH_SELF_ON_RESUME = "finish_self_on_resume";
        // Sets the fixed orientation (can be one of {@link ActivityInfo.ScreenOrientation}
        public static final String EXTRA_PIP_ORIENTATION = "fixed_orientation";
        // The amount to delay to artificially introduce in onPause()
        // (before EXTRA_ENTER_PIP_ON_PAUSE is processed)
        public static final String EXTRA_ON_PAUSE_DELAY = "on_pause_delay";
        // Calls setPictureInPictureAspectRatio with the aspect ratio specified in the value
        public static final String EXTRA_SET_ASPECT_RATIO_DENOMINATOR =
                "set_aspect_ratio_denominator";
        // Calls setPictureInPictureAspectRatio with the aspect ratio specified in the value
        public static final String EXTRA_SET_ASPECT_RATIO_NUMERATOR = "set_aspect_ratio_numerator";
        // Calls setPictureInPictureAspectRatio with the aspect ratio specified in the value with a
        // fixed delay
        public static final String EXTRA_SET_ASPECT_RATIO_WITH_DELAY_NUMERATOR =
                "set_aspect_ratio_with_delay_numerator";
        // Calls setPictureInPictureAspectRatio with the aspect ratio specified in the value with a
        // fixed delay
        public static final String EXTRA_SET_ASPECT_RATIO_WITH_DELAY_DENOMINATOR =
                "set_aspect_ratio_with_delay_denominator";
        // Shows this activity over the keyguard
        public static final String EXTRA_SHOW_OVER_KEYGUARD = "show_over_keyguard";
        // Starts the activity (component name) provided by the value at the end of onCreate
        public static final String EXTRA_START_ACTIVITY = "start_activity";
        // Adds a click listener to finish this activity when it is clicked
        public static final String EXTRA_TAP_TO_FINISH = "tap_to_finish";
        // Dismiss keyguard when activity show.
        public static final String EXTRA_DISMISS_KEYGUARD = "dismiss_keyguard";
    }

    /**
     * Extra key constants for {@link android.server.wm.app.TopActivity} and
     * {@link TranslucentTopActivity}.
     *
     * TODO(b/73346885): These constants should be in {@link android.server.wm.app.TopActivity}
     * once the activity is moved to test APK.
     */
    public static class TopActivity {
        public static final String EXTRA_FINISH_DELAY = "FINISH_DELAY";
        public static final String EXTRA_FINISH_IN_ON_CREATE = "FINISH_IN_ON_CREATE";
        public static final String EXTRA_TOP_WALLPAPER = "USE_WALLPAPER";
        public static final String ACTION_CONVERT_TO_TRANSLUCENT = "convert_to_translucent";
        public static final String ACTION_CONVERT_FROM_TRANSLUCENT = "convert_from_translucent";
    }

    public static class UnresponsiveActivity {
        public static final String EXTRA_ON_CREATE_DELAY_MS = "ON_CREATE_DELAY_MS";
        public static final String EXTRA_DELAY_UI_THREAD_MS = "DELAY_UI_THREAD_MS";
        public static final String EXTRA_ON_KEYDOWN_DELAY_MS = "ON_KEYDOWN_DELAY_MS";
        public static final String EXTRA_ON_MOTIONEVENT_DELAY_MS = "ON_MOTIONEVENT_DELAY_MS";
        public static final String PROCESS_NAME = ".unresponsive_activity_process";
    }

    public static class RenderService {
        public static final String PROCESS_NAME = ".render_process";
    }

    /**
     * Extra key constants for {@link android.server.wm.app.VirtualDisplayActivity}.
     *
     * TODO(b/73346885): These constants should be in
     * {@link android.server.wm.app.VirtualDisplayActivity} once the activity is moved to test APK.
     */
    public static class VirtualDisplayActivity {
        public static final String VIRTUAL_DISPLAY_PREFIX = "HostedVirtualDisplay";
        public static final String KEY_CAN_SHOW_WITH_INSECURE_KEYGUARD =
                "can_show_with_insecure_keyguard";
        public static final String KEY_COMMAND = "command";
        public static final String KEY_COUNT = "count";
        public static final String KEY_DENSITY_DPI = "density_dpi";
        public static final String KEY_PUBLIC_DISPLAY = "public_display";
        public static final String KEY_RESIZE_DISPLAY = "resize_display";
        public static final String KEY_SHOW_SYSTEM_DECORATIONS = "show_system_decorations";
        public static final String KEY_PRESENTATION_DISPLAY = "presentation_display";
        // Value constants of {@link #KEY_COMMAND}.
        public static final String COMMAND_CREATE_DISPLAY = "create_display";
        public static final String COMMAND_DESTROY_DISPLAY = "destroy_display";
        public static final String COMMAND_RESIZE_DISPLAY = "resize_display";
    }

    public static class ClickableToastActivity {
        public static final String ACTION_TOAST_DISPLAYED = "toast_displayed";
        public static final String ACTION_TOAST_TAP_DETECTED = "toast_tap_detected";
    }

    public static class PresentationActivity {
        public static final String KEY_DISPLAY_ID = "display_id";
    }

    public static class LaunchingActivity {
        public static final String KEY_FINISH_BEFORE_LAUNCH = "finish_before_launch";
    }

    private static ComponentName component(String className) {
        return component(Components.class, className);
    }

    public static String getPackageName() {
        return getPackageName(Components.class);
    }
}
