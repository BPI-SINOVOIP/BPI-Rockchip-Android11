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

import java.util.HashMap;
import java.util.Map;

import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit test the logic for host-side {@link Iterate}
 */
@RunWith(JUnit4.class)
public class ShuffleTest extends ShuffleTestBase<Map<String, String>> {
    @Override
    protected ShuffleBase<Map<String, String>, Integer> getShuffle() {
        return new Shuffle<Integer>();
    }

    @Override
    protected Map<String, String> getArguments(boolean shuffle, long seed) {
        Map<String, String> args = new HashMap<>();
        args.put(SHUFFLE_OPTION_NAME, String.valueOf(shuffle));
        args.put(SEED_OPTION_NAME, String.valueOf(seed));
        return args;
    }
}
