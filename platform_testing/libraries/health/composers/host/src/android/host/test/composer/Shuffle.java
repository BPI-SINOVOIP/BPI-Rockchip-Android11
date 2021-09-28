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
package android.host.test.composer;

import java.util.Map;
import java.util.Random;

/**
 * An extension of {@link ShuffleBase} for host-side testing.
 */
public class Shuffle<U> extends ShuffleBase<Map<String, String>, U> {
    @Override
    protected boolean getShuffleArgument(Map<String, String> args) {
        if (args.containsKey(SHUFFLE_OPTION_NAME)) {
            return Boolean.parseBoolean(args.get(SHUFFLE_OPTION_NAME));
        } else {
            return mShuffleDefaultValue;
        }
    }

    @Override
    protected long getSeedArgument(Map<String, String> args) {
        if (args.containsKey(SEED_OPTION_NAME)) {
            String seed = args.get(SEED_OPTION_NAME);
            try {
                return Long.parseLong(seed);
            } catch (NumberFormatException e) {
                throw new RuntimeException(
                        String.format("Failed to parse seed option: %s", seed), e);
            }
        } else {
            return new Random().nextLong();
        }
    }
}
