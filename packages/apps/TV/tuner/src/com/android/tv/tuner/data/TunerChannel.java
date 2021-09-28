/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv.tuner.data;

import android.database.Cursor;
import android.support.annotation.NonNull;
import android.util.Log;
import com.android.tv.common.util.StringUtils;
import com.android.tv.tuner.data.Channel.DeliverySystemType;
import com.android.tv.tuner.data.Channel.TunerChannelProto;
import com.android.tv.tuner.data.Track.AtscAudioTrack;
import com.android.tv.tuner.data.Track.AtscCaptionTrack;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/** A class that represents a single channel accessible through a tuner. */
public class TunerChannel implements Comparable<TunerChannel>, PsipData.TvTracksInterface {
    private static final String TAG = "TunerChannel";

    /** Channel number separator between major number and minor number. */
    public static final char CHANNEL_NUMBER_SEPARATOR = '-';

    // See ATSC Code Points Registry.
    private static final String[] ATSC_SERVICE_TYPE_NAMES =
            new String[] {
                "ATSC Reserved",
                "Analog television channels",
                "ATSC_digital_television",
                "ATSC_audio",
                "ATSC_data_only_service",
                "Software Download",
                "Unassociated/Small Screen Service",
                "Parameterized Service",
                "ATSC NRT Service",
                "Extended Parameterized Service"
            };
    private static final String ATSC_SERVICE_TYPE_NAME_RESERVED =
            ATSC_SERVICE_TYPE_NAMES[Channel.AtscServiceType.SERVICE_TYPE_ATSC_RESERVED_VALUE];

    public static final int INVALID_FREQUENCY = -1;

    // According to RFC4259, The number of available PIDs ranges from 0 to 8191.
    public static final int INVALID_PID = -1;

    // According to ISO13818-1, Mpeg2 StreamType has a range from 0x00 to 0xff.
    public static final int INVALID_STREAMTYPE = -1;

    // @GuardedBy(this) Writing operations and toByteArray will be guarded. b/34197766
    private TunerChannelProto mProto;

    private TunerChannel(
            PsipData.VctItem channel,
            int programNumber,
            List<PsiData.PmtItem> pmtItems,
            Channel.TunerType type) {
        String shortName = "";
        String longName = "";
        String description = "";
        int tsid = 0;
        int virtualMajor = 0;
        int virtualMinor = 0;
        Channel.AtscServiceType serviceType =
                Channel.AtscServiceType.SERVICE_TYPE_ATSC_DIGITAL_TELEVISION;
        if (channel != null) {
            shortName = channel.getShortName();
            tsid = channel.getChannelTsid();
            programNumber = channel.getProgramNumber();
            virtualMajor = channel.getMajorChannelNumber();
            virtualMinor = channel.getMinorChannelNumber();
            Channel.AtscServiceType chanServiceType =
                    Channel.AtscServiceType.forNumber(channel.getServiceType());
            if (chanServiceType != null) {
                serviceType = chanServiceType;
            }
            longName = (channel.getLongName() != null ? channel.getLongName() : longName);
            description =
                    (channel.getDescription() != null ? channel.getDescription() : description);
        }
        TunerChannelProto tunerChannelProto =
                TunerChannelProto.newBuilder()
                        .setShortName(shortName)
                        .setTsid(tsid)
                        .setProgramNumber(programNumber)
                        .setVirtualMajor(virtualMajor)
                        .setVirtualMinor(virtualMinor)
                        .setServiceType(serviceType)
                        .setLongName(longName)
                        .setDescription(description)
                        .build();
        initProto(pmtItems, type, tunerChannelProto);
    }

