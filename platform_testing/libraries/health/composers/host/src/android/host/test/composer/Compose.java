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

import java.util.List;
import java.util.Map;
import java.util.function.BiFunction;

/**
 * A {@code BiFunction} for composing the {@code List<T>} based on a {@code Map<String, String>} of
 * arguments.
 */
@FunctionalInterface
public interface Compose<T, U> extends BiFunction<T, List<U>, List<U>> {
    default Compose<T, U> andThen(Compose<T, U> next) {
        return (args, list) -> next.apply(args, this.apply(args, list));
    }
}
