/*
 * Copyright (C) 2009 The Android Open Source Project
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

import android.content.ComponentName;
import android.content.Context;
import android.media.MediaScannerConnection;
import android.media.MediaScannerConnection.MediaScannerConnectionClient;
import android.net.Uri;
import android.os.IBinder;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;

import com.android.compatibility.common.util.FileCopyHelper;
import com.android.compatibility.common.util.PollingCheck;

import java.io.File;

@NonMediaMainlineTest
@AppModeFull(reason = "TODO: evaluate and port to instant")
public class MediaScannerConnectionTest extends AndroidTestCase {

    private static final String MEDIA_TYPE = "audio/mpeg";
    private File mMediaFile;
    private static final int TIME_OUT = 10000;
    private MockMediaScannerConnection mMediaScannerConnection;
    private MockMediaScannerConnectionClient mMediaScannerConnectionClient;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // prepare the media file.

        FileCopyHelper copier = new FileCopyHelper(mContext);
        String fileName = "test" + System.currentTimeMillis() + ".mp3";
        File dir = getContext().getExternalMediaDirs()[0];
        mMediaFile = new File(dir, fileName);
        copier.copyToExternalStorage(R.raw.testmp3, mMediaFile);

        assertTrue(mMediaFile.exists());
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        if (mMediaFile != null) {
            mMediaFile.delete();
        }
        if (mMediaScannerConnection != null) {
            mMediaScannerConnection.disconnect();
            mMediaScannerConnection = null;
        }
    }

    public void testMediaScannerConnection() throws InterruptedException {
        mMediaScannerConnectionClient = new MockMediaScannerConnectionClient();
        mMediaScannerConnection = new MockMediaScannerConnection(getContext(),
                                    mMediaScannerConnectionClient);

        assertFalse(mMediaScannerConnection.isConnected());

        // test connect and disconnect.
        mMediaScannerConnection.connect();
        checkConnectionState(true);

        mMediaScannerConnection.disconnect();

        checkConnectionState(false);

        mMediaScannerConnection.connect();

        checkConnectionState(true);

        mMediaScannerConnection.scanFile(mMediaFile.getAbsolutePath(), MEDIA_TYPE);

        checkMediaScannerConnection();

        assertEquals(mMediaFile.getAbsolutePath(), mMediaScannerConnectionClient.mediaPath);
        assertNotNull(mMediaScannerConnectionClient.mediaUri);
    }

    private void checkMediaScannerConnection() {
        new PollingCheck(TIME_OUT) {
            protected boolean check() {
                return mMediaScannerConnectionClient.isOnMediaScannerConnectedCalled;
            }
        }.run();
        new PollingCheck(TIME_OUT) {
            protected boolean check() {
                return mMediaScannerConnectionClient.mediaPath != null;
            }
        }.run();
    }

    private void checkConnectionState(final boolean expected) {
        new PollingCheck(TIME_OUT) {
            protected boolean check() {
                return mMediaScannerConnection.isConnected() == expected;
            }
        }.run();
    }

    class MockMediaScannerConnection extends MediaScannerConnection {
        public MockMediaScannerConnection(Context context, MediaScannerConnectionClient client) {
            super(context, client);
        }
    }

    class MockMediaScannerConnectionClient implements MediaScannerConnectionClient {

        public boolean isOnMediaScannerConnectedCalled;
        public String mediaPath;
        public Uri mediaUri;
        public void onMediaScannerConnected() {
            isOnMediaScannerConnectedCalled = true;
        }

        public void onScanCompleted(String path, Uri uri) {
            mediaPath = path;
            if (uri != null) {
                mediaUri = uri;
            }
        }

    }

}
