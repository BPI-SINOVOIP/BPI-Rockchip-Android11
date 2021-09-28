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

package com.android.documentsui;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Paint;
import android.os.Build;
import android.os.ParcelFileDescriptor;

import androidx.fragment.app.FragmentManager;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.dirlist.RenameDocumentFragment;
import com.android.documentsui.files.DeleteDocumentFragment;
import com.android.documentsui.files.FilesActivity;
import com.android.documentsui.queries.SearchFragment;
import com.android.documentsui.sorting.SortListFragment;
import com.android.documentsui.sorting.SortModel;

import com.google.android.material.textfield.TextInputEditText;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import java.io.FileInputStream;
import java.io.IOException;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class DialogUiTest {

    private static final String CREATE_FRAGEMENT_TAG = "create_directory";
    CreateDirectoryFragment mCreateDirectoryFragment;
    FragmentManager mFragmentManager;
    ScreenDensitySession mScreenDensitySession;
    Intent mFileActivityIntent;

    @Rule
    public ActivityTestRule<FilesActivity> mActivityTestRule = new ActivityTestRule<>(
            FilesActivity.class);

    @Before
    public void setup() {
        mFileActivityIntent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        mFragmentManager = mActivityTestRule.getActivity().getSupportFragmentManager();
        mScreenDensitySession = new ScreenDensitySession();
    }

    @After
    public void tearDown() {
        mScreenDensitySession.close();
        mCreateDirectoryFragment = null;
    }

    @Test
    public void testCreateDialogShows() throws Throwable {
        mActivityTestRule.runOnUiThread(() -> CreateDirectoryFragment.show(mFragmentManager));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mCreateDirectoryFragment =
                (CreateDirectoryFragment) mFragmentManager.findFragmentByTag(CREATE_FRAGEMENT_TAG);

        assertNotNull("Dialog was null", mCreateDirectoryFragment.getDialog());
        assertTrue("Dialog was not being shown", mCreateDirectoryFragment.getDialog().isShowing());
    }

    @Test
    public void testCreateDialogShowsDismiss() throws Throwable {
        mActivityTestRule.runOnUiThread(() -> CreateDirectoryFragment.show(mFragmentManager));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mCreateDirectoryFragment =
                (CreateDirectoryFragment) mFragmentManager.findFragmentByTag(CREATE_FRAGEMENT_TAG);

        assertNotNull("Dialog was null", mCreateDirectoryFragment.getDialog());
        assertTrue("Dialog was not being shown", mCreateDirectoryFragment.getDialog().isShowing());

        mActivityTestRule.runOnUiThread(() -> mCreateDirectoryFragment.getDialog().dismiss());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        assertNull("Dialog should be null after dismiss()", mCreateDirectoryFragment.getDialog());
    }

    @Test
    public void testCreateDialogShows_textInputEditText_shouldNotTruncateOnPortrait()
            throws Throwable {
        mActivityTestRule.runOnUiThread(() -> CreateDirectoryFragment.show(mFragmentManager));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mCreateDirectoryFragment =
                (CreateDirectoryFragment) mFragmentManager.findFragmentByTag(CREATE_FRAGEMENT_TAG);

        final TextInputEditText inputView =
                mCreateDirectoryFragment.getDialog().findViewById(android.R.id.text1);

        assertTrue(inputView.getHeight() > getInputTextHeight(inputView));
    }

    @Test
    public void testCreateDialog_textInputEditText_shouldNotTruncateOnLargeDensity()
            throws Throwable {

        mScreenDensitySession.setLargeDensity();
        mActivityTestRule.finishActivity();
        mActivityTestRule.launchActivity(mFileActivityIntent);
        mFragmentManager = mActivityTestRule.getActivity().getSupportFragmentManager();

        mActivityTestRule.runOnUiThread(() -> CreateDirectoryFragment.show(mFragmentManager));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mCreateDirectoryFragment =
                (CreateDirectoryFragment) mFragmentManager.findFragmentByTag(CREATE_FRAGEMENT_TAG);

        final TextInputEditText inputView =
                mCreateDirectoryFragment.getDialog().getWindow().findViewById(android.R.id.text1);

        assertTrue(inputView.getHeight() > getInputTextHeight(inputView));

    }

    @Test
    public void testCreateDialog_textInputEditText_shouldNotTruncateOnLargerDensity()
            throws Throwable {

        mScreenDensitySession.setLargerDensity();
        mActivityTestRule.finishActivity();
        mActivityTestRule.launchActivity(mFileActivityIntent);
        mFragmentManager = mActivityTestRule.getActivity().getSupportFragmentManager();

        mActivityTestRule.runOnUiThread(() -> CreateDirectoryFragment.show(mFragmentManager));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mCreateDirectoryFragment =
                (CreateDirectoryFragment) mFragmentManager.findFragmentByTag(CREATE_FRAGEMENT_TAG);

        final TextInputEditText inputView =
                mCreateDirectoryFragment.getDialog().getWindow().findViewById(android.R.id.text1);

        assertTrue(inputView.getHeight() > getInputTextHeight(inputView));
    }

    @Test
    public void testCreateDialog_textInputEditText_shouldNotTruncateOnLargestDensity()
            throws Throwable {

        mScreenDensitySession.setLargestDensity();
        mActivityTestRule.finishActivity();
        mActivityTestRule.launchActivity(mFileActivityIntent);
        mFragmentManager = mActivityTestRule.getActivity().getSupportFragmentManager();

        mActivityTestRule.runOnUiThread(() -> CreateDirectoryFragment.show(mFragmentManager));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mCreateDirectoryFragment =
                (CreateDirectoryFragment) mFragmentManager.findFragmentByTag(CREATE_FRAGEMENT_TAG);

        final TextInputEditText inputView =
                mCreateDirectoryFragment.getDialog().getWindow().findViewById(android.R.id.text1);

        assertTrue(inputView.getHeight() > getInputTextHeight(inputView));
    }

    @Test
    public void testCreateDirectoryFragmentShows_textInputEditText_shouldNotTruncateOnLandscape()
            throws Throwable {
        switchOrientation(mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mScreenDensitySession.setLargestDensity();
        mActivityTestRule.finishActivity();
        mActivityTestRule.launchActivity(mFileActivityIntent);
        mFragmentManager = mActivityTestRule.getActivity().getSupportFragmentManager();

        mActivityTestRule.runOnUiThread(() -> CreateDirectoryFragment.show(mFragmentManager));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mCreateDirectoryFragment =
                (CreateDirectoryFragment) mFragmentManager.findFragmentByTag(CREATE_FRAGEMENT_TAG);

        final TextInputEditText inputView =
                mCreateDirectoryFragment.getDialog().getWindow().findViewById(android.R.id.text1);

        assertTrue(
                "Failed with inputView height " + inputView.getHeight() + " and input text height "
                        + getInputTextHeight(inputView),
                inputView.getHeight() > getInputTextHeight(inputView));

        switchOrientation(mActivityTestRule.getActivity());
    }

    @Test
    public void testCreateDirectoryFragmentShows_skipWhenStateSaved() {
        mFragmentManager = Mockito.mock(FragmentManager.class);
        Mockito.when(mFragmentManager.isStateSaved()).thenReturn(true);

        // Use mock FragmentManager will cause NPE then test fail when DialogFragment.show is
        // called, so test pass means it skip.
        CreateDirectoryFragment.show(mFragmentManager);
    }

    @Test
    public void testDeleteDocumentFragmentShows_skipWhenStateSaved() {
        mFragmentManager = Mockito.mock(FragmentManager.class);
        Mockito.when(mFragmentManager.isStateSaved()).thenReturn(true);

        DeleteDocumentFragment.show(mFragmentManager, null, null);
    }

    @Test
    public void testRenameDocumentFragmentShows_skipWhenStateSaved() {
        mFragmentManager = Mockito.mock(FragmentManager.class);
        Mockito.when(mFragmentManager.isStateSaved()).thenReturn(true);

        RenameDocumentFragment.show(mFragmentManager, new DocumentInfo());
    }

    @Test
    public void testSearchFragmentShows_skipWhenStateSaved() {
        mFragmentManager = Mockito.mock(FragmentManager.class);
        Mockito.when(mFragmentManager.isStateSaved()).thenReturn(true);

        SearchFragment.showFragment(mFragmentManager, "");
    }

    @Test
    public void testSortListFragmentShows_skipWhenStateSaved() {
        mFragmentManager = Mockito.mock(FragmentManager.class);
        Mockito.when(mFragmentManager.isStateSaved()).thenReturn(true);
        SortModel sortModel = Mockito.mock(SortModel.class);

        SortListFragment.show(mFragmentManager, sortModel);
    }

    private static int getInputTextHeight(TextInputEditText v) {
        Paint paint = v.getPaint();
        final float textSize = paint.getTextSize();
        final float textSpace = paint.getFontSpacing();
        return Math.round(textSize + textSpace);
    }

    private static void switchOrientation(final Activity activity) {
        final int[] orientations = getOrientations(activity);
        activity.setRequestedOrientation(orientations[1]);
    }

    private static int[] getOrientations(final Activity activity) {
        final int originalOrientation = activity.getResources().getConfiguration().orientation;
        final int newOrientation = originalOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                ? ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
        return new int[]{originalOrientation, newOrientation};
    }

    private static class ScreenDensitySession implements AutoCloseable {
        private static final String DENSITY_PROP_DEVICE = "ro.sf.lcd_density";
        private static final String DENSITY_PROP_EMULATOR = "qemu.sf.lcd_density";

        private static final float DENSITY_DEFAULT = 1f;
        private static final float DENSITY_LARGE = 1.09f;
        private static final float DENSITY_LARGER = 1.19f;
        private static final float DENSITY_LARGEST = 1.29f;

        void setDefaultDensity() {
            final int stableDensity = getStableDensity();
            final int targetDensity = (int) (stableDensity * DENSITY_DEFAULT);
            setDensity(targetDensity);
        }

        void setLargeDensity() {
            final int stableDensity = getStableDensity();
            final int targetDensity = (int) (stableDensity * DENSITY_LARGE);
            setDensity(targetDensity);
        }

        void setLargerDensity() {
            final int stableDensity = getStableDensity();
            final int targetDensity = (int) (stableDensity * DENSITY_LARGER);
            setDensity(targetDensity);
        }

        void setLargestDensity() {
            final int stableDensity = getStableDensity();
            final int targetDensity = (int) (stableDensity * DENSITY_LARGEST);
            setDensity(targetDensity);
        }

        @Override
        public void close() {
            resetDensity();
        }

        private int getStableDensity() {
            final String densityProp;
            if (Build.IS_EMULATOR) {
                densityProp = DENSITY_PROP_EMULATOR;
            } else {
                densityProp = DENSITY_PROP_DEVICE;
            }

            return Integer.parseInt(executeShellCommand("getprop " + densityProp).trim());
        }

        private void setDensity(int targetDensity) {
            executeShellCommand("wm density " + targetDensity);

            // Verify that the density is changed.
            final String output = executeShellCommand("wm density");
            final boolean success = output.contains("Override density: " + targetDensity);

            assertTrue("Failed to set density to " + targetDensity, success);
        }

        private void resetDensity() {
            executeShellCommand("wm density reset");
        }
    }

    public static String executeShellCommand(String cmd) {
        try {
            return runShellCommand(InstrumentationRegistry.getInstrumentation(), cmd);
        } catch (IOException e) {
            fail("Failed reading command output: " + e);
            return "";
        }
    }

    public static String runShellCommand(Instrumentation instrumentation, String cmd)
            throws IOException {
        return runShellCommand(instrumentation.getUiAutomation(), cmd);
    }

    public static String runShellCommand(UiAutomation automation, String cmd)
            throws IOException {
        if (cmd.startsWith("pm grant ") || cmd.startsWith("pm revoke ")) {
            throw new UnsupportedOperationException("Use UiAutomation.grantRuntimePermission() "
                    + "or revokeRuntimePermission() directly, which are more robust.");
        }
        ParcelFileDescriptor pfd = automation.executeShellCommand(cmd);
        byte[] buf = new byte[512];
        int bytesRead;
        FileInputStream fis = new ParcelFileDescriptor.AutoCloseInputStream(pfd);
        StringBuffer stdout = new StringBuffer();
        while ((bytesRead = fis.read(buf)) != -1) {
            stdout.append(new String(buf, 0, bytesRead));
        }
        fis.close();
        return stdout.toString();
    }
}
