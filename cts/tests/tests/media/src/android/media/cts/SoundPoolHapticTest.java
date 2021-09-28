/*
 * Copyright (C) 2018 The Android Open Source Project
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

import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.cts.R;
import android.platform.test.annotations.AppModeFull;

@AppModeFull(reason = "TODO: evaluate and port to instant")
public class SoundPoolHapticTest extends SoundPoolTest {
    @Override
    protected AudioAttributes getAudioAttributes() {
        // Unmute haptic channels here so that haptic playback
        // will be selected if it is available.
        return new AudioAttributes.Builder()
                .setLegacyStreamType(AudioManager.STREAM_MUSIC)
                .setHapticChannelsMuted(false).build();
    }

    @Override
    protected int getSoundA() {
        return R.raw.a_4_haptic;
    }

    @Override
    protected int getSoundCs() {
        return R.raw.c_sharp_5_haptic;
    }

    @Override
    protected int getSoundE() {
        return R.raw.e_5_haptic;
    }

    @Override
    protected int getSoundB() {
        return R.raw.b_5_haptic;
    }

    @Override
    protected int getSoundGs() {
        return R.raw.g_sharp_5_haptic;
    }

    @Override
    protected String getFileName() {
        return "a_4_haptic.ogg";
    }
}
