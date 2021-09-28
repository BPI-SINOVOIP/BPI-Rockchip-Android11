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

package android.matchflags.cts;

import static org.junit.Assert.fail;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class MatchFlagTests {

    private static final String ONLY_BROWSER_URI =
            "https://nohandler-02xgpcssu1v7xvpek0skc905glnyu7ihjtza3eufox0mauqyri.com";
    private static final String UNIQUE_URI =
            "https://unique-5gle2bs6woovjn8xabwyb3js01xl0ducci3gd3fpe622h48lyg.com";
    @Rule
    public TestName name = new TestName();

    @BeforeClass
    public static void setup() {

    }

    @Test
    public void startNoBrowserIntentWithNoMatchingApps() throws Exception {
        Intent onlyBrowserIntent = new Intent(Intent.ACTION_VIEW)
                .addCategory(Intent.CATEGORY_BROWSABLE)
                .setData(Uri.parse(ONLY_BROWSER_URI));

        if (isBrowserPresent(true)) {
            startActivity(onlyBrowserIntent);
        } else {
            try {
                startActivity(onlyBrowserIntent);
                fail("Device without browser should not launch browser only intent");
            } catch (ActivityNotFoundException e) {
                // hooray
            }
        }

        Intent noBrowserWithBrowserOnlyIntent = new Intent(onlyBrowserIntent)
                .addFlags(Intent.FLAG_ACTIVITY_REQUIRE_NON_BROWSER);

        try {
            startActivity(noBrowserWithBrowserOnlyIntent);
            fail("Should not have started a browser-only intent with NON_BROWSER flag set");
        } catch (ActivityNotFoundException e) {
            // hooray!
        }
    }

    @Test
    public void startRequireDefaultWithNoDefault() throws Exception {
        Intent sharedIntent = new Intent("android.matchflags.app.SHARED_ACTION");

        startActivity(sharedIntent);

        Intent sharedIntentRequireDefault = new Intent(sharedIntent)
                .addFlags(Intent.FLAG_ACTIVITY_REQUIRE_DEFAULT);

        try {
            startActivity(sharedIntentRequireDefault);
            fail("Should have started intent with no default when default required");
        } catch (ActivityNotFoundException e) {
            // hooray!
        }
    }

    @Test
    public void startRequireDefaultWithSingleMatch() throws Exception {
        Intent uniqueIntent = new Intent("android.matchflags.app.UNIQUE_ACTION")
                .addFlags(Intent.FLAG_ACTIVITY_REQUIRE_DEFAULT);
        startActivity(uniqueIntent);
    }

    @Test
    public void startNoBrowserRequireDefault() throws Exception {
        Intent uniqueUriIntent = new Intent(Intent.ACTION_VIEW)
                .addCategory(Intent.CATEGORY_BROWSABLE)
                .setData(Uri.parse(UNIQUE_URI));

        startActivity(uniqueUriIntent);

        Intent uniqueUriIntentNoBrowserRequireDefault = new Intent(uniqueUriIntent)
                .addFlags(Intent.FLAG_ACTIVITY_REQUIRE_NON_BROWSER)
                .addFlags(Intent.FLAG_ACTIVITY_REQUIRE_DEFAULT);

        if (isBrowserPresent(false)) {
            // with non-browser, we'd expect the resolver
            // with require default, we'll get activity not found
            try {
                startActivity(uniqueUriIntentNoBrowserRequireDefault);
                fail("Should fail to launch when started with non-browser and require default");
            } catch (ActivityNotFoundException e) {
                // hooray!
            }
        } else {
            // with non-browser, but no browser present, we'd get a single result
            // with require default, we'll resolve to that single result
            startActivity(uniqueUriIntentNoBrowserRequireDefault);
        }
    }

    private static boolean isBrowserPresent(boolean includeFallback) {
        return InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageManager()
                .queryIntentActivities(new Intent(Intent.ACTION_VIEW).addCategory(
                        Intent.CATEGORY_BROWSABLE).setData(Uri.parse(ONLY_BROWSER_URI)),
                        0 /* flags */)
                .stream().anyMatch(resolveInfo ->
                        resolveInfo.handleAllWebDataURI
                        && (includeFallback || resolveInfo.priority >= 0));
    }

    private static void startActivity(Intent onlyBrowserIntent) {
        InstrumentationRegistry.getInstrumentation().getContext().startActivity(
                onlyBrowserIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
    }

}
