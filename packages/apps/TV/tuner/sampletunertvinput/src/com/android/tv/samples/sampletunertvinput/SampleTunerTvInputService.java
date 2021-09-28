package com.android.tv.samples.sampletunertvinput;

import android.content.Context;
import android.media.tv.tuner.frontend.AtscFrontendSettings;
import android.media.tv.tuner.frontend.FrontendSettings;
import android.media.tv.tuner.Tuner;
import android.media.tv.TvInputService;
import android.net.Uri;
import android.util.Log;
import android.view.Surface;


/** SampleTunerTvInputService */
public class SampleTunerTvInputService extends TvInputService {
    private static final String TAG = "SampleTunerTvInput";
    private static final boolean DEBUG = true;

    public static final String INPUT_ID =
        "com.android.tv.samples.sampletunertvinput/.SampleTunerTvInputService";
    private String mSessionId;

    @Override
    public TvInputSessionImpl onCreateSession(String inputId, String sessionId) {
        TvInputSessionImpl session =  new TvInputSessionImpl(this);
        if (DEBUG) {
            Log.d(TAG, "onCreateSession(inputId=" + inputId + ", sessionId=" + sessionId + ")");
        }
        mSessionId = sessionId;
        return session;
    }

    @Override
    public TvInputSessionImpl onCreateSession(String inputId) {
        return new TvInputSessionImpl(this);
    }

    class TvInputSessionImpl extends Session {

        private Surface surface;
        private final Context mContext;
        Tuner tuner;


        public TvInputSessionImpl(Context context) {
            super(context);
            mContext = context;
        }

        @Override
        public void onRelease() {
            if (DEBUG) {
                Log.d(TAG, "onRelease");
            }
        }

        @Override
        public boolean onSetSurface(Surface surface) {
            if (DEBUG) {
                Log.d(TAG, "onSetSurface");
            }
            this.surface = surface;
            return true;
        }

        @Override
        public void onSetStreamVolume(float v) {
            if (DEBUG) {
                Log.d(TAG, "onSetStreamVolume " + v);
            }
        }

        @Override
        public boolean onTune(Uri uri) {
            if (DEBUG) {
                Log.d(TAG, "onTune " + uri);
            }
            tuner = new Tuner(mContext, mSessionId,
                    TvInputService.PRIORITY_HINT_USE_CASE_TYPE_LIVE);

            int feCount = tuner.getFrontendIds().size();
            if (feCount <= 0) return false;

            AtscFrontendSettings settings =
                    AtscFrontendSettings
                            .builder()
                            .setFrequency(2000)
                            .setModulation(AtscFrontendSettings.MODULATION_AUTO)
                            .build();
            tuner.tune(settings);

            return true;
        }

        @Override
        public void onSetCaptionEnabled(boolean b) {
            if (DEBUG) {
                Log.d(TAG, "onSetCaptionEnabled " + b);
            }
        }
    }
}