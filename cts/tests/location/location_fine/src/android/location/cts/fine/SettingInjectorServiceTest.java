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

package android.location.cts.fine;

import static android.location.SettingInjectorService.ENABLED_KEY;
import static android.location.SettingInjectorService.MESSENGER_KEY;
import static android.location.SettingInjectorService.SUMMARY_KEY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.content.Intent;
import android.location.SettingInjectorService;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public class SettingInjectorServiceTest {

    private static final long TIMEOUT_MS = 5000;
    private static final long FAILURE_TIMEOUT_MS = 200;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testRefreshSettings() {
        // Simply calls the method to make sure it exists.
        SettingInjectorService.refreshSettings(mContext);
    }

    @Test
    public void testSettingInjectorService() throws Exception {
        TestSettingInjectorService service = new TestSettingInjectorService();
        MessageCapture messageCapture = new MessageCapture();
        Intent intent = new Intent().putExtra(MESSENGER_KEY, messageCapture.getMessenger());

        service.setEnabledAndSummary(false, null);
        service.onStartCommand(intent, 0, 0);
        Message message = messageCapture.getNextMessage(TIMEOUT_MS);
        assertFalse(message.getData().getBoolean(ENABLED_KEY));
        assertNull(message.getData().getString(SUMMARY_KEY));

        service.setEnabledAndSummary(true, "summary");
        service.onStartCommand(intent, 0, 0);
        message = messageCapture.getNextMessage(TIMEOUT_MS);
        assertTrue(message.getData().getBoolean(ENABLED_KEY));
        assertEquals("summary", message.getData().getString(SUMMARY_KEY));

        service.setEnabledAndSummary(false, "another_summary");
        service.onStartCommand(intent, 0, 0);
        message = messageCapture.getNextMessage(TIMEOUT_MS);
        assertFalse(message.getData().getBoolean(ENABLED_KEY));
        assertEquals("another_summary", message.getData().getString(SUMMARY_KEY));

        assertNull(messageCapture.getNextMessage(FAILURE_TIMEOUT_MS));
    }

    @Test
    public void testSettingInjectorService_Exception() throws Exception {
        BadSettingInjectorService service = new BadSettingInjectorService();
        MessageCapture messageCapture = new MessageCapture();
        Intent intent = new Intent().putExtra(MESSENGER_KEY, messageCapture.getMessenger());

        try {
            service.onStartCommand(intent, 0, 0);
            fail("Should throw RuntimeException");
        } catch (RuntimeException e) {
            // pass
        }

        Message message = messageCapture.getNextMessage(TIMEOUT_MS);
        assertFalse(message.getData().getBoolean(ENABLED_KEY));
        assertNull(message.getData().getString(SUMMARY_KEY));

        assertNull(messageCapture.getNextMessage(FAILURE_TIMEOUT_MS));
    }

    @Test
    public void testSettingInjectorService_Bind() {
        TestSettingInjectorService service = new TestSettingInjectorService();
        assertNull(service.onBind(new Intent()));
    }

    @Test
    public void testSettingInjectorService_EmptyIntent() {
        TestSettingInjectorService service = new TestSettingInjectorService();
        service.onStartCommand(new Intent(), 0, 0);
    }

    private static class TestSettingInjectorService extends SettingInjectorService {

        private boolean mEnabled;
        private String mSummary;

        TestSettingInjectorService() {
            super("TestSettingInjectorService");
        }

        @Override
        protected String onGetSummary() {
            return mSummary;
        }

        @Override
        protected boolean onGetEnabled() {
            return mEnabled;
        }

        public void setEnabledAndSummary(boolean enabled, String summary) {
            mEnabled = enabled;
            mSummary = summary;
        }
    }

    private static class BadSettingInjectorService extends SettingInjectorService {

        BadSettingInjectorService() {
            super("BadSettingInjectorService");
        }

        @Override
        protected String onGetSummary() {
            throw new RuntimeException();
        }

        @Override
        protected boolean onGetEnabled() {
            throw new RuntimeException();
        }
    }

    private static class MessageCapture extends Handler {

        private final Messenger mMessenger = new Messenger(this);

        private final LinkedBlockingQueue<Message> mMessages = new LinkedBlockingQueue<>();

        MessageCapture() {
            super(Looper.getMainLooper());
        }

        public Messenger getMessenger() {
            return mMessenger;
        }

        public Message getNextMessage(long timeoutMs) throws InterruptedException {
            return mMessages.poll(timeoutMs, TimeUnit.MILLISECONDS);
        }

        @Override
        public void handleMessage(Message m) {
            mMessages.add(Message.obtain(m));
        }
    }
}
