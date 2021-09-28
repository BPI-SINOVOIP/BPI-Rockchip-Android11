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
package android.platform.test.composer;

import android.host.test.composer.ShuffleBase;
import android.os.Bundle;

import java.util.Random;

/**
 * An extension of {@link android.host.test.composer.ShuffleBase} for device-side testing.
 */
public class Shuffle<U> extends ShuffleBase<Bundle, U> {
    @Override
    protected boolean getShuffleArgument(Bundle args) {
        return Boolean.parseBoolean(
                args.getString(SHUFFLE_OPTION_NAME, String.valueOf(mShuffleDefaultValue)));
    }

    @Override
    protected long getSeedArgument(Bundle args) {
        String seed = args.getString(SEED_OPTION_NAME, String.valueOf(new Random().nextLong()));
        try {
            return Long.parseLong(seed);
        } catch (NumberFormatException e) {
            throw new RuntimeException(String.format("Failed to parse seed option: %s", seed), e);
        }
    }
}
