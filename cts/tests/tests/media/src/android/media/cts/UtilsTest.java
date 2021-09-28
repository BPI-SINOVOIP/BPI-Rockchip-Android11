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

package android.media.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.media.Utils;
import android.os.Handler;
import android.os.HandlerThread;
import androidx.test.runner.AndroidJUnit4;
import java.util.concurrent.Executor;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import org.junit.Test;
import org.junit.runner.RunWith;

@NonMediaMainlineTest
@RunWith(AndroidJUnit4.class)
public class UtilsTest {
    @Test
    public void testListenerList() throws Exception {
        // The ListenerList is a Test API only to enable a dedicated unit test
        // to ensure it works as expected.
        Utils.ListenerList<Long> listeners = new Utils.ListenerList();

        final int[] resultEventCode = { 0 };
        final long[] resultLong = { 0L };

        final Object key = new Object();
        final Semaphore semaphore = new Semaphore(0);

        // Use a Handler for an executor (version 1).
        final HandlerThread thread = new HandlerThread("Tester");
        thread.start();
        final Executor executor = new Executor() {
            private final Handler mHandler = Handler.createAsync(thread.getLooper());
            @Override
            public void execute(Runnable runnable) {
                mHandler.post(runnable);
            }
        };

        listeners.add(key, executor, (int eventCode, Long l) -> {
            resultEventCode[0] = eventCode;
            resultLong[0] = l;
            semaphore.release();
        });

        listeners.notify(1, 2L);
        assertTrue("semaphore should have been triggered",
            semaphore.tryAcquire(1 /* permits */, 1, TimeUnit.SECONDS));

        assertEquals("event code must match", 1, resultEventCode[0]);
        assertEquals("value must match", 2L, resultLong[0]);
        listeners.remove(key);

        assertFalse("semaphore should not have been triggered",
            semaphore.tryAcquire(1 /* permits */, 250, TimeUnit.MILLISECONDS));

        // should do nothing as we aren't listening.
        listeners.notify(3, 4L);
        assertEquals("event code must match", 1, resultEventCode[0]);
        assertEquals("value must match", 2L, resultLong[0]);
    }
}