    private void initProto(
            List<PsiData.PmtItem> pmtItems,
            Channel.TunerType type,
            TunerChannelProto tunerChannelProto) {
        int videoPid = INVALID_PID;
        int pcrPid = 0;
        Channel.VideoStreamType videoStreamType = Channel.VideoStreamType.UNSET;
        List<Integer> audioPids = new ArrayList<>();
        List<Channel.AudioStreamType> audioStreamTypes = new ArrayList<>();
        for (PsiData.PmtItem pmt : pmtItems) {
            switch (pmt.getStreamType()) {
                    // MPEG ES stream video types
                case Channel.VideoStreamType.MPEG1_VALUE:
                case Channel.VideoStreamType.MPEG2_VALUE:
                case Channel.VideoStreamType.H263_VALUE:
                case Channel.VideoStreamType.H264_VALUE:
                case Channel.VideoStreamType.H265_VALUE:
                    videoPid = pmt.getEsPid();
                    videoStreamType = Channel.VideoStreamType.forNumber(pmt.getStreamType());
                    break;

                    // MPEG ES stream audio types
                case Channel.AudioStreamType.MPEG1AUDIO_VALUE:
                case Channel.AudioStreamType.MPEG2AUDIO_VALUE:
                case Channel.AudioStreamType.MPEG2AACAUDIO_VALUE:
                case Channel.AudioStreamType.MPEG4LATMAACAUDIO_VALUE:
                case Channel.AudioStreamType.A52AC3AUDIO_VALUE:
                case Channel.AudioStreamType.EAC3AUDIO_VALUE:
                    audioPids.add(pmt.getEsPid());
                    audioStreamTypes.add(Channel.AudioStreamType.forNumber(pmt.getStreamType()));
                    break;

                    // Non MPEG ES stream types
                case 0x100: // PmtItem.ES_PID_PCR:
                    pcrPid = pmt.getEsPid();
                    break;
                default:
                    // fall out
            }
        }
        mProto =
                TunerChannelProto.newBuilder(tunerChannelProto)
                        .setType(type)
                        .setChannelId(-1L)
                        .setFrequency(INVALID_FREQUENCY)
                        .setVideoPid(videoPid)
                        .setVideoStreamType(videoStreamType)
                        .addAllAudioPids(audioPids)
                        .setAudioTrackIndex(audioPids.isEmpty() ? -1 : 0)
                        .addAllAudioStreamTypes(audioStreamTypes)
                        .setPcrPid(pcrPid)
                        .build();
    }

    // b/141952456 may cause hasAudioTrackIndex and hasVideo to return false
    private void initChannelInfoIfNeeded() {
        if (!mProto.hasAudioTrackIndex()) {
            // Force select audio track 0
            selectAudioTrack(0);
        }
        if (!mProto.hasVideoPid() && mProto.getVideoPid() != INVALID_PID) {
            // Force set Video PID
            setVideoPid(mProto.getVideoPid());
        }
    }

    private TunerChannel(
            int programNumber,
            Channel.TunerType type,
            PsipData.SdtItem channel,
            List<PsiData.PmtItem> pmtItems) {
        String shortName = "";
        Channel.AtscServiceType serviceType =
                Channel.AtscServiceType.SERVICE_TYPE_ATSC_DIGITAL_TELEVISION;
        if (channel != null) {
            shortName = channel.getServiceName();
            programNumber = channel.getServiceId();
            Channel.AtscServiceType chanServiceType =
                    Channel.AtscServiceType.forNumber(channel.getServiceType());
            if (chanServiceType != null) {
                serviceType = chanServiceType;
            }
        }
        TunerChannelProto tunerChannelProto =
                TunerChannelProto.newBuilder()
                        .setShortName(shortName)
                        .setProgramNumber(programNumber)
                        .setServiceType(serviceType)
                        .build();
        initProto(pmtItems, type, tunerChannelProto);
    }

    /** Initialize tuner channel with VCT items and PMT items. */
    public TunerChannel(PsipData.VctItem channel, List<PsiData.PmtItem> pmtItems) {
        this(channel, 0, pmtItems, Channel.TunerType.TYPE_TUNER);
    }

    /** Initialize tuner channel with program number and PMT items. */
    public TunerChannel(int programNumber, List<PsiData.PmtItem> pmtItems) {
        this(null, programNumber, pmtItems, Channel.TunerType.TYPE_TUNER);
    }

    /** Initialize tuner channel with SDT items and PMT items. */
    public TunerChannel(PsipData.SdtItem channel, List<PsiData.PmtItem> pmtItems) {
        this(0, Channel.TunerType.TYPE_TUNER, channel, pmtItems);
    }

    private TunerChannel(TunerChannelProto tunerChannelProto) {
        mProto = tunerChannelProto;
        initChannelInfoIfNeeded();
    }

