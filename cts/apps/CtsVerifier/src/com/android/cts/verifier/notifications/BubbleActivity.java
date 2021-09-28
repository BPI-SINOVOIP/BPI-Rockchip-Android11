package com.android.cts.verifier.notifications;

import android.app.Activity;
import android.os.Bundle;

import com.android.cts.verifier.R;

/**
 * Used in BubblesVerifierActivity as the contents of the bubble.
 */
public class BubbleActivity extends Activity {

    @Override
    public void onCreate(Bundle savedState) {
        super.onCreate(savedState);

        setContentView(R.layout.bubble_activity);
    }
}
