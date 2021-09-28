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

package com.android.cts.verifier.audio;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.MediaCodec;
import android.media.MediaCodecList;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaPlayer;
import android.net.Uri;
import android.util.Log;

import com.android.cts.verifier.audio.wavelib.PipeShort;

import java.io.IOException;
import java.nio.ByteBuffer;

public class SoundPlayerObject implements Runnable, AudioTrack.OnPlaybackPositionUpdateListener {
    private static final String LOGTAG = "SoundPlayerObject";

    private static final int TIME_OUT_US = 5000;
    private static final int DEFAULT_BLOCK_SIZE = 4096;

    private MediaPlayer mMediaPlayer;
    private boolean isInitialized = false;
    private boolean isRunning = false;

    private int bufferSize = 65536;
    private PipeShort mDecoderPipe = new PipeShort(bufferSize);
    public PipeShort mPipe = new PipeShort(65536);
    private short[] mAudioShortArray;

    private AudioTrack mAudioTrack;
    private int mSamplingRate = 48000;
    private int mChannelConfigOut = AudioFormat.CHANNEL_OUT_MONO;
    private int mAudioFormat = AudioFormat.ENCODING_PCM_16BIT;
    private int mMinPlayBufferSizeInBytes = 0;
    private int mMinBufferSizeInSamples = 0;

    private int mStreamType = AudioManager.STREAM_MUSIC;
    private int mResId = -1;
    private final boolean mUseMediaPlayer; //true: MediaPlayer, false: AudioTrack
    private float mBalance = 0.5f; //0 left, 1 right

    private Object mLock = new Object();
    private MediaCodec mCodec = null;
    private MediaExtractor mExtractor = null;
    private int mInputAudioFormat;
    private int mInputSampleRate;
    private int mInputChannelCount;

    private boolean mLooping = true;
    private Context mContext = null;

    private final int mBlockSizeSamples;

    private Thread mPlaybackThread;

    public SoundPlayerObject() {
        mUseMediaPlayer = true;
        mBlockSizeSamples = DEFAULT_BLOCK_SIZE;
    }

    public SoundPlayerObject(boolean useMediaPlayer, int blockSize) {
        mUseMediaPlayer = useMediaPlayer;
        mBlockSizeSamples = blockSize;
    }

    public int getCurrentResId() {
        return mResId;
    }

    public int getStreamType () {
        return mStreamType;
    }

    public int getChannelCount() {
        return mInputChannelCount;
    }

