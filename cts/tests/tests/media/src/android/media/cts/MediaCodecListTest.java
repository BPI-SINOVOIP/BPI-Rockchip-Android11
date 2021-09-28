/*
 * Copyright (C) 2012 The Android Open Source Project
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

import static android.media.MediaCodecInfo.CodecCapabilities.FEATURE_SecurePlayback;
import static android.media.MediaCodecInfo.CodecCapabilities.FEATURE_TunneledPlayback;

import com.android.compatibility.common.util.PropertyUtil;

import android.content.pm.PackageManager;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.AudioCapabilities;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.EncoderCapabilities;
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.platform.test.annotations.Presubmit;
import android.platform.test.annotations.RequiresDevice;
import android.test.AndroidTestCase;
import android.util.Log;
import android.util.Pair;
import android.util.Size;

import androidx.test.filters.SmallTest;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

@Presubmit
@SmallTest
@RequiresDevice
public class MediaCodecListTest extends AndroidTestCase {

    private static final String TAG = "MediaCodecListTest";
    private static final String MEDIA_CODEC_XML_FILE = "/etc/media_codecs.xml";
    private static final String VENDOR_MEDIA_CODEC_XML_FILE = "/vendor/etc/media_codecs.xml";
    private static final String ODM_MEDIA_CODEC_XML_FILE = "/odm/etc/media_codecs.xml";
    private final MediaCodecList mRegularCodecs =
            new MediaCodecList(MediaCodecList.REGULAR_CODECS);
    private final MediaCodecList mAllCodecs =
            new MediaCodecList(MediaCodecList.ALL_CODECS);
    private final MediaCodecInfo[] mRegularInfos =
            mRegularCodecs.getCodecInfos();
    private final MediaCodecInfo[] mAllInfos =
            mAllCodecs.getCodecInfos();

    class CodecType {
        CodecType(String type, boolean isEncoder, MediaFormat sampleFormat) {
            mMimeTypeName = type;
            mIsEncoder = isEncoder;
            mSampleFormat = sampleFormat;
        }

        boolean equals(CodecType codecType) {
            return (mMimeTypeName.compareTo(codecType.mMimeTypeName) == 0) &&
                    mIsEncoder == codecType.mIsEncoder;
        }

        boolean canBeFound() {
            return codecCanBeFound(mIsEncoder, mSampleFormat);
        }

        @Override
        public String toString() {
            return mMimeTypeName + (mIsEncoder ? " encoder" : " decoder") + " for " + mSampleFormat;
        }

        private String mMimeTypeName;
        private boolean mIsEncoder;
        private MediaFormat mSampleFormat;
    };

    class AudioCodec extends CodecType {
        AudioCodec(String mime, boolean isEncoder, int sampleRate) {
            super(mime, isEncoder, MediaFormat.createAudioFormat(
                    mime, sampleRate, 1 /* channelCount */));
        }
    }

    class VideoCodec extends CodecType {
        VideoCodec(String mime, boolean isEncoder) {
            // implicit assumption that QVGA video is always valid
            super(mime, isEncoder, MediaFormat.createVideoFormat(
                    mime, 176 /* width */, 144 /* height */));
        }
    }

    public static boolean hasExpandedCodecInfo() {
        return PropertyUtil.isVendorApiLevelNewerThan(29);
    }

    public static void testMediaCodecXmlFileExist() {
        File file = new File(MEDIA_CODEC_XML_FILE);
        File vendorFile = new File(VENDOR_MEDIA_CODEC_XML_FILE);
        File odmFile = new File(ODM_MEDIA_CODEC_XML_FILE);
        assertTrue("media_codecs.xml does not exist in /odm/etc, /vendor/etc or /etc.",
                file.exists() || vendorFile.exists() || odmFile.exists());
    }

    private MediaCodecInfo[] getLegacyInfos() {
        Log.d(TAG, "getLegacyInfos");

        int codecCount = MediaCodecList.getCodecCount();
        MediaCodecInfo[] res = new MediaCodecInfo[codecCount];

        for (int i = 0; i < codecCount; ++i) {
            res[i] = MediaCodecList.getCodecInfoAt(i);
        }
        return res;
    }

    public void assertEqualsOrSuperset(Set big, Set tiny, boolean superset) {
        if (!superset) {
            assertEquals(big, tiny);
        } else {
            assertTrue(big.containsAll(tiny));
        }
    }

    private static <T> Set<T> asSet(T[] array) {
        Set<T> s = new HashSet<T>();
        for (T el : array) {
            s.add(el);
        }
        return s;
    }

    private static Set<Integer> asSet(int[] array) {
        Set<Integer> s = new HashSet<Integer>();
        for (int el : array) {
            s.add(el);
        }
        return s;
    }

    public void assertEqualsOrSuperset(
            CodecCapabilities big, CodecCapabilities tiny, boolean superset) {
        // ordering of enumerations may differ
        assertEqualsOrSuperset(asSet(big.colorFormats), asSet(tiny.colorFormats), superset);
        assertEqualsOrSuperset(asSet(big.profileLevels), asSet(tiny.profileLevels), superset);
        AudioCapabilities bigAudCaps = big.getAudioCapabilities();
        VideoCapabilities bigVidCaps = big.getVideoCapabilities();
        EncoderCapabilities bigEncCaps = big.getEncoderCapabilities();
        AudioCapabilities tinyAudCaps = tiny.getAudioCapabilities();
        VideoCapabilities tinyVidCaps = tiny.getVideoCapabilities();
        EncoderCapabilities tinyEncCaps = tiny.getEncoderCapabilities();
        assertEquals(bigAudCaps != null, tinyAudCaps != null);
        assertEquals(bigAudCaps != null, tinyAudCaps != null);
        assertEquals(bigAudCaps != null, tinyAudCaps != null);
    }

    public void assertEqualsOrSuperset(
            MediaCodecInfo big, MediaCodecInfo tiny, boolean superset) {
        assertEquals(big.getName(), tiny.getName());
        assertEquals(big.isEncoder(), tiny.isEncoder());
        assertEqualsOrSuperset(
                asSet(big.getSupportedTypes()), asSet(tiny.getSupportedTypes()), superset);
        for (String type : big.getSupportedTypes()) {
            assertEqualsOrSuperset(
                    big.getCapabilitiesForType(type),
                    tiny.getCapabilitiesForType(type),
                    superset);
        }
    }

    public void assertSuperset(MediaCodecInfo big, MediaCodecInfo tiny) {
        assertEqualsOrSuperset(big, tiny, true /* superset */);
    }

    public void assertEquals(MediaCodecInfo big, MediaCodecInfo tiny) {
        assertEqualsOrSuperset(big, tiny, false /* superset */);
    }

    // Each component advertised by MediaCodecList should at least be
    // instantiable.
    private void testComponentInstantiation(MediaCodecInfo[] infos) throws IOException {
        for (MediaCodecInfo info : infos) {
            Log.d(TAG, "codec: " + info.getName());
            Log.d(TAG, "  isEncoder = " + info.isEncoder());

            MediaCodec codec = MediaCodec.createByCodecName(info.getName());

            assertEquals(codec.getName(), info.getName());
            assertEquals(codec.getCanonicalName(), info.getCanonicalName());
            assertEquals(codec.getCodecInfo(), info);

            codec.release();
            codec = null;
        }
    }

    public void testRegularComponentInstantiation() throws IOException {
        Log.d(TAG, "testRegularComponentInstantiation");
        testComponentInstantiation(mRegularInfos);
    }

    public void testAllComponentInstantiation() throws IOException {
        Log.d(TAG, "testAllComponentInstantiation");
        testComponentInstantiation(mAllInfos);
    }

    public void testLegacyComponentInstantiation() throws IOException {
        Log.d(TAG, "testLegacyComponentInstantiation");
        testComponentInstantiation(getLegacyInfos());
    }

    // For each type advertised by any of the components we should be able
    // to get capabilities.
    private void testGetCapabilities(MediaCodecInfo[] infos) {
        for (MediaCodecInfo info : infos) {
            Log.d(TAG, "codec: " + info.getName());
            Log.d(TAG, "  isEncoder = " + info.isEncoder());

            String[] types = info.getSupportedTypes();
            for (int j = 0; j < types.length; ++j) {
                Log.d(TAG, "calling getCapabilitiesForType " + types[j]);
                CodecCapabilities cap = info.getCapabilitiesForType(types[j]);
            }
        }
    }

    public void testGetRegularCapabilities() {
        Log.d(TAG, "testGetRegularCapabilities");
        testGetCapabilities(mRegularInfos);
    }

    public void testGetAllCapabilities() {
        Log.d(TAG, "testGetAllCapabilities");
        testGetCapabilities(mAllInfos);
    }

    public void testGetLegacyCapabilities() {
        Log.d(TAG, "testGetLegacyCapabilities");
        testGetCapabilities(getLegacyInfos());
    }

    public void testLegacyMediaCodecListIsSameAsRegular() {
        // regular codecs should be equivalent to legacy codecs, including
        // codec ordering
        MediaCodecInfo[] legacyInfos = getLegacyInfos();
        assertEquals(legacyInfos.length, mRegularInfos.length);
        for (int i = 0; i < legacyInfos.length; ++i) {
            assertEquals(legacyInfos[i], mRegularInfos[i]);
        }
    }

    public void testRegularMediaCodecListIsASubsetOfAll() {
        Log.d(TAG, "testRegularMediaCodecListIsASubsetOfAll");
        // regular codecs should be a subsequence of all codecs, including
        // codec ordering
        int ix = 0;
        for (MediaCodecInfo info : mAllInfos) {
            if (ix == mRegularInfos.length) {
                break;
            }
            if (!mRegularInfos[ix].getName().equals(info.getName())) {
                Log.d(TAG, "skipping non-regular codec " + info.getName());
                continue;
            }
            Log.d(TAG, "checking codec " + info.getName());
            assertSuperset(info, mRegularInfos[ix]);
            ++ix;
        }
        assertEquals(
                "some regular codecs are not listed in all codecs", ix, mRegularInfos.length);
    }

    public void testRequiredMediaCodecList() {
        List<CodecType> requiredList = getRequiredCodecTypes();
        List<CodecType> supportedList = getSupportedCodecTypes();
        assertTrue(areRequiredCodecTypesSupported(requiredList, supportedList));
        for (CodecType type : requiredList) {
            assertTrue("cannot find " + type, type.canBeFound());
        }
    }

    private boolean hasCamera() {
        PackageManager pm = getContext().getPackageManager();
        return pm.hasSystemFeature(pm.FEATURE_CAMERA_FRONT) ||
                pm.hasSystemFeature(pm.FEATURE_CAMERA);
    }

    private boolean hasMicrophone() {
        PackageManager pm = getContext().getPackageManager();
        return pm.hasSystemFeature(pm.FEATURE_MICROPHONE);
    }

    private boolean isWatch() {
        PackageManager pm = getContext().getPackageManager();
        return pm.hasSystemFeature(pm.FEATURE_WATCH);
    }

    private boolean isHandheld() {
        // handheld nature is not exposed to package manager, for now
        // we check for touchscreen and NOT watch and NOT tv
        PackageManager pm = getContext().getPackageManager();
        return pm.hasSystemFeature(pm.FEATURE_TOUCHSCREEN)
                && !pm.hasSystemFeature(pm.FEATURE_WATCH)
                && !pm.hasSystemFeature(pm.FEATURE_TELEVISION)
                && !pm.hasSystemFeature(pm.FEATURE_AUTOMOTIVE);
    }

    private boolean isAutomotive() {
        PackageManager pm = getContext().getPackageManager();
        return pm.hasSystemFeature(pm.FEATURE_AUTOMOTIVE);
    }

    private boolean isPC() {
        PackageManager pm = getContext().getPackageManager();
        return pm.hasSystemFeature(pm.FEATURE_PC);
    }

    // Find whether the given codec can be found using MediaCodecList.find methods.
    private boolean codecCanBeFound(boolean isEncoder, MediaFormat format) {
        String codecName = isEncoder
                ? mRegularCodecs.findEncoderForFormat(format)
                : mRegularCodecs.findDecoderForFormat(format);
        return codecName != null;
    }

    /*
     * Find whether all required media codec types are supported
     */
    private boolean areRequiredCodecTypesSupported(
        List<CodecType> requiredList, List<CodecType> supportedList) {
        for (CodecType requiredCodec: requiredList) {
            boolean isSupported = false;
            for (CodecType supportedCodec: supportedList) {
                if (requiredCodec.equals(supportedCodec)) {
                    isSupported = true;
                }
            }
            if (!isSupported) {
                String codec = requiredCodec.mMimeTypeName
                                + ", " + (requiredCodec.mIsEncoder? "encoder": "decoder");
                Log.e(TAG, "Media codec (" + codec + ") is not supported");
                return false;
            }
        }
        return true;
    }

    /*
     * Find all the media codec types are supported.
     */
    private List<CodecType> getSupportedCodecTypes() {
        List<CodecType> supportedList = new ArrayList<CodecType>();
        for (MediaCodecInfo info : mRegularInfos) {
            String[] types = info.getSupportedTypes();
            assertTrue("Unexpected number of supported types", types.length > 0);
            boolean isEncoder = info.isEncoder();
            for (int j = 0; j < types.length; ++j) {
                supportedList.add(new CodecType(types[j], isEncoder, null /* sampleFormat */));
            }
        }
        return supportedList;
    }

    /*
     * This list should be kept in sync with the CCD document
     * See http://developer.android.com/guide/appendix/media-formats.html
     */
    private List<CodecType> getRequiredCodecTypes() {
        List<CodecType> list = new ArrayList<CodecType>(16);

        // Mandatory audio decoders

        list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_FLAC, false, 48000));
        list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_MPEG, false, 8000));  // mp3
        list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_MPEG, false, 48000)); // mp3
        list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_VORBIS, false, 8000));
        list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_VORBIS, false, 48000));
        list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_AAC, false, 8000));
        list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_AAC, false, 48000));
        list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_RAW, false, 8000));
        list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_RAW, false, 44100));
        list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_OPUS, false, 48000));

        // Mandatory audio encoders (for non-watch devices with camera)

        if (hasMicrophone() && !isWatch()) {
            list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_AAC, true, 8000));
            list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_AAC, true, 48000));
            // flac encoder is not required
            // list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_FLAC, true));  // encoder
        }

        // Mandatory audio encoders for handheld devices
        if (isHandheld()) {
            list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_AMR_NB, false, 8000));  // decoder
            list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_AMR_NB, true,  8000));  // encoder
            list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_AMR_WB, false, 16000)); // decoder
            list.add(new AudioCodec(MediaFormat.MIMETYPE_AUDIO_AMR_WB, true,  16000)); // encoder
        }

        // Mandatory video codecs (for non-watch devices)

        if (!isWatch()) {
            list.add(new VideoCodec(MediaFormat.MIMETYPE_VIDEO_AVC, false));   // avc decoder
            list.add(new VideoCodec(MediaFormat.MIMETYPE_VIDEO_AVC, true));    // avc encoder
            list.add(new VideoCodec(MediaFormat.MIMETYPE_VIDEO_VP8, false));   // vp8 decoder
            list.add(new VideoCodec(MediaFormat.MIMETYPE_VIDEO_VP8, true));    // vp8 encoder
            list.add(new VideoCodec(MediaFormat.MIMETYPE_VIDEO_VP9, false));   // vp9 decoder

            // According to CDD, hevc decoding is not mandatory for automotive and PC devices.
            if (!isAutomotive() && !isPC()) {
                list.add(new VideoCodec(MediaFormat.MIMETYPE_VIDEO_HEVC, false));  // hevc decoder
            }
            list.add(new VideoCodec(MediaFormat.MIMETYPE_VIDEO_MPEG4, false)); // m4v decoder
            list.add(new VideoCodec(MediaFormat.MIMETYPE_VIDEO_H263, false));  // h263 decoder
            if (hasCamera()) {
                list.add(new VideoCodec(MediaFormat.MIMETYPE_VIDEO_H263, true)); // h263 encoder
            }
        }

        return list;
    }

    public void testFindDecoderWithAacProfile() throws Exception {
        Log.d(TAG, "testFindDecoderWithAacProfile");
        MediaFormat format = MediaFormat.createAudioFormat(
                MediaFormat.MIMETYPE_AUDIO_AAC, 8000, 1);
        List<Integer> profiles = new ArrayList<>();
        profiles.add(MediaCodecInfo.CodecProfileLevel.AACObjectLC);
        profiles.add(MediaCodecInfo.CodecProfileLevel.AACObjectHE);
        profiles.add(MediaCodecInfo.CodecProfileLevel.AACObjectHE_PS);
        // The API is added at 5.0, so the profile below must be supported.
        profiles.add(MediaCodecInfo.CodecProfileLevel.AACObjectELD);
        for (int profile : profiles) {
            format.setInteger(MediaFormat.KEY_AAC_PROFILE, profile);
            String codecName = mRegularCodecs.findDecoderForFormat(format);
            assertNotNull("Profile " + profile + " must be supported.", codecName);
        }
    }

    public void testFindEncoderWithAacProfile() throws Exception {
        Log.d(TAG, "testFindEncoderWithAacProfile");
        MediaFormat format = MediaFormat.createAudioFormat(
                MediaFormat.MIMETYPE_AUDIO_AAC, 8000, 1);
        List<Integer> profiles = new ArrayList<>();
        if (hasMicrophone() && !isWatch()) {
            profiles.add(MediaCodecInfo.CodecProfileLevel.AACObjectLC);
            // The API is added at 5.0, so the profiles below must be supported.
            profiles.add(MediaCodecInfo.CodecProfileLevel.AACObjectHE);
            profiles.add(MediaCodecInfo.CodecProfileLevel.AACObjectELD);
        }
        for (int profile : profiles) {
            format.setInteger(MediaFormat.KEY_AAC_PROFILE, profile);
            String codecName = mRegularCodecs.findEncoderForFormat(format);
            assertNotNull("Profile " + profile + " must be supported.", codecName);
        }
    }

    public void testAudioCodecChannels() {
        for (MediaCodecInfo info : mAllInfos) {
            String[] types = info.getSupportedTypes();
            for (int j = 0; j < types.length; ++j) {
                CodecCapabilities cap = info.getCapabilitiesForType(types[j]);
                AudioCapabilities audioCap = cap.getAudioCapabilities();
                if (audioCap == null) {
                    assertFalse("no audio capabilities for audio media type " + types[j] + " of "
                                    + info.getName(),
                                types[j].toLowerCase().startsWith("audio/"));
                    continue;
                }
                int n = audioCap.getMaxInputChannelCount();
                Log.d(TAG, info.getName() + ": " + n);
                assertTrue(info.getName() + " max input channel not positive: " + n, n > 0);
            }
        }
    }

    private void testCanonicalCodecIsNotAnAlias(String canonicalName) {
        // canonical name must point to a non-alias
        for (MediaCodecInfo canonical : mAllInfos) {
            if (canonical.getName().equals(canonicalName)) {
                assertFalse(canonical.isAlias());
                return;
            }
        }
        fail("could not find info to canonical name '" + canonicalName + "'");
    }

    private String getCustomPartOfComponentName(MediaCodecInfo info) {
        String name = info.getName();
        if (name.startsWith("OMX.") || name.startsWith("c2.")) {
            // strip off OMX.<vendor_name>.
            return name.replaceFirst("^OMX\\.([^.]+)\\.", "");
        }
        return name;
    }

    private void testKindInCodecNamesIsMeaningful(MediaCodecInfo info) {
        String name = getCustomPartOfComponentName(info);
        // codec names containing 'encoder' or 'enc' must be encoders, 'decoder' or 'dec' must
        // be decoders
        if (name.matches("(?i)\\b(encoder|enc)\\b")) {
            assertTrue(info.isEncoder());
        }
        if (name.matches("(?i)\\b(decoder|dec)\\b")) {
            assertFalse(info.isEncoder());
        }
    }

    private void testMediaTypeInCodecNamesIsMeaningful(MediaCodecInfo info) {
        // Codec names containing media type names must support that media type
        String name = getCustomPartOfComponentName(info);

        Set<String> supportedTypes = new HashSet<String>(Arrays.asList(info.getSupportedTypes()));

        // video types
        if (name.matches("(?i)\\b(mp(eg)?2)\\b")) {
            // this may refer to audio mpeg1-layer2 or video mpeg2
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_VIDEO_MPEG2)
                        || supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_MPEG + "-L2"));
        }
        if (name.matches("(?i)\\b(h\\.?263)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_VIDEO_H263));
        }
        if (name.matches("(?i)\\b(mp(eg)?4)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_VIDEO_MPEG4));
        }
        if (name.matches("(?i)\\b(h\\.?264|avc)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_VIDEO_AVC));
        }
        if (name.matches("(?i)\\b(vp8)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_VIDEO_VP8));
        }
        if (name.matches("(?i)\\b(h\\.?265|hevc)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_VIDEO_HEVC));
        }
        if (name.matches("(?i)\\b(vp9)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_VIDEO_VP9));
        }
        if (name.matches("(?i)\\b(av0?1)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_VIDEO_AV1));
        }

        // audio types
        if (name.matches("(?i)\\b(mp(eg)?3)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_MPEG));
        }
        if (name.matches("(?i)\\b(x?aac)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_AAC));
        }
        if (name.matches("(?i)\\b(pcm)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_RAW));
        }
        if (name.matches("(?i)\\b(raw)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_RAW)
                        || supportedTypes.contains(MediaFormat.MIMETYPE_VIDEO_RAW));
        }
        if (name.matches("(?i)\\b(amr)\\b")) {
            if (name.matches("(?i)\\b(nb)\\b")) {
                assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_AMR_NB));
            } else if (name.matches("(?i)\\b(wb)\\b")) {
                assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_AMR_WB));
            } else {
                assertTrue(
                    supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_AMR_NB)
                            || supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_AMR_WB));
            }
        }
        if (name.matches("(?i)\\b(opus)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_OPUS));
        }
        if (name.matches("(?i)\\b(vorbis)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_VORBIS));
        }
        if (name.matches("(?i)\\b(flac)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_FLAC));
        }
        if (name.matches("(?i)\\b(ac3)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_AC3));
        }
        if (name.matches("(?i)\\b(ac4)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_AC4));
        }
        if (name.matches("(?i)\\b(eac3)\\b")) {
            assertTrue(supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_EAC3)
                        || supportedTypes.contains(MediaFormat.MIMETYPE_AUDIO_EAC3_JOC));
        }
    }

    public void testCodecCharacterizations() {
        for (MediaCodecInfo info : mAllInfos) {
            Log.d(TAG, "codec: " + info.getName() + " canonical: " + info.getCanonicalName());

            // AOSP codecs must not be marked as vendor or hardware accelerated
            if (info.getName().startsWith("OMX.google.")) {
                assertFalse(info.isVendor());
                assertFalse(info.isHardwareAccelerated());
            }

            // Codec 2.0 based AOSP codecs must run in a software-only process
            if (info.getName().startsWith("c2.android.")) {
                assertTrue(info.isSoftwareOnly());
                assertFalse(info.isVendor());
                assertFalse(info.isHardwareAccelerated());
            }

            // validate aliases
            if (info.isAlias()) {
                assertFalse(info.getName().equals(info.getCanonicalName()));
                testCanonicalCodecIsNotAnAlias(info.getCanonicalName());
            } else {
                // validate codec names: (Canonical) codec names must be meaningful.
                // We only test this on canonical infos as we allow aliases to support
                // existing codec names that do not fit this.
                assertEquals(info.getName(), info.getCanonicalName());
                testKindInCodecNamesIsMeaningful(info);
                testMediaTypeInCodecNamesIsMeaningful(info);
            }

            // hardware accelerated codecs cannot be software only
            assertFalse(info.isHardwareAccelerated() && info.isSoftwareOnly());
        }
    }

    public void testVideoPerformancePointsSanity() {
        MediaFormat hd25Format =
            MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 1280, 720);
        hd25Format.setFloat(MediaFormat.KEY_FRAME_RATE, 25.f);

        MediaFormat portraitHd240Format =
            MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 720, 1280);
        portraitHd240Format.setInteger(MediaFormat.KEY_FRAME_RATE, 240);

        /* common-sense checks */
        assertTrue(VideoCapabilities.PerformancePoint.HD_30.covers(hd25Format));
        assertTrue(VideoCapabilities.PerformancePoint.HD_25.covers(hd25Format));
        assertFalse(VideoCapabilities.PerformancePoint.HD_24.covers(hd25Format));
        assertTrue(VideoCapabilities.PerformancePoint.FHD_30.covers(hd25Format));
        assertTrue(VideoCapabilities.PerformancePoint.FHD_25.covers(hd25Format));
        assertFalse(VideoCapabilities.PerformancePoint.FHD_24.covers(hd25Format));

        assertTrue(VideoCapabilities.PerformancePoint.HD_240.covers(portraitHd240Format));
        assertFalse(VideoCapabilities.PerformancePoint.HD_200.covers(portraitHd240Format));
        assertTrue(VideoCapabilities.PerformancePoint.FHD_240.covers(portraitHd240Format));
        assertFalse(VideoCapabilities.PerformancePoint.FHD_200.covers(portraitHd240Format));

        /* test macroblock size and conversion support */
        VideoCapabilities.PerformancePoint bigBlockFHD30_120 =
            new VideoCapabilities.PerformancePoint(1920, 1080, 30, 120, new Size(128, 64));
        assertEquals(120, bigBlockFHD30_120.getMaxFrameRate());
        assertEquals(8160, bigBlockFHD30_120.getMaxMacroBlocks());
        assertEquals(244800, bigBlockFHD30_120.getMaxMacroBlockRate());

        VideoCapabilities.PerformancePoint bigRotBlockFHD30_120 =
            new VideoCapabilities.PerformancePoint(1920, 1080, 30, 120, new Size(64, 128));
        assertEquals(120, bigRotBlockFHD30_120.getMaxFrameRate());
        assertEquals(8640, bigRotBlockFHD30_120.getMaxMacroBlocks());
        assertEquals(259200, bigRotBlockFHD30_120.getMaxMacroBlockRate());

        /* test conversion logic */
        {
            /* 900*900@25-50 */
            VideoCapabilities.PerformancePoint unusual =
                new VideoCapabilities.PerformancePoint(900, 900, 25, 50, new Size(1, 1));
            assertEquals(50, unusual.getMaxFrameRate());
            assertEquals(3249, unusual.getMaxMacroBlocks());
            assertEquals(81225, unusual.getMaxMacroBlockRate());

            /* becomes 960*1024@25-50 */
            VideoCapabilities.PerformancePoint converted1 =
                new VideoCapabilities.PerformancePoint(unusual, new Size(128, 64));
            assertEquals(50, converted1.getMaxFrameRate());
            assertEquals(3840, converted1.getMaxMacroBlocks());
            assertEquals(96000, converted1.getMaxMacroBlockRate());

            /* becomes 1024*960@25-50 */
            VideoCapabilities.PerformancePoint converted2 =
                new VideoCapabilities.PerformancePoint(unusual, new Size(64, 128));
            assertEquals(50, converted2.getMaxFrameRate());
            assertEquals(3840, converted2.getMaxMacroBlocks());
            assertEquals(96000, converted2.getMaxMacroBlockRate());

            /* becomes 1024*1024@25-50 */
            VideoCapabilities.PerformancePoint converted3 =
                new VideoCapabilities.PerformancePoint(converted1, new Size(64, 128));
            assertEquals(50, converted3.getMaxFrameRate());
            assertEquals(4096, converted3.getMaxMacroBlocks());
            assertEquals(102400, converted3.getMaxMacroBlockRate());

            assertEquals(converted1, converted2);
            assertEquals(converted2, converted1);
            assertEquals(converted1, converted3);
            assertEquals(converted3, converted1);
            assertTrue(converted1.covers(converted2));
            assertTrue(converted2.covers(converted1));
            assertTrue(converted2.covers(converted3));
            assertTrue(converted3.covers(converted2));
        }

        // big macroblock size does not impact standard performance points as the dimensions are set
        VideoCapabilities.PerformancePoint bigBlockFHD30 =
            new VideoCapabilities.PerformancePoint(1920, 1080, 30, 30, new Size(128, 64));

        assertTrue(bigBlockFHD30.covers(VideoCapabilities.PerformancePoint.FHD_30));
        assertTrue(VideoCapabilities.PerformancePoint.FHD_30.covers(bigBlockFHD30));
        assertTrue(bigBlockFHD30.equals(VideoCapabilities.PerformancePoint.FHD_30));
        assertTrue(VideoCapabilities.PerformancePoint.FHD_30.equals(bigBlockFHD30));

        // but it impacts the case where dimensions differ
        assertFalse(bigBlockFHD30.covers(new VideoCapabilities.PerformancePoint(1080, 1920, 30)));
        assertFalse(bigBlockFHD30.covers(new VideoCapabilities.PerformancePoint(1936, 1072, 30)));
        assertFalse(bigBlockFHD30.covers(new VideoCapabilities.PerformancePoint(1280, 720, 63)));
        assertTrue(bigBlockFHD30_120.covers(new VideoCapabilities.PerformancePoint(1280, 720, 63)));
        assertFalse(bigBlockFHD30_120.covers(new VideoCapabilities.PerformancePoint(1280, 720, 64)));
        assertTrue(VideoCapabilities.PerformancePoint.FHD_30.covers(
                new VideoCapabilities.PerformancePoint(1080, 1920, 30)));
        assertTrue(VideoCapabilities.PerformancePoint.FHD_30.covers(
                new VideoCapabilities.PerformancePoint(1936, 1072, 30)));
        assertTrue(new VideoCapabilities.PerformancePoint(1920, 1080, 30, 120, new Size(1, 1))
                   .covers(new VideoCapabilities.PerformancePoint(1280, 720, 68)));
    }

    private void verifyPerformancePoints(
            MediaCodecInfo info, String mediaType,
            List<VideoCapabilities.PerformancePoint> points) {
        List<VideoCapabilities.PerformancePoint> standardPoints = Arrays.asList(
                VideoCapabilities.PerformancePoint.UHD_240,
                VideoCapabilities.PerformancePoint.UHD_200,
                VideoCapabilities.PerformancePoint.UHD_120,
                VideoCapabilities.PerformancePoint.UHD_100,
                VideoCapabilities.PerformancePoint.UHD_60,
                VideoCapabilities.PerformancePoint.UHD_50,
                VideoCapabilities.PerformancePoint.UHD_30,
                VideoCapabilities.PerformancePoint.UHD_25,
                VideoCapabilities.PerformancePoint.UHD_24,
                VideoCapabilities.PerformancePoint.FHD_240,
                VideoCapabilities.PerformancePoint.FHD_200,
                VideoCapabilities.PerformancePoint.FHD_120,
                VideoCapabilities.PerformancePoint.FHD_100,
                VideoCapabilities.PerformancePoint.FHD_60,
                VideoCapabilities.PerformancePoint.FHD_50,
                VideoCapabilities.PerformancePoint.FHD_30,
                VideoCapabilities.PerformancePoint.FHD_25,
                VideoCapabilities.PerformancePoint.FHD_24,
                VideoCapabilities.PerformancePoint.HD_240,
                VideoCapabilities.PerformancePoint.HD_200,
                VideoCapabilities.PerformancePoint.HD_120,
                VideoCapabilities.PerformancePoint.HD_100,
                VideoCapabilities.PerformancePoint.HD_60,
                VideoCapabilities.PerformancePoint.HD_50,
                VideoCapabilities.PerformancePoint.HD_30,
                VideoCapabilities.PerformancePoint.HD_25,
                VideoCapabilities.PerformancePoint.HD_24,
                VideoCapabilities.PerformancePoint.SD_60,
                VideoCapabilities.PerformancePoint.SD_50,
                VideoCapabilities.PerformancePoint.SD_48,
                VideoCapabilities.PerformancePoint.SD_30,
                VideoCapabilities.PerformancePoint.SD_25,
                VideoCapabilities.PerformancePoint.SD_24);

        // Components must list all supported standard performance points unless those performance
        // points are covered by other listed standard performance points.
        for (VideoCapabilities.PerformancePoint pp : points) {
            if (standardPoints.contains(pp)) {
                // standard points must not be covered by other listed standard points
                for (VideoCapabilities.PerformancePoint pp2 : points) {
                    if (!standardPoints.contains(pp2)) {
                        continue;
                    }
                    // using object equality to determine otherness
                    assertFalse("standard " + pp2 + " for " + info.getCanonicalName()
                            + " for media type " + mediaType + " covers standard " + pp,
                            pp2 != pp && pp2.covers(pp));
                }
            } else {
                // non-standard points must list all covered standard point not covered by another
                // listed standard point
                for (VideoCapabilities.PerformancePoint spp : standardPoints) {
                    if (pp.covers(spp)) {
                        // Must be either listed or covered by another standard. Since a point
                        // covers itself, it is sufficient to check that it is covered by a listed
                        // standard point.
                        boolean covered = false;
                        for (VideoCapabilities.PerformancePoint pp2 : points) {
                            // using object equality to determine otherness
                            if (standardPoints.contains(pp2) && pp2.covers(spp)) {
                                covered = true;
                                break;
                            }
                        }
                        assertTrue(pp + " for " + info.getCanonicalName() + " for media type "
                                + mediaType + " covers standard " + spp
                                + " that is not covered by a listed standard point",
                                covered);
                    }
                }
                // non-standard points should not be covered by any other performance point
                for (VideoCapabilities.PerformancePoint pp2 : points) {
                    // using object equality to determine otherness
                    assertFalse(pp2 + " for " + info.getCanonicalName()
                            + " for media type " + mediaType + " covers " + pp,
                            pp2 != pp && pp2.covers(pp));
                }
            }
        }
    }

    public void testAllHardwareAcceleratedVideoCodecsPublishPerformancePoints() {
        List<String> mandatoryTypes = Arrays.asList(
                MediaFormat.MIMETYPE_VIDEO_AVC,
                MediaFormat.MIMETYPE_VIDEO_VP8,
                MediaFormat.MIMETYPE_VIDEO_DOLBY_VISION,
                MediaFormat.MIMETYPE_VIDEO_HEVC,
                MediaFormat.MIMETYPE_VIDEO_VP9,
                MediaFormat.MIMETYPE_VIDEO_AV1);

        String[] featuresToConfig = new String[] {
            FEATURE_SecurePlayback,
            FEATURE_TunneledPlayback,
        };

        Set<Pair<String, Integer>> describedTypes = new HashSet<>(); // mediaType - featureIndex
        Set<Pair<String, Integer>> supportedTypes = new HashSet<>(); // mediaType - featureIndex

        // Once any hardware codec performance is described, we assume that all hardware codecs
        // must be described, even if we cannot confirm expanded codec info support.
        boolean hasPerformancePoints = hasExpandedCodecInfo();
        if (!hasPerformancePoints) {
            for (MediaCodecInfo info : mAllInfos) {
                String[] types = info.getSupportedTypes();
                for (int j = 0; j < types.length; ++j) {
                    String mediaType = types[j];
                    CodecCapabilities cap = info.getCapabilitiesForType(mediaType);
                    VideoCapabilities videoCap = cap.getVideoCapabilities();
                    if (videoCap != null
                            && videoCap.getSupportedPerformancePoints() != null) {
                        hasPerformancePoints = true;
                        break;
                    }
                }
                if (hasPerformancePoints) {
                    break;
                }
            }
        }

        for (MediaCodecInfo info : mAllInfos) {
            String[] types = info.getSupportedTypes();
            for (int j = 0; j < types.length; ++j) {
                String mediaType = types[j];
                CodecCapabilities cap = info.getCapabilitiesForType(mediaType);
                MediaFormat defaultFormat = cap.getDefaultFormat();
                VideoCapabilities videoCap = cap.getVideoCapabilities();

                Log.d(TAG, "codec: " + info.getName() + " canonical: " + info.getCanonicalName()
                        + " type: " + mediaType);

                if (videoCap == null) {
                    assertFalse("no video capabilities for video media type " + mediaType + " of "
                                    + info.getName(),
                                mediaType.toLowerCase().startsWith("video/"));
                    continue;
                }

                List<VideoCapabilities.PerformancePoint> pps =
                    videoCap.getSupportedPerformancePoints();

                // see which feature combinations are supported by this codec
                // we do this by counting in binary up to a number of bits
                List<Integer> supportedFeatureConfigs = new ArrayList<Integer>();
                for (int cfg_ix = 1 << featuresToConfig.length; --cfg_ix >= 0; ) {
                    boolean supported = true;
                    for (int f_ix = 0; supported && f_ix < featuresToConfig.length; ++f_ix) {
                        if (((cfg_ix >> f_ix) & 1) != 0) {
                            // feature is to be enabled
                            supported = supported && cap.isFeatureSupported(featuresToConfig[f_ix]);
                        } else {
                            // feature is not to be enabled
                            supported = supported && !cap.isFeatureRequired(featuresToConfig[f_ix]);
                        }
                    }
                    if (supported) {
                        supportedFeatureConfigs.add(cfg_ix);
                    }
                }

                Log.d(TAG, "codec supports configs " + Arrays.toString(
                        supportedFeatureConfigs.stream().mapToInt(Integer::intValue).toArray()));
                boolean isMandatory = mandatoryTypes.contains(mediaType);
                if (info.isHardwareAccelerated()) {
                    for (Integer cfg_ix : supportedFeatureConfigs) {
                        Pair<String, Integer> type = Pair.create(mediaType, cfg_ix);
                        if (hasPerformancePoints && isMandatory) {
                            supportedTypes.add(type);
                        }
                        if (pps != null && pps.size() > 0) {
                            describedTypes.add(type);
                        }
                    }
                }

                if (pps == null) {
                    if (hasExpandedCodecInfo()) {
                        // Hardware-accelerated video components must publish performance points,
                        // even if it is an empty list.
                        assertFalse("HW-accelerated codec '" + info.getName()
                                + "' must publish performance points", info.isHardwareAccelerated());
                    }

                    continue;
                }

                // At least one hardware accelerated codec for each media type (including secure
                // codecs) must publish valid performance points for AVC/VP8/VP9/HEVC/AV1.
                if (pps.size() == 0) {
                    if (isMandatory) {
                        Log.d(TAG, "empty performance points list published by HW accelerated" +
                                   "component " + info.getName() + " for " + types[j]);
                    }
                } else {
                    for (VideoCapabilities.PerformancePoint p : pps) {
                        Log.d(TAG, "got performance point " + p);
                    }
                    verifyPerformancePoints(info, types[j], pps);
                }
            }
        }

        for (Pair<String, Integer> type : supportedTypes) {
            assertTrue("codecs for media type " + type.first + " in configuration " + type.second
                    + " do not have substantial performance point data",
                    describedTypes.contains(type));
        }
    }
}
