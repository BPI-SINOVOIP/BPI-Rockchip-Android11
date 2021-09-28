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
package com.android.tradefed.cluster;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.cluster.ClusterEventUploaderTest.Event;

import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/** Function tests for {@link ClusterEventUploader}. */
@RunWith(JUnit4.class)
public class ClusterEventUploaderFuncTest {

    private ClusterEventUploader<Event> mUploader;

    @Ignore
    @Test
    public void testPostCommandEvent_multipleThread() throws Exception {
        final Event event1 = new Event("event1");
        final Event event2 = new Event("event2");

        final Lock lock = new ReentrantLock();

        mUploader =
                new ClusterEventUploader<Event>() {
                    @Override
                    protected void doUploadEvents(List<Event> events) throws IOException {
                        try {
                            lock.lock();
                        } finally {
                            lock.unlock();
                        }
                    }
                };

        Thread thread1 =
                new Thread(
                        new Runnable() {
                            @Override
                            public void run() {
                                mUploader.postEvent(event1);
                                // Thread1 uses flush will be blocked.
                                mUploader.flush();
                            }
                        });
        Thread thread2 =
                new Thread(
                        new Runnable() {
                            @Override
                            public void run() {
                                mUploader.postEvent(event2);
                                // Thread2 doesn't use flush will not be blocked.
                            }
                        });
        // Thread1 will be blocked. Thread2 will not be blocked.
        lock.lock();
        thread1.start();
        thread2.start();
        thread2.join(60 * 1000); // timeout in milliseconds
        assertTrue(thread1.isAlive());
        assertFalse(thread2.isAlive());
        // Unblock thread 1.
        lock.unlock();
        thread1.join(60 * 1000); // timeout in milliseconds
        assertFalse(thread1.isAlive());
    }
}