    public static TunerChannel forFile(PsipData.VctItem channel, List<PsiData.PmtItem> pmtItems) {
        return new TunerChannel(channel, 0, pmtItems, Channel.TunerType.TYPE_FILE);
    }

    public static TunerChannel forDvbFile(
            PsipData.SdtItem channel, List<PsiData.PmtItem> pmtItems) {
        return new TunerChannel(0, Channel.TunerType.TYPE_FILE, channel, pmtItems);
    }

    /**
     * Create a TunerChannel object suitable for network tuners
     *
     * @param major Channel number major
     * @param minor Channel number minor
     * @param programNumber Program number
     * @param shortName Short name
     * @param recordingProhibited Recording prohibition info
     * @param videoFormat Video format. Should be {@code null} or one of the followings: {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_240P}, {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_360P}, {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_480I}, {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_480P}, {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_576I}, {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_576P}, {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_720P}, {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_1080I}, {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_1080P}, {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_2160P}, {@link
     *     android.media.tv.TvContract.Channels#VIDEO_FORMAT_4320P}
     * @return a TunerChannel object
     */
    public static TunerChannel forNetwork(
            int major,
            int minor,
            int programNumber,
            String shortName,
            boolean recordingProhibited,
            String videoFormat) {
        TunerChannel tunerChannel =
                new TunerChannel(
                        null,
                        programNumber,
                        Collections.EMPTY_LIST,
                        Channel.TunerType.TYPE_NETWORK);
        tunerChannel.setVirtualMajor(major);
        tunerChannel.setVirtualMinor(minor);
        tunerChannel.setShortName(shortName);
        // Set audio and video pids in order to work around the audio-only channel check.
        tunerChannel.setAudioPids(new ArrayList<>(Arrays.asList(0)));
        tunerChannel.selectAudioTrack(0);
        tunerChannel.setVideoPid(0);
        tunerChannel.setRecordingProhibited(recordingProhibited);
        if (videoFormat != null) {
            tunerChannel.setVideoFormat(videoFormat);
        }
        return tunerChannel;
    }

    public String getName() {
        return !mProto.getShortName().isEmpty() ? mProto.getShortName() : mProto.getLongName();
    }

    public String getShortName() {
        return mProto.getShortName();
    }

    public int getProgramNumber() {
        return mProto.getProgramNumber();
    }

    public int getServiceType() {
        return mProto.getServiceType().getNumber();
    }

    public String getServiceTypeName() {
        int serviceType = getServiceType();
        if (serviceType >= 0 && serviceType < ATSC_SERVICE_TYPE_NAMES.length) {
            return ATSC_SERVICE_TYPE_NAMES[serviceType];
        }
        return ATSC_SERVICE_TYPE_NAME_RESERVED;
    }

    public int getVirtualMajor() {
        return mProto.getVirtualMajor();
    }

    public int getVirtualMinor() {
        return mProto.getVirtualMinor();
    }

    public DeliverySystemType getDeliverySystemType() {
        return mProto.getDeliverySystemType();
    }

    public int getFrequency() {
        return mProto.getFrequency();
    }

    public String getModulation() {
        return mProto.getModulation();
    }

    public int getTsid() {
        return mProto.getTsid();
    }

    public int getVideoPid() {
        return mProto.getVideoPid();
    }

    public synchronized void setVideoPid(int videoPid) {
        mProto = mProto.toBuilder().setVideoPid(videoPid).build();
    }

    public int getVideoStreamType() {
        return mProto.getVideoStreamType().getNumber();
    }

    public int getAudioPid() {
        if (!mProto.hasAudioTrackIndex() || mProto.getAudioTrackIndex() == -1) {
            return INVALID_PID;
        }
        return mProto.getAudioPids(mProto.getAudioTrackIndex());
    }

    public int getAudioStreamType() {
        if (!mProto.hasAudioTrackIndex() || mProto.getAudioTrackIndex() == -1) {
            return INVALID_STREAMTYPE;
        }
        return mProto.getAudioStreamTypes(mProto.getAudioTrackIndex()).getNumber();
    }

    public List<Integer> getAudioPids() {
        return mProto.getAudioPidsList();
    }

