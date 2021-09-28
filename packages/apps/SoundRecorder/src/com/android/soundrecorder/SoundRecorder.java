/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.soundrecorder;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Date;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Intent;
import android.content.Context;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.database.Cursor;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.media.MediaRecorder;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.PowerManager;
import android.os.StatFs;
import android.os.PowerManager.WakeLock;
import android.provider.MediaStore;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toast;

/**
 * Calculates remaining recording time based on available disk space and
 * optionally a maximum recording file size.
 * <p>
 * The reason why this is not trivial is that the file grows in blocks
 * every few seconds or so, while we want a smooth countdown.
 */

class RemainingTimeCalculator {
    public static final int UNKNOWN_LIMIT = 0;
    public static final int FILE_SIZE_LIMIT = 1;
    public static final int DISK_SPACE_LIMIT = 2;

    // which of the two limits we will hit (or have fit) first
    private int mCurrentLowerLimit = UNKNOWN_LIMIT;

    private File mSDCardDirectory;

    // State for tracking file size of recording.
    private File mRecordingFile;
    private long mMaxBytes;

    // Rate at which the file grows
    private int mBytesPerSecond;

    // time at which number of free blocks last changed
    private long mBlocksChangedTime;
    // number of available blocks at that time
    private long mLastBlocks;

    // time at which the size of the file has last changed
    private long mFileSizeChangedTime;
    // size of the file at that time
    private long mLastFileSize;

    public RemainingTimeCalculator() {
        mSDCardDirectory = Environment.getExternalStorageDirectory();
    }

    /**
     * If called, the calculator will return the minimum of two estimates:
     * how long until we run out of disk space and how long until the file
     * reaches the specified size.
     *
     * @param file     the file to watch
     * @param maxBytes the limit
     */

    public void setFileSizeLimit(File file, long maxBytes) {
        mRecordingFile = file;
        mMaxBytes = maxBytes;
    }

    /**
     * Resets the interpolation.
     */
    public void reset() {
        mCurrentLowerLimit = UNKNOWN_LIMIT;
        mBlocksChangedTime = -1;
        mFileSizeChangedTime = -1;
    }

    /**
     * Returns how long (in seconds) we can continue recording.
     */
    public long timeRemaining() {
        // Calculate how long we can record based on free disk space

        StatFs fs = new StatFs(mSDCardDirectory.getAbsolutePath());
        long blocks = fs.getAvailableBlocks();
        long blockSize = fs.getBlockSize();
        long now = System.currentTimeMillis();

        if (mBlocksChangedTime == -1 || blocks != mLastBlocks) {
            mBlocksChangedTime = now;
            mLastBlocks = blocks;
        }

        /* The calculation below always leaves one free block, since free space
           in the block we're currently writing to is not added. This
           last block might get nibbled when we close and flush the file, but
           we won't run out of disk. */

        // at mBlocksChangedTime we had this much time
        long result = mLastBlocks * blockSize / mBytesPerSecond;
        // so now we have this much time
        result -= (now - mBlocksChangedTime) / 1000;

        if (mRecordingFile == null) {
            mCurrentLowerLimit = DISK_SPACE_LIMIT;
            return result;
        }

        // If we have a recording file set, we calculate a second estimate
        // based on how long it will take us to reach mMaxBytes.

        mRecordingFile = new File(mRecordingFile.getAbsolutePath());
        long fileSize = mRecordingFile.length();
        if (mFileSizeChangedTime == -1 || fileSize != mLastFileSize) {
            mFileSizeChangedTime = now;
            mLastFileSize = fileSize;
        }

        long result2 = (mMaxBytes - fileSize) / mBytesPerSecond;
        result2 -= (now - mFileSizeChangedTime) / 1000;
        result2 -= 1; // just for safety

        mCurrentLowerLimit = result < result2
                ? DISK_SPACE_LIMIT : FILE_SIZE_LIMIT;

        return Math.min(result, result2);
    }

    /**
     * Indicates which limit we will hit (or have hit) first, by returning one
     * of FILE_SIZE_LIMIT or DISK_SPACE_LIMIT or UNKNOWN_LIMIT. We need this to
     * display the correct message to the user when we hit one of the limits.
     */
    public int currentLowerLimit() {
        return mCurrentLowerLimit;
    }

    /**
     * Is there any point of trying to start recording?
     */
    public boolean diskSpaceAvailable() {
        StatFs fs = new StatFs(mSDCardDirectory.getAbsolutePath());
        // keep one free block
        return fs.getAvailableBlocks() > 1;
    }

    /**
     * Sets the bit rate used in the interpolation.
     *
     * @param bitRate the bit rate to set in bits/sec.
     */
    public void setBitRate(int bitRate) {
        mBytesPerSecond = bitRate / 8;
    }
}

