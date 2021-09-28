/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.systemui.statusbar.phone;

import static android.app.StatusBarManager.NAVIGATION_HINT_BACK_ALT;
import static android.app.StatusBarManager.NAVIGATION_HINT_IME_SHOWN;
import static android.inputmethodservice.InputMethodService.BACK_DISPOSITION_DEFAULT;
import static android.inputmethodservice.InputMethodService.IME_VISIBLE;
import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.DisplayAdjustments.DEFAULT_DISPLAY_ADJUSTMENTS;

import static com.android.systemui.statusbar.phone.NavigationBarFragment.NavBarActionEvent.NAVBAR_ASSIST_LONGPRESS;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.annotation.LayoutRes;
import android.annotation.Nullable;
import android.app.Fragment;
import android.app.FragmentController;
import android.app.FragmentHostCallback;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.IntentFilter;
import android.hardware.display.DisplayManagerGlobal;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.UserHandle;
import android.testing.AndroidTestingRunner;
import android.testing.LeakCheck.Tracker;
import android.testing.TestableLooper;
import android.testing.TestableLooper.RunWithLooper;
import android.view.Display;
import android.view.DisplayInfo;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityManager.AccessibilityServicesStateChangeListener;

import androidx.test.filters.SmallTest;

import com.android.internal.logging.MetricsLogger;
import com.android.internal.logging.UiEventLogger;
import com.android.systemui.Dependency;
import com.android.systemui.SysuiBaseFragmentTest;
import com.android.systemui.SysuiTestableContext;
import com.android.systemui.accessibility.SystemActions;
import com.android.systemui.assist.AssistManager;
import com.android.systemui.broadcast.BroadcastDispatcher;
import com.android.systemui.model.SysUiState;
import com.android.systemui.plugins.statusbar.StatusBarStateController;
import com.android.systemui.recents.OverviewProxyService;
import com.android.systemui.recents.Recents;
import com.android.systemui.stackdivider.Divider;
import com.android.systemui.statusbar.CommandQueue;
import com.android.systemui.statusbar.NotificationRemoteInputManager;
import com.android.systemui.statusbar.policy.AccessibilityManagerWrapper;
import com.android.systemui.statusbar.policy.DeviceProvisionedController;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.Optional;

@RunWith(AndroidTestingRunner.class)
@RunWithLooper(setAsMainLooper = true)
@SmallTest
public class NavigationBarFragmentTest extends SysuiBaseFragmentTest {
    private static final int EXTERNAL_DISPLAY_ID = 2;
    private static final int NAV_BAR_VIEW_ID = 43;

    private Fragment mFragmentExternalDisplay;
    private FragmentController mControllerExternalDisplay;

    private SysuiTestableContext mSysuiTestableContextExternal;
    private OverviewProxyService mOverviewProxyService;
    private CommandQueue mCommandQueue;
    private SysUiState mMockSysUiState;
    @Mock
    private BroadcastDispatcher mBroadcastDispatcher;
    @Mock
    private Divider mDivider;
    @Mock
    private Recents mRecents;
    @Mock
    private SystemActions mSystemActions;
    @Mock
    private UiEventLogger mUiEventLogger;

    private AccessibilityManagerWrapper mAccessibilityWrapper =
            new AccessibilityManagerWrapper(mContext) {
                Tracker mTracker = mLeakCheck.getTracker("accessibility_manager");

                @Override
                public void addCallback(AccessibilityServicesStateChangeListener listener) {
                    mTracker.getLeakInfo(listener).addAllocation(new Throwable());
                }

                @Override
                public void removeCallback(AccessibilityServicesStateChangeListener listener) {
                    mTracker.getLeakInfo(listener).clearAllocations();
                }
            };

    public NavigationBarFragmentTest() {
        super(NavigationBarFragment.class);
    }

    protected void createRootView() {
        mView = new NavigationBarFrame(mSysuiContext);
        mView.setId(NAV_BAR_VIEW_ID);
    }

