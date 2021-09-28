/*
 * Copyright (C) 2014 The Android Open Source Project
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

import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaDrm;
import android.media.MediaDrm.KeyStatus;
import android.media.MediaDrm.MediaDrmStateException;
import android.media.MediaDrmException;
import android.media.MediaFormat;
import android.media.cts.TestUtils.Monitor;
import android.net.Uri;
import android.os.Looper;
import android.platform.test.annotations.Presubmit;
import android.util.Base64;
import android.util.Log;

import android.view.Surface;

import com.android.compatibility.common.util.ApiLevelUtil;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import java.util.Vector;

import androidx.annotation.NonNull;

/**
 * Tests of MediaPlayer streaming capabilities.
 */
public class MediaDrmClearkeyTest extends MediaCodecPlayerTestBase<MediaStubActivity> {

    private static final String TAG = MediaDrmClearkeyTest.class.getSimpleName();

    // Add additional keys here if the content has more keys.
    private static final byte[] CLEAR_KEY_CENC = {
            (byte)0x3f, (byte)0x0a, (byte)0x33, (byte)0xf3, (byte)0x40, (byte)0x98, (byte)0xb9, (byte)0xe2,
            (byte)0x2b, (byte)0xc0, (byte)0x78, (byte)0xe0, (byte)0xa1, (byte)0xb5, (byte)0xe8, (byte)0x54 };

    private static final byte[] CLEAR_KEY_WEBM = "_CLEAR_KEY_WEBM_".getBytes();

    private static final int NUMBER_OF_SECURE_STOPS = 10;
    private static final int VIDEO_WIDTH_CENC = 1280;
    private static final int VIDEO_HEIGHT_CENC = 720;
    private static final int VIDEO_WIDTH_WEBM = 352;
    private static final int VIDEO_HEIGHT_WEBM = 288;
    private static final int VIDEO_WIDTH_MPEG2TS = 320;
    private static final int VIDEO_HEIGHT_MPEG2TS = 240;
    private static final String MIME_VIDEO_AVC = MediaFormat.MIMETYPE_VIDEO_AVC;
    private static final String MIME_VIDEO_VP8 = MediaFormat.MIMETYPE_VIDEO_VP8;

    // Property Keys
    private static final String ALGORITHMS_PROPERTY_KEY = MediaDrm.PROPERTY_ALGORITHMS;
    private static final String DESCRIPTION_PROPERTY_KEY = MediaDrm.PROPERTY_DESCRIPTION;
    private static final String DEVICEID_PROPERTY_KEY = "deviceId";
    private static final String INVALID_PROPERTY_KEY = "invalid property key";
    private static final String LISTENER_TEST_SUPPORT_PROPERTY_KEY = "listenerTestSupport";
    private static final String VENDOR_PROPERTY_KEY = MediaDrm.PROPERTY_VENDOR;
    private static final String VERSION_PROPERTY_KEY = MediaDrm.PROPERTY_VERSION;

    // Error message
    private static final String ERR_MSG_CRYPTO_SCHEME_NOT_SUPPORTED = "Crypto scheme is not supported";

    private static final String CENC_AUDIO_PATH = "/clear/h264/llama/llama_aac_audio.mp4";
    private static final String CENC_VIDEO_PATH = "/clearkey/llama_h264_main_720p_8000.mp4";
    private static final Uri WEBM_URL = Uri.parse(
            "android.resource://android.media.cts/" + R.raw.video_320x240_webm_vp8_800kbps_30fps_vorbis_stereo_128kbps_44100hz_crypt);
    private static final Uri MPEG2TS_SCRAMBLED_URL = Uri.parse(
            "android.resource://android.media.cts/" + R.raw.segment000001_scrambled);
    private static final Uri MPEG2TS_CLEAR_URL = Uri.parse(
            "android.resource://android.media.cts/" + R.raw.segment000001);

    private static final UUID COMMON_PSSH_SCHEME_UUID =
            new UUID(0x1077efecc0b24d02L, 0xace33c1e52e2fb4bL);
    private static final UUID CLEARKEY_SCHEME_UUID =
            new UUID(0xe2719d58a985b3c9L, 0x781ab030af78d30eL);

    private byte[] mDrmInitData;
    private byte[] mKeySetId;
    private byte[] mSessionId;
    private Monitor mSessionMonitor = new Monitor();
    private Looper mLooper;
    private MediaDrm mDrm = null;
    private final Object mLock = new Object();
    private boolean mLostStateReceived;

    public MediaDrmClearkeyTest() {
        super(MediaStubActivity.class);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (false == deviceHasMediaDrm()) {
            tearDown();
        }
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
    }

    private boolean deviceHasMediaDrm() {
        // ClearKey is introduced after KitKat.
        if (ApiLevelUtil.isAtMost(android.os.Build.VERSION_CODES.KITKAT)) {
            return false;
        }
        return true;
    }

    /**
     * Extracts key ids from the pssh blob returned by getKeyRequest() and
     * places it in keyIds.
     * keyRequestBlob format (section 5.1.3.1):
     * https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#clear-key
     *
     * @return size of keyIds vector that contains the key ids, 0 for error
     */
    private static int getKeyIds(byte[] keyRequestBlob, Vector<String> keyIds) {
        if (0 == keyRequestBlob.length || keyIds == null)
            return 0;

        String jsonLicenseRequest = new String(keyRequestBlob);
        keyIds.clear();

        try {
            JSONObject license = new JSONObject(jsonLicenseRequest);
            final JSONArray ids = license.getJSONArray("kids");
            for (int i = 0; i < ids.length(); ++i) {
                keyIds.add(ids.getString(i));
            }
        } catch (JSONException e) {
            Log.e(TAG, "Invalid JSON license = " + jsonLicenseRequest);
            return 0;
        }
        return keyIds.size();
    }

