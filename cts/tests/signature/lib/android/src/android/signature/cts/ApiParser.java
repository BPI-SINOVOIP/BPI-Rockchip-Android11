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
package android.signature.cts;

import java.util.stream.Stream;

/**
 * Base class for parsers of API specification.
 */
abstract class ApiParser {

    /**
     * Parse the contents of the path and generate a stream of {@link JDiffClassDescription}
     * instances.
     *
     * @param path the path to the API specification.
     * @return the stream of {@link JDiffClassDescription} instances.
     */
    abstract Stream<JDiffClassDescription> parseAsStream(VirtualPath path);
}