    @Before
    public void setupFragment() throws Exception {
        MockitoAnnotations.initMocks(this);

        mCommandQueue = new CommandQueue(mContext);
        setupSysuiDependency();
        createRootView();
        mOverviewProxyService =
                mDependency.injectMockDependency(OverviewProxyService.class);
        TestableLooper.get(this).runWithLooper(() -> {
            mHandler = new Handler();

            mFragment = instantiate(mSysuiContext, NavigationBarFragment.class.getName(), null);
            mFragments = FragmentController.createController(
                    new HostCallbacksForExternalDisplay(mSysuiContext));
            mFragments.attachHost(null);
            mFragments.getFragmentManager().beginTransaction()
                    .replace(NAV_BAR_VIEW_ID, mFragment)
                    .commit();
            mControllerExternalDisplay = FragmentController.createController(
                    new HostCallbacksForExternalDisplay(mSysuiTestableContextExternal));
            mControllerExternalDisplay.attachHost(null);
            mFragmentExternalDisplay = instantiate(mSysuiTestableContextExternal,
                    NavigationBarFragment.class.getName(), null);
            mControllerExternalDisplay.getFragmentManager().beginTransaction()
                    .replace(NAV_BAR_VIEW_ID, mFragmentExternalDisplay)
                    .commit();
        });
    }

    private void setupSysuiDependency() {
        Display display = new Display(DisplayManagerGlobal.getInstance(), EXTERNAL_DISPLAY_ID,
                new DisplayInfo(), DEFAULT_DISPLAY_ADJUSTMENTS);
        mSysuiTestableContextExternal = (SysuiTestableContext) mSysuiContext.createDisplayContext(
                display);

        injectLeakCheckedDependencies(ALL_SUPPORTED_CLASSES);
        WindowManager windowManager = mock(WindowManager.class);
        Display defaultDisplay = mContext.getSystemService(WindowManager.class).getDefaultDisplay();
        when(windowManager.getDefaultDisplay()).thenReturn(
                defaultDisplay);
        mContext.addMockSystemService(Context.WINDOW_SERVICE, windowManager);

        mDependency.injectTestDependency(Dependency.BG_LOOPER, Looper.getMainLooper());
        mDependency.injectTestDependency(AccessibilityManagerWrapper.class, mAccessibilityWrapper);

        mMockSysUiState = mock(SysUiState.class);
        when(mMockSysUiState.setFlag(anyInt(), anyBoolean())).thenReturn(mMockSysUiState);
    }

    @Test
    public void testHomeLongPress() {
        NavigationBarFragment navigationBarFragment = (NavigationBarFragment) mFragment;

        mFragments.dispatchResume();
        processAllMessages();
        navigationBarFragment.onHomeLongClick(navigationBarFragment.getView());

        verify(mUiEventLogger, times(1)).log(NAVBAR_ASSIST_LONGPRESS);
    }

    @Test
    public void testRegisteredWithDispatcher() {
        mFragments.dispatchResume();
        processAllMessages();

        verify(mBroadcastDispatcher).registerReceiverWithHandler(
                any(BroadcastReceiver.class),
                any(IntentFilter.class),
                any(Handler.class),
                any(UserHandle.class));
    }

    @Test
    public void testSetImeWindowStatusWhenImeSwitchOnDisplay() {
        // Create default & external NavBar fragment.
        NavigationBarFragment defaultNavBar = (NavigationBarFragment) mFragment;
        NavigationBarFragment externalNavBar = (NavigationBarFragment) mFragmentExternalDisplay;
        mFragments.dispatchCreate();
        processAllMessages();
        mFragments.dispatchResume();
        processAllMessages();
        mControllerExternalDisplay.dispatchCreate();
        processAllMessages();
        mControllerExternalDisplay.dispatchResume();
        processAllMessages();

        // Set IME window status for default NavBar.
        mCommandQueue.setImeWindowStatus(DEFAULT_DISPLAY, null, IME_VISIBLE,
                BACK_DISPOSITION_DEFAULT, true, false);
        processAllMessages();

        // Verify IME window state will be updated in default NavBar & external NavBar state reset.
        assertEquals(NAVIGATION_HINT_BACK_ALT | NAVIGATION_HINT_IME_SHOWN,
                defaultNavBar.getNavigationIconHints());
        assertFalse((externalNavBar.getNavigationIconHints() & NAVIGATION_HINT_BACK_ALT) != 0);
        assertFalse((externalNavBar.getNavigationIconHints() & NAVIGATION_HINT_IME_SHOWN) != 0);

        // Set IME window status for external NavBar.
        mCommandQueue.setImeWindowStatus(EXTERNAL_DISPLAY_ID, null,
                IME_VISIBLE, BACK_DISPOSITION_DEFAULT, true, false);
        processAllMessages();

        // Verify IME window state will be updated in external NavBar & default NavBar state reset.
        assertEquals(NAVIGATION_HINT_BACK_ALT | NAVIGATION_HINT_IME_SHOWN,
                externalNavBar.getNavigationIconHints());
        assertFalse((defaultNavBar.getNavigationIconHints() & NAVIGATION_HINT_BACK_ALT) != 0);
        assertFalse((defaultNavBar.getNavigationIconHints() & NAVIGATION_HINT_IME_SHOWN) != 0);
    }

