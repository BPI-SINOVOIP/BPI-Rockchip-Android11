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
package android.contentcaptureservice.cts;

import static android.contentcaptureservice.cts.Helper.GENERIC_TIMEOUT_MS;
import static android.contentcaptureservice.cts.Helper.sContext;

import android.app.Application;
import android.content.Intent;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.ActivitiesWatcher;
import com.android.compatibility.common.util.ActivitiesWatcher.ActivityWatcher;
import com.android.compatibility.common.util.Visitor;

import org.junit.After;
import org.junit.Before;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;

public abstract class AbstractContentCaptureIntegrationAutoActivityLaunchTest
        <A extends AbstractContentCaptureActivity> extends AbstractContentCaptureIntegrationTest {

    protected ActivitiesWatcher mActivitiesWatcher;

    private final Class<A> mActivityClass;

    protected AbstractContentCaptureIntegrationAutoActivityLaunchTest(
            @NonNull Class<A> activityClass) {
        mActivityClass = activityClass;
    }

    @Before
    public void registerLifecycleCallback() {
        Log.v(mTag, "@Before: Registering lifecycle callback");
        final Application app = (Application) sContext.getApplicationContext();
        mActivitiesWatcher = new ActivitiesWatcher(GENERIC_TIMEOUT_MS);
        app.registerActivityLifecycleCallbacks(mActivitiesWatcher);
    }

    @After
    public void unregisterLifecycleCallback() {
        Log.d(mTag, "@After: Unregistering lifecycle callback: " + mActivitiesWatcher);
        if (mActivitiesWatcher != null) {
            final Application app = (Application) sContext.getApplicationContext();
            app.unregisterActivityLifecycleCallbacks(mActivitiesWatcher);
        }
    }

    /**
     * Gets the {@link ActivityTestRule} use to launch this activity.
     *
     * <p><b>NOTE: </b>implementation must return a static singleton, otherwise it might be
     * {@code null} when used it in this class' {@code @Rule}
     */
    protected abstract ActivityTestRule<A> getActivityTestRule();

    /**
     * {@inheritDoc}
     *
     * <p>By default it returns {@link #getActivityTestRule()}, but subclasses with more than one
     * rule can override it to return a {@link RuleChain}.
     */
    @NonNull
    @Override
    protected TestRule getMainTestRule() {
        return getActivityTestRule();
    }

    protected A launchActivity() {
        Log.d(mTag, "Launching " + mActivityClass.getSimpleName());

        return getActivityTestRule().launchActivity(getLaunchIntent());
    }

    protected A launchActivity(@Nullable Visitor<Intent> visitor) {
        Log.d(mTag, "Launching " + mActivityClass.getSimpleName());

        final Intent intent = getLaunchIntent();
        if (visitor != null) {
            visitor.visit(intent);
        }
        return getActivityTestRule().launchActivity(intent);
    }

    private Intent getLaunchIntent() {
        return new Intent(sContext, mActivityClass);
    }

    @NonNull
    protected ActivityWatcher startWatcher() {
        return mActivitiesWatcher.watch(mActivityClass);
    }
}
