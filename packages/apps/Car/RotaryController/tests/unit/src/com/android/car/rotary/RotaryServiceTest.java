/*
 * Copyright (C) 2021 The Android Open Source Project
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

package com.android.car.rotary;

import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED;
import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED;
import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_CLICKED;
import static android.view.accessibility.AccessibilityEvent.TYPE_VIEW_FOCUSED;
import static android.view.accessibility.AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED;
import static android.view.accessibility.AccessibilityWindowInfo.TYPE_APPLICATION;

import static com.android.car.ui.utils.DirectManipulationHelper.DIRECT_MANIPULATION;
import static com.android.car.ui.utils.RotaryConstants.ACTION_RESTORE_DEFAULT_FOCUS;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.testng.AssertJUnit.assertNull;

import android.app.Activity;
import android.app.UiAutomation;
import android.car.input.CarInputManager;
import android.car.input.RotaryEvent;
import android.content.ComponentName;
import android.content.Intent;
import android.hardware.input.InputManager;
import android.view.KeyEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;
import android.widget.Button;

import androidx.annotation.LayoutRes;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import com.android.car.ui.FocusParkingView;
import com.android.car.ui.utils.DirectManipulationHelper;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class RotaryServiceTest {

    private final static String HOST_APP_PACKAGE_NAME = "host.app.package.name";
    private final static String CLIENT_APP_PACKAGE_NAME = "client.app.package.name";

    private static UiAutomation sUiAutomoation;

    private final List<AccessibilityNodeInfo> mNodes = new ArrayList<>();

    private AccessibilityNodeInfo mWindowRoot;
    private ActivityTestRule<NavigatorTestActivity> mActivityRule;
    private Intent mIntent;
    private NodeBuilder mNodeBuilder;

    private @Spy
    RotaryService mRotaryService;
    private @Spy
    Navigator mNavigator;

    @BeforeClass
    public static void setUpClass() {
        sUiAutomoation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
    }

    @Before
    public void setUp() {
        mActivityRule = new ActivityTestRule<>(NavigatorTestActivity.class);
        mIntent = new Intent();
        mIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);

        MockitoAnnotations.initMocks(this);
        mRotaryService.setNavigator(mNavigator);
        mRotaryService.setNodeCopier(MockNodeCopierProvider.get());
        mRotaryService.setInputManager(mock(InputManager.class));
        mNodeBuilder = new NodeBuilder(new ArrayList<>());
    }

    @After
    public void tearDown() {
        mActivityRule.finishActivity();
        Utils.recycleNode(mWindowRoot);
        Utils.recycleNodes(mNodes);
    }

    /**
     * Tests {@link RotaryService#initFocus()} in the following view tree:
     * <pre>
     *                      root
     *                     /    \
     *                    /      \
     *       focusParkingView   focusArea
     *                        /     |     \
     *                       /      |       \
     *               button1  defaultFocus  button3
     *                         (focused)
     * </pre>
     * and {@link RotaryService#mFocusedNode} is already set to defaultFocus.
     */
    @Test
    public void testInitFocus_alreadyInitialized() {
        initActivity(R.layout.rotary_service_test_1_activity);

        AccessibilityWindowInfo window = new WindowBuilder()
                .setRoot(mWindowRoot)
                .setBoundsInScreen(mWindowRoot.getBoundsInScreen())
                .build();
        List<AccessibilityWindowInfo> windows = Collections.singletonList(window);
        when(mRotaryService.getWindows()).thenReturn(windows);

        AccessibilityNodeInfo defaultFocusNode = createNode("defaultFocus");
        assertThat(defaultFocusNode.isFocused()).isTrue();
        mRotaryService.setFocusedNode(defaultFocusNode);
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(defaultFocusNode);

        boolean consumed = mRotaryService.initFocus();
        assertThat(consumed).isFalse();
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(defaultFocusNode);
    }

    /**
     * Tests {@link RotaryService#initFocus()} in the following view tree:
     * <pre>
     *                      root
     *                     /    \
     *                    /      \
     *       focusParkingView   focusArea
     *                        /     |     \
     *                       /      |       \
     *               button1  defaultFocus  button3
     *                                      (focused)
     * </pre>
     * and {@link RotaryService#mFocusedNode} is not initialized.
     */
    @Test
    public void testInitFocus_focusOnAlreadyFocusedView() {
        initActivity(R.layout.rotary_service_test_1_activity);

        AccessibilityWindowInfo window = new WindowBuilder()
                .setRoot(mWindowRoot)
                .setBoundsInScreen(mWindowRoot.getBoundsInScreen())
                .build();
        List<AccessibilityWindowInfo> windows = Collections.singletonList(window);
        when(mRotaryService.getWindows()).thenReturn(windows);

        Activity activity = mActivityRule.getActivity();
        Button button3 = activity.findViewById(R.id.button3);
        button3.post(() -> button3.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(button3.isFocused()).isTrue();
        assertNull(mRotaryService.getFocusedNode());

        boolean consumed = mRotaryService.initFocus();
        AccessibilityNodeInfo button3Node = createNode("button3");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(button3Node);
        assertThat(consumed).isFalse();
    }

    /**
     * Tests {@link RotaryService#initFocus()} in the following view tree:
     * <pre>
     *                      root
     *                     /    \
     *                    /      \
     *       focusParkingView   focusArea
     *          (focused)      /     |     \
     *                       /      |       \
     *               button1  defaultFocus  button3
     * </pre>
     * and {@link RotaryService#mFocusedNode} is null.
     */
    @Test
    public void testInitFocus_focusOnDefaultFocusView() {
        initActivity(R.layout.rotary_service_test_1_activity);

        AccessibilityWindowInfo window = new WindowBuilder()
                .setRoot(mWindowRoot)
                .setBoundsInScreen(mWindowRoot.getBoundsInScreen())
                .build();
        List<AccessibilityWindowInfo> windows = Collections.singletonList(window);
        when(mRotaryService.getWindows()).thenReturn(windows);
        when(mRotaryService.getRootInActiveWindow())
                .thenReturn(MockNodeCopierProvider.get().copy(mWindowRoot));

        // Move focus to the FocusParkingView.
        Activity activity = mActivityRule.getActivity();
        FocusParkingView fpv = activity.findViewById(R.id.focusParkingView);
        fpv.setShouldRestoreFocus(false);
        fpv.post(() -> fpv.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(fpv.isFocused()).isTrue();
        assertNull(mRotaryService.getFocusedNode());

        boolean consumed = mRotaryService.initFocus();
        AccessibilityNodeInfo defaultFocusNode = createNode("defaultFocus");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(defaultFocusNode);
        assertThat(consumed).isTrue();
    }

    /**
     * Tests {@link RotaryService#initFocus()} in the following view tree:
     * <pre>
     *                      root
     *                     /    \
     *                    /      \
     *       focusParkingView   focusArea
     *          (focused)      /     |     \
     *                       /      |       \
     *               button1  defaultFocus  button3
     *                         (disabled)  (last touched)
     * </pre>
     * and {@link RotaryService#mFocusedNode} is null.
     */
    @Test
    public void testInitFocus_focusOnLastTouchedView() {
        initActivity(R.layout.rotary_service_test_1_activity);

        AccessibilityWindowInfo window = new WindowBuilder()
                .setRoot(mWindowRoot)
                .setBoundsInScreen(mWindowRoot.getBoundsInScreen())
                .build();
        List<AccessibilityWindowInfo> windows = Collections.singletonList(window);
        when(mRotaryService.getWindows()).thenReturn(windows);
        when(mRotaryService.getRootInActiveWindow())
                .thenReturn(MockNodeCopierProvider.get().copy(mWindowRoot));

        // The user touches button3. In reality it should enter touch mode therefore no view will
        // be focused. To emulate this case, this test just moves focus to the FocusParkingView
        // and sets last touched node to button3.
        Activity activity = mActivityRule.getActivity();
        FocusParkingView fpv = activity.findViewById(R.id.focusParkingView);
        fpv.setShouldRestoreFocus(false);
        fpv.post(fpv::requestFocus);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(fpv.isFocused()).isTrue();
        AccessibilityNodeInfo button3Node = createNode("button3");
        mRotaryService.setLastTouchedNode(button3Node);
        assertNull(mRotaryService.getFocusedNode());

        boolean consumed = mRotaryService.initFocus();
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(button3Node);
        assertThat(consumed).isTrue();
    }

    /**
     * Tests {@link RotaryService#initFocus()} in the following view tree:
     * <pre>
     *                      root
     *                     /    \
     *                    /      \
     *       focusParkingView   focusArea
     *          (focused)      /     |     \
     *                       /      |       \
     *               button1  defaultFocus  button3
     *                         (disabled)
     * </pre>
     * and {@link RotaryService#mFocusedNode} is null.
     */
    @Test
    public void testInitFocus_focusOnFirstFocusableView() {
        initActivity(R.layout.rotary_service_test_1_activity);

        AccessibilityWindowInfo window = new WindowBuilder()
                .setRoot(mWindowRoot)
                .setBoundsInScreen(mWindowRoot.getBoundsInScreen())
                .build();
        List<AccessibilityWindowInfo> windows = Collections.singletonList(window);
        when(mRotaryService.getWindows()).thenReturn(windows);
        when(mRotaryService.getRootInActiveWindow())
                .thenReturn(MockNodeCopierProvider.get().copy(mWindowRoot));

        // Move focus to the FocusParkingView and disable the default focus view.
        Activity activity = mActivityRule.getActivity();
        FocusParkingView fpv = activity.findViewById(R.id.focusParkingView);
        Button defaultFocus = activity.findViewById(R.id.defaultFocus);
        fpv.setShouldRestoreFocus(false);
        fpv.post(() -> {
            fpv.requestFocus();
            defaultFocus.setEnabled(false);

        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(fpv.isFocused()).isTrue();
        assertThat(defaultFocus.isEnabled()).isFalse();
        assertNull(mRotaryService.getFocusedNode());

        boolean consumed = mRotaryService.initFocus();
        AccessibilityNodeInfo button1Node = createNode("button1");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(button1Node);
        assertThat(consumed).isTrue();
    }

    /**
     * Tests {@link RotaryService#initFocus()} in the following node tree:
     * <pre>
     *                  clientAppRoot
     *                     /    \
     *                    /      \
     *              button1  surfaceView(focused)
     *                             |
     *                        hostAppRoot
     *                           /    \
     *                         /       \
     *            focusParkingView     button2(focused)
     * </pre>
     * and {@link RotaryService#mFocusedNode} is null.
     */
    @Test
    public void testInitFocus_focusOnHostNode() {
        mNavigator.addClientApp(CLIENT_APP_PACKAGE_NAME);
        mNavigator.mSurfaceViewHelper.mHostApp = HOST_APP_PACKAGE_NAME;

        AccessibilityNodeInfo clientAppRoot = mNodeBuilder
                .setPackageName(CLIENT_APP_PACKAGE_NAME)
                .build();
        AccessibilityNodeInfo button1 = mNodeBuilder
                .setParent(clientAppRoot)
                .setPackageName(CLIENT_APP_PACKAGE_NAME)
                .build();
        AccessibilityNodeInfo surfaceView = mNodeBuilder
                .setParent(clientAppRoot)
                .setFocused(true)
                .setPackageName(CLIENT_APP_PACKAGE_NAME)
                .setClassName(Utils.SURFACE_VIEW_CLASS_NAME)
                .build();

        AccessibilityNodeInfo hostAppRoot = mNodeBuilder
                .setParent(surfaceView)
                .setPackageName(HOST_APP_PACKAGE_NAME)
                .build();
        AccessibilityNodeInfo focusParkingView = mNodeBuilder
                .setParent(hostAppRoot)
                .setPackageName(HOST_APP_PACKAGE_NAME)
                .setFpv()
                .build();
        AccessibilityNodeInfo button2 = mNodeBuilder
                .setParent(hostAppRoot)
                .setFocused(true)
                .setPackageName(HOST_APP_PACKAGE_NAME)
                .build();

        AccessibilityWindowInfo window = new WindowBuilder().setRoot(clientAppRoot).build();
        List<AccessibilityWindowInfo> windows = Collections.singletonList(window);
        when(mRotaryService.getWindows()).thenReturn(windows);

        boolean consumed = mRotaryService.initFocus();
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(button2);
        assertThat(consumed).isFalse();
    }

    /**
     * Tests {@link RotaryService#onRotaryEvents} in the following view tree:
     * <pre>
     *                      root
     *                     /    \
     *                    /      \
     *       focusParkingView   focusArea
     *          (focused)      /     |     \
     *                       /      |       \
     *               button1  defaultFocus  button3
     * </pre>
     */
    @Test
    public void testOnRotaryEvents_withoutFocusedView() {
        initActivity(R.layout.rotary_service_test_1_activity);

        AccessibilityWindowInfo window = new WindowBuilder()
                .setRoot(mWindowRoot)
                .setBoundsInScreen(mWindowRoot.getBoundsInScreen())
                .build();
        List<AccessibilityWindowInfo> windows = Collections.singletonList(window);
        when(mRotaryService.getWindows()).thenReturn(windows);
        when(mRotaryService.getRootInActiveWindow())
                .thenReturn(MockNodeCopierProvider.get().copy(mWindowRoot));

        // Move focus to the FocusParkingView.
        Activity activity = mActivityRule.getActivity();
        FocusParkingView fpv = activity.findViewById(R.id.focusParkingView);
        fpv.setShouldRestoreFocus(false);
        fpv.post(() -> fpv.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(fpv.isFocused()).isTrue();
        assertNull(mRotaryService.getFocusedNode());

        // Since there is no non-FocusParkingView focused, rotating the controller should
        // initialize the focus.

        int inputType = CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION;
        boolean clockwise = true;
        long[] timestamps = new long[]{0};
        RotaryEvent rotaryEvent = new RotaryEvent(inputType, clockwise, timestamps);
        List<RotaryEvent> events = Collections.singletonList(rotaryEvent);

        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        mRotaryService.onRotaryEvents(validDisplayId, events);

        AccessibilityNodeInfo defaultFocusNode = createNode("defaultFocus");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(defaultFocusNode);
    }

    /**
     * Tests {@link RotaryService#onRotaryEvents} in the following view tree:
     * <pre>
     *                      root
     *                     /    \
     *                    /      \
     *       focusParkingView   focusArea
     *                        /     |     \
     *                       /      |       \
     *               button1  defaultFocus  button3
     *                          (focused)
     * </pre>
     */
    @Test
    public void testOnRotaryEvents_withFocusedView() {
        initActivity(R.layout.rotary_service_test_1_activity);

        AccessibilityWindowInfo window = new WindowBuilder()
                .setRoot(mWindowRoot)
                .setBoundsInScreen(mWindowRoot.getBoundsInScreen())
                .build();
        List<AccessibilityWindowInfo> windows = Collections.singletonList(window);
        when(mRotaryService.getWindows()).thenReturn(windows);
        doAnswer(invocation -> 1)
                .when(mRotaryService).getRotateAcceleration(any(Integer.class), any(Long.class));

        AccessibilityNodeInfo defaultFocusNode = createNode("defaultFocus");
        assertThat(defaultFocusNode.isFocused()).isTrue();
        mRotaryService.setFocusedNode(defaultFocusNode);
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(defaultFocusNode);

        // Since RotaryService#mFocusedNode is already initialized, rotating the controller
        // clockwise should move the focus from defaultFocus to button3.

        int inputType = CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION;
        boolean clockwise = true;
        long[] timestamps = new long[]{0};
        RotaryEvent rotaryEvent = new RotaryEvent(inputType, clockwise, timestamps);
        List<RotaryEvent> events = Collections.singletonList(rotaryEvent);

        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        mRotaryService.onRotaryEvents(validDisplayId, events);

        AccessibilityNodeInfo button3Node = createNode("button3");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(button3Node);

        // Rotating the controller clockwise again should do nothing because button3 is the last
        // child of its ancestor FocusArea and the ancestor FocusArea doesn't support wrap-around.
        mRotaryService.onRotaryEvents(validDisplayId, events);
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(button3Node);

        // Rotating the controller counterclockwise should move focus to defaultFocus.
        clockwise = false;
        rotaryEvent = new RotaryEvent(inputType, clockwise, timestamps);
        events = Collections.singletonList(rotaryEvent);
        mRotaryService.onRotaryEvents(validDisplayId, events);
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(defaultFocusNode);
    }

    /**
     * Tests {@link RotaryService#nudgeTo(List, int)} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      HUN FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testNudgeTo_nudgeToHun() {
        initActivity(R.layout.rotary_service_test_2_activity);

        AccessibilityNodeInfo hunRoot = createNode("hun_root");
        AccessibilityWindowInfo hunWindow = new WindowBuilder()
                .setRoot(hunRoot)
                .build();
        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(hunWindow);
        windows.add(appWindow);
        when(mRotaryService.getWindows()).thenReturn(windows);

        AccessibilityNodeInfo hunButton1 = createNode("hun_button1");
        AccessibilityNodeInfo mockHunFpv = mock(AccessibilityNodeInfo.class);
        doAnswer(invocation -> {
            mRotaryService.setFocusedNode(hunButton1);
            return true;
        }).when(mockHunFpv).performAction(ACTION_RESTORE_DEFAULT_FOCUS);
        when(mockHunFpv.refresh()).thenReturn(true);
        when(mockHunFpv.getClassName()).thenReturn(Utils.FOCUS_PARKING_VIEW_CLASS_NAME);
        when(mNavigator.findFocusParkingViewInRoot(hunRoot)).thenReturn(mockHunFpv);
        when(mNavigator.findHunWindow(anyList())).thenReturn(hunWindow);

        assertThat(mRotaryService.getFocusedNode()).isNotEqualTo(hunButton1);

        int hunNudgeDirection = mRotaryService.mHunNudgeDirection;
        mRotaryService.nudgeTo(windows, hunNudgeDirection);
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(hunButton1);
    }

    /**
     * Tests {@link RotaryService#nudgeTo(List, int)} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      HUN FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testNudgeTo_nudgeToNudgeShortcut() {
        initActivity(R.layout.rotary_service_test_2_activity);

        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);

        Activity activity = mActivityRule.getActivity();
        Button appButton1 = activity.findViewById(R.id.app_button1);
        appButton1.post(() -> appButton1.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(appButton1.isFocused()).isTrue();
        AccessibilityNodeInfo appButton1Node = createNode("app_button1");
        mRotaryService.setFocusedNode(appButton1Node);

        mRotaryService.nudgeTo(windows, View.FOCUS_RIGHT);
        AccessibilityNodeInfo nudgeShortcutNode = createNode("nudge_shortcut");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(nudgeShortcutNode);
    }

    /**
     * Tests {@link RotaryService#nudgeTo(List, int)} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      HUN FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testNudgeTo_nudgeToUserSpecifiedTarget() {
        initActivity(R.layout.rotary_service_test_2_activity);

        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);

        Activity activity = mActivityRule.getActivity();
        Button appButton2 = activity.findViewById(R.id.app_button2);
        appButton2.post(() -> appButton2.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(appButton2.isFocused()).isTrue();
        AccessibilityNodeInfo appButton2Node = createNode("app_button2");
        mRotaryService.setFocusedNode(appButton2Node);

        mRotaryService.nudgeTo(windows, View.FOCUS_LEFT);
        AccessibilityNodeInfo appDefaultFocusNode = createNode("app_default_focus");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appDefaultFocusNode);
    }

    /**
     * Tests {@link RotaryService#nudgeTo(List, int)} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      HUN FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testNudgeTo_nudgeToNearestTarget() {
        initActivity(R.layout.rotary_service_test_2_activity);

        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);

        Activity activity = mActivityRule.getActivity();
        Button appButton3 = activity.findViewById(R.id.app_button3);
        appButton3.post(() -> appButton3.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(appButton3.isFocused()).isTrue();
        AccessibilityNodeInfo appButton3Node = createNode("app_button3");
        AccessibilityNodeInfo appFocusArea3Node = createNode("app_focus_area3");
        mRotaryService.setFocusedNode(appButton3Node);

        AccessibilityNodeInfo appFocusArea1Node = createNode("app_focus_area1");
        when(mNavigator.findNudgeTargetFocusArea(
                windows, appButton3Node, appFocusArea3Node, View.FOCUS_UP))
                .thenReturn(AccessibilityNodeInfo.obtain(appFocusArea1Node));

        mRotaryService.nudgeTo(windows, View.FOCUS_UP);
        AccessibilityNodeInfo appButton1Node = createNode("app_button1");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appButton1Node);
    }

    /**
     * Tests {@link RotaryService#onKeyEvents} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  . (target)  .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  . (source)  .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnKeyEvents_nudgeUp_moveFocus() {
        initActivity(R.layout.rotary_service_test_2_activity);

        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);
        when(mRotaryService.getWindows()).thenReturn(windows);

        Activity activity = mActivityRule.getActivity();
        Button appButton3 = activity.findViewById(R.id.app_button3);
        appButton3.post(() -> appButton3.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(appButton3.isFocused()).isTrue();
        AccessibilityNodeInfo appButton3Node = createNode("app_button3");
        AccessibilityNodeInfo appFocusArea3Node = createNode("app_focus_area3");
        mRotaryService.setFocusedNode(appButton3Node);

        AccessibilityNodeInfo appFocusArea1Node = createNode("app_focus_area1");
        when(mNavigator.findNudgeTargetFocusArea(
                windows, appButton3Node, appFocusArea3Node, View.FOCUS_UP))
                .thenReturn(AccessibilityNodeInfo.obtain(appFocusArea1Node));

        // Nudge up the controller.
        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        KeyEvent nudgeUpEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SYSTEM_NAVIGATION_UP);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(nudgeUpEventActionDown));
        KeyEvent nudgeUpEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SYSTEM_NAVIGATION_UP);
        mRotaryService.onKeyEvents(validDisplayId, Collections.singletonList(nudgeUpEventActionUp));

        // It should move focus to the FocusArea above.
        AccessibilityNodeInfo appButton1Node = createNode("app_button1");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appButton1Node);
    }

    /**
     * Tests {@link RotaryService#onKeyEvents} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnKeyEvents_nudgeUp_initFocus() {
        initActivity(R.layout.rotary_service_test_2_activity);

        // RotaryService.mFocusedNode is not initialized.
        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);
        when(mRotaryService.getWindows()).thenReturn(windows);
        when(mRotaryService.getRootInActiveWindow())
                .thenReturn(MockNodeCopierProvider.get().copy(mWindowRoot));

        // Move focus to the FocusParkingView.
        Activity activity = mActivityRule.getActivity();
        FocusParkingView fpv = activity.findViewById(R.id.app_fpv);
        fpv.setShouldRestoreFocus(false);
        fpv.post(() -> fpv.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(fpv.isFocused()).isTrue();
        assertNull(mRotaryService.getFocusedNode());

        // Nudge up the controller.
        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        KeyEvent nudgeUpEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SYSTEM_NAVIGATION_UP);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(nudgeUpEventActionDown));
        KeyEvent nudgeUpEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SYSTEM_NAVIGATION_UP);
        mRotaryService.onKeyEvents(validDisplayId, Collections.singletonList(nudgeUpEventActionUp));

        // It should initialize the focus.
        AccessibilityNodeInfo appDefaultFocusNode = createNode("app_default_focus");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appDefaultFocusNode);
    }

    /**
     * Tests {@link RotaryService#onKeyEvents} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  . (focused) .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnKeyEvents_nudgeToHunEscapeNudgeDirection_leaveTheHun() {
        initActivity(R.layout.rotary_service_test_2_activity);

        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        AccessibilityNodeInfo hunRoot = createNode("hun_root");
        AccessibilityWindowInfo hunWindow = new WindowBuilder()
                .setRoot(hunRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);
        windows.add(hunWindow);
        when(mRotaryService.getWindows()).thenReturn(windows);

        // A Button in the HUN window is focused.
        Activity activity = mActivityRule.getActivity();
        Button hunButton1 = activity.findViewById(R.id.hun_button1);
        hunButton1.post(() -> hunButton1.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(hunButton1.isFocused()).isTrue();
        AccessibilityNodeInfo hunButton1Node = createNode("hun_button1");
        AccessibilityNodeInfo hunFocusAreaNode = createNode("hun_focus_area");
        mRotaryService.setFocusedNode(hunButton1Node);

        // Set HUN escape nudge direction to View.FOCUS_DOWN.
        mRotaryService.mHunEscapeNudgeDirection = View.FOCUS_DOWN;

        AccessibilityNodeInfo appFocusArea3Node = createNode("app_focus_area3");
        when(mNavigator.findNudgeTargetFocusArea(
                windows, hunButton1Node, hunFocusAreaNode, View.FOCUS_DOWN))
                .thenReturn(AccessibilityNodeInfo.obtain(appFocusArea3Node));

        // Nudge down the controller.
        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        KeyEvent nudgeEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SYSTEM_NAVIGATION_DOWN);
        mRotaryService.onKeyEvents(validDisplayId, Collections.singletonList(nudgeEventActionDown));
        KeyEvent nudgeEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SYSTEM_NAVIGATION_DOWN);
        mRotaryService.onKeyEvents(validDisplayId, Collections.singletonList(nudgeEventActionUp));

        // Nudging down should exit the HUN and focus in app_focus_area3.
        AccessibilityNodeInfo appDefaultFocusNode = createNode("app_default_focus");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appDefaultFocusNode);
    }

    /**
     * Tests {@link RotaryService#onKeyEvents} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  . (focused) .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnKeyEvents_nudgeToNonHunEscapeNudgeDirection_stayInTheHun() {
        initActivity(R.layout.rotary_service_test_2_activity);

        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        AccessibilityNodeInfo hunRoot = createNode("hun_root");
        AccessibilityWindowInfo hunWindow = new WindowBuilder()
                .setRoot(hunRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);
        windows.add(hunWindow);
        when(mRotaryService.getWindows()).thenReturn(windows);

        // A Button in the HUN window is focused.
        Activity activity = mActivityRule.getActivity();
        Button hunButton1 = activity.findViewById(R.id.hun_button1);
        hunButton1.post(() -> hunButton1.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(hunButton1.isFocused()).isTrue();
        AccessibilityNodeInfo hunButton1Node = createNode("hun_button1");
        AccessibilityNodeInfo hunFocusAreaNode = createNode("hun_focus_area");
        mRotaryService.setFocusedNode(hunButton1Node);

        // Set HUN escape nudge direction to View.FOCUS_UP.
        mRotaryService.mHunEscapeNudgeDirection = View.FOCUS_UP;

        // RotaryService.mFocusedNode.getWindow() returns null in the test, so just pass null value
        // to simplify the test.
        when(mNavigator.isHunWindow(null)).thenReturn(true);

        AccessibilityNodeInfo appFocusArea3Node = createNode("app_focus_area3");
        when(mNavigator.findNudgeTargetFocusArea(
                windows, hunButton1Node, hunFocusAreaNode, View.FOCUS_DOWN))
                .thenReturn(AccessibilityNodeInfo.obtain(appFocusArea3Node));

        // Nudge down the controller.
        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        KeyEvent nudgeEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SYSTEM_NAVIGATION_DOWN);
        mRotaryService.onKeyEvents(validDisplayId, Collections.singletonList(nudgeEventActionDown));
        KeyEvent nudgeEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_SYSTEM_NAVIGATION_DOWN);
        mRotaryService.onKeyEvents(validDisplayId, Collections.singletonList(nudgeEventActionUp));

        // Nudging down should stay in the HUN because HUN escape nudge direction is View.FOCUS_UP.
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(hunButton1Node);
    }

    /**
     * Tests {@link RotaryService#onKeyEvents} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnKeyEvents_centerButtonClick_initFocus() {
        initActivity(R.layout.rotary_service_test_2_activity);

        // RotaryService.mFocusedNode is not initialized.
        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);
        when(mRotaryService.getWindows()).thenReturn(windows);
        when(mRotaryService.getRootInActiveWindow())
                .thenReturn(MockNodeCopierProvider.get().copy(mWindowRoot));
        assertThat(mRotaryService.getFocusedNode()).isNull();

        // Click the center button of the controller.
        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        KeyEvent centerButtonEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionDown));
        KeyEvent centerButtonEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionUp));

        // It should initialize the focus.
        AccessibilityNodeInfo appDefaultFocusNode = createNode("app_default_focus");
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appDefaultFocusNode);
    }

    /**
     * Tests {@link RotaryService#onKeyEvents} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  . (focused) .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnKeyEvents_centerButtonClickInAppWindow_injectDpadCenterEvent() {
        initActivity(R.layout.rotary_service_test_2_activity);

        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .setType(TYPE_APPLICATION)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);
        when(mRotaryService.getWindows()).thenReturn(windows);
        when(mRotaryService.getRootInActiveWindow())
                .thenReturn(MockNodeCopierProvider.get().copy(mWindowRoot));

        AccessibilityNodeInfo mockAppButton3Node = mNodeBuilder
                .setFocused(true)
                .setWindow(appWindow)
                .build();
        mRotaryService.setFocusedNode(mockAppButton3Node);

        assertThat(mRotaryService.mIgnoreViewClickedNode).isNull();

        // Click the center button of the controller.
        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        KeyEvent centerButtonEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionDown));
        KeyEvent centerButtonEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionUp));

        // RotaryService should inject KEYCODE_DPAD_CENTER event because mockAppButton3Node is in
        // the application window.
        verify(mRotaryService, times(1))
                .injectKeyEvent(KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.ACTION_DOWN);
        verify(mRotaryService, times(1))
                .injectKeyEvent(KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.ACTION_UP);
        assertThat(mRotaryService.mIgnoreViewClickedNode).isEqualTo(mockAppButton3Node);
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(mockAppButton3Node);
    }

    /**
     * Tests {@link RotaryService#onKeyEvents} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  . (focused) .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnKeyEvents_centerButtonClickInSystemWindow_performActionClick() {
        initActivity(R.layout.rotary_service_test_2_activity);

        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);
        when(mRotaryService.getWindows()).thenReturn(windows);
        when(mRotaryService.getRootInActiveWindow())
                .thenReturn(MockNodeCopierProvider.get().copy(mWindowRoot));

        Activity activity = mActivityRule.getActivity();
        Button appButton3 = activity.findViewById(R.id.app_button3);
        appButton3.setOnClickListener(v -> v.setActivated(true));
        appButton3.post(() -> appButton3.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(appButton3.isFocused()).isTrue();
        AccessibilityNodeInfo appButton3Node = createNode("app_button3");
        mRotaryService.setFocusedNode(appButton3Node);
        mRotaryService.mLongPressMs = 400;

        assertThat(appButton3.isActivated()).isFalse();
        assertThat(mRotaryService.mIgnoreViewClickedNode).isNull();

        // Click the center button of the controller.
        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        KeyEvent centerButtonEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionDown));
        KeyEvent centerButtonEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionUp));

        // appButton3Node.getWindow() will return null (because the test doesn't have the permission
        // to create an AccessibilityWindowInfo), so appButton3Node isn't considered in the
        // application window. Instead, it's considered in the system window. So RotaryService
        // should perform ACTION_CLICK on it.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(appButton3.isActivated()).isTrue();
        assertThat(mRotaryService.mIgnoreViewClickedNode).isEqualTo(appButton3Node);
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appButton3Node);
    }

    /**
     * Tests {@link RotaryService#onKeyEvents} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  . (focused) .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnKeyEvents_centerButtonLongClickInSystemWindow_performActionLongClick() {
        initActivity(R.layout.rotary_service_test_2_activity);

        AccessibilityNodeInfo appRoot = createNode("app_root");
        AccessibilityWindowInfo appWindow = new WindowBuilder()
                .setRoot(appRoot)
                .build();
        List<AccessibilityWindowInfo> windows = new ArrayList<>();
        windows.add(appWindow);
        when(mRotaryService.getWindows()).thenReturn(windows);
        when(mRotaryService.getRootInActiveWindow())
                .thenReturn(MockNodeCopierProvider.get().copy(mWindowRoot));

        Activity activity = mActivityRule.getActivity();
        Button appButton3 = activity.findViewById(R.id.app_button3);
        appButton3.setOnLongClickListener(v -> {
            v.setActivated(true);
            return true;
        });
        appButton3.post(() -> appButton3.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(appButton3.isFocused()).isTrue();
        AccessibilityNodeInfo appButton3Node = createNode("app_button3");
        mRotaryService.setFocusedNode(appButton3Node);
        mRotaryService.mLongPressMs = 0;

        assertThat(appButton3.isActivated()).isFalse();
        assertThat(mRotaryService.mIgnoreViewClickedNode).isNull();

        // Click the center button of the controller.
        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        KeyEvent centerButtonEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionDown));
        KeyEvent centerButtonEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionUp));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // appButton3Node.getWindow() will return null (because the test doesn't have the permission
        // to create an AccessibilityWindowInfo), so appButton3Node isn't considered in the
        // application window. Instead, it's considered in the system window. So RotaryService
        // should perform ACTION_LONG_CLICK on it.
        assertThat(appButton3.isActivated()).isTrue();
        assertThat(mRotaryService.mIgnoreViewClickedNode).isNull();
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appButton3Node);
    }

    /**
     * Tests {@link RotaryService#onAccessibilityEvent} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  . (focused) .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnAccessibilityEvent_typeViewFocused() {
        initActivity(R.layout.rotary_service_test_2_activity);

        // The app focuses appDefaultFocus, then the accessibility framework sends a
        // TYPE_VIEW_FOCUSED event.
        // RotaryService should set mFocusedNode to appDefaultFocusNode.

        Activity activity = mActivityRule.getActivity();
        Button appDefaultFocus = activity.findViewById(R.id.app_default_focus);
        AccessibilityNodeInfo appDefaultFocusNode = createNode("app_default_focus");
        assertThat(appDefaultFocus.isFocused()).isTrue();
        assertThat(mRotaryService.getFocusedNode()).isNull();

        mRotaryService.mInRotaryMode = true;
        AccessibilityEvent event = mock(AccessibilityEvent.class);
        when(event.getSource()).thenReturn(AccessibilityNodeInfo.obtain(appDefaultFocusNode));
        when(event.getEventType()).thenReturn(TYPE_VIEW_FOCUSED);
        mRotaryService.onAccessibilityEvent(event);

        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appDefaultFocusNode);
    }

    /**
     * Tests {@link RotaryService#onAccessibilityEvent} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  . (focused) .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnAccessibilityEvent_typeViewFocused2() {
        initActivity(R.layout.rotary_service_test_2_activity);

        // RotaryService focuses appDefaultFocus, then the app focuses on the FocusParkingView
        // and the accessibility framework sends a TYPE_VIEW_FOCUSED event.
        // RotaryService should set mFocusedNode to null.

        Activity activity = mActivityRule.getActivity();
        Button appDefaultFocus = activity.findViewById(R.id.app_default_focus);
        AccessibilityNodeInfo appDefaultFocusNode = createNode("app_default_focus");
        assertThat(appDefaultFocus.isFocused()).isTrue();
        mRotaryService.setFocusedNode(appDefaultFocusNode);
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appDefaultFocusNode);

        mRotaryService.mInRotaryMode = true;

        AccessibilityNodeInfo fpvNode = createNode("app_fpv");
        AccessibilityEvent event = mock(AccessibilityEvent.class);
        when(event.getSource()).thenReturn(AccessibilityNodeInfo.obtain(fpvNode));
        when(event.getEventType()).thenReturn(TYPE_VIEW_FOCUSED);
        mRotaryService.onAccessibilityEvent(event);

        assertThat(mRotaryService.getFocusedNode()).isNull();
    }

    /**
     * Tests {@link RotaryService#onAccessibilityEvent} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  . (focused) .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnAccessibilityEvent_typeViewClicked() {
        initActivity(R.layout.rotary_service_test_2_activity);

        // The focus is on appDefaultFocus, then the user clicks it via the rotary controller.

        Activity activity = mActivityRule.getActivity();
        Button appDefaultFocus = activity.findViewById(R.id.app_default_focus);
        AccessibilityNodeInfo appDefaultFocusNode = createNode("app_default_focus");
        assertThat(appDefaultFocus.isFocused()).isTrue();
        mRotaryService.setFocusedNode(appDefaultFocusNode);
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appDefaultFocusNode);

        mRotaryService.mInRotaryMode = true;
        mRotaryService.mIgnoreViewClickedNode = AccessibilityNodeInfo.obtain(appDefaultFocusNode);

        AccessibilityEvent event = mock(AccessibilityEvent.class);
        when(event.getSource()).thenReturn(AccessibilityNodeInfo.obtain(appDefaultFocusNode));
        when(event.getEventType()).thenReturn(TYPE_VIEW_CLICKED);
        when(event.getEventTime()).thenReturn(-1l);
        mRotaryService.onAccessibilityEvent(event);

        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appDefaultFocusNode);
        assertThat(mRotaryService.mIgnoreViewClickedNode).isNull();
        assertThat(mRotaryService.mLastTouchedNode).isNull();
    }

    /**
     * Tests {@link RotaryService#onAccessibilityEvent} in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  . (focused) .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  .           .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testOnAccessibilityEvent_typeViewClicked2() {
        initActivity(R.layout.rotary_service_test_2_activity);

        // The focus is on appDefaultFocus, then the user clicks appButton3 via the touch screen.

        Activity activity = mActivityRule.getActivity();
        Button appDefaultFocus = activity.findViewById(R.id.app_default_focus);
        AccessibilityNodeInfo appDefaultFocusNode = createNode("app_default_focus");
        assertThat(appDefaultFocus.isFocused()).isTrue();
        mRotaryService.setFocusedNode(appDefaultFocusNode);
        assertThat(mRotaryService.getFocusedNode()).isEqualTo(appDefaultFocusNode);

        mRotaryService.mInRotaryMode = true;

        AccessibilityNodeInfo appButton3Node = createNode("app_button3");
        AccessibilityEvent event = mock(AccessibilityEvent.class);
        when(event.getSource()).thenReturn(AccessibilityNodeInfo.obtain(appButton3Node));
        when(event.getEventType()).thenReturn(TYPE_VIEW_CLICKED);
        when(event.getEventTime()).thenReturn(-1l);
        mRotaryService.onAccessibilityEvent(event);

        assertThat(mRotaryService.getFocusedNode()).isNull();
        assertThat(mRotaryService.mIgnoreViewClickedNode).isNull();
        assertThat(mRotaryService.mLastTouchedNode).isEqualTo(appButton3Node);
    }

    @Test
    public void testOnAccessibilityEvent_typeWindowStateChanged() {
        AccessibilityWindowInfo window = mock(AccessibilityWindowInfo.class);
        when(window.getType()).thenReturn(TYPE_APPLICATION);
        when(window.getDisplayId()).thenReturn(DEFAULT_DISPLAY);

        AccessibilityNodeInfo node = mock(AccessibilityNodeInfo.class);
        when(node.getWindow()).thenReturn(window);

        AccessibilityEvent event = mock(AccessibilityEvent.class);
        when(event.getSource()).thenReturn(node);
        when(event.getEventType()).thenReturn(TYPE_WINDOW_STATE_CHANGED);
        final String packageName = "package.name";
        final String className = "class.name";
        when(event.getPackageName()).thenReturn(packageName);
        when(event.getClassName()).thenReturn(className);
        mRotaryService.onAccessibilityEvent(event);

        ComponentName foregroundActivity = new ComponentName(packageName, className);
        assertThat(mRotaryService.mForegroundActivity).isEqualTo(foregroundActivity);
    }

    /**
     * Tests Direct Manipulation mode in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  . (focused) .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testDirectManipulationMode1() {
        initActivity(R.layout.rotary_service_test_2_activity);

        Activity activity = mActivityRule.getActivity();
        Button appButton3 = activity.findViewById(R.id.app_button3);
        DirectManipulationHelper.setSupportsRotateDirectly(appButton3, true);
        appButton3.post(() -> appButton3.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(appButton3.isFocused()).isTrue();
        AccessibilityNodeInfo appButton3Node = createNode("app_button3");
        mRotaryService.setFocusedNode(appButton3Node);
        mRotaryService.mInRotaryMode = true;
        assertThat(mRotaryService.mInDirectManipulationMode).isFalse();
        assertThat(appButton3.isSelected()).isFalse();

        // Click the center button of the controller.
        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        KeyEvent centerButtonEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionDown));
        KeyEvent centerButtonEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionUp));

        // RotaryService should enter Direct Manipulation mode because appButton3Node
        // supports rotate directly.
        assertThat(mRotaryService.mInDirectManipulationMode).isTrue();
        assertThat(appButton3.isSelected()).isTrue();

        // Click the back button of the controller.
        KeyEvent backButtonEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(backButtonEventActionDown));
        KeyEvent backButtonEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(backButtonEventActionUp));

        // RotaryService should exit Direct Manipulation mode because appButton3Node
        // supports rotate directly.
        assertThat(mRotaryService.mInDirectManipulationMode).isFalse();
        assertThat(appButton3.isSelected()).isFalse();
    }

    /**
     * Tests Direct Manipulation mode in the following view tree:
     * <pre>
     *      The HUN window:
     *
     *      hun FocusParkingView
     *      ==========HUN focus area==========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .hun button1.  .hun button2.  =
     *      =  .           .  .           .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     *
     *      The app window:
     *
     *      app FocusParkingView
     *      ===========focus area 1===========    ============focus area 2===========
     *      =                                =    =                                 =
     *      =  .............  .............  =    =  .............                  =
     *      =  .           .  .           .  =    =  .           .                  =
     *      =  .app button1.  .   nudge   .  =    =  .app button2.                  =
     *      =  .           .  .  shortcut .  =    =  .           .                  =
     *      =  .............  .............  =    =  .............                  =
     *      =                                =    =                                 =
     *      ==================================    ===================================
     *
     *      ===========focus area 3===========
     *      =                                =
     *      =  .............  .............  =
     *      =  .           .  .           .  =
     *      =  .app button3.  .  default  .  =
     *      =  . (focused) .  .   focus   .  =
     *      =  .............  .............  =
     *      =                                =
     *      ==================================
     * </pre>
     */
    @Test
    public void testDirectManipulationMode2() {
        initActivity(R.layout.rotary_service_test_2_activity);

        Activity activity = mActivityRule.getActivity();
        Button appButton3 = activity.findViewById(R.id.app_button3);
        appButton3.post(() -> appButton3.requestFocus());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertThat(appButton3.isFocused()).isTrue();
        AccessibilityNodeInfo appButton3Node = createNode("app_button3");
        mRotaryService.setFocusedNode(appButton3Node);
        mRotaryService.mInRotaryMode = true;
        when(mRotaryService.isInApplicationWindow(appButton3Node)).thenReturn(true);
        assertThat(mRotaryService.mInDirectManipulationMode).isFalse();
        assertThat(mRotaryService.mIgnoreViewClickedNode).isNull();

        // Click the center button of the controller.
        int validDisplayId = CarInputManager.TARGET_DISPLAY_TYPE_MAIN;
        KeyEvent centerButtonEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionDown));
        KeyEvent centerButtonEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_CENTER);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(centerButtonEventActionUp));

        // RotaryService should inject KEYCODE_DPAD_CENTER event because appButton3Node doesn't
        // support rotate directly and is in the application window.
        verify(mRotaryService, times(1))
                .injectKeyEvent(KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.ACTION_DOWN);
        verify(mRotaryService, times(1))
                .injectKeyEvent(KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.ACTION_UP);
        assertThat(mRotaryService.mIgnoreViewClickedNode).isEqualTo(appButton3Node);

        // The app sends a TYPE_VIEW_ACCESSIBILITY_FOCUSED event to RotaryService.
        // RotaryService should enter Direct Manipulation mode when receiving the event.
        AccessibilityEvent event = mock(AccessibilityEvent.class);
        when(event.getSource()).thenReturn(AccessibilityNodeInfo.obtain(appButton3Node));
        when(event.getEventType()).thenReturn(TYPE_VIEW_ACCESSIBILITY_FOCUSED);
        when(event.getClassName()).thenReturn(DIRECT_MANIPULATION);
        mRotaryService.onAccessibilityEvent(event);
        assertThat(mRotaryService.mInDirectManipulationMode).isTrue();

        // Click the back button of the controller.
        KeyEvent backButtonEventActionDown =
                new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(backButtonEventActionDown));
        KeyEvent backButtonEventActionUp =
                new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK);
        mRotaryService.onKeyEvents(validDisplayId,
                Collections.singletonList(backButtonEventActionUp));

        // RotaryService should inject KEYCODE_BACK event because appButton3Node doesn't
        // support rotate directly and is in the application window.
        verify(mRotaryService, times(1))
                .injectKeyEvent(KeyEvent.KEYCODE_BACK, KeyEvent.ACTION_DOWN);
        verify(mRotaryService, times(1))
                .injectKeyEvent(KeyEvent.KEYCODE_BACK, KeyEvent.ACTION_UP);

        // The app sends a TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED event to RotaryService.
        // RotaryService should exit Direct Manipulation mode when receiving the event.
        event = mock(AccessibilityEvent.class);
        when(event.getSource()).thenReturn(AccessibilityNodeInfo.obtain(appButton3Node));
        when(event.getEventType()).thenReturn(TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
        when(event.getClassName()).thenReturn(DIRECT_MANIPULATION);
        mRotaryService.onAccessibilityEvent(event);
        assertThat(mRotaryService.mInDirectManipulationMode).isFalse();
    }

    /**
     * Starts the test activity with the given layout and initializes the root
     * {@link AccessibilityNodeInfo}.
     */
    private void initActivity(@LayoutRes int layoutResId) {
        mIntent.putExtra(NavigatorTestActivity.KEY_LAYOUT_ID, layoutResId);
        mActivityRule.launchActivity(mIntent);
        mWindowRoot = sUiAutomoation.getRootInActiveWindow();
    }

    /**
     * Returns the {@link AccessibilityNodeInfo} related to the provided {@code viewId}. Returns
     * null if no such node exists. Callers should ensure {@link #initActivity} has already been
     * called. Caller shouldn't recycle the result because it will be recycled in {@link #tearDown}.
     */
    private AccessibilityNodeInfo createNode(String viewId) {
        String fullViewId = "com.android.car.rotary.tests.unit:id/" + viewId;
        List<AccessibilityNodeInfo> nodes =
                mWindowRoot.findAccessibilityNodeInfosByViewId(fullViewId);
        if (nodes.isEmpty()) {
            L.e("Failed to create node by View ID " + viewId);
            return null;
        }
        mNodes.addAll(nodes);
        return nodes.get(0);
    }
}
