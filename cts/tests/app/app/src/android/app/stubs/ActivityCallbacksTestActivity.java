/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package android.app.stubs;

import android.annotation.Nullable;
import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.util.Pair;

import java.util.ArrayList;

public class ActivityCallbacksTestActivity extends Activity {

    public enum Event {
        ON_PRE_CREATE,
        ON_CREATE,
        ON_POST_CREATE,

        ON_PRE_START,
        ON_START,
        ON_POST_START,

        ON_PRE_RESUME,
        ON_RESUME,
        ON_POST_RESUME,

        ON_PRE_PAUSE,
        ON_PAUSE,
        ON_POST_PAUSE,

        ON_PRE_STOP,
        ON_STOP,
        ON_POST_STOP,

        ON_PRE_DESTROY,
        ON_DESTROY,
        ON_POST_DESTROY,
    }

    public enum Source {
        ACTIVITY,
        ACTIVITY_CALLBACK,
        ACTIVITY_CALLBACK_CREATE_ONLY,
        APPLICATION_ACTIVITY_CALLBACK
    }

    private final Application.ActivityLifecycleCallbacks mActivityCallbacks;
    private final Application.ActivityLifecycleCallbacks mActivityCallbacksCreateOnly;

    private ArrayList<Pair<Source, Event>> mCollectedEvents = new ArrayList<>();

    public ActivityCallbacksTestActivity() {
        mActivityCallbacks = new Application.ActivityLifecycleCallbacks() {

            @Override
            public void onActivityPreCreated(Activity activity, Bundle savedInstanceState) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_PRE_CREATE);
            }

            @Override
            public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_CREATE);
            }

            @Override
            public void onActivityPostCreated(Activity activity, Bundle savedInstanceState) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_POST_CREATE);
            }

            @Override
            public void onActivityPreStarted(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_PRE_START);
            }

            @Override
            public void onActivityStarted(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_START);
            }

            @Override
            public void onActivityPostStarted(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_POST_START);
            }

            @Override
            public void onActivityPreResumed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_PRE_RESUME);
            }

            @Override
            public void onActivityResumed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_RESUME);
            }

            @Override
            public void onActivityPostResumed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_POST_RESUME);
            }

            @Override
            public void onActivityPrePaused(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_PRE_PAUSE);
            }

            @Override
            public void onActivityPaused(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_PAUSE);
            }

            @Override
            public void onActivityPostPaused(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_POST_PAUSE);
            }

            @Override
            public void onActivityPreStopped(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_PRE_STOP);
            }

            @Override
            public void onActivityStopped(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_STOP);
            }

            @Override
            public void onActivityPostStopped(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_POST_STOP);
            }

            @Override
            public void onActivitySaveInstanceState(Activity activity, Bundle outState) {

            }

            @Override
            public void onActivityPreDestroyed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_PRE_DESTROY);
            }

            @Override
            public void onActivityDestroyed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_DESTROY);
            }

            @Override
            public void onActivityPostDestroyed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK, Event.ON_POST_DESTROY);
            }
        };
        registerActivityLifecycleCallbacks(mActivityCallbacks);
        mActivityCallbacksCreateOnly = new Application.ActivityLifecycleCallbacks() {

            @Override
            public void onActivityPreCreated(Activity activity, Bundle savedInstanceState) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_PRE_CREATE);
            }

            @Override
            public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_CREATE);
            }

            @Override
            public void onActivityPostCreated(Activity activity, Bundle savedInstanceState) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_POST_CREATE);
                // We shouldn't get any additional callbacks after this point
                unregisterActivityLifecycleCallbacks(this);
            }

            @Override
            public void onActivityStarted(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_START);
            }

            @Override
            public void onActivityPostStarted(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_POST_START);
            }

            @Override
            public void onActivityPreResumed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_PRE_RESUME);
            }

            @Override
            public void onActivityResumed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_RESUME);
            }

            @Override
            public void onActivityPostResumed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_POST_RESUME);
            }

            @Override
            public void onActivityPrePaused(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_PRE_PAUSE);
            }

            @Override
            public void onActivityPaused(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_PAUSE);
            }

            @Override
            public void onActivityPostPaused(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_POST_PAUSE);
            }

            @Override
            public void onActivityPreStopped(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_PRE_STOP);
            }

            @Override
            public void onActivityStopped(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_STOP);
            }

            @Override
            public void onActivityPostStopped(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_POST_STOP);
            }

            @Override
            public void onActivitySaveInstanceState(Activity activity, Bundle outState) {

            }

            @Override
            public void onActivityPreDestroyed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_PRE_DESTROY);
            }

            @Override
            public void onActivityDestroyed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_DESTROY);
            }

            @Override
            public void onActivityPostDestroyed(Activity activity) {
                collectEvent(Source.ACTIVITY_CALLBACK_CREATE_ONLY, Event.ON_POST_DESTROY);
            }
        };
        registerActivityLifecycleCallbacks(mActivityCallbacksCreateOnly);
    }

    public void collectEvent(Source source, Event event) {
        mCollectedEvents.add(new Pair<>(source, event));
    }

    public ArrayList<Pair<Source, Event>> getCollectedEvents() {
        return mCollectedEvents;
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        collectEvent(Source.ACTIVITY, Event.ON_PRE_CREATE);
        super.onCreate(savedInstanceState);
        collectEvent(Source.ACTIVITY, Event.ON_POST_CREATE);
    }

    @Override
    protected void onStart() {
        collectEvent(Source.ACTIVITY, Event.ON_PRE_START);
        super.onStart();
        collectEvent(Source.ACTIVITY, Event.ON_POST_START);
    }

    @Override
    protected void onResume() {
        collectEvent(Source.ACTIVITY, Event.ON_PRE_RESUME);
        super.onResume();
        collectEvent(Source.ACTIVITY, Event.ON_POST_RESUME);
    }

    @Override
    protected void onPause() {
        collectEvent(Source.ACTIVITY, Event.ON_PRE_PAUSE);
        super.onPause();
        collectEvent(Source.ACTIVITY, Event.ON_POST_PAUSE);
    }

    @Override
    protected void onStop() {
        collectEvent(Source.ACTIVITY, Event.ON_PRE_STOP);
        super.onStop();
        collectEvent(Source.ACTIVITY, Event.ON_POST_STOP);
    }

    @Override
    protected void onDestroy() {
        collectEvent(Source.ACTIVITY, Event.ON_PRE_DESTROY);
        super.onDestroy();
        collectEvent(Source.ACTIVITY, Event.ON_POST_DESTROY);
    }
}
