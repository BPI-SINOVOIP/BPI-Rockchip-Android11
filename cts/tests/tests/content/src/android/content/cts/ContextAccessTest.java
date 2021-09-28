/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.content.cts;

import static android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY;
import static android.hardware.display.DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC;
import static android.view.Display.DEFAULT_DISPLAY;
import static android.view.WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.app.Service;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.ImageReader;
import android.os.Bundle;
import android.os.IBinder;
import android.platform.test.annotations.Presubmit;
import android.view.Display;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.rule.ServiceTestRule;

import org.junit.Rule;
import org.junit.Test;

import java.util.concurrent.TimeoutException;

/**
 * Test for {@link Context#getDisplay()}.
 * <p>Test context type listed below:</p>
 * <ul>
 *     <li>{@link android.app.Application} - throw exception</li>
 *     <li>{@link Service} - throw exception</li>
 *     <li>{@link Activity} - get {@link Display} entity</li>
 *     <li>Context via {@link Context#createWindowContext(int, Bundle)}
 *     - get {@link Display} entity</li>
 *     <li>Context via {@link Context#createDisplayContext(Display)}
 *     - get {@link Display} entity</li>
 *     <li>Context derived from display context
 *     - get {@link Display} entity</li>
 *     <li>{@link ContextWrapper} with base display-associated {@link Context}
 *     - get {@link Display} entity</li>
 *     <li>{@link ContextWrapper} with base non-display-associated {@link Context}
 *     - get {@link Display} entity</li>
 * </ul>
 *
 * <p>Build/Install/Run:
 *     atest CtsContentTestCases:ContextAccessTest
 */
@Presubmit
@SmallTest
public class ContextAccessTest {
    private Context mContext = ApplicationProvider.getApplicationContext();

    @Rule
    public final ActivityTestRule<MockActivity> mActivityRule =
            new ActivityTestRule<>(MockActivity.class);

    @Test(expected = UnsupportedOperationException.class)
    public void testGetDisplayFromApplication() {
        mContext.getDisplay();
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testGetDisplayFromService() throws TimeoutException {
        getTestService().getDisplay();
    }

    @Test
    public void testGetDisplayFromActivity() throws Throwable {
        final Display d = getTestActivity().getDisplay();

        assertNotNull("Display must be accessible from visual components", d);
    }

    @Test
    public void testGetDisplayFromDisplayContext() {
        final Display display = mContext.getSystemService(DisplayManager.class)
                .getDisplay(DEFAULT_DISPLAY);
        Context displayContext = mContext.createDisplayContext(display);

        assertEquals(display, displayContext.getDisplay());
    }

    @Test
    public void testGetDisplayFromDisplayContextDerivedContextOnPrimaryDisplay() {
        verifyGetDisplayFromDisplayContextDerivedContext(false /* onSecondaryDisplay */);
    }

    @Test
    public void testGetDisplayFromDisplayContextDerivedContextOnSecondaryDisplay() {
        verifyGetDisplayFromDisplayContextDerivedContext(true /* onSecondaryDisplay */);
    }

    private void verifyGetDisplayFromDisplayContextDerivedContext(
            boolean onSecondaryDisplay) {
        final DisplayManager displayManager = mContext.getSystemService(DisplayManager.class);
        final Display display;
        if (onSecondaryDisplay) {
            display = getSecondaryDisplay(displayManager);
        } else {
            display = displayManager.getDisplay(DEFAULT_DISPLAY);
        }
        final Context context = mContext.createDisplayContext(display)
                .createConfigurationContext(new Configuration());
        assertEquals(display, context.getDisplay());
    }

    private static Display getSecondaryDisplay(DisplayManager displayManager) {
        final int width = 800;
        final int height = 480;
        final int density = 160;
        ImageReader reader = ImageReader.newInstance(width, height, PixelFormat.RGBA_8888,
                2 /* maxImages */);
        VirtualDisplay virtualDisplay = displayManager.createVirtualDisplay(
                ContextTest.class.getName(), width, height, density, reader.getSurface(),
                VIRTUAL_DISPLAY_FLAG_PUBLIC | VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY);
        return virtualDisplay.getDisplay();
    }

    @Test
    public void testGetDisplayFromWindowContext() {
        final Display display = mContext.getSystemService(DisplayManager.class)
                .getDisplay(DEFAULT_DISPLAY);
        Context windowContext =  mContext.createDisplayContext(display)
                .createWindowContext(TYPE_APPLICATION_OVERLAY, null /*  options */);
        assertEquals(display, windowContext.getDisplay());
    }

    @Test
    public void testGetDisplayFromVisualWrapper() throws Throwable {
        final Display d = new ContextWrapper(getTestActivity()).getDisplay();

        assertNotNull("Display must be accessible from visual components", d);
    }

    @Test(expected = UnsupportedOperationException.class)
    public void testGetDisplayFromNonVisualWrapper() {
        ContextWrapper wrapper = new ContextWrapper(mContext);
        wrapper.getDisplay();
    }

    private Activity getTestActivity() throws Throwable {
        MockActivity[] activity = new MockActivity[1];
        mActivityRule.runOnUiThread(() -> {
            activity[0] = mActivityRule.getActivity();
        });
        return activity[0];
    }

    private Service getTestService() throws TimeoutException {
        final Intent intent = new Intent(mContext.getApplicationContext(), MockService.class);
        final ServiceTestRule serviceRule = new ServiceTestRule();
        IBinder serviceToken;
        serviceToken = serviceRule.bindService(intent);
        return ((MockService.MockBinder) serviceToken).getService();
    }
}
