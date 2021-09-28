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

package android.accessibilityservice.cts.utils;

import static java.util.concurrent.TimeUnit.SECONDS;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.view.View;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

/** A click event listener that keeps an ordered record of events. */
public class EventCapturingClickListener implements View.OnClickListener {

    private final BlockingQueue<View> mViews = new LinkedBlockingQueue<>();

    @Override
    public void onClick(View view) {
        mViews.offer(view);
    }

    /** Insure that the specified views have received click events. */
    public void assertClicked(View... views) {
        View view;
        try {
            for (View v : views) {
                long waitTime = 5; // seconds
                view = mViews.poll(waitTime, SECONDS);
                assertNotNull(
                        "Expected click event for "
                                + v.toString()
                                + " but none present after "
                                + waitTime
                                + " seconds",
                        view);
                if (v != view) {
                    fail("Unexpected click event for view" + view.toString());
                }
            }
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    /** Insure that no click events have been received. */
    public void assertNoneClicked() {
        try {
            long waitTime = 1; // seconds
            View view = mViews.poll(waitTime, SECONDS);
            if (view != null) {
                fail("Unexpected click event for view" + view.toString());
            }
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }
}
