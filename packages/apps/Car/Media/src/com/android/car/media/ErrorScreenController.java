package com.android.car.media;

import android.app.PendingIntent;
import android.car.content.pm.CarPackageManager;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

import com.android.car.media.common.PlaybackErrorViewController;
import com.android.car.media.common.source.MediaSource;

/**
 * A view controller that displays the playback state error iif there is no browse tree.
 */
public class ErrorScreenController extends ViewControllerBase {

    private final PlaybackErrorViewController mPlaybackErrorViewController;

    ErrorScreenController(FragmentActivity activity,
            CarPackageManager carPackageManager, ViewGroup container) {
        super(activity, carPackageManager, container, R.layout.fragment_error);

        mPlaybackErrorViewController = new PlaybackErrorViewController(mContent);
    }

    @Override
    void onMediaSourceChanged(@Nullable MediaSource mediaSource) {
        super.onMediaSourceChanged(mediaSource);

        mAppBarController.setListener(new BasicAppBarListener());
        mAppBarController.setTitle(getAppBarDefaultTitle(mediaSource));

        mPlaybackErrorViewController.hideErrorNoAnim();
    }

    public void setError(String message, String label, PendingIntent pendingIntent,
            boolean distractionOptimized) {
        mPlaybackErrorViewController.setError(message, label, pendingIntent, distractionOptimized);
    }
}