    public synchronized void setAudioPids(List<Integer> audioPids) {
        mProto = mProto.toBuilder().clearAudioPids().addAllAudioPids(audioPids).build();
    }

    public List<Integer> getAudioStreamTypes() {
        List<Channel.AudioStreamType> audioStreamTypes = mProto.getAudioStreamTypesList();
        List<Integer> audioStreamTypesValues = new ArrayList<>(audioStreamTypes.size());

        for (Channel.AudioStreamType audioStreamType : audioStreamTypes) {
            audioStreamTypesValues.add(audioStreamType.getNumber());
        }
        return audioStreamTypesValues;
    }

    public synchronized void setAudioStreamTypes(List<Integer> audioStreamTypesValues) {
        List<Channel.AudioStreamType> audioStreamTypes =
                new ArrayList<>(audioStreamTypesValues.size());

        for (Integer audioStreamTypesValue : audioStreamTypesValues) {
            audioStreamTypes.add(Channel.AudioStreamType.forNumber(audioStreamTypesValue));
        }
        mProto =
                mProto.toBuilder()
                        .clearAudioStreamTypes()
                        .addAllAudioStreamTypes(audioStreamTypes)
                        .build();
    }

    public int getPcrPid() {
        return mProto.getPcrPid();
    }

    public Channel.TunerType getType() {
        return mProto.getType();
    }

    public synchronized void setFilepath(String filepath) {
        mProto = mProto.toBuilder().setFilepath(filepath == null ? "" : filepath).build();
    }

    public String getFilepath() {
        return mProto.getFilepath();
    }

    public synchronized void setVirtualMajor(int virtualMajor) {
        mProto = mProto.toBuilder().setVirtualMajor(virtualMajor).build();
    }

    public synchronized void setVirtualMinor(int virtualMinor) {
        mProto = mProto.toBuilder().setVirtualMinor(virtualMinor).build();
    }

    public synchronized void setShortName(String shortName) {
        mProto = mProto.toBuilder().setShortName(shortName == null ? "" : shortName).build();
    }

    public synchronized void setDeliverySystemType(DeliverySystemType deliverySystemType) {
        mProto = mProto.toBuilder().setDeliverySystemType(deliverySystemType).build();
    }

    public synchronized void setFrequency(int frequency) {
        mProto = mProto.toBuilder().setFrequency(frequency).build();
    }

    public synchronized void setModulation(String modulation) {
        mProto = mProto.toBuilder().setModulation(modulation == null ? "" : modulation).build();
    }

    public boolean hasVideo() {
        return mProto.hasVideoPid() && mProto.getVideoPid() != INVALID_PID;
    }

    public boolean hasAudio() {
        return getAudioPid() != INVALID_PID;
    }

    public long getChannelId() {
        return mProto.getChannelId();
    }

    public synchronized void setChannelId(long channelId) {
        mProto = mProto.toBuilder().setChannelId(channelId).build();
    }

    /**
     * The flag indicating whether this TV channel is locked or not. This is primarily used for
     * alternative parental control to prevent unauthorized users from watching the current channel
     * regardless of the content rating
     *
     * @see <a
     *     href="https://developer.android.com/reference/android/media/tv/TvContract.Channels.html#COLUMN_LOCKED">link</a>
     */
    public boolean isLocked() {
        return mProto.getLocked();
    }

    public synchronized void setLocked(boolean locked) {
        mProto = mProto.toBuilder().setLocked(locked).build();
    }

    public String getDisplayNumber() {
        return getDisplayNumber(true);
    }

    public String getDisplayNumber(boolean ignoreZeroMinorNumber) {
        if (getVirtualMajor() != 0 && (getVirtualMinor() != 0 || !ignoreZeroMinorNumber)) {
            return String.format(
                    "%d%c%d", getVirtualMajor(), CHANNEL_NUMBER_SEPARATOR, getVirtualMinor());
        } else if (getVirtualMajor() != 0) {
            return Integer.toString(getVirtualMajor());
        } else {
            return Integer.toString(getProgramNumber());
        }
    }

    public String getDescription() {
        return mProto.getDescription();
    }

    @Override
    public synchronized void setHasCaptionTrack() {
        mProto = mProto.toBuilder().setHasCaptionTrack(true).build();
    }