public class SoundRecorder extends Activity
        implements Button.OnClickListener, Recorder.OnStateChangedListener {
    static final String TAG = "SoundRecorder";
    static final String STATE_FILE_NAME = "soundrecorder.state";
    static final String RECORDER_STATE_KEY = "recorder_state";
    static final String SAMPLE_INTERRUPTED_KEY = "sample_interrupted";
    static final String MAX_FILE_SIZE_KEY = "max_file_size";

    static final String AUDIO_3GPP = "audio/3gpp";
    static final String AUDIO_MPEG4 = "audio/mpeg";
    static final String AUDIO_AMR = "audio/amr";
    static final String AUDIO_ANY = "audio/*";
    static final String ANY_ANY = "*/*";
    static final String AUDIO_AWB = "audio/awb";
    static final String AUDIO_AAC = "audio/aac";

    static final int BITRATE_AMR = 5900; // bits/sec

    static final int BITRATE_MPEG4 = 128000;
    static final int BITRATE_3GPP = 12200;
    static final int BITRATE_AWB = 96024;
    static final int BITRATE_AAC = 128000;


    static final int SAMPLE_RATE_AAC = 48000;
    static final int SAMPLE_RATE_AWB = 32000;
    static final int SAMPLE_RATE_AMR = 8000;

    private static final int BIT_RATE = 8;

    static final String SOUND_RECORDER_DATA = "sound_recorder_data";

    private boolean mRunFromLauncher = true;

    WakeLock mWakeLock;
    String mRequestedType = AUDIO_ANY;
    Recorder mRecorder;
    boolean mSampleInterrupted = false;
    String mErrorUiMessage = null; // Some error messages are displayed in the UI,
    // not a dialog. This happens when a recording
    // is interrupted for some reason.

    long mMaxFileSize = -1;        // can be specified in the intent
    RemainingTimeCalculator mRemainingTimeCalculator;

    String mTimerFormat;
    final Handler mHandler = new Handler();
    Runnable mUpdateTimer = new Runnable() {
        public void run() {
            updateTimerView();
        }
    };

    ImageButton mRecordButton;
    ImageButton mPlayButton;
    ImageButton mStopButton;
    ImageButton mFileListButton;

    ImageView mStateLED;
    TextView mStateMessage1;
    TextView mStateMessage2;
    ProgressBar mStateProgressBar;
    TextView mTimerView;

    LinearLayout mExitButtons;
    Button mAcceptButton;
    Button mDiscardButton;
    VUMeter mVUMeter;
    private BroadcastReceiver mSDCardMountEventReceiver = null;

    private Menu mMenu;
    private static final int OPTIONMENU_SELECT_FORMAT = 0;
    // add the recording mode menu
    private static final int OPTIONMENU_SELECT_MODE = 1;

    private static final int CHANNEL_CHOOSE_DIALOG = 1;
    private static final int MODE_CHOOSE_DIALOG = 4;

    private SharedPreferences mPrefs;

    private boolean HAVE_AACENCODE_FEATURE = true;
    private boolean HAVE_VORBISENC_FEATURE = false;
    private boolean HAVE_AWBENCODE_FEATURE = true;
    private boolean AUDIO_HD_REC_SUPPORT = false;

    static final int HIGH = 0;
    static final int MID = 1;
    static final int LOW = 2;
    static final int NORMAL = 0;
    static final int INDOOR = 1;
    static final int OUTDOOR = 2;
    private int mSelectedFormat = -1;// current recording format:high=0/mid/low
    private int mSelectedMode = -1;

    static final String SELECTED_RECORDING_FORMAT = "selected_recording_format";
    // add recording mode **
    static final String SELECTED_RECORDING_MODE = "selected_recording_mode";
    static final String BYTE_RATE = "byte_rate";

    private String mDoWhat = null;
    public static final String DOWHAT = "dowhat";
    public static final String PLAY = "play";
    public static final String RECORD = "record";
    private static final String PATH = "path";
    private static final String DURATION = "duration";
    private static final int REQURST_FILE_LIST = 1;

    private int mByteRate;

    @Override
    public void onCreate(Bundle icycle) {
        super.onCreate(icycle);
        Intent i = getIntent();
        if (i != null) {
            String s = i.getType();
            //mRunFromLauncher= i.getAction().equals("android.intent.action.MAIN");
            mRunFromLauncher = "android.intent.action.MAIN".equals(i.getAction());
            if (AUDIO_AAC.equals(s) || AUDIO_AMR.equals(s) || AUDIO_3GPP.equals(s) || AUDIO_ANY.equals(s)
                    || ANY_ANY.equals(s) || AUDIO_MPEG4.equals(s)) {
                mRequestedType = s;
            } else if (s != null) {
                // we only support amr and 3gpp formats right now
                setResult(RESULT_CANCELED);
                finish();
                return;
            }

            final String EXTRA_MAX_BYTES
                    = MediaStore.Audio.Media.EXTRA_MAX_BYTES;
            mMaxFileSize = i.getLongExtra(EXTRA_MAX_BYTES, -1);
        }

        if (CheckPermissionActivity.jump2PermissionActivity(this, i != null ? i.getAction() : null)) {
            finish();
            return;
        }

        if (AUDIO_ANY.equals(mRequestedType) || ANY_ANY.equals(mRequestedType)) {
            mRequestedType = AUDIO_MPEG4;
        }

        setContentView(R.layout.main);

        mRecorder = new Recorder();
        mRecorder.setOnStateChangedListener(this);
        mRemainingTimeCalculator = new RemainingTimeCalculator();

        PowerManager pm
                = (PowerManager) getSystemService(Context.POWER_SERVICE);
        mWakeLock = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK,
                "SoundRecorder");

        initResourceRefs();

        setResult(RESULT_CANCELED);
        registerExternalStorageListener();
        if (icycle != null) {
            Bundle recorderState = icycle.getBundle(RECORDER_STATE_KEY);
            if (recorderState != null) {
                mRecorder.restoreState(recorderState);
                mSampleInterrupted = recorderState.getBoolean(SAMPLE_INTERRUPTED_KEY, false);
                mMaxFileSize = recorderState.getLong(MAX_FILE_SIZE_KEY, -1);

                if (0 != mByteRate) {
                    mRemainingTimeCalculator.setBitRate(mByteRate * BIT_RATE);
                }
            }
        }

        if (null == mPrefs) {
            mPrefs = getSharedPreferences(SOUND_RECORDER_DATA, 0);
        }
        mSelectedFormat = mPrefs.getInt(SELECTED_RECORDING_FORMAT, HIGH);
        mSelectedMode = mPrefs.getInt(SELECTED_RECORDING_MODE, NORMAL);
        mByteRate = mPrefs.getInt(BYTE_RATE, 0);

        updateUi();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        setContentView(R.layout.main);
        initResourceRefs();
        updateUi();
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        if (mRecorder.sampleLength() == 0)
            return;

        Bundle recorderState = new Bundle();

        mRecorder.saveState(recorderState);
        recorderState.putBoolean(SAMPLE_INTERRUPTED_KEY, mSampleInterrupted);
        recorderState.putLong(MAX_FILE_SIZE_KEY, mMaxFileSize);

        outState.putBundle(RECORDER_STATE_KEY, recorderState);
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        super.onPrepareOptionsMenu(menu);
        mMenu = menu;
        if (!mRunFromLauncher
                || mRecorder.state() == Recorder.RECORDING_STATE
                || mRecorder.state() == Recorder.PLAYING_STATE
        ) {
            menu.getItem(OPTIONMENU_SELECT_FORMAT).setEnabled(false);
            if (AUDIO_HD_REC_SUPPORT) {
                menu.getItem(OPTIONMENU_SELECT_MODE).setVisible(false);
            }
        } else {
            menu.getItem(OPTIONMENU_SELECT_FORMAT).setEnabled(true);
            if (AUDIO_HD_REC_SUPPORT) {
                menu.getItem(OPTIONMENU_SELECT_MODE).setVisible(true);
            }
        }
        return true;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        if (HAVE_AACENCODE_FEATURE || HAVE_AWBENCODE_FEATURE
                || HAVE_VORBISENC_FEATURE) {
            menu.add(0, OPTIONMENU_SELECT_FORMAT, 0,
                    getString(R.string.voice_quality));
            if (AUDIO_HD_REC_SUPPORT) {
                menu.add(0, OPTIONMENU_SELECT_MODE, 0,
                        getString(R.string.recording_mode));
            }
        }

        Log.i(TAG, "onCreateOptionsMenu");

        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (OPTIONMENU_SELECT_FORMAT == id) {
            showDialog(CHANNEL_CHOOSE_DIALOG);
        } else if (OPTIONMENU_SELECT_MODE == id) {
            showDialog(MODE_CHOOSE_DIALOG);
        }
        return true;
    }

    @Override
    protected Dialog onCreateDialog(int id, Bundle args) {
        AlertDialog chooseDialog;
        switch (id) {
            case CHANNEL_CHOOSE_DIALOG:
                chooseDialog = dlgChooseChannel();
                break;
            case MODE_CHOOSE_DIALOG:
                chooseDialog = modeChooseDialog();
                break;
            default:
                chooseDialog = null;
                break;
        }
        return chooseDialog;
    }

    protected AlertDialog dlgChooseChannel() {
        CharSequence[] encodeFormatArray = null;

        if ((HAVE_AACENCODE_FEATURE) || (HAVE_VORBISENC_FEATURE)) {
            if (HAVE_AWBENCODE_FEATURE) {
                encodeFormatArray = new CharSequence[3];
                if (HAVE_VORBISENC_FEATURE) {
                    encodeFormatArray[0] = getString(R.string.recording_format_high)
                            + "(.ogg)";
                } else if (HAVE_AACENCODE_FEATURE) {
                    encodeFormatArray[0] = getString(R.string.recording_format_high)
                            + "(.m4a)";
                }
                // encodeFormatArray[0] =
                // getString(R.string.recording_format_high)+"(.3gpp)";
                encodeFormatArray[1] = getString(R.string.recording_format_mid)
                        + "(.m4a)";
                encodeFormatArray[2] = getString(R.string.recording_format_low)
                        + "(.amr)";
            } else {
                encodeFormatArray = new CharSequence[2];
                if (HAVE_VORBISENC_FEATURE) {
                    encodeFormatArray[0] = getString(R.string.recording_format_high)
                            + "(.ogg)";
                } else if (HAVE_AACENCODE_FEATURE) {
                    encodeFormatArray[0] = getString(R.string.recording_format_high)
                            + "(.m4a)";
                }
                // encodeFormatArray[0] =
                // getString(R.string.recording_format_high)+"(.3gpp)";
                encodeFormatArray[1] = getString(R.string.recording_format_low)
                        + "(.amr)";
            }
        } else if (HAVE_AWBENCODE_FEATURE) {
            encodeFormatArray = new CharSequence[2];
            if (HAVE_VORBISENC_FEATURE) {
                encodeFormatArray[0] = getString(R.string.recording_format_high)
                        + "(.ogg)";
            } else if (HAVE_AACENCODE_FEATURE) {
                encodeFormatArray[0] = getString(R.string.recording_format_high)
                        + "(.m4a)";
            }

            encodeFormatArray[1] = getString(R.string.recording_format_low)
                    + "(.amr)";
        } else {
            Log.e(TAG, "No featureOption enable");
        }

        // Resources mResources = this.getResources();
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.select_voice_quality)
                .setSingleChoiceItems(encodeFormatArray, mSelectedFormat,
                        new DialogInterface.OnClickListener() {

                            public void onClick(DialogInterface dialog,
                                                int which) {
                                switch (which) {
                                    case 0:
                                        if ((HAVE_AACENCODE_FEATURE)
                                                || (HAVE_VORBISENC_FEATURE)) {
                                            mSelectedFormat = HIGH;
                                        } else if (HAVE_AWBENCODE_FEATURE) {
                                            mSelectedFormat = MID;// mid
                                        } else {
                                            Log.e(TAG,
                                                    "No featureOption enable");
                                        }
                                        break;

                                    case 1:
                                        if ((HAVE_AACENCODE_FEATURE)
                                                || (HAVE_VORBISENC_FEATURE)) {
                                            if (HAVE_AWBENCODE_FEATURE) {
                                                mSelectedFormat = MID;
                                            } else {
                                                mSelectedFormat = LOW;
                                            }
                                        } else if (HAVE_AWBENCODE_FEATURE) {
                                            mSelectedFormat = LOW;
                                        } else {
                                            Log.e(TAG,
                                                    "No featureOption enable");
                                        }
                                        break;

                                    case 2:
                                        mSelectedFormat = LOW;// low
                                        break;

                                    default:
                                        break;
                                }
                                dialog.dismiss();
                            }
                        }).setNegativeButton(getString(R.string.cancel), null);

        return builder.create();
    }

    protected AlertDialog modeChooseDialog() {
        CharSequence[] modeArray = null;
        modeArray = new CharSequence[3];
        modeArray[0] = getString(R.string.recording_mode_nomal);
        modeArray[1] = getString(R.string.recording_mode_meeting);
        modeArray[2] = getString(R.string.recording_mode_lecture);

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(getString(R.string.select_recording_mode))
                .setSingleChoiceItems(modeArray, mSelectedMode,
                        new DialogInterface.OnClickListener() {

                            public void onClick(DialogInterface dialog,
                                                int which) {
                                switch (which) {
                                    case 0:
                                        mSelectedMode = NORMAL;
                                        break;
                                    case 1:
                                        mSelectedMode = INDOOR;
                                        break;
                                    case 2:
                                        mSelectedMode = OUTDOOR;
                                        break;
                                    default:
                                        break;
                                }
                                dialog.dismiss();
                            }
                        }).setNegativeButton(getString(R.string.cancel), null);

        return builder.create();
    }


    /*
     * Whenever the UI is re-created (due f.ex. to orientation change) we have
     * to reinitialize references to the views.
     */
    private void initResourceRefs() {
        mRecordButton = (ImageButton) findViewById(R.id.recordButton);
        mPlayButton = (ImageButton) findViewById(R.id.playButton);
        mStopButton = (ImageButton) findViewById(R.id.stopButton);
        mFileListButton = (ImageButton) findViewById(R.id.fileListButton);

        if (mRunFromLauncher) {
            mFileListButton.setOnClickListener(this);
        } else {
            mFileListButton.setVisibility(View.GONE);
        }

        mStateLED = (ImageView) findViewById(R.id.stateLED);
        mStateMessage1 = (TextView) findViewById(R.id.stateMessage1);
        mStateMessage2 = (TextView) findViewById(R.id.stateMessage2);
        mStateProgressBar = (ProgressBar) findViewById(R.id.stateProgressBar);
        mTimerView = (TextView) findViewById(R.id.timerView);

        mExitButtons = (LinearLayout) findViewById(R.id.exitButtons);
        mAcceptButton = (Button) findViewById(R.id.acceptButton);
        mDiscardButton = (Button) findViewById(R.id.discardButton);
        mVUMeter = (VUMeter) findViewById(R.id.uvMeter);


        mRecordButton.setOnClickListener(this);
        mPlayButton.setOnClickListener(this);
        mStopButton.setOnClickListener(this);
        mAcceptButton.setOnClickListener(this);
        mDiscardButton.setOnClickListener(this);

        mTimerFormat = getResources().getString(R.string.timer_format);

        mVUMeter.setRecorder(mRecorder);
    }

    /*
     * Make sure we're not recording music playing in the background, ask
     * the MediaPlaybackService to pause playback.
     */
    private void stopAudioPlayback() {
        AudioManager am = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        am.requestAudioFocus(null, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
    }

    private void record() {
        if (!(mDoWhat != null && mDoWhat.equals(PLAY))) {
            mRecorder.delete();
        }
        mDoWhat = null;
        mRecorder.mSampleFile = null;
        if (mMenu != null) {
            mMenu.close();
        }
        mRemainingTimeCalculator.reset();
        if (!Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
            mSampleInterrupted = true;
            mErrorUiMessage = getResources().getString(R.string.insert_sd_card);
            updateUi();
        } else if (!mRemainingTimeCalculator.diskSpaceAvailable()) {
            mSampleInterrupted = true;
            mErrorUiMessage = getResources().getString(R.string.storage_is_full);
            updateUi();
        } else {
            stopAudioPlayback();

            // set the recording mode
            if (AUDIO_HD_REC_SUPPORT) {
                switch (mSelectedMode) {
                    case NORMAL:
                        mRecorder.setAudioSamplingRate(SAMPLE_RATE_AMR);
                        break;
                    case INDOOR:
                        mRecorder.setAudioSamplingRate(SAMPLE_RATE_AWB);
                        break;
                    case OUTDOOR:
                        mRecorder.setAudioSamplingRate(SAMPLE_RATE_AAC);
                        break;
                    default:
                        break;
                }
            }
            int outputFileFormat = MediaRecorder.OutputFormat.AMR_NB;
            int recordingType = MediaRecorder.AudioEncoder.AMR_NB;
            String extension = ".amr";

            if (AUDIO_AMR.equals(mRequestedType)) {
                mRemainingTimeCalculator.setBitRate(BITRATE_AMR);
                outputFileFormat = MediaRecorder.OutputFormat.AMR_NB;
                recordingType = MediaRecorder.AudioEncoder.AMR_NB;
                extension = ".amr";
                //mRecorder.startRecording(MediaRecorder.OutputFormat.AMR_NB,MediaRecorder.AudioEncoder.AMR_NB, ".amr", this);
            } else if (AUDIO_AWB.equals(mRequestedType)) {
                mRemainingTimeCalculator.setBitRate(BITRATE_AWB);
                outputFileFormat = MediaRecorder.OutputFormat.THREE_GPP;
                recordingType = MID;
                extension = ".awb";
            } else if (AUDIO_AAC.equals(mRequestedType)) {
                mRemainingTimeCalculator.setBitRate(BITRATE_AAC);
                outputFileFormat = MediaRecorder.OutputFormat.AAC_ADTS;
                recordingType = MediaRecorder.AudioEncoder.AAC;
                extension = ".aac";
            } else if (AUDIO_3GPP.equals(mRequestedType)) {
                mRemainingTimeCalculator.setBitRate(BITRATE_AAC);
                outputFileFormat = MediaRecorder.OutputFormat.THREE_GPP;
                recordingType = MediaRecorder.AudioEncoder.AAC;
                extension = ".3gpp";
            } else if (AUDIO_MPEG4.equals(mRequestedType)) {
                mRemainingTimeCalculator.setBitRate(BITRATE_MPEG4);
                switch (mSelectedFormat) {
                    case HIGH:
                        if (HAVE_AACENCODE_FEATURE) {
                            mRemainingTimeCalculator.setBitRate(BITRATE_AAC);
                            outputFileFormat = MediaRecorder.OutputFormat.MPEG_4;
                            recordingType = MediaRecorder.AudioEncoder.AAC;
                            extension = ".m4a";
                        }
                        break;

                    case MID:
                        mRemainingTimeCalculator.setBitRate(BITRATE_AWB);
                        outputFileFormat = MediaRecorder.OutputFormat.MPEG_4;
                        recordingType = MediaRecorder.AudioEncoder.AMR_WB;
                        extension = ".m4a";
                        break;

                    case LOW:
                        mRemainingTimeCalculator.setBitRate(BITRATE_AMR);
                        outputFileFormat = MediaRecorder.OutputFormat.AMR_NB;
                        recordingType = MediaRecorder.AudioEncoder.AMR_NB;
                        extension = ".amr";
                        break;

                    default:
                        break;
                }
            } else {
                throw new IllegalArgumentException("Invalid output file type requested");
            }


            mRecorder.startRecording(outputFileFormat, recordingType,
                    extension, this);

            if (mMaxFileSize != -1) {
                mRemainingTimeCalculator.setFileSizeLimit(
                        mRecorder.sampleFile(), mMaxFileSize);
            }
        }
    }

    /*
     * Handle the buttons.
     */
    public void onClick(View button) {
        if (!button.isEnabled())
            return;

        switch (button.getId()) {
            case R.id.recordButton:
                record();
                break;
            case R.id.playButton:
                mRecorder.startPlayback();
                break;
            case R.id.stopButton:
                mRecorder.stop();
                break;
            case R.id.acceptButton:
                mRecorder.stop();
                saveSample();

                if (!mRunFromLauncher) {
                    finish();
                }
                break;
            case R.id.fileListButton:
                mFileListButton.setEnabled(false);
                //leave this activity,set mSampleFile is null
                if ((mRecorder != null) && mRecorder.sampleFile() != null) {
                    mRecorder.mSampleFile = null;
                    mRecorder.mSampleLength = 0;
                }
                Intent mIntent = new Intent();
                mIntent.setClass(this, RecordingFileList.class);
                startActivityForResult(mIntent, REQURST_FILE_LIST);
                break;

            case R.id.discardButton:
                mRecorder.delete();
                if (!mRunFromLauncher) {
                    finish();
                }
                break;
        }
    }

    /*
     * Handle the "back" hardware key.
     */
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            switch (mRecorder.state()) {
                case Recorder.IDLE_STATE:
                    if (mRecorder.sampleLength() > 0) {
                        if (!(mDoWhat != null && mDoWhat.equals(PLAY))) {
                            saveSample();
                        }
                    }
                    finish();
                    break;
                case Recorder.PLAYING_STATE:
                    mRecorder.stop();
                    if (mRecorder.mSampleLength > 0 && mDoWhat == null) {
                        saveSample();
                    }
                    break;
                case Recorder.RECORDING_STATE:
                    mRecorder.stop();
                    break;
            }
            return true;
        } else {
            return super.onKeyDown(keyCode, event);
        }
    }

    @Override
    public void onStop() {
        mRecorder.stop();
        super.onStop();
    }

    @Override
    protected void onPause() {
        mSampleInterrupted = mRecorder.state() == Recorder.RECORDING_STATE;
        mRecorder.stop();

        if (null == mPrefs) {
            mPrefs = getSharedPreferences(SOUND_RECORDER_DATA, 0);
        }
        SharedPreferences.Editor ed = mPrefs.edit();
        ed.putInt(SELECTED_RECORDING_FORMAT, mSelectedFormat);
        ed.putInt(SELECTED_RECORDING_MODE, mSelectedMode);
        ed.commit();

        super.onPause();
    }

    /*
     * If we have just recorded a smaple, this adds it to the media data base
     * and sets the result to the sample's URI.
     */
    private void saveSample() {
        if (mRecorder.sampleLength() == 0)
            return;
        Uri uri = null;
        try {
            mRecorder.sampleFileDelSuffix();
            uri = this.addToMediaDB(mRecorder.sampleFile());
        } catch (UnsupportedOperationException ex) {  // Database manipulation failure
            return;
        }
        if (uri == null) {
            return;
        }
        Intent intent = new Intent();
        intent.setData(uri);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        setResult(RESULT_OK, intent);

        if (mRunFromLauncher) {
            /*new AlertDialog.Builder(this)
                    .setTitle(R.string.app_name)
                    .setMessage(R.string.success_mediadb_new_record)
                    .setPositiveButton(R.string.button_ok, null)
                    .setCancelable(false)
                    .show();*/
            Toast.makeText(SoundRecorder.this,
                    R.string.success_mediadb_new_record, Toast.LENGTH_SHORT)
                    .show();
            mExitButtons.setVisibility(View.INVISIBLE);
            //onCreate(null);
            mRecorder = new Recorder();
            mRecorder.setOnStateChangedListener(this);
            mRemainingTimeCalculator = new RemainingTimeCalculator();
            mDoWhat = null;
            mVUMeter.setRecorder(mRecorder);
            updateUi();
        }
    }

    /*
     * Called on destroy to unregister the SD card mount event receiver.
     */
    @Override
    public void onDestroy() {
        if (mSDCardMountEventReceiver != null) {
            unregisterReceiver(mSDCardMountEventReceiver);
            mSDCardMountEventReceiver = null;
        }
        super.onDestroy();
    }

    /*
     * Registers an intent to listen for ACTION_MEDIA_EJECT/ACTION_MEDIA_MOUNTED
     * notifications.
     */
    private void registerExternalStorageListener() {
        if (mSDCardMountEventReceiver == null) {
            mSDCardMountEventReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    String action = intent.getAction();
                    if (action.equals(Intent.ACTION_MEDIA_EJECT)) {
                        mRecorder.delete();
                    } else if (action.equals(Intent.ACTION_MEDIA_MOUNTED)) {
                        mSampleInterrupted = false;
                        updateUi();
                    }
                }
            };
            IntentFilter iFilter = new IntentFilter();
            iFilter.addAction(Intent.ACTION_MEDIA_EJECT);
            iFilter.addAction(Intent.ACTION_MEDIA_MOUNTED);
            iFilter.addDataScheme("file");
            registerReceiver(mSDCardMountEventReceiver, iFilter);
        }
    }

    /*
     * A simple utility to do a query into the databases.
     */
    private Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        try {
            ContentResolver resolver = getContentResolver();
            if (resolver == null) {
                return null;
            }
            return resolver.query(uri, projection, selection, selectionArgs, sortOrder);
        } catch (UnsupportedOperationException ex) {
            return null;
        }
    }

    /*
     * Add the given audioId to the playlist with the given playlistId; and maintain the
     * play_order in the playlist.
     */
    private void addToPlaylist(ContentResolver resolver, int audioId, long playlistId) {
        try {
            String[] cols = new String[]{
                    "_id"
            };
            Uri uri = MediaStore.Audio.Playlists.Members.getContentUri("external", playlistId);
            Cursor cur = resolver.query(uri, cols, null, null, null);
            cur.moveToFirst();
            final int base = cur.getCount();//cur.getInt(0);
            cur.close();
            ContentValues values = new ContentValues();
            //values.put(MediaStore.Audio.Playlists.Members.PLAY_ORDER, Integer.valueOf(base + audioId));
            values.put(MediaStore.Audio.Playlists.Members.AUDIO_ID, audioId);
            Log.d(TAG, "base="+base + ", insert=" + uri + ", values="+values);
            resolver.insert(uri, values);
        } catch (IllegalArgumentException ex) {
            Log.e(TAG, "Catch IllegalArgumentException: Invalid column count(*)");
        }
    }

    /*
     * Obtain the id for the default play list from the audio_playlists table.
     */
    private int getPlaylistId(Resources res) {
        Uri uri = MediaStore.Audio.Playlists.getContentUri("external");
        final String[] ids = new String[]{MediaStore.Audio.Playlists._ID};
        final String where = MediaStore.Audio.Playlists.NAME + "=?";
        final String[] args = new String[]{res.getString(R.string.audio_db_playlist_name)};
        Cursor cursor = query(uri, ids, where, args, null);
        if (cursor == null) {
            Log.v(TAG, "query returns null");
        }
        int id = -1;
        if (cursor != null) {
            cursor.moveToFirst();
            if (!cursor.isAfterLast()) {
                id = cursor.getInt(0);
            }
        }
        cursor.close();
        return id;
    }

    /*
     * Create a playlist with the given default playlist name, if no such playlist exists.
     */
    private Uri createPlaylist(Resources res, ContentResolver resolver) {
        ContentValues cv = new ContentValues();
        cv.put(MediaStore.Audio.Playlists.NAME, res.getString(R.string.audio_db_playlist_name));
        Uri uri = resolver.insert(MediaStore.Audio.Playlists.getContentUri("external"), cv);
        if (uri == null) {
            new AlertDialog.Builder(this)
                    .setTitle(R.string.app_name)
                    .setMessage(R.string.error_mediadb_new_record)
                    .setPositiveButton(R.string.button_ok, null)
                    .setCancelable(false)
                    .show();
        }
        return uri;
    }

    /*
     * Adds file and returns content uri.
     */
    private Uri addToMediaDB(File file) {
        Resources res = getResources();
        ContentValues cv = new ContentValues();
        long current = System.currentTimeMillis();
        Log.d("ljh", "----------------------------------");
        Log.d("ljh", "current=" + current);
        Log.d("ljh", "----------------------------------");
        long modDate = file.lastModified();
        Date date = new Date(current);
        SimpleDateFormat formatter = new SimpleDateFormat(
                res.getString(R.string.audio_db_title_format));
        String title = formatter.format(date);
        Log.d("ljh", "----------------------------------");
        Log.d("ljh", "title=" + title + ",file.getAbsolutePath()=" + file.getAbsolutePath());
        Log.d("ljh", "----------------------------------");
        long sampleLengthMillis = 0;
        try {
            MediaPlayer mediaPlayer = new MediaPlayer();
            mediaPlayer.reset();
            mediaPlayer.setDataSource(file.getAbsolutePath());
            mediaPlayer.prepare();
            sampleLengthMillis = mediaPlayer.getDuration();
            mediaPlayer.release();
        } catch (Exception e) {
            Log.e(TAG, "get record duration happen error");
            e.printStackTrace();
        }
        if (0 == sampleLengthMillis) {
            sampleLengthMillis = mRecorder.sampleLength() * 1000L;
        }

        // Lets label the recorded audio file as NON-MUSIC so that the file
        // won't be displayed automatically, except for in the playlist.
        cv.put(MediaStore.Audio.Media.IS_MUSIC, "0");

        cv.put(MediaStore.Audio.Media.TITLE, title);
        cv.put(MediaStore.Audio.Media.DATA, file.getAbsolutePath());
        cv.put(MediaStore.Audio.Media.DATE_ADDED, (int) (current / 1000));
        cv.put(MediaStore.Audio.Media.DATE_MODIFIED, (int) (modDate / 1000));
        cv.put(MediaStore.Audio.Media.DURATION, sampleLengthMillis);
        if (AUDIO_MPEG4 == mRequestedType && LOW == mSelectedFormat) {
            cv.put(MediaStore.Audio.Media.MIME_TYPE, AUDIO_AMR);
        } else {
            cv.put(MediaStore.Audio.Media.MIME_TYPE, mRequestedType);
        }
        cv.put(MediaStore.Audio.Media.ARTIST,
                res.getString(R.string.audio_db_artist_name));
        cv.put(MediaStore.Audio.Media.ALBUM,
                res.getString(R.string.audio_db_album_name));
        Log.d(TAG, "Inserting audio record: " + cv.toString());
        ContentResolver resolver = getContentResolver();
        Uri base = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
        Log.d(TAG, "ContentURI: " + base);
        Uri result = resolver.insert(base, cv);
        if (result == null) {
            new AlertDialog.Builder(this)
                    .setTitle(R.string.app_name)
                    .setMessage(R.string.error_mediadb_new_record)
                    .setPositiveButton(R.string.button_ok, null)
                    .setCancelable(false)
                    .show();
            return null;
        }
        /*int playlistId = getPlaylistId(res);
        Log.d(TAG, "playlistId="+playlistId);
        if (playlistId == -1) {
            createPlaylist(res, resolver);
        }
        int audioId = Integer.valueOf(result.getLastPathSegment());
        addToPlaylist(resolver, audioId, getPlaylistId(res));*/

        // Notify those applications such as Music listening to the
        // scanner events that a recorded audio file just created.
        sendBroadcast(new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, result));
        return result;
    }

    /**
     * Update the big MM:SS timer. If we are in playback, also update the
     * progress bar.
     */
    private void updateTimerView() {
        Resources res = getResources();
        int state = mRecorder.state();

        boolean ongoing = state == Recorder.RECORDING_STATE || state == Recorder.PLAYING_STATE;

        long time = ongoing ? mRecorder.progress() : mRecorder.sampleLength();
        String timeStr = String.format(mTimerFormat, time / 60, time % 60);
        mTimerView.setText(timeStr);

        if (state == Recorder.PLAYING_STATE) {
            mStateProgressBar.setProgress((int) (100 * time / mRecorder.sampleLength()));
        } else if (state == Recorder.RECORDING_STATE) {
            updateTimeRemaining();
        }

        if (ongoing)
            mHandler.postDelayed(mUpdateTimer, 1000);
    }

    /*
     * Called when we're in recording state. Find out how much longer we can
     * go on recording. If it's under 5 minutes, we display a count-down in
     * the UI. If we've run out of time, stop the recording.
     */
    private void updateTimeRemaining() {
        long t = mRemainingTimeCalculator.timeRemaining();

        if (t <= 10) {
            mSampleInterrupted = true;
            int limit = mRemainingTimeCalculator.currentLowerLimit();
            if (limit == RemainingTimeCalculator.DISK_SPACE_LIMIT) {
                mErrorUiMessage = getResources().getString(R.string.storage_is_full);
            }
            mRecorder.stop();
            return;
        }

        if (t <= 0) {
            mSampleInterrupted = true;

            int limit = mRemainingTimeCalculator.currentLowerLimit();
            switch (limit) {
                /*case RemainingTimeCalculator.DISK_SPACE_LIMIT:
                    mErrorUiMessage
                            = getResources().getString(R.string.storage_is_full);
                    break;*/
                case RemainingTimeCalculator.FILE_SIZE_LIMIT:
                    mErrorUiMessage
                            = getResources().getString(R.string.max_length_reached);
                    break;
                default:
                    mErrorUiMessage = null;
                    break;
            }

            mRecorder.stop();
            return;
        }

        Resources res = getResources();
        String timeStr = "";

        if (t < 60)
            timeStr = String.format(res.getString(R.string.sec_available), t);
        else if (t < 540)
            // timeStr = String.format(res.getString(R.string.min_available), t/60 + 1);
            timeStr = String.format(res.getString(R.string.min_sec_available), t / 60, t % 60);


        mStateMessage1.setText(timeStr);
    }

    /**
     * Shows/hides the appropriate child views for the new state.
     */
    private void updateUi() {
        Resources res = getResources();

        switch (mRecorder.state()) {
            case Recorder.IDLE_STATE:
                if (mRecorder.sampleLength() == 0) {
                    mRecordButton.setEnabled(true);
                    mRecordButton.setFocusable(true);
                    mPlayButton.setEnabled(false);
                    mPlayButton.setFocusable(false);
                    mStopButton.setEnabled(false);
                    mStopButton.setFocusable(false);
                    mRecordButton.requestFocus();

                    mFileListButton.setEnabled(true);
                    mFileListButton.setFocusable(true);

                    mStateMessage1.setVisibility(View.INVISIBLE);
                    mStateLED.setVisibility(View.INVISIBLE);
                    mStateMessage2.setVisibility(View.INVISIBLE);

                    mExitButtons.setVisibility(View.INVISIBLE);
                    mVUMeter.setVisibility(View.VISIBLE);

                    mStateProgressBar.setVisibility(View.INVISIBLE);

                    setTitle(res.getString(R.string.record_your_message));
                } else {
                    mRecordButton.setEnabled(true);
                    mRecordButton.setFocusable(true);
                    mPlayButton.setEnabled(true);
                    mPlayButton.setFocusable(true);
                    mStopButton.setEnabled(false);
                    mStopButton.setFocusable(false);

                    mStateMessage1.setVisibility(View.INVISIBLE);
                    mStateLED.setVisibility(View.INVISIBLE);
                    mStateMessage2.setVisibility(View.INVISIBLE);

                    if (!(mDoWhat != null && mDoWhat.equals(PLAY))) {
                        mExitButtons.setVisibility(View.VISIBLE);
                    } else {
                        mExitButtons.setVisibility(View.INVISIBLE);
                    }
                    mVUMeter.setVisibility(View.INVISIBLE);

                    mStateProgressBar.setVisibility(View.INVISIBLE);

                    setTitle(res.getString(R.string.message_recorded));
                }

                if (mSampleInterrupted) {
                    mStateMessage2.setVisibility(View.VISIBLE);
                    mStateMessage2.setText(res.getString(R.string.recording_stopped));
                    mStateLED.setVisibility(View.INVISIBLE);
                }

                if (mErrorUiMessage != null) {
                    mStateMessage1.setText(mErrorUiMessage);
                    mStateMessage1.setVisibility(View.VISIBLE);
                }

                break;
            case Recorder.RECORDING_STATE:
                mRecordButton.setEnabled(false);
                mRecordButton.setFocusable(false);
                mPlayButton.setEnabled(false);
                mPlayButton.setFocusable(false);
                mStopButton.setEnabled(true);
                mStopButton.setFocusable(true);

                mFileListButton.setEnabled(false);
                mFileListButton.setFocusable(false);

                mStateMessage1.setVisibility(View.VISIBLE);
                mStateLED.setVisibility(View.VISIBLE);
                mStateLED.setImageResource(R.drawable.recording_led);
                mStateMessage2.setVisibility(View.VISIBLE);
                mStateMessage2.setText(res.getString(R.string.recording));

                mExitButtons.setVisibility(View.INVISIBLE);
                mVUMeter.setVisibility(View.VISIBLE);

                mStateProgressBar.setVisibility(View.INVISIBLE);

                setTitle(res.getString(R.string.record_your_message));

                break;

            case Recorder.PLAYING_STATE:
                mRecordButton.setEnabled(true);
                mRecordButton.setFocusable(true);
                mPlayButton.setEnabled(false);
                mPlayButton.setFocusable(false);
                mStopButton.setEnabled(true);
                mStopButton.setFocusable(true);

                mStateMessage1.setVisibility(View.INVISIBLE);
                mStateLED.setVisibility(View.INVISIBLE);
                mStateMessage2.setVisibility(View.INVISIBLE);


                if (!(mDoWhat != null && mDoWhat.equals(PLAY))) {
                    mExitButtons.setVisibility(View.VISIBLE);
                } else {
                    mExitButtons.setVisibility(View.INVISIBLE);
                }
                mVUMeter.setVisibility(View.INVISIBLE);

                mStateProgressBar.setVisibility(View.VISIBLE);

                setTitle(res.getString(R.string.review_message));

                break;
        }

        updateTimerView();
        mVUMeter.invalidate();
    }

    /*
     * Called when Recorder changed it's state.
     */
    public void onStateChanged(int state) {
        if (state == Recorder.PLAYING_STATE || state == Recorder.RECORDING_STATE) {
            mSampleInterrupted = false;
            mErrorUiMessage = null;
            mWakeLock.acquire(); // we don't want to go to sleep while recording or playing
        } else {
            if (mWakeLock.isHeld())
                mWakeLock.release();
        }

        updateUi();
    }

    /*
     * Called when MediaPlayer encounters an error.
     */
    public void onError(int error) {
        Resources res = getResources();

        String title = res.getString(R.string.app_name);
        String message = null;
        switch (error) {
            case Recorder.SDCARD_ACCESS_ERROR:
                message = res.getString(R.string.error_sdcard_access);
                break;
            case Recorder.IN_CALL_RECORD_ERROR:
                // TODO: update error message to reflect that the recording could not be
                //       performed during a call.
            case Recorder.INTERNAL_ERROR:
                message = res.getString(R.string.error_app_internal);
                break;
        }
        if (message != null) {
            new AlertDialog.Builder(this)
                    .setTitle(title)
                    .setMessage(message)
                    .setPositiveButton(R.string.button_ok, null)
                    .setCancelable(false)
                    .show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (RESULT_OK == resultCode) {
            mFileListButton.setEnabled(true);
            Intent intent = data;
            Bundle bundle = intent.getExtras();
            if (bundle != null) {
                mDoWhat = bundle.getString(DOWHAT);
                if (mDoWhat != null) {
                    if (mDoWhat.equals(RECORD)) {
                        record();
                    } else if (mDoWhat.equals(PLAY)) {
                        // playing hide the save & cancel
                        // set the recording file
                        String path = null;
                        if (intent.getExtras() != null
                                && intent.getExtras().getString(PATH) != null) {
                            path = intent.getExtras().getString(PATH);
                            File file = new File(path);
                            mRecorder.mSampleFile = file;
                            mRecorder.mSampleLength = intent.getExtras()
                                    .getInt(DURATION) / 1000;
                            mRecorder.startPlayback();
                        }
                    } else {
                        // init the activity
                        mRecorder = new Recorder();
                        mRecorder.setOnStateChangedListener(this);
                        mRemainingTimeCalculator = new RemainingTimeCalculator();
                        mDoWhat = null;
                        mVUMeter.setRecorder(mRecorder);
                    }
                }
            }
        }
        updateUi();
    }
}
