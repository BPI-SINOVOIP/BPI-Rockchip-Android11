/*
 * Copyright (C) 2019 The Android Open Source Project
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

import android.app.Activity;
import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.TextureView.SurfaceTextureListener;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;

public class MediaStubActivity2 extends Activity implements SurfaceTextureListener {
    private static final String TAG = "MediaStubActivity2";
    private Surface mSurface;
    private CompletableFuture<Surface> mSurface2 = new CompletableFuture<>();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.mediacodecplayer);

        SurfaceView surfaceV = (SurfaceView)findViewById(R.id.surface);
        mSurface = surfaceV.getHolder().getSurface();

        TextureView textureV = (TextureView)findViewById(R.id.surface2);
        textureV.setSurfaceTextureListener(this);
    }

    public List<Surface> getSurfaces() {
        /*
         * Assume `onSurfaceTextureAvailable` will arrive on a different thread than the thread
         * calling `getSurfaces`. `getSurfaces` should be called from a CTS thread.
         */
        try {
            return Arrays.asList(mSurface, mSurface2.get());
        } catch (InterruptedException | ExecutionException e) {
            return Arrays.asList(mSurface);
        }
    }

    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture st, int width, int height) {
        Log.i(TAG, "onSurfaceTextureAvailable");
        final Surface s = new Surface(st);
        InputSurface.clearSurface(s);
        mSurface2.complete(s);
    }

    @Override
    public void onSurfaceTextureSizeChanged(SurfaceTexture st, int width, int height) {
    }

    @Override
    public boolean onSurfaceTextureDestroyed(SurfaceTexture st) {
        return true;
    }

    @Override
    public void onSurfaceTextureUpdated(SurfaceTexture st) {
    }
}
