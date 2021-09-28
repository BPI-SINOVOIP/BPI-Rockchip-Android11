/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.tools.metalava

import org.junit.Test

class NullabilityAnnotationsValidatorTest : DriverTest() {

    @Test
    fun `Empty report when all expected annotations present`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;

                        public interface Appendable {
                            Appendable append(CharSequence csq) throws IOException;
                        }

                        public interface List<T> {
                            T get(int index);
                        }

                        // This is not annotated at all, shouldn't complain about this.
                        public interface NotAnnotated {
                            NotAnnotated combine(NotAnnotated other);
                        }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            mergeJavaStubAnnotations = """
                package test.pkg;

                import libcore.util.NonNull;
                import libcore.util.Nullable;
                import libcore.util.NullFromTypeParam;

                public interface Appendable {
                    @NonNull Appendable append(@Nullable java.lang.CharSequence csq);
                }

                public interface List<T> {
                    @NullFromTypeParam T get(int index);
                }
                """,
            extraArguments = arrayOf(ARG_VALIDATE_NULLABILITY_FROM_MERGED_STUBS),
            validateNullability = setOf()
        )
    }

    @Test
    fun `Missing parameter annotation`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;

                        public interface Appendable {
                            Appendable append(CharSequence csq) throws IOException;
                        }

                        public interface List<T> {
                            T get(int index);
                        }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            mergeJavaStubAnnotations = """
                package test.pkg;

                import libcore.util.NonNull;
                import libcore.util.NullFromTypeParam;

                public interface Appendable {
                    @NonNull Appendable append(java.lang.CharSequence csq);
                }

                public interface List<T> {
                    @NullFromTypeParam T get(int index);
                }
                """,
            extraArguments = arrayOf(ARG_VALIDATE_NULLABILITY_FROM_MERGED_STUBS),
            validateNullability = setOf(
                "WARNING: method test.pkg.Appendable.append(CharSequence), parameter csq, MISSING"
            )
        )
    }

    @Test
    fun `Missing return type annotations`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;

                        public interface Appendable {
                            Appendable append(CharSequence csq) throws IOException;
                        }

                        public interface List<T> {
                            T get(int index);
                        }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            mergeJavaStubAnnotations = """
                package test.pkg;

                import libcore.util.Nullable;

                public interface Appendable {
                    Appendable append(@Nullable java.lang.CharSequence csq);
                }

                public interface List<T> {
                }
                """,
            extraArguments = arrayOf(ARG_VALIDATE_NULLABILITY_FROM_MERGED_STUBS),
            validateNullability = setOf(
                "WARNING: method test.pkg.Appendable.append(CharSequence), return value, MISSING",
                "WARNING: method test.pkg.List.get(int), return value, MISSING"
            )
        )
    }

    @Test
    fun `Error from annotation on primitive`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;

                        public interface Appendable {
                            Appendable append(CharSequence csq) throws IOException;
                        }

                        public interface List<T> {
                            T get(int index);
                        }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            mergeJavaStubAnnotations = """
                package test.pkg;

                import libcore.util.NonNull;
                import libcore.util.Nullable;
                import libcore.util.NullFromTypeParam;

                public interface Appendable {
                    @NonNull Appendable append(@Nullable java.lang.CharSequence csq);
                }

                public interface List<T> {
                    @NullFromTypeParam T get(@NonNull int index);
                }
                """,
            extraArguments = arrayOf(ARG_VALIDATE_NULLABILITY_FROM_MERGED_STUBS),
            validateNullability = setOf(
                "ERROR: method test.pkg.List.get(int), parameter index, ON_PRIMITIVE"
            )
        )
    }

    @Test
    fun `Error from NullFromTypeParam not on type param`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;

                        public interface Appendable {
                            Appendable append(CharSequence csq) throws IOException;
                        }

                        public interface List<T> {
                            T get(int index);
                        }
                    """
                )
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            mergeJavaStubAnnotations = """
                package test.pkg;

                import libcore.util.Nullable;
                import libcore.util.NullFromTypeParam;

                public interface Appendable {
                    @NullFromTypeParam Appendable append(@Nullable java.lang.CharSequence csq);
                }

                public interface List<T> {
                    @NullFromTypeParam T get(int index);
                }
                """,
            extraArguments = arrayOf(ARG_VALIDATE_NULLABILITY_FROM_MERGED_STUBS),
            validateNullability = setOf(
                "ERROR: method test.pkg.Appendable.append(CharSequence), return value, BAD_TYPE_PARAM"
            )
        )
    }

    @Test
    fun `Using class list`() {
        check(
            sourceFiles = arrayOf(
                java(
                    """
                        package test.pkg;

                        import libcore.util.Nullable;

                        // This will be validated. It is missing an annotation on its return type.
                        public interface Appendable {
                            Appendable append(@Nullable CharSequence csq) throws IOException;
                        }

                        // This is missing an annotation on its return type, but will not be validated.
                        public interface List<T> {
                            T get(int index);
                        }
                    """
                ),
                libcoreNullableSource
            ),
            compatibilityMode = false,
            outputKotlinStyleNulls = false,
            omitCommonPackages = false,
            extraArguments = arrayOf(ARG_VALIDATE_NULLABILITY_FROM_MERGED_STUBS),
            validateNullabilityFromList =
            """
                # a comment, then a blank line, then the class to validate

                test.pkg.Appendable
            """,
            validateNullability = setOf(
                "WARNING: method test.pkg.Appendable.append(CharSequence), return value, MISSING"
            )
        )
    }
}