    @Override
    public boolean hasCaptionTrack() {
        return mProto.getHasCaptionTrack();
    }

    @Override
    public List<AtscAudioTrack> getAudioTracks() {
        return mProto.getAudioTracksList();
    }

    public synchronized void setAudioTracks(List<AtscAudioTrack> audioTracks) {
        mProto = mProto.toBuilder().clearAudioTracks().addAllAudioTracks(audioTracks).build();
    }

    @Override
    public List<AtscCaptionTrack> getCaptionTracks() {
        return mProto.getCaptionTracksList();
    }

    public synchronized void setCaptionTracks(List<AtscCaptionTrack> captionTracks) {
        mProto = mProto.toBuilder().clearCaptionTracks().addAllCaptionTracks(captionTracks).build();
    }

    public synchronized void selectAudioTrack(int index) {
        if (index < 0 || index >= mProto.getAudioPidsCount()) {
            index = -1;
        }
        mProto = mProto.toBuilder().setAudioTrackIndex(index).build();
    }

    public synchronized void setRecordingProhibited(boolean recordingProhibited) {
        mProto = mProto.toBuilder().setRecordingProhibited(recordingProhibited).build();
    }

    public boolean isRecordingProhibited() {
        return mProto.getRecordingProhibited();
    }

    public synchronized void setVideoFormat(String videoFormat) {
        mProto = mProto.toBuilder().setVideoFormat(videoFormat == null ? "" : videoFormat).build();
    }

    public String getVideoFormat() {
        return mProto.getVideoFormat();
    }

    @Override
    public String toString() {
        switch (getType()) {
            case TYPE_FILE:
                return String.format(
                        "{%d-%d %s} Filepath: %s, ProgramNumber %d",
                        getVirtualMajor(),
                        getVirtualMinor(),
                        getShortName(),
                        getFilepath(),
                        getProgramNumber());
                // case Channel.TunerType.TYPE_TUNER:
            default:
                return String.format(
                        "{%d-%d %s} Frequency: %d, ProgramNumber %d",
                        getVirtualMajor(),
                        getVirtualMinor(),
                        getShortName(),
                        getFrequency(),
                        getProgramNumber());
        }
    }

    @Override
    public int compareTo(@NonNull TunerChannel channel) {
        // In the same frequency, the program number acts as the sub-channel number.
        int ret = getFrequency() - channel.getFrequency();
        if (ret != 0) {
            return ret;
        }
        ret = getProgramNumber() - channel.getProgramNumber();
        if (ret != 0) {
            return ret;
        }
        ret = StringUtils.compare(getName(), channel.getName());
        if (ret != 0) {
            return ret;
        }
        if (getDeliverySystemType() != channel.getDeliverySystemType()) {
            return 1;
        }
        // For FileTsStreamer, file paths should be compared.
        return StringUtils.compare(getFilepath(), channel.getFilepath());
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof TunerChannel)) {
            return false;
        }
        return compareTo((TunerChannel) o) == 0;
    }

    @Override
    public int hashCode() {
        return Objects.hash(getDeliverySystemType(), getFrequency(), getProgramNumber(), getName(),
                getFilepath());
    }

    // Serialization
    public synchronized byte[] toByteArray() {
        try {
            return mProto.toByteArray();
        } catch (Exception e) {
            // Retry toByteArray. b/34197766
            Log.w(
                    TAG,
                    "TunerChannel or its variables are modified in multiple thread without lock",
                    e);
            return mProto.toByteArray();
        }
    }

    public static TunerChannel parseFrom(byte[] data) {
        if (data == null) {
            return null;
        }
        try {
            return new TunerChannel(TunerChannelProto.parseFrom(data));
        } catch (IOException e) {
            Log.e(TAG, "Could not parse from byte array", e);
            return null;
        }
    }

    public static TunerChannel fromCursor(Cursor cursor) {
        long channelId = cursor.getLong(0);
        boolean locked = cursor.getInt(1) > 0;
        byte[] data = cursor.getBlob(2);
        TunerChannel channel = TunerChannel.parseFrom(data);
        if (channel != null) {
            channel.setChannelId(channelId);
            channel.setLocked(locked);
        }
        return channel;
    }
}