    /**
     * Creates the JSON Web Key string.
     *
     * @return JSON Web Key string.
     */
    private static String createJsonWebKeySet(
            Vector<String> keyIds, Vector<String> keys, int keyType) {
        String jwkSet = "{\"keys\":[";
        for (int i = 0; i < keyIds.size(); ++i) {
            String id = new String(keyIds.get(i).getBytes(Charset.forName("UTF-8")));
            String key = new String(keys.get(i).getBytes(Charset.forName("UTF-8")));

            jwkSet += "{\"kty\":\"oct\",\"kid\":\"" + id +
                    "\",\"k\":\"" + key + "\"}";
        }
        jwkSet += "], \"type\":";
        if (keyType == MediaDrm.KEY_TYPE_OFFLINE || keyType == MediaDrm.KEY_TYPE_RELEASE) {
            jwkSet += "\"persistent-license\" }";
        } else {
            jwkSet += "\"temporary\" }";
        }
        return jwkSet;
    }

    /**
     * Retrieves clear key ids from getKeyRequest(), create JSON Web Key
     * set and send it to the CDM via provideKeyResponse().
     *
     * @return key set ID
     */
    static byte[] retrieveKeys(MediaDrm drm, String initDataType,
            byte[] sessionId, byte[] drmInitData, int keyType, byte[][] clearKeyIds) {
        MediaDrm.KeyRequest drmRequest = null;
        try {
            drmRequest = drm.getKeyRequest(sessionId, drmInitData, initDataType,
                    keyType, null);
        } catch (Exception e) {
            e.printStackTrace();
            Log.i(TAG, "Failed to get key request: " + e.toString());
        }
        if (drmRequest == null) {
            Log.e(TAG, "Failed getKeyRequest");
            return null;
        }

        Vector<String> keyIds = new Vector<String>();
        if (0 == getKeyIds(drmRequest.getData(), keyIds)) {
            Log.e(TAG, "No key ids found in initData");
            return null;
        }

        if (clearKeyIds.length != keyIds.size()) {
            Log.e(TAG, "Mismatch number of key ids and keys: ids=" +
                    keyIds.size() + ", keys=" + clearKeyIds.length);
            return null;
        }

        // Base64 encodes clearkeys. Keys are known to the application.
        Vector<String> keys = new Vector<String>();
        for (int i = 0; i < clearKeyIds.length; ++i) {
            String clearKey = Base64.encodeToString(clearKeyIds[i],
                    Base64.NO_PADDING | Base64.NO_WRAP);
            keys.add(clearKey);
        }

        String jwkSet = createJsonWebKeySet(keyIds, keys, keyType);
        byte[] jsonResponse = jwkSet.getBytes(Charset.forName("UTF-8"));

        try {
            try {
                return drm.provideKeyResponse(sessionId, jsonResponse);
            } catch (IllegalStateException e) {
                Log.e(TAG, "Failed to provide key response: " + e.toString());
            }
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, "Failed to provide key response: " + e.toString());
        }
        return null;
    }

    /**
     * Retrieves clear key ids from getKeyRequest(), create JSON Web Key
     * set and send it to the CDM via provideKeyResponse().
     */
    private void getKeys(MediaDrm drm, String initDataType,
            byte[] sessionId, byte[] drmInitData, int keyType, byte[][] clearKeyIds) {
        mKeySetId = retrieveKeys(drm, initDataType, sessionId, drmInitData, keyType, clearKeyIds);
    }

    private @NonNull MediaDrm startDrm(final byte[][] clearKeyIds, final String initDataType,
                                       final UUID drmSchemeUuid, int keyType) {
        if (!MediaDrm.isCryptoSchemeSupported(drmSchemeUuid)) {
            throw new Error(ERR_MSG_CRYPTO_SCHEME_NOT_SUPPORTED);
        }

        new Thread() {
            @Override
            public void run() {
                if (mDrm != null) {
                    Log.e(TAG, "Failed to startDrm: already started");
                    return;
                }
                // Set up a looper to handle events
                Looper.prepare();

                // Save the looper so that we can terminate this thread
                // after we are done with it.
                mLooper = Looper.myLooper();

                try {
                    mDrm = new MediaDrm(drmSchemeUuid);
                } catch (MediaDrmException e) {
                    Log.e(TAG, "Failed to create MediaDrm: " + e.getMessage());
                    return;
                }

                synchronized(mLock) {
                    mDrm.setOnEventListener(new MediaDrm.OnEventListener() {
                            @Override
                            public void onEvent(MediaDrm md, byte[] sessionId, int event,
                                    int extra, byte[] data) {
                                if (event == MediaDrm.EVENT_KEY_REQUIRED) {
                                    Log.i(TAG, "MediaDrm event: Key required");
                                    getKeys(mDrm, initDataType, mSessionId, mDrmInitData,
                                            keyType, clearKeyIds);
                                } else if (event == MediaDrm.EVENT_KEY_EXPIRED) {
                                    Log.i(TAG, "MediaDrm event: Key expired");
                                    getKeys(mDrm, initDataType, mSessionId, mDrmInitData,
                                            keyType, clearKeyIds);
                                } else {
                                    Log.e(TAG, "Events not supported" + event);
                                }
                            }
                        });
                    mDrm.setOnSessionLostStateListener(new MediaDrm.OnSessionLostStateListener() {
                            @Override
                            public void onSessionLostState(MediaDrm md, byte[] sid) {
                                if (md != mDrm) {
                                    Log.e(TAG, "onSessionLostState callback: drm object mismatch");
                                } else if (!Arrays.equals(mSessionId, sid)) {
                                    Log.e(TAG, "onSessionLostState callback: sessionId mismatch: |" +
                                            Arrays.toString(mSessionId) + "| vs |" + Arrays.toString(sid) + "|");
                                } else {
                                    mLostStateReceived = true;
                                }
                            }
                        }, null);
                    mDrm.setOnKeyStatusChangeListener(new MediaDrm.OnKeyStatusChangeListener() {
                            @Override
                            public void onKeyStatusChange(MediaDrm md, byte[] sessionId,
                                    List<KeyStatus> keyInformation, boolean hasNewUsableKey) {
                                Log.d(TAG, "onKeyStatusChange");
                                assertTrue(md == mDrm);
                                assertTrue(Arrays.equals(sessionId, mSessionId));
                                mSessionMonitor.signal();
                                assertTrue(hasNewUsableKey);

                                assertEquals(3, keyInformation.size());
                                KeyStatus keyStatus = keyInformation.get(0);
                                assertTrue(Arrays.equals(keyStatus.getKeyId(), new byte[] {0xa, 0xb, 0xc}));
                                assertTrue(keyStatus.getStatusCode() == MediaDrm.KeyStatus.STATUS_USABLE);
                                keyStatus = keyInformation.get(1);
                                assertTrue(Arrays.equals(keyStatus.getKeyId(), new byte[] {0xd, 0xe, 0xf}));
                                assertTrue(keyStatus.getStatusCode() == MediaDrm.KeyStatus.STATUS_EXPIRED);
                                keyStatus = keyInformation.get(2);
                                assertTrue(Arrays.equals(keyStatus.getKeyId(), new byte[] {0x0, 0x1, 0x2}));
                                assertTrue(keyStatus.getStatusCode() == MediaDrm.KeyStatus.STATUS_USABLE_IN_FUTURE);

                            }
                        }, null);

                    mLock.notify();
                }
                Looper.loop();  // Blocks forever until Looper.quit() is called.
            }
        }.start();

        // wait for mDrm to be created
        synchronized(mLock) {
            try {
                mLock.wait(1000);
            } catch (Exception e) {
            }
        }
        return mDrm;
    }

    private void stopDrm(MediaDrm drm) {
        if (drm != mDrm) {
            Log.e(TAG, "invalid drm specified in stopDrm");
        }
        mLooper.quit();
        mDrm.close();
        mDrm = null;
    }

    private @NonNull byte[] openSession(MediaDrm drm) {
        byte[] mSessionId = null;
        boolean mRetryOpen;
        do {
            try {
                mRetryOpen = false;
                mSessionId = drm.openSession();
            } catch (Exception e) {
                mRetryOpen = true;
            }
        } while (mRetryOpen);
        return mSessionId;
    }

    private void closeSession(MediaDrm drm, byte[] sessionId) {
        drm.closeSession(sessionId);
    }

    /**
     * Tests clear key system playback.
     */
    private void testClearKeyPlayback(
            UUID drmSchemeUuid,
            String videoMime, String[] videoFeatures,
            String initDataType, byte[][] clearKeyIds,
            Uri audioUrl, boolean audioEncrypted,
            Uri videoUrl, boolean videoEncrypted,
            int videoWidth, int videoHeight, boolean scrambled, int keyType) throws Exception {

        if (isWatchDevice()) {
            return;
        }

        MediaDrm drm = null;
        mSessionId = null;
        final boolean hasDrm = !scrambled && drmSchemeUuid != null;
        if (hasDrm) {
            drm = startDrm(clearKeyIds, initDataType, drmSchemeUuid, keyType);
            mSessionId = openSession(drm);
        }

        if (!preparePlayback(videoMime, videoFeatures, audioUrl, audioEncrypted, videoUrl,
                videoEncrypted, videoWidth, videoHeight, scrambled, mSessionId, getSurfaces())) {
            return;
        }

        if (hasDrm) {
            mDrmInitData = mMediaCodecPlayer.getDrmInitData();
            getKeys(mDrm, initDataType, mSessionId, mDrmInitData, keyType, clearKeyIds);
        }

        if (hasDrm && keyType == MediaDrm.KEY_TYPE_OFFLINE) {
            closeSession(drm, mSessionId);
            mSessionMonitor.waitForSignal();
            mSessionId = openSession(drm);
            if (mKeySetId.length > 0) {
                drm.restoreKeys(mSessionId, mKeySetId);
            } else {
                closeSession(drm, mSessionId);
                stopDrm(drm);
                throw new Error("Invalid keySetId size for offline license");
            }
        }

        // starts video playback
        playUntilEnd();
        if (hasDrm) {
            closeSession(drm, mSessionId);
            stopDrm(drm);
        }
    }

    /**
     * Tests KEY_TYPE_RELEASE for offline license.
     */
    @Presubmit
    public void testReleaseOfflineLicense() throws Exception {
        if (isWatchDevice()) {
            return;
        }

        byte[][] clearKeyIds = new byte[][] { CLEAR_KEY_CENC };
        mSessionId = null;
        String initDataType = "cenc";

        MediaDrm drm = startDrm(clearKeyIds, initDataType,
                CLEARKEY_SCHEME_UUID, MediaDrm.KEY_TYPE_OFFLINE);
        mSessionId = openSession(drm);

        Uri videoUrl = Uri.parse(Utils.getMediaPath() + CENC_VIDEO_PATH);
        if (false == playbackPreCheck(MIME_VIDEO_AVC,
                new String[] { CodecCapabilities.FEATURE_SecurePlayback }, videoUrl,
                VIDEO_WIDTH_CENC, VIDEO_HEIGHT_CENC)) {
            Log.e(TAG, "Failed playback precheck");
            return;
        }

        mMediaCodecPlayer = new MediaCodecClearKeyPlayer(
                getSurfaces(),
                mSessionId, false /*scrambled */,
                mContext.getResources());

        Uri audioUrl = Uri.parse(Utils.getMediaPath() + CENC_AUDIO_PATH);
        mMediaCodecPlayer.setAudioDataSource(audioUrl, null, false);
        mMediaCodecPlayer.setVideoDataSource(videoUrl, null, true);
        mMediaCodecPlayer.start();
        mMediaCodecPlayer.prepare();
        mDrmInitData = mMediaCodecPlayer.getDrmInitData();

        // Create and store the offline license
        getKeys(mDrm, initDataType, mSessionId, mDrmInitData, MediaDrm.KEY_TYPE_OFFLINE,
                clearKeyIds);

        // Verify the offline license is valid
        closeSession(drm, mSessionId);
        mSessionMonitor.waitForSignal();
        mDrm.clearOnKeyStatusChangeListener();
        mSessionId = openSession(drm);
        drm.restoreKeys(mSessionId, mKeySetId);
        closeSession(drm, mSessionId);

        // Release the offline license
        getKeys(mDrm, initDataType, mKeySetId, mDrmInitData, MediaDrm.KEY_TYPE_RELEASE,
                clearKeyIds);

        // Verify restoreKeys will throw an exception if the offline license
        // has already been released
        mSessionId = openSession(drm);
        try {
            drm.restoreKeys(mSessionId, mKeySetId);
        } catch (MediaDrmStateException e) {
            // Expected exception caught, all is good
            return;
        } finally {
            closeSession(drm, mSessionId);
            stopDrm(drm);
        }

        // Did not receive expected exception, throw an Error
        throw new Error("Did not receive expected exception from restoreKeys");
    }

    private boolean queryKeyStatus(@NonNull final MediaDrm drm, @NonNull final byte[] sessionId) {
        final HashMap<String, String> keyStatus = drm.queryKeyStatus(sessionId);
        if (keyStatus.isEmpty()) {
            Log.e(TAG, "queryKeyStatus: empty key status");
            return false;
        }

        final Set<String> keySet = keyStatus.keySet();
        final int numKeys = keySet.size();
        final String[] keys = keySet.toArray(new String[numKeys]);
        for (int i = 0; i < numKeys; ++i) {
            final String key = keys[i];
            Log.i(TAG, "queryKeyStatus: key=" + key + ", value=" + keyStatus.get(key));
        }

        return true;
    }

    @Presubmit
    public void testQueryKeyStatus() throws Exception {
        if (isWatchDevice()) {
            // skip this test on watch because it calls
            // addTrack that requires codec
            return;
        }

        MediaDrm drm = startDrm(new byte[][] { CLEAR_KEY_CENC }, "cenc",
                CLEARKEY_SCHEME_UUID, MediaDrm.KEY_TYPE_STREAMING);

        mSessionId = openSession(drm);

        // Test default key status, should not be defined
        final HashMap<String, String> keyStatus = drm.queryKeyStatus(mSessionId);
        if (!keyStatus.isEmpty()) {
            closeSession(drm, mSessionId);
            stopDrm(drm);
            throw new Error("query default key status failed");
        }

        // Test valid key status
        mMediaCodecPlayer = new MediaCodecClearKeyPlayer(
                getSurfaces(),
                mSessionId, false,
                mContext.getResources());
        mMediaCodecPlayer.setAudioDataSource(
                Uri.parse(Utils.getMediaPath() + CENC_AUDIO_PATH), null, false);
        mMediaCodecPlayer.setVideoDataSource(
                Uri.parse(Utils.getMediaPath() + CENC_VIDEO_PATH), null, true);
        mMediaCodecPlayer.start();
        mMediaCodecPlayer.prepare();

        mDrmInitData = mMediaCodecPlayer.getDrmInitData();
        getKeys(drm, "cenc", mSessionId, mDrmInitData, MediaDrm.KEY_TYPE_STREAMING,
                new byte[][] { CLEAR_KEY_CENC });
        boolean success = true;
        if (!queryKeyStatus(drm, mSessionId)) {
            success = false;
        }

        mMediaCodecPlayer.reset();
        closeSession(drm, mSessionId);
        stopDrm(drm);
        if (!success) {
            throw new Error("query key status failed");
        }
    }

    @Presubmit
    public void testOfflineKeyManagement() throws Exception {
        if (isWatchDevice()) {
            // skip this test on watch because it calls
            // addTrack that requires codec
            return;
        }

        MediaDrm drm = startDrm(new byte[][] { CLEAR_KEY_CENC }, "cenc",
                CLEARKEY_SCHEME_UUID, MediaDrm.KEY_TYPE_OFFLINE);

        if (getClearkeyVersion(drm).matches("1.[01]")) {
            Log.i(TAG, "Skipping testsOfflineKeyManagement: clearkey 1.2 required");
            return;
        }

        mSessionId = openSession(drm);

        // Test get offline keys
        mMediaCodecPlayer = new MediaCodecClearKeyPlayer(
                getSurfaces(),
                mSessionId, false,
                mContext.getResources());
        mMediaCodecPlayer.setAudioDataSource(
                Uri.parse(Utils.getMediaPath() + CENC_AUDIO_PATH), null, false);
        mMediaCodecPlayer.setVideoDataSource(
                Uri.parse(Utils.getMediaPath() + CENC_VIDEO_PATH), null, true);
        mMediaCodecPlayer.start();
        mMediaCodecPlayer.prepare();

        try {
            mDrmInitData = mMediaCodecPlayer.getDrmInitData();

            List<byte[]> keySetIds = drm.getOfflineLicenseKeySetIds();
            int preCount = keySetIds.size();

            getKeys(drm, "cenc", mSessionId, mDrmInitData, MediaDrm.KEY_TYPE_OFFLINE,
                    new byte[][] { CLEAR_KEY_CENC });

            if (drm.getOfflineLicenseState(mKeySetId) != MediaDrm.OFFLINE_LICENSE_STATE_USABLE) {
                throw new Error("Offline license state is not usable");
            }

            keySetIds = drm.getOfflineLicenseKeySetIds();

            if (keySetIds.size() != preCount + 1) {
                throw new Error("KeySetIds size did not increment");
            }

            boolean found = false;
            for (int i = 0; i < keySetIds.size(); i++) {
                if (Arrays.equals(keySetIds.get(i), mKeySetId)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw new Error("New KeySetId is missing from KeySetIds");
            }

            drm.removeOfflineLicense(mKeySetId);

            keySetIds = drm.getOfflineLicenseKeySetIds();
            if (keySetIds.size() != preCount) {
                throw new Error("KeySetIds size is incorrect");
            }

            found = false;
            for (int i = 0; i < keySetIds.size(); i++) {
                if (Arrays.equals(keySetIds.get(i), mKeySetId)) {
                    found = true;
                    break;
                }
            }

            if (found) {
                throw new Error("New KeySetId is still in from KeySetIds after removal");
            }

            // TODO: after RELEASE is implemented: add offline key, release it
            // get offline key status, check state is inactive
        } finally {
            mMediaCodecPlayer.reset();
            closeSession(drm, mSessionId);
            stopDrm(drm);
        }
    }

    @Presubmit
    public void testClearKeyPlaybackCenc() throws Exception {
        testClearKeyPlayback(
            COMMON_PSSH_SCHEME_UUID,
            // using secure codec even though it is clear key DRM
            MIME_VIDEO_AVC, new String[] { CodecCapabilities.FEATURE_SecurePlayback },
            "cenc", new byte[][] { CLEAR_KEY_CENC },
            Uri.parse(Utils.getMediaPath() + CENC_AUDIO_PATH), false  /* audioEncrypted */,
            Uri.parse(Utils.getMediaPath() + CENC_VIDEO_PATH), true /* videoEncrypted */,
            VIDEO_WIDTH_CENC, VIDEO_HEIGHT_CENC, false /* scrambled */,
            MediaDrm.KEY_TYPE_STREAMING);
    }

    @Presubmit
    public void testClearKeyPlaybackCenc2() throws Exception {
        testClearKeyPlayback(
            CLEARKEY_SCHEME_UUID,
            // using secure codec even though it is clear key DRM
            MIME_VIDEO_AVC, new String[] { CodecCapabilities.FEATURE_SecurePlayback },
            "cenc", new byte[][] { CLEAR_KEY_CENC },
            Uri.parse(Utils.getMediaPath() + CENC_AUDIO_PATH), false /* audioEncrypted */ ,
            Uri.parse(Utils.getMediaPath() + CENC_VIDEO_PATH), true /* videoEncrypted */,
            VIDEO_WIDTH_CENC, VIDEO_HEIGHT_CENC, false /* scrambled */,
            MediaDrm.KEY_TYPE_STREAMING);
    }

    @Presubmit
    public void testClearKeyPlaybackOfflineCenc() throws Exception {
        testClearKeyPlayback(
                CLEARKEY_SCHEME_UUID,
                // using secure codec even though it is clear key DRM
                MIME_VIDEO_AVC, new String[] { CodecCapabilities.FEATURE_SecurePlayback },
                "cenc", new byte[][] { CLEAR_KEY_CENC },
                Uri.parse(Utils.getMediaPath() + CENC_AUDIO_PATH), false /* audioEncrypted */ ,
                Uri.parse(Utils.getMediaPath() + CENC_VIDEO_PATH), true /* videoEncrypted */,
                VIDEO_WIDTH_CENC, VIDEO_HEIGHT_CENC, false /* scrambled */,
                MediaDrm.KEY_TYPE_OFFLINE);
    }

    public void testClearKeyPlaybackWebm() throws Exception {
        testClearKeyPlayback(
            CLEARKEY_SCHEME_UUID,
            MIME_VIDEO_VP8, new String[0],
            "webm", new byte[][] { CLEAR_KEY_WEBM },
            WEBM_URL, true /* audioEncrypted */,
            WEBM_URL, true /* videoEncrypted */,
            VIDEO_WIDTH_WEBM, VIDEO_HEIGHT_WEBM, false /* scrambled */,
            MediaDrm.KEY_TYPE_STREAMING);
    }

    public void testClearKeyPlaybackMpeg2ts() throws Exception {
        testClearKeyPlayback(
            CLEARKEY_SCHEME_UUID,
            MIME_VIDEO_AVC, new String[0],
            "mpeg2ts", null,
            MPEG2TS_SCRAMBLED_URL, false /* audioEncrypted */,
            MPEG2TS_SCRAMBLED_URL, false /* videoEncrypted */,
            VIDEO_WIDTH_MPEG2TS, VIDEO_HEIGHT_MPEG2TS, true /* scrambled */,
            MediaDrm.KEY_TYPE_STREAMING);
    }

    public void testPlaybackMpeg2ts() throws Exception {
        testClearKeyPlayback(
            CLEARKEY_SCHEME_UUID,
            MIME_VIDEO_AVC, new String[0],
            "mpeg2ts", null,
            MPEG2TS_CLEAR_URL, false /* audioEncrypted */,
            MPEG2TS_CLEAR_URL, false /* videoEncrypted */,
            VIDEO_WIDTH_MPEG2TS, VIDEO_HEIGHT_MPEG2TS, false /* scrambled */,
            MediaDrm.KEY_TYPE_STREAMING);
    }

    private String getStringProperty(final MediaDrm drm,  final String key) {
        String value = "";
        try {
            value = drm.getPropertyString(key);
        } catch (IllegalArgumentException e) {
            // Expected exception for invalid key
            Log.d(TAG, "Expected result: " + e.getMessage());
        } catch (Exception e) {
            throw new Error(e.getMessage() + "-" + key);
        }
        return value;
    }

    private byte[] getByteArrayProperty(final MediaDrm drm,  final String key) {
        byte[] bytes = new byte[0];
        try {
            bytes = drm.getPropertyByteArray(key);
        } catch (IllegalArgumentException e) {
            // Expected exception for invalid key
            Log.d(TAG, "Expected: " + e.getMessage() + " - " + key);
        } catch (Exception e) {
            throw new Error(e.getMessage() + "-" + key);
        }
        return bytes;
    }

    private void setStringProperty(final MediaDrm drm, final String key, final String value) {
        try {
            drm.setPropertyString(key, value);
        } catch (IllegalArgumentException e) {
            // Expected exception for invalid key
            Log.d(TAG, "Expected: " + e.getMessage() + " - " + key);
        } catch (Exception e) {
            throw new Error(e.getMessage() + "-" + key);
        }
    }

    private void setByteArrayProperty(final MediaDrm drm, final String key, final byte[] bytes) {
        try {
            drm.setPropertyByteArray(key, bytes);
        } catch (IllegalArgumentException e) {
            // Expected exception for invalid key
            Log.d(TAG, "Expected: " + e.getMessage() + " - " + key);
        } catch (Exception e) {
            throw new Error(e.getMessage() + "-" + key);
        }
    }

    @Presubmit
    public void testGetProperties() throws Exception {
        if (watchHasNoClearkeySupport()) {
            return;
        }

        MediaDrm drm = startDrm(new byte[][] { CLEAR_KEY_CENC },
                "cenc", CLEARKEY_SCHEME_UUID, MediaDrm.KEY_TYPE_STREAMING);

        try {
            // The following tests will not verify the value we are getting
            // back since it could change in the future.
            final String[] sKeys = {
                    DESCRIPTION_PROPERTY_KEY, LISTENER_TEST_SUPPORT_PROPERTY_KEY,
                    VENDOR_PROPERTY_KEY, VERSION_PROPERTY_KEY};
            String value;
            for (String key : sKeys) {
                value = getStringProperty(drm, key);
                Log.d(TAG, "getPropertyString returns: " + key + ", " + value);
                if (value.isEmpty()) {
                    throw new Error("Failed to get property for: " + key);
                }
            }

            if (cannotHandleGetPropertyByteArray(drm)) {
                Log.i(TAG, "Skipping testGetProperties: byte array properties not implemented "
                        + "on devices launched before P");
                return;
            }

            byte[] bytes = getByteArrayProperty(drm, DEVICEID_PROPERTY_KEY);
            if (0 == bytes.length) {
                throw new Error("Failed to get property for: " + DEVICEID_PROPERTY_KEY);
            }

            // Test with an invalid property key.
            value = getStringProperty(drm, INVALID_PROPERTY_KEY);
            bytes = getByteArrayProperty(drm, INVALID_PROPERTY_KEY);
            if (!value.isEmpty() || 0 != bytes.length) {
                throw new Error("get property failed using an invalid property key");
            }
        } finally {
            stopDrm(drm);
        }
    }

    @Presubmit
    public void testSetProperties() throws Exception {
        if (watchHasNoClearkeySupport()) {
            return;
        }

        MediaDrm drm = startDrm(new byte[][]{CLEAR_KEY_CENC},
                "cenc", CLEARKEY_SCHEME_UUID, MediaDrm.KEY_TYPE_STREAMING);

        try {
            if (cannotHandleSetPropertyString(drm)) {
                Log.i(TAG, "Skipping testSetProperties: set property string not implemented "
                        + "on devices launched before P");
                return;
            }

            // Test setting predefined string property
            // - Save the value to be restored later
            // - Set the property value
            // - Check the value that was set
            // - Restore previous value
            String listenerTestSupport = getStringProperty(drm, LISTENER_TEST_SUPPORT_PROPERTY_KEY);

            setStringProperty(drm, LISTENER_TEST_SUPPORT_PROPERTY_KEY, "testing");

            String value = getStringProperty(drm, LISTENER_TEST_SUPPORT_PROPERTY_KEY);
            if (!value.equals("testing")) {
                throw new Error("Failed to set property: " + LISTENER_TEST_SUPPORT_PROPERTY_KEY);
            }

            setStringProperty(drm, LISTENER_TEST_SUPPORT_PROPERTY_KEY, listenerTestSupport);

            // Test setting immutable properties
            HashMap<String, String> defaultImmutableProperties = new HashMap<String, String>();
            defaultImmutableProperties.put(ALGORITHMS_PROPERTY_KEY,
                    getStringProperty(drm, ALGORITHMS_PROPERTY_KEY));
            defaultImmutableProperties.put(DESCRIPTION_PROPERTY_KEY,
                    getStringProperty(drm, DESCRIPTION_PROPERTY_KEY));
            defaultImmutableProperties.put(VENDOR_PROPERTY_KEY,
                    getStringProperty(drm, VENDOR_PROPERTY_KEY));
            defaultImmutableProperties.put(VERSION_PROPERTY_KEY,
                    getStringProperty(drm, VERSION_PROPERTY_KEY));

            HashMap<String, String> immutableProperties = new HashMap<String, String>();
            immutableProperties.put(ALGORITHMS_PROPERTY_KEY, "brute force");
            immutableProperties.put(DESCRIPTION_PROPERTY_KEY, "testing only");
            immutableProperties.put(VENDOR_PROPERTY_KEY, "my Google");
            immutableProperties.put(VERSION_PROPERTY_KEY, "undefined");

            for (String key : immutableProperties.keySet()) {
                setStringProperty(drm, key, immutableProperties.get(key));
            }

            // Verify the immutable properties have not been set
            for (String key : immutableProperties.keySet()) {
                value = getStringProperty(drm, key);
                if (!defaultImmutableProperties.get(key).equals(getStringProperty(drm, key))) {
                    throw new Error("Immutable property has changed, key=" + key);
                }
            }

            // Test setPropertyByteArray for immutable property
            final byte[] bytes = new byte[] {
                    0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8,
                    0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0};

            final byte[] deviceId = getByteArrayProperty(drm, DEVICEID_PROPERTY_KEY);

            setByteArrayProperty(drm, DEVICEID_PROPERTY_KEY, bytes);

            // Verify deviceId has not changed
            if (!Arrays.equals(deviceId, getByteArrayProperty(drm, DEVICEID_PROPERTY_KEY))) {
                throw new Error("Failed to set byte array for key=" + DEVICEID_PROPERTY_KEY);
            }
        } finally {
            stopDrm(drm);
        }
    }

    private final static int CLEARKEY_MAX_SESSIONS = 10;

    @Presubmit
    public void testGetNumberOfSessions() {
        if (watchHasNoClearkeySupport()) {
            return;
        }

        MediaDrm drm = startDrm(new byte[][] { CLEAR_KEY_CENC },
                "cenc", CLEARKEY_SCHEME_UUID, MediaDrm.KEY_TYPE_STREAMING);

        try {
            if (getClearkeyVersion(drm).equals("1.0")) {
                Log.i(TAG, "Skipping testGetNumberOfSessions: not supported by clearkey 1.0");
                return;
            }

            int maxSessionCount = drm.getMaxSessionCount();
            if (maxSessionCount != CLEARKEY_MAX_SESSIONS) {
                throw new Error("expected max session count to be " +
                        CLEARKEY_MAX_SESSIONS);
            }
            int initialOpenSessionCount = drm.getOpenSessionCount();
            if (initialOpenSessionCount == maxSessionCount) {
                throw new Error("all sessions open, can't do increment test");
            }
            mSessionId = openSession(drm);
            try {
                if (drm.getOpenSessionCount() != initialOpenSessionCount + 1) {
                    throw new Error("openSessionCount didn't increment");
                }
            } finally {
                closeSession(drm, mSessionId);
            }
        } finally {
            stopDrm(drm);
        }
    }

    @Presubmit
    public void testHdcpLevels() {
        if (watchHasNoClearkeySupport()) {
            return;
        }

        MediaDrm drm = null;
        try {
            drm = new MediaDrm(CLEARKEY_SCHEME_UUID);

            if (getClearkeyVersion(drm).equals("1.0")) {
                Log.i(TAG, "Skipping testHdcpLevels: not supported by clearkey 1.0");
                return;
            }

            if (drm.getConnectedHdcpLevel() != MediaDrm.HDCP_NONE) {
                throw new Error("expected connected hdcp level to be HDCP_NONE");
            }

            if (drm.getMaxHdcpLevel() != MediaDrm.HDCP_NO_DIGITAL_OUTPUT) {
                throw new Error("expected max hdcp level to be HDCP_NO_DIGITAL_OUTPUT");
            }
        } catch(Exception e) {
            throw new Error("Unexpected exception ", e);
        } finally {
            if (drm != null) {
                drm.close();
            }
        }
    }

    @Presubmit
    public void testSecurityLevels() {
        if (watchHasNoClearkeySupport()) {
            return;
        }

        MediaDrm drm = null;
        byte[] sessionId = null;
        try {
            drm = new MediaDrm(CLEARKEY_SCHEME_UUID);

            if (getClearkeyVersion(drm).equals("1.0")) {
                Log.i(TAG, "Skipping testSecurityLevels: not supported by clearkey 1.0");
                return;
            }

            sessionId = drm.openSession(MediaDrm.SECURITY_LEVEL_SW_SECURE_CRYPTO);
            if (drm.getSecurityLevel(sessionId) != MediaDrm.SECURITY_LEVEL_SW_SECURE_CRYPTO) {
                throw new Error("expected security level to be SECURITY_LEVEL_SW_SECURE_CRYPTO");
            }
            drm.closeSession(sessionId);
            sessionId = null;

            sessionId = drm.openSession();
            if (drm.getSecurityLevel(sessionId) != MediaDrm.SECURITY_LEVEL_SW_SECURE_CRYPTO) {
                throw new Error("expected security level to be SECURITY_LEVEL_SW_SECURE_CRYPTO");
            }
            drm.closeSession(sessionId);
            sessionId = null;

            try {
                sessionId = drm.openSession(MediaDrm.SECURITY_LEVEL_SW_SECURE_DECODE);
            } catch (IllegalArgumentException e) {
                /* caught expected exception */
            } catch (Exception e) {
                throw new Exception ("did't get expected IllegalArgumentException" +
                        " while opening a session with disallowed security level");
            } finally  {
                if (sessionId != null) {
                    drm.closeSession(sessionId);
                    sessionId = null;
                }
            }
        } catch(Exception e) {
            throw new Error("Unexpected exception ", e);
        } finally  {
            if (sessionId != null) {
                drm.closeSession(sessionId);
            }
            if (drm != null) {
                drm.close();
            }
        }
     }

    @Presubmit
    public void testSecureStop() {
        if (watchHasNoClearkeySupport()) {
            return;
        }

        MediaDrm drm = startDrm(new byte[][] {CLEAR_KEY_CENC}, "cenc",
                CLEARKEY_SCHEME_UUID, MediaDrm.KEY_TYPE_STREAMING);

        byte[] sessionId = null;
        try {
            if (getClearkeyVersion(drm).equals("1.0")) {
                Log.i(TAG, "Skipping testSecureStop: not supported in ClearKey v1.0");
                return;
            }

            drm.removeAllSecureStops();
            Log.d(TAG, "Test getSecureStops from an empty list.");
            List<byte[]> secureStops = drm.getSecureStops();
            assertTrue(secureStops.isEmpty());

            Log.d(TAG, "Test getSecureStopIds from an empty list.");
            List<byte[]> secureStopIds = drm.getSecureStopIds();
            assertTrue(secureStopIds.isEmpty());

            mSessionId = openSession(drm);

            mMediaCodecPlayer = new MediaCodecClearKeyPlayer(
                    getSurfaces(), mSessionId, false, mContext.getResources());
            mMediaCodecPlayer.setAudioDataSource(
                    Uri.parse(Utils.getMediaPath() + CENC_AUDIO_PATH), null, false);
            mMediaCodecPlayer.setVideoDataSource(
                    Uri.parse(Utils.getMediaPath() + CENC_VIDEO_PATH), null, true);
            mMediaCodecPlayer.start();
            mMediaCodecPlayer.prepare();
            mDrmInitData = mMediaCodecPlayer.getDrmInitData();

            for (int i = 0; i < NUMBER_OF_SECURE_STOPS; ++i) {
                getKeys(drm, "cenc", mSessionId, mDrmInitData,
                        MediaDrm.KEY_TYPE_STREAMING, new byte[][] {CLEAR_KEY_CENC});
            }
            Log.d(TAG, "Test getSecureStops.");
            secureStops = drm.getSecureStops();
            assertEquals(NUMBER_OF_SECURE_STOPS, secureStops.size());

            Log.d(TAG, "Test getSecureStopIds.");
            secureStopIds = drm.getSecureStopIds();
            assertEquals(NUMBER_OF_SECURE_STOPS, secureStopIds.size());

            Log.d(TAG, "Test getSecureStop using secure stop Ids.");
            for (int i = 0; i < secureStops.size(); ++i) {
                byte[] secureStop = drm.getSecureStop(secureStopIds.get(i));
                assertTrue(Arrays.equals(secureStops.get(i), secureStop));
            }

            Log.d(TAG, "Test removeSecureStop given a secure stop Id.");
            drm.removeSecureStop(secureStopIds.get(NUMBER_OF_SECURE_STOPS - 1));
            secureStops = drm.getSecureStops();
            secureStopIds = drm.getSecureStopIds();
            assertEquals(NUMBER_OF_SECURE_STOPS - 1, secureStops.size());
            assertEquals(NUMBER_OF_SECURE_STOPS - 1, secureStopIds.size());

            Log.d(TAG, "Test releaseSecureStops given a release message.");
            // Simulate server response message by removing
            // every other secure stops to make it interesting.
            List<byte[]> releaseList = new ArrayList<byte[]>();
            int releaseListSize = 0;
            for (int i = 0; i < secureStops.size(); i += 2) {
                byte[] secureStop = secureStops.get(i);
                releaseList.add(secureStop);
                releaseListSize += secureStop.length;
            }

            // ClearKey's release message format (this is a format shared between
            // the server and the drm service).
            // The clearkey implementation expects the message to contain
            // a 4 byte count of the number of fixed length secure stops
            // to follow.
            String count = String.format("%04d", releaseList.size());
            byte[] releaseMessage = new byte[count.length() + releaseListSize];

            byte[] buffer = count.getBytes();
            System.arraycopy(buffer, 0, releaseMessage, 0, count.length());

            int destPosition = count.length();
            for (int i = 0; i < releaseList.size(); ++i) {
                byte[] secureStop = releaseList.get(i);
                int secureStopSize = secureStop.length;
                System.arraycopy(secureStop, 0, releaseMessage, destPosition, secureStopSize);
                destPosition += secureStopSize;
            }

            drm.releaseSecureStops(releaseMessage);
            secureStops = drm.getSecureStops();
            secureStopIds = drm.getSecureStopIds();
            // All odd numbered secure stops are removed in the test,
            // leaving 2nd, 4th, 6th and the 8th element.
            assertEquals((NUMBER_OF_SECURE_STOPS - 1) / 2, secureStops.size());
            assertEquals((NUMBER_OF_SECURE_STOPS - 1 ) / 2, secureStopIds.size());

            Log.d(TAG, "Test removeAllSecureStops.");
            drm.removeAllSecureStops();
            secureStops = drm.getSecureStops();
            assertTrue(secureStops.isEmpty());
            secureStopIds = drm.getSecureStopIds();
            assertTrue(secureStopIds.isEmpty());

            mMediaCodecPlayer.reset();
            closeSession(drm, mSessionId);
        } catch (Exception e) {
            throw new Error("Unexpected exception", e);
        } finally {
            if (sessionId != null) {
                drm.closeSession(sessionId);
            }
            stopDrm(drm);
        }
    }

    /**
     * Test that the framework handles a device returning
     * ::android::hardware::drm@1.2::Status::ERROR_DRM_RESOURCE_CONTENTION.
     * Expected behavior: throws MediaDrm.SessionException with
     * errorCode ERROR_RESOURCE_CONTENTION
     */
    @Presubmit
    public void testResourceContentionError() {

        if (watchHasNoClearkeySupport()) {
            return;
        }

        MediaDrm drm = null;
        boolean gotException = false;

        try {
            drm = new MediaDrm(CLEARKEY_SCHEME_UUID);
            drm.setPropertyString("drmErrorTest", "resourceContention");
            byte[] sessionId = drm.openSession();

            try {
                byte[] ignoredInitData = new byte[] { 1 };
                drm.getKeyRequest(sessionId, ignoredInitData, "cenc", MediaDrm.KEY_TYPE_STREAMING, null);
            } catch (MediaDrm.SessionException e) {
                if (e.getErrorCode() != MediaDrm.SessionException.ERROR_RESOURCE_CONTENTION) {
                    throw new Error("Incorrect error code, expected ERROR_RESOURCE_CONTENTION");
                }
                gotException = true;
            }
        } catch(Exception e) {
            throw new Error("Unexpected exception ", e);
        } finally {
            if (drm != null) {
                drm.close();
            }
        }
        if (!gotException) {
            throw new Error("Didn't receive expected MediaDrm.SessionException");
        }
    }

    /**
     * Test that the framework handles a device returning invoking
     * the ::android::hardware::drm@1.2::sendSessionLostState callback
     * Expected behavior: OnSessionLostState is called with
     * the sessionId
     */
    @Presubmit
    public void testSessionLostStateError() {

        if (watchHasNoClearkeySupport()) {
            return;
        }

        boolean gotException = false;
        mLostStateReceived = false;

        MediaDrm drm = startDrm(new byte[][] { CLEAR_KEY_CENC }, "cenc",
                CLEARKEY_SCHEME_UUID, MediaDrm.KEY_TYPE_STREAMING);

        mDrm.setPropertyString("drmErrorTest", "lostState");
        mSessionId = openSession(drm);

        // simulates session lost state here, detected by closeSession

        try {
            try {
                closeSession(drm, mSessionId);
            } catch (MediaDrmStateException e) {
                gotException = true; // expected for lost state
            }
            // wait up to 2 seconds for event
            for (int i = 0; i < 20 && !mLostStateReceived; i++) {
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                }
            }
            if (!mLostStateReceived) {
                throw new Error("Callback for OnSessionLostStateListener not received");
            }
        } catch(Exception e) {
            throw new Error("Unexpected exception ", e);
        } finally {
            stopDrm(drm);
        }
        if (!gotException) {
            throw new Error("Didn't receive expected MediaDrmStateException");
        }
    }

    @Presubmit
    public void testIsCryptoSchemeSupportedWithSecurityLevel() {
        if (watchHasNoClearkeySupport()) {
            return;
        }

        if (MediaDrm.isCryptoSchemeSupported(CLEARKEY_SCHEME_UUID, "cenc",
                                             MediaDrm.SECURITY_LEVEL_HW_SECURE_ALL)) {
            throw new Error("Clearkey claims to support SECURITY_LEVEL_HW_SECURE_ALL");
        }
        if (!MediaDrm.isCryptoSchemeSupported(CLEARKEY_SCHEME_UUID, "cenc",
                                              MediaDrm.SECURITY_LEVEL_SW_SECURE_CRYPTO)) {
            throw new Error("Clearkey claims not to support SECURITY_LEVEL_SW_SECURE_CRYPTO");
        }
    }

    private String getClearkeyVersion(MediaDrm drm) {
        try {
            return drm.getPropertyString("version");
        } catch (Exception e) {
            return "unavailable";
        }
    }

    private boolean cannotHandleGetPropertyByteArray(MediaDrm drm) {
        boolean apiNotSupported = false;
        byte[] bytes = new byte[0];
        try {
            bytes = drm.getPropertyByteArray(DEVICEID_PROPERTY_KEY);
        } catch (IllegalArgumentException e) {
            // Expected exception for invalid key or api not implemented
            apiNotSupported = true;
        }
        return apiNotSupported;
    }

    private boolean cannotHandleSetPropertyString(MediaDrm drm) {
        boolean apiNotSupported = false;
        final byte[] bytes = new byte[0];
        try {
            drm.setPropertyString(LISTENER_TEST_SUPPORT_PROPERTY_KEY, "testing");
        } catch (IllegalArgumentException e) {
            // Expected exception for invalid key or api not implemented
            apiNotSupported = true;
        }
        return apiNotSupported;
    }

    private boolean watchHasNoClearkeySupport() {
        if (!MediaDrm.isCryptoSchemeSupported(CLEARKEY_SCHEME_UUID)) {
            if (isWatchDevice()) {
                return true;
            } else {
                throw new Error(ERR_MSG_CRYPTO_SCHEME_NOT_SUPPORTED);
            }
        }
        return false;
    }

    private List<Surface> getSurfaces() {
        return Arrays.asList(getActivity().getSurfaceHolder().getSurface());
    }
}