    @Override
    protected Fragment instantiate(Context context, String className, Bundle arguments) {
        DeviceProvisionedController deviceProvisionedController =
                mock(DeviceProvisionedController.class);
        when(deviceProvisionedController.isDeviceProvisioned()).thenReturn(true);
        assertNotNull(mAccessibilityWrapper);
        return new NavigationBarFragment(
                context.getDisplayId() == DEFAULT_DISPLAY ? mAccessibilityWrapper
                        : mock(AccessibilityManagerWrapper.class),
                deviceProvisionedController,
                new MetricsLogger(),
                mock(AssistManager.class),
                mOverviewProxyService,
                mock(NavigationModeController.class),
                mock(StatusBarStateController.class),
                mMockSysUiState,
                mBroadcastDispatcher,
                mCommandQueue,
                mDivider,
                Optional.of(mRecents),
                () -> mock(StatusBar.class),
                mock(ShadeController.class),
                mock(NotificationRemoteInputManager.class),
                mock(SystemActions.class),
                mHandler,
                mUiEventLogger);
    }

    private class HostCallbacksForExternalDisplay extends
            FragmentHostCallback<NavigationBarFragmentTest> {
        private Context mDisplayContext;

        HostCallbacksForExternalDisplay(Context context) {
            super(context, mHandler, 0);
            mDisplayContext = context;
        }

        @Override
        public NavigationBarFragmentTest onGetHost() {
            return NavigationBarFragmentTest.this;
        }

        @Override
        public Fragment instantiate(Context context, String className, Bundle arguments) {
            return NavigationBarFragmentTest.this.instantiate(context, className, arguments);
        }

        @Override
        public View onFindViewById(int id) {
            return mView.findViewById(id);
        }

        @Override
        public LayoutInflater onGetLayoutInflater() {
            return new LayoutInflaterWrapper(mDisplayContext);
        }
    }

    private static class LayoutInflaterWrapper extends LayoutInflater {
        protected LayoutInflaterWrapper(Context context) {
            super(context);
        }

        @Override
        public LayoutInflater cloneInContext(Context newContext) {
            return null;
        }

        @Override
        public View inflate(@LayoutRes int resource, @Nullable ViewGroup root,
                boolean attachToRoot) {
            NavigationBarView view = mock(NavigationBarView.class);
            when(view.getDisplay()).thenReturn(mContext.getDisplay());
            when(view.getBackButton()).thenReturn(mock(ButtonDispatcher.class));
            when(view.getHomeButton()).thenReturn(mock(ButtonDispatcher.class));
            when(view.getRecentsButton()).thenReturn(mock(ButtonDispatcher.class));
            when(view.getAccessibilityButton()).thenReturn(mock(ButtonDispatcher.class));
            when(view.getRotateSuggestionButton()).thenReturn(mock(RotationContextButton.class));
            when(view.getBarTransitions()).thenReturn(mock(NavigationBarTransitions.class));
            when(view.getLightTransitionsController()).thenReturn(
                    mock(LightBarTransitionsController.class));
            when(view.getRotationButtonController()).thenReturn(
                    mock(RotationButtonController.class));
            when(view.isRecentsButtonVisible()).thenReturn(true);
            return view;
        }
    }
}