    public void run() {
        isRunning = true;
        int decodeRounds = 0;
        MediaCodec.BufferInfo buffInfo = new MediaCodec.BufferInfo();

        while (isRunning) {
            if (Thread.interrupted()) {
                log("got thread interrupted!");
                isRunning = false;
                return;
            }

            if (mUseMediaPlayer) {
                log("run . using media player");
                try {
                    Thread.sleep(10);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            } else {
                if (isInitialized && !outputEosSignalled) {

                    synchronized (mLock) {

                        int bytesPerSample = getBytesPerSample(mInputAudioFormat);

                        int valuesAvailable = mDecoderPipe.availableToRead();
                        if (valuesAvailable > 0 && mAudioTrack != null) {
                            int valuesOfInterest = valuesAvailable;
                            if (mMinBufferSizeInSamples < valuesOfInterest) {
                                valuesOfInterest = mMinBufferSizeInSamples;
                            }
                            mDecoderPipe.read(mAudioShortArray, 0, valuesOfInterest);
                            //inject into output.
                            mAudioTrack.write(mAudioShortArray, 0, valuesOfInterest);

                            //delay
                            int delayMs = (int)(1000.0f * valuesOfInterest /
                                    (float)(mInputSampleRate * bytesPerSample *
                                            mInputChannelCount));
                            delayMs = Math.max(2, delayMs);

                            try {
                                Thread.sleep(delayMs);
                            } catch (InterruptedException e) {
                                e.printStackTrace();
                            }
                        } else {
                            //read another block if possible

                            decodeRounds++;
                            if (!inputEosSignalled) {
                                final int inputBufIndex = mCodec.dequeueInputBuffer(TIME_OUT_US);
                                if (inputBufIndex >= 0) {
                                    ByteBuffer encodedBuf = mCodec.getInputBuffer(inputBufIndex);
                                    final int sampleSize =
                                            mExtractor.readSampleData(encodedBuf, 0 /* offset */);
                                    long presentationTimeUs = 0;
                                    if (sampleSize < 0) {
                                        inputEosSignalled = true;
                                        log("[v] input EOS at decode round " + decodeRounds);

                                    } else {
                                        presentationTimeUs = mExtractor.getSampleTime();
                                    }
                                    mCodec.queueInputBuffer(inputBufIndex, 0/*offset*/,
                                            inputEosSignalled ? 0 : sampleSize, presentationTimeUs,
                                            (inputEosSignalled && !mLooping) ?
                                                    MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0);
                                    if (!inputEosSignalled) {
                                        mExtractor.advance();
                                    }

                                    if (inputEosSignalled && mLooping) {
                                        log("looping is enabled. Rewinding");
                                        mExtractor.seekTo(0, MediaExtractor.SEEK_TO_PREVIOUS_SYNC);
                                        inputEosSignalled = false;
                                    }
                                } else {
                                    log("[v] no input buffer available at decode round "
                                            + decodeRounds);
                                }
                            } //!inputEosSignalled

                            final int outputRes = mCodec.dequeueOutputBuffer(buffInfo, TIME_OUT_US);

                            if (outputRes >= 0) {
                                if (buffInfo.size > 0) {
                                    final int outputBufIndex = outputRes;
                                    final ByteBuffer decodedBuf =
                                            mCodec.getOutputBuffer(outputBufIndex);

                                    short sValue = 0; //scaled to 16 bits
                                    int index = 0;
                                    for (int i = 0; i < buffInfo.size && index <
                                            mAudioShortArray.length; i += bytesPerSample) {
                                        switch (mInputAudioFormat) {
                                            case AudioFormat.ENCODING_PCM_FLOAT:
                                                sValue = (short) (decodedBuf.getFloat(i) * 32768.0);
                                                break;
                                            case AudioFormat.ENCODING_PCM_16BIT:
                                                sValue = (short) decodedBuf.getShort(i);
                                                break;
                                            case AudioFormat.ENCODING_PCM_8BIT:
                                                sValue = (short) ((decodedBuf.getChar(i) - 128) *
                                                        128);
                                                break;
                                        }
                                        mAudioShortArray[index] = sValue;
                                        index++;
                                    }
                                    mDecoderPipe.write(mAudioShortArray, 0, index);
                                    mPipe.write(mAudioShortArray, 0, index);

                                    mCodec.getOutputBuffer(outputBufIndex).position(0);
                                    mCodec.releaseOutputBuffer(outputBufIndex,
                                            false/*render to surface*/);
                                    if ((buffInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) !=
                                            0) {
                                        outputEosSignalled = true;
                                    }
                                }
                            } else if (outputRes == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                                log("[w] INFO_OUTPUT_FORMAT_CHANGED at decode round " +
                                        decodeRounds);
                                decodeRounds = 0;
                            } else if (outputRes == MediaCodec.INFO_TRY_AGAIN_LATER) {
                                log("[w] INFO_TRY_AGAIN_LATER at decode round " + decodeRounds);
                                if (!mLooping) {
                                    outputEosSignalled = true; //quit!
                                }
                            }
                        }
                    }

                    if (outputEosSignalled) {
                        log ("note: outputEosSignalled");
                    }
                }
                else {
                    try {
                        Thread.sleep(10);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
        }
        log("done running thread");
    }

    public void setBalance(float balance) {
        mBalance = balance;
        if (mUseMediaPlayer) {
            if (mMediaPlayer != null) {
                float left = Math.min(2.0f * (1.0f - mBalance), 1.0f);
                float right = Math.min(2.0f * mBalance, 1.0f);
                mMediaPlayer.setVolume(left, right);
                log(String.format("Setting balance to %f", mBalance));
            }
        }
    }

    public void setStreamType(int streamType) {
        mStreamType = streamType;
    }

    public void rewind() {
        if (mUseMediaPlayer) {
            if (mMediaPlayer != null) {
                mMediaPlayer.seekTo(0);
            }
        }
    }

    boolean inputEosSignalled = false;
    boolean outputEosSignalled = false;

    public void setSoundWithResId(Context context, int resId) {

        setSoundWithResId(context, resId, true);
    }

    public void setSoundWithResId(Context context, int resId, boolean looping) {
        log("setSoundWithResId " + resId + ", looping: " + looping);
        mLooping = looping;
        mContext = context;
        if (mContext == null) {
            log("Context can't be null");
            return;
        }

        boolean playing = isPlaying();
        if (playing) {
            play(false);
        }
        //release player
        releasePlayer();
        isInitialized = false;

        log("loading uri: " + resId);
        mResId = resId;
        Uri uri = Uri.parse("android.resource://com.android.cts.verifier/" + resId);
        if (mUseMediaPlayer) {
            mMediaPlayer = new MediaPlayer();
            try {
                log("opening resource with stream type: " + mStreamType);
                mMediaPlayer.setAudioStreamType(mStreamType);
                mMediaPlayer.setDataSource(mContext.getApplicationContext(),
                        uri);
                mMediaPlayer.prepare();
            } catch (IOException e) {
                e.printStackTrace();
                log("Error: " + e.toString());
            }
            mMediaPlayer.setLooping(mLooping);
            setBalance(mBalance);
            isInitialized = true;

        } else {
            synchronized (mLock) {
                //TODO: encapsulate MediaPlayer and AudioTrack related code into separate classes
                // with common interface. Simplify locking code.
                mDecoderPipe.flush();
                mPipe.flush();

                if (mCodec != null)
                    mCodec = null;
                try {
                    mExtractor = new MediaExtractor();
                    mExtractor.setDataSource(mContext.getApplicationContext(), uri, null);
                    final int trackCount = mExtractor.getTrackCount();

//                    log("Track count: " + trackCount);
                    // find first audio track
                    MediaFormat format = null;
                    String mime = null;
                    for (int i = 0; i < trackCount; i++) {
                        format = mExtractor.getTrackFormat(i);
                        mime = format.getString(MediaFormat.KEY_MIME);
                        if (mime.startsWith("audio/")) {
                            mExtractor.selectTrack(i);
//                            log("found track " + i + " with MIME = " + mime);
                            break;
                        }
                    }
                    if (format == null) {
                        log("found 0 audio tracks in " + uri);
                    }
                    MediaCodecList mclist = new MediaCodecList(MediaCodecList.ALL_CODECS);

                    String myDecoderName = mclist.findDecoderForFormat(format);
//                    log("my decoder name = " + myDecoderName);

                    mCodec = MediaCodec.createByCodecName(myDecoderName);

//                    log("[ok] about to configure codec with " + format);
                    mCodec.configure(format, null /* surface */, null /* crypto */, 0 /* flags */);

                    // prepare decoding
                    mCodec.start();

                    inputEosSignalled = false;
                    outputEosSignalled = false;

                    MediaFormat outputFormat = format;

                    printAudioFormat(outputFormat);

                    //int sampleRate
                    mInputSampleRate = getMediaFormatInteger(outputFormat,
                            MediaFormat.KEY_SAMPLE_RATE, 48000);
                    //int channelCount
                    mInputChannelCount = getMediaFormatInteger(outputFormat,
                            MediaFormat.KEY_CHANNEL_COUNT, 1);

                    mInputAudioFormat = getMediaFormatInteger(outputFormat,
                            MediaFormat.KEY_PCM_ENCODING,
                            AudioFormat.ENCODING_PCM_16BIT);

                    int channelMask = channelMaskFromCount(mInputChannelCount);
                    int buffSize = AudioTrack.getMinBufferSize(mInputSampleRate, channelMask,
                            mInputAudioFormat);

                    AudioAttributes.Builder aab = new AudioAttributes.Builder()
                            .setLegacyStreamType(mStreamType);

                    AudioFormat.Builder afb = new AudioFormat.Builder()
                            .setEncoding(mInputAudioFormat)
                            .setSampleRate(mInputSampleRate)
                            .setChannelMask(channelMask);

                    AudioTrack.Builder atb = new AudioTrack.Builder()
                            .setAudioAttributes(aab.build())
                            .setAudioFormat(afb.build())
                            .setBufferSizeInBytes(buffSize)
                            .setTransferMode(AudioTrack.MODE_STREAM);

                    mAudioTrack = atb.build();

                    mMinPlayBufferSizeInBytes = AudioTrack.getMinBufferSize(mInputSampleRate,
                            mChannelConfigOut, mInputAudioFormat);

                    mMinBufferSizeInSamples = mMinPlayBufferSizeInBytes / 2;
                    mAudioShortArray = new short[mMinBufferSizeInSamples * 100];
                    mAudioTrack.setPlaybackPositionUpdateListener(this);
                    mAudioTrack.setPositionNotificationPeriod(mBlockSizeSamples);
                    isInitialized = true;
                } catch (IOException e) {
                    e.printStackTrace();
                    log("Error creating codec or extractor: " + e.toString());
                }
            }
        } //mLock

//        log("done preparing media player");
        if (playing)
            play(true); //start playing if it was playing before
    }

    public boolean isPlaying() {
        boolean result = false;
        if (mUseMediaPlayer) {
            if (mMediaPlayer != null) {
                result = mMediaPlayer.isPlaying();
            }
        } else {
            synchronized (mLock) {
                if (mAudioTrack != null) {
                    result = mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING;
                }
            }
        }
        return result;
    }

    public boolean isAlive() {
        if (mUseMediaPlayer) {
            return true;
        }

        synchronized (mLock) {
            if (mPlaybackThread != null) {
                return mPlaybackThread.isAlive();
            }
        }
        return false;
    }

    public void start() {
        if (!mUseMediaPlayer) {
            synchronized (mLock) {
                if (mPlaybackThread == null) {
                    mPlaybackThread = new Thread(this);
                    mPlaybackThread.setName("playbackThread");
                    log("Created playback thread " + mPlaybackThread);
                }

                if (!mPlaybackThread.isAlive()) {
                    mPlaybackThread.start();
                    mPlaybackThread.setPriority(Thread.MAX_PRIORITY);
                    log("Started playback thread " + mPlaybackThread);
                }
            }
        }
    }

    public void play(boolean play) {
        if (mUseMediaPlayer) {
            if (mMediaPlayer != null) {
                if (play) {
                    mMediaPlayer.start();
                } else {
                    mMediaPlayer.pause();
                }
            }
        } else {
            synchronized (mLock) {
                log(" called Play : " + play);
                if (mAudioTrack != null && isInitialized) {
                    if (play) {
                        log("Play");
                        mDecoderPipe.flush();
                        mPipe.flush();
                        mAudioTrack.play();
                    } else {
                        log("pause");
                        mAudioTrack.pause();
                        isRunning = false;
                    }
                }
            }
            start();
        }
    }

    public void finish() {
        play(false);
        releasePlayer();
    }

    private void releasePlayer() {
        if (mMediaPlayer != null) {
            mMediaPlayer.stop();
            mMediaPlayer.release();
            mMediaPlayer = null;
        }

        isRunning = false;
        synchronized (mLock) {
            if (mAudioTrack != null) {
                mAudioTrack.stop();
                mAudioTrack.setPlaybackPositionUpdateListener(null);
                mAudioTrack.release();
                mAudioTrack = null;
            }
        }

        log("Deleting playback thread " + mPlaybackThread);
        Thread zeThread = mPlaybackThread;
        mPlaybackThread = null;

        if (zeThread != null) {
            log("terminating zeThread...");
            zeThread.interrupt();
            try {
                log("zeThread join...");
                zeThread.join();
            } catch (InterruptedException e) {
                log("issue deleting playback thread " + e.toString());
                zeThread.interrupt();
            }
        }
        isInitialized = false;
        log("Done deleting thread");

    }

    /*
       Misc
    */
    private static void log(String msg) {
        Log.v(LOGTAG, msg);
    }

    private final int getMediaFormatInteger(MediaFormat mf, String name, int defaultValue) {
        try {
            return mf.getInteger(name);
        } catch (NullPointerException e) {
            log("Warning: MediaFormat " + name +
                " field does not exist. Using default " + defaultValue); /* no such field */
        } catch (ClassCastException e) {
            log("Warning: MediaFormat " + name +
                    " field unexpected type"); /* field of different type */
        }
        return defaultValue;
    }

    public static int getBytesPerSample(int audioFormat) {
        switch(audioFormat) {
            case AudioFormat.ENCODING_PCM_16BIT:
                return 2;
            case AudioFormat.ENCODING_PCM_8BIT:
                return 1;
            case AudioFormat.ENCODING_PCM_FLOAT:
                return 4;
        }
        return 0;
    }

    private void printAudioFormat(MediaFormat format) {
        try {
            log("channel mask = " + format.getInteger(MediaFormat.KEY_CHANNEL_MASK));
        } catch (NullPointerException npe) {
            log("channel mask unknown");
        }
        try {
            log("channel count = " + format.getInteger(MediaFormat.KEY_CHANNEL_COUNT));
        } catch (NullPointerException npe) {
            log("channel count unknown");
        }
        try {
            log("sample rate = " + format.getInteger(MediaFormat.KEY_SAMPLE_RATE));
        } catch (NullPointerException npe) {
            log("sample rate unknown");
        }
        try {
            log("sample format = " + format.getInteger(MediaFormat.KEY_PCM_ENCODING));
        } catch (NullPointerException npe) {
            log("sample format unknown");
        }
    }

    public static int channelMaskFromCount(int channelCount) {
        switch(channelCount) {
            case 1:
                return AudioFormat.CHANNEL_OUT_MONO;
            case 2:
                return AudioFormat.CHANNEL_OUT_STEREO;
            case 3:
                return AudioFormat.CHANNEL_OUT_STEREO | AudioFormat.CHANNEL_OUT_FRONT_CENTER;
            case 4:
                return AudioFormat.CHANNEL_OUT_FRONT_LEFT |
                        AudioFormat.CHANNEL_OUT_FRONT_RIGHT | AudioFormat.CHANNEL_OUT_BACK_LEFT |
                        AudioFormat.CHANNEL_OUT_BACK_RIGHT;
            case 5:
                return AudioFormat.CHANNEL_OUT_FRONT_LEFT |
                        AudioFormat.CHANNEL_OUT_FRONT_RIGHT | AudioFormat.CHANNEL_OUT_BACK_LEFT |
                        AudioFormat.CHANNEL_OUT_BACK_RIGHT
                        | AudioFormat.CHANNEL_OUT_FRONT_CENTER;
            case 6:
                return AudioFormat.CHANNEL_OUT_5POINT1;
            default:
                return 0;
        }
    }

    public void periodicNotification(AudioTrack track) {
    }

    public void markerReached(AudioTrack track) {
    }

    @Override
    public void onMarkerReached(AudioTrack track) {
        markerReached(track);
    }

    @Override
    public void onPeriodicNotification(AudioTrack track) {
        periodicNotification(track);
    }
}
