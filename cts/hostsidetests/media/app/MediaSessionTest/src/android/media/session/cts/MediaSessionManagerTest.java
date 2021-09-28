/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.media.session.cts;

import static android.media.cts.MediaSessionTestHelperConstants.MEDIA_SESSION_TEST_HELPER_PKG;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ComponentName;
import android.content.Context;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.MediaSessionManager.RemoteUserInfo;
import android.os.Process;
import android.service.notification.NotificationListenerService;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;

import java.util.List;

/**
 * Tests {@link MediaSessionManager} with the multi-user environment.
 * <p>Don't run tests here directly. They aren't stand-alone tests and each test will be run
 * indirectly by the host-side test CtsMediaHostTestCases after the proper device setup.
 */
@SmallTest
public class MediaSessionManagerTest extends NotificationListenerService {
    private Context mContext;
    private MediaSessionManager mMediaSessionManager;
    private ComponentName mComponentName;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getTargetContext();
        mMediaSessionManager =
                mContext.getSystemService(MediaSessionManager.class);
        mComponentName = new ComponentName(mContext, MediaSessionManagerTest.class);
    }

    /**
     * Tests if the MediaSessionTestHelper doesn't have an active media session.
     */
    @Test
    public void testGetActiveSessions_noMediaSessionFromMediaSessionTestHelper() throws Exception {
        List<MediaController> controllers = mMediaSessionManager.getActiveSessions(mComponentName);
        for (MediaController controller : controllers) {
            if (controller.getPackageName().equals(MEDIA_SESSION_TEST_HELPER_PKG)) {
                fail("Media session for the media session app shouldn't be available");
                return;
            }
        }
    }

    /**
     * Tests if the MediaSessionTestHelper has an active media session.
     */
    @Test
    public void testGetActiveSessions_hasMediaSessionFromMediaSessionTestHelper() throws Exception {
        boolean found = false;
        List<MediaController> controllers = mMediaSessionManager.getActiveSessions(mComponentName);
        for (MediaController controller : controllers) {
            if (controller.getPackageName().equals(MEDIA_SESSION_TEST_HELPER_PKG)) {
                if (found) {
                    fail("Multiple media session for the media session app is unexpected");
                }
                found = true;
            }
        }
        if (!found) {
            fail("Media session for the media session app is expected");
        }
    }

    /**
     * Tests if there's no media session.
     */
    @Test
    public void testGetActiveSessions_noMediaSession() throws Exception {
        List<MediaController> controllers = mMediaSessionManager.getActiveSessions(mComponentName);
        assertTrue(controllers.isEmpty());
    }

    /**
     * Tests if this application is trusted.
     */
    @Test
    public void testIsTrusted_returnsTrue() throws Exception {
        RemoteUserInfo userInfo = new RemoteUserInfo(
                mContext.getPackageName(), Process.myPid(), Process.myUid());
        assertTrue(mMediaSessionManager.isTrustedForMediaControl(userInfo));
    }

    /**
     * Tests if this application isn't trusted.
     */
    @Test
    public void testIsTrusted_returnsFalse() throws Exception {
        RemoteUserInfo userInfo = new RemoteUserInfo(
                mContext.getPackageName(), Process.myPid(), Process.myUid());
        assertFalse(mMediaSessionManager.isTrustedForMediaControl(userInfo));
    }
}
