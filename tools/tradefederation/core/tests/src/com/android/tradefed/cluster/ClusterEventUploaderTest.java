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

import static org.junit.Assert.assertEquals;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.IOException;
import java.util.LinkedList;
import java.util.List;

/** Unit tests for {@link ClusterEventUploader}. */
@RunWith(JUnit4.class)
public class ClusterEventUploaderTest {

    private IClusterEventUploader<Event> mUploader;
    private List<Event> mUploadedEvents = null;

    /** Event class used in tests. */
    static class Event implements IClusterEvent {

        String mName = null;
        boolean mUploadSuccess = true;

        public Event(String name) {
            this(name, true);
        }

        public Event(String name, boolean uploadSuccess) {
            mName = name;
            mUploadSuccess = uploadSuccess;
        }

        @Override
        public JSONObject toJSON() throws JSONException {
            return null;
        }
    }

    @Before
    public void setUp() throws Exception {
        mUploadedEvents = new LinkedList<>();
        mUploader =
                new ClusterEventUploader<Event>() {
                    @Override
                    protected void doUploadEvents(List<Event> events) throws IOException {
                        for (Event e : events) {
                            if (e.mUploadSuccess) {
                                mUploadedEvents.add(e);
                            } else {
                                throw new IOException();
                            }
                        }
                    }
                };
    }

    @Test
    public void testPostCommandEvent_simpleEvent() {
        // Create an event to post
        final Event event = new Event("event");
        mUploader.postEvent(event);
        mUploader.flush();
        assertEquals(1, mUploadedEvents.size());
        assertEquals("event", mUploadedEvents.get(0).mName);
    }

    /*
     * If there are no events in the queue, do not post.
     */
    @Test
    public void testPostCommandEvent_NoEvent() {
        mUploader.flush();
        assertEquals(0, mUploadedEvents.size());
    }

    @Test
    public void testPostCommandEvent_multipleEvents() {
        final Event event = new Event("event");
        mUploader.postEvent(event);
        mUploader.postEvent(event);
        mUploader.postEvent(event);
        mUploader.postEvent(event);
        mUploader.flush();
        assertEquals(4, mUploadedEvents.size());
    }

    @Test
    public void testPostCommandEvent_multipleBatches() {
        final Event event = new Event("event");
        mUploader.setMaxBatchSize(2);
        mUploader.postEvent(event);
        mUploader.postEvent(event);
        mUploader.postEvent(event);
        mUploader.postEvent(event);
        mUploader.flush();
        assertEquals(4, mUploadedEvents.size());
    }

    @Test
    public void testPostCommandEvent_failed() {
        final Event event = new Event("event");
        final Event failedEvent = new Event("failedEvent", false);
        mUploader.postEvent(event);
        mUploader.postEvent(failedEvent);
        mUploader.flush();
        assertEquals(1, mUploadedEvents.size());
        assertEquals("event", mUploadedEvents.get(0).mName);
        failedEvent.mUploadSuccess = true;
        mUploader.flush();
        assertEquals(2, mUploadedEvents.size());
        assertEquals("failedEvent", mUploadedEvents.get(1).mName);
    }
}
