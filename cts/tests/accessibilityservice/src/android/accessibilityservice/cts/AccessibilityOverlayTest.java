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

package android.accessibilityservice.cts;

import static org.junit.Assert.assertTrue;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityService;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.accessibilityservice.cts.utils.AsyncUtils;
import android.accessibilityservice.cts.utils.DisplayUtils;
import android.app.UiAutomation;
import android.content.Context;
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.Display;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityWindowInfo;
import android.widget.Button;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.List;

// Test that an AccessibilityService can display an accessibility overlay
@RunWith(AndroidJUnit4.class)
public class AccessibilityOverlayTest {

    private static UiAutomation sUiAutomation;
    InstrumentedAccessibilityService mService;

    private InstrumentedAccessibilityServiceTestRule<StubAccessibilityButtonService>
            mServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
                    StubAccessibilityButtonService.class);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mServiceRule)
            .around(mDumpOnFailureRule);

    @BeforeClass
    public static void oneTimeSetUp() {
        sUiAutomation = InstrumentationRegistry.getInstrumentation()
                .getUiAutomation(UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
        AccessibilityServiceInfo info = sUiAutomation.getServiceInfo();
        info.flags |= AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS;
        sUiAutomation.setServiceInfo(info);
    }

    @Before
    public void setUp() {
        mService = mServiceRule.getService();
    }

    @Test
    public void testA11yServiceShowsOverlay_shouldAppear() throws Exception {
        final String overlayTitle = "Overlay title";

        sUiAutomation.executeAndWaitForEvent(() -> mService.runOnServiceSync(() -> {
            addOverlayWindow(mService, overlayTitle);
        }), (event) -> findOverlayWindow(Display.DEFAULT_DISPLAY) != null, AsyncUtils.DEFAULT_TIMEOUT_MS);

        assertTrue(TextUtils.equals(findOverlayWindow(Display.DEFAULT_DISPLAY).getTitle(), overlayTitle));
    }

    @Test
    public void testA11yServiceShowsOverlayOnVirtualDisplay_shouldAppear() throws Exception {
        try (DisplayUtils.VirtualDisplaySession displaySession =
                     new DisplayUtils.VirtualDisplaySession()) {
            Display newDisplay = displaySession.createDisplayWithDefaultDisplayMetricsAndWait(
                    mService, false);
            final int displayId = newDisplay.getDisplayId();
            final Context newDisplayContext = mService.createDisplayContext(newDisplay);
            final String overlayTitle = "Overlay title on virtualDisplay";

            sUiAutomation.executeAndWaitForEvent(() -> mService.runOnServiceSync(() -> {
                addOverlayWindow(newDisplayContext, overlayTitle);
            }), (event) -> findOverlayWindow(displayId) != null, AsyncUtils.DEFAULT_TIMEOUT_MS);

            assertTrue(TextUtils.equals(findOverlayWindow(displayId).getTitle(), overlayTitle));
        }
    }

    private void addOverlayWindow(Context context, String overlayTitle) {
        final Button button = new Button(context);
        button.setText("Button");
        final WindowManager.LayoutParams params = new WindowManager.LayoutParams();
        params.width = WindowManager.LayoutParams.MATCH_PARENT;
        params.height = WindowManager.LayoutParams.MATCH_PARENT;
        params.flags = WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                | WindowManager.LayoutParams.FLAG_LAYOUT_INSET_DECOR;
        params.type = WindowManager.LayoutParams.TYPE_ACCESSIBILITY_OVERLAY;
        params.setTitle(overlayTitle);
        context.getSystemService(WindowManager.class).addView(button, params);
    }

    private AccessibilityWindowInfo findOverlayWindow(int displayId) {
        final SparseArray<List<AccessibilityWindowInfo>> allWindows =
                sUiAutomation.getWindowsOnAllDisplays();
        final int index = allWindows.indexOfKey(displayId);
        final List<AccessibilityWindowInfo> windows = allWindows.valueAt(index);
        if (windows != null) {
            for (AccessibilityWindowInfo window : windows) {
                if (window.getType() == AccessibilityWindowInfo.TYPE_ACCESSIBILITY_OVERLAY) {
                    return window;
                }
            }
        }
        return null;
    }
}
