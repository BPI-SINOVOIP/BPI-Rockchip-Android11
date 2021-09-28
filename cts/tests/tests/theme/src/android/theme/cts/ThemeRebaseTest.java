/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.theme.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.util.TypedValue;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ThemeRebaseTest {

    private Context mContext;
    private Resources.Theme mTheme;
    private Configuration mInitialConfig;
    private TypedValue mOutValue = new TypedValue();

    @Before
    public void before() {
        Context targetContext = InstrumentationRegistry.getTargetContext();
        Configuration config = new Configuration();
        config.orientation = Configuration.ORIENTATION_PORTRAIT;
        mContext = targetContext.createConfigurationContext(config);
        mTheme = mContext.getTheme();
        mInitialConfig = new Configuration(mContext.getResources().getConfiguration());
    }

    /**
     * {@link Resources#updateConfiguration(Configuration, DisplayMetrics)} doesn't update the
     * {@link android.app.ResourcesManager} cache's {@link android.content.res.ResourcesKey},
     * so we need to manually reset the {@link Configuration} so that new calls to
     * {@link Context#createConfigurationContext} work as expected.
     */
    @After
    public void after() {
        Resources resources = mContext.getResources();
        resources.updateConfiguration(mInitialConfig, resources.getDisplayMetrics());
    }

    @Test
    public void testConfigChangeAndRebase() {
        mTheme.applyStyle(R.style.RebaseTestThemeBase, false);

        assertConfigText("base");

        mTheme.applyStyle(R.style.RebaseTestThemeOverlay, true);

        assertConfigText("rebase_portrait");

        Configuration newConfig = new Configuration(mInitialConfig);
        newConfig.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mContext.getResources().updateConfiguration(newConfig, null);

        assertNotEquals(mTheme.getChangingConfigurations(), 0);

        // Assert value hasn't changed before rebase
        assertConfigText("rebase_portrait");

        mTheme.rebase();

        assertConfigText("rebase_landscape");
    }

    @Test
    public void testNoConfigChangeDoesNothing() {
        mTheme.applyStyle(R.style.RebaseTestThemeBase, false);

        assertConfigText("base");

        mTheme.applyStyle(R.style.RebaseTestThemeOverlay, true);

        assertConfigText("rebase_portrait");

        mTheme.rebase();

        assertConfigText("rebase_portrait");
    }

    private void assertConfigText(String expectedText) {
        mTheme.resolveAttribute(R.attr.rebase_configuration_dependent_text, mOutValue, true);
        assertEquals(expectedText, mOutValue.string);
    }
}
