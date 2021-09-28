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

package android.processor.compat.changeid;

import static java.nio.charset.StandardCharsets.UTF_8;

import static javax.tools.StandardLocation.CLASS_OUTPUT;

import com.google.common.collect.ObjectArrays;
import com.google.testing.compile.Compilation;
import com.google.testing.compile.CompilationSubject;
import com.google.testing.compile.Compiler;
import com.google.testing.compile.JavaFileObjects;

import org.junit.Test;

import javax.tools.JavaFileObject;


public class ChangeIdProcessorTest {

    // Hard coding the annotation definitions to avoid dependency on libcore, where they're defined.
    private static final JavaFileObject[] mAnnotations = {
            JavaFileObjects.forSourceLines("android.compat.annotation.ChangeId",
                    "package android.compat.annotation;",
                    "import static java.lang.annotation.ElementType.FIELD;",
                    "import static java.lang.annotation.ElementType.PARAMETER;",
                    "import static java.lang.annotation.RetentionPolicy.SOURCE;",
                    "import java.lang.annotation.Retention;",
                    "import java.lang.annotation.Target;",
                    "@Retention(SOURCE)",
                    "@Target({FIELD, PARAMETER})",
                    "public @interface ChangeId {",
                    "}"),
            JavaFileObjects.forSourceLines("android.compat.annotation.Disabled",
                    "package android.compat.annotation;",
                    "import static java.lang.annotation.ElementType.FIELD;",
                    "import static java.lang.annotation.RetentionPolicy.SOURCE;",
                    "import java.lang.annotation.Retention;",
                    "import java.lang.annotation.Target;",
                    "@Retention(SOURCE)",
                    "@Target({FIELD})",
                    "public @interface Disabled {",
                    "}"),
            JavaFileObjects.forSourceLines("android.compat.annotation.LoggingOnly",
                    "package android.compat.annotation;",
                    "import static java.lang.annotation.ElementType.FIELD;",
                    "import static java.lang.annotation.RetentionPolicy.SOURCE;",
                    "import java.lang.annotation.Retention;",
                    "import java.lang.annotation.Target;",
                    "@Retention(SOURCE)",
                    "@Target({FIELD})",
                    "public @interface LoggingOnly {",
                    "}"),
            JavaFileObjects.forSourceLines("android.compat.annotation.EnabledAfter",
                    "package android.compat.annotation;",
                    "import static java.lang.annotation.ElementType.FIELD;",
                    "import static java.lang.annotation.RetentionPolicy.SOURCE;",
                    "import java.lang.annotation.Retention;",
                    "import java.lang.annotation.Target;",
                    "@Retention(SOURCE)",
                    "@Target({FIELD})",
                    "public @interface EnabledAfter {",
                    "int targetSdkVersion();",
                    "}")

    };

    private static final String HEADER =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>";

    @Test
    public void testCompatConfigXmlOutput() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "import android.compat.annotation.Disabled;",
                        "public class Compat {",
                        "    /**",
                        "    * description of",
                        "    * MY_CHANGE_ID",
                        "    */",
                        "    @EnabledAfter(targetSdkVersion=29)",
                        "    @ChangeId",
                        "    static final long MY_CHANGE_ID = 123456789l;",
                        "    /** description of ANOTHER_CHANGE **/",
                        "    @ChangeId",
                        "    @Disabled",
                        "    public static final long ANOTHER_CHANGE = 23456700l;",
                        "}")
        };
        String expectedFile = HEADER + "<config>" +
                "<compat-change description=\"description of MY_CHANGE_ID\" "
                + "enableAfterTargetSdk=\"29\" id=\"123456789\" name=\"MY_CHANGE_ID\">"
                + "<meta-data definedIn=\"libcore.util.Compat\" "
                + "sourcePosition=\"libcore/util/Compat.java:11\"/></compat-change>"
                + "<compat-change description=\"description of ANOTHER_CHANGE\" disabled=\"true\" "
                + "id=\"23456700\" name=\"ANOTHER_CHANGE\"><meta-data definedIn=\"libcore.util"
                + ".Compat\" sourcePosition=\"libcore/util/Compat.java:14\"/></compat-change>"
                + "</config>";
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).succeeded();
        CompilationSubject.assertThat(compilation).generatedFile(CLASS_OUTPUT, "libcore.util",
                "Compat_compat_config.xml").contentsAsString(UTF_8).isEqualTo(expectedFile);
    }

    @Test
    public void testCompatConfigXmlOutput_multiplePackages() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "import android.compat.annotation.Disabled;",
                        "public class Compat {",
                        "    /**",
                        "    * description of",
                        "    * MY_CHANGE_ID",
                        "    */",
                        "    @ChangeId",
                        "    static final long MY_CHANGE_ID = 123456789l;",
                        "}"),
                JavaFileObjects.forSourceLines("android.util.SomeClass",
                        "package android.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "import android.compat.annotation.Disabled;",
                        "public class SomeClass {",
                        "    /** description of ANOTHER_CHANGE **/",
                        "    @ChangeId",
                        "    public static final long ANOTHER_CHANGE = 23456700l;",
                        "}")
        };
        String libcoreExpectedFile = HEADER + "<config>" +
                "<compat-change description=\"description of MY_CHANGE_ID\" "
                + "id=\"123456789\" name=\"MY_CHANGE_ID\">"
                + "<meta-data definedIn=\"libcore.util.Compat\" "
                + "sourcePosition=\"libcore/util/Compat.java:10\"/></compat-change>"
                + "</config>";
        String androidExpectedFile = HEADER + "<config>" +
                "<compat-change description=\"description of ANOTHER_CHANGE\" "
                + "id=\"23456700\" name=\"ANOTHER_CHANGE\"><meta-data definedIn=\"android.util"
                + ".SomeClass\" sourcePosition=\"android/util/SomeClass.java:7\"/></compat-change>"
                + "</config>";
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).succeeded();
        CompilationSubject.assertThat(compilation).generatedFile(CLASS_OUTPUT, "libcore.util",
                "Compat_compat_config.xml").contentsAsString(UTF_8).isEqualTo(libcoreExpectedFile);
        CompilationSubject.assertThat(compilation).generatedFile(CLASS_OUTPUT, "android.util",
                "SomeClass_compat_config.xml").contentsAsString(UTF_8).isEqualTo(
                androidExpectedFile);
    }

    @Test
    public void testCompatConfigXmlOutput_innerClass() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "import android.compat.annotation.Disabled;",
                        "public class Compat {",
                        "    public class Inner {",
                        "        /**",
                        "        * description of",
                        "        * MY_CHANGE_ID",
                        "        */",
                        "        @ChangeId",
                        "        static final long MY_CHANGE_ID = 123456789l;",
                        "    }",
                        "}"),
        };
        String expectedFile = HEADER + "<config>" +
                "<compat-change description=\"description of MY_CHANGE_ID\" "
                + "id=\"123456789\" name=\"MY_CHANGE_ID\"><meta-data definedIn=\"libcore.util"
                + ".Compat.Inner\" sourcePosition=\"libcore/util/Compat.java:11\"/>"
                + "</compat-change></config>";
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).succeeded();
        CompilationSubject.assertThat(compilation).generatedFile(CLASS_OUTPUT, "libcore.util",
                "Compat.Inner_compat_config.xml").contentsAsString(UTF_8).isEqualTo(expectedFile);
    }

    @Test
    public void testCompatConfigXmlOutput_interface() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "import android.compat.annotation.Disabled;",
                        "public interface Compat {",
                        "    /**",
                        "     * description of",
                        "     * MY_CHANGE_ID",
                        "     */",
                        "    @ChangeId",
                        "    static final long MY_CHANGE_ID = 123456789l;",
                        "}"),
        };
        String expectedFile = HEADER + "<config>" +
                "<compat-change description=\"description of MY_CHANGE_ID\" "
                + "id=\"123456789\" name=\"MY_CHANGE_ID\"><meta-data definedIn=\"libcore.util"
                + ".Compat\" sourcePosition=\"libcore/util/Compat.java:10\"/>"
                + "</compat-change></config>";
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).succeeded();
        CompilationSubject.assertThat(compilation).generatedFile(CLASS_OUTPUT, "libcore.util",
                "Compat_compat_config.xml").contentsAsString(UTF_8).isEqualTo(expectedFile);
    }

    @Test
    public void testCompatConfigXmlOutput_enum() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "import android.compat.annotation.Disabled;",
                        "public enum Compat {",
                        "    ENUM_CONSTANT;",
                        "    /**",
                        "     * description of",
                        "     * MY_CHANGE_ID",
                        "     */",
                        "    @ChangeId",
                        "    private static final long MY_CHANGE_ID = 123456789l;",
                        "}"),
        };
        String expectedFile = HEADER + "<config>" +
                "<compat-change description=\"description of MY_CHANGE_ID\" "
                + "id=\"123456789\" name=\"MY_CHANGE_ID\"><meta-data definedIn=\"libcore.util"
                + ".Compat\" sourcePosition=\"libcore/util/Compat.java:11\"/>"
                + "</compat-change></config>";
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).succeeded();
        CompilationSubject.assertThat(compilation).generatedFile(CLASS_OUTPUT, "libcore.util",
                "Compat_compat_config.xml").contentsAsString(UTF_8).isEqualTo(expectedFile);
    }

    @Test
    public void testBothDisabledAndEnabledAfter() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "import android.compat.annotation.Disabled;",
                        "public class Compat {",
                        "    @EnabledAfter(targetSdkVersion=29)",
                        "    @Disabled",
                        "    @ChangeId",
                        "    static final long MY_CHANGE_ID = 123456789l;",
                        "}")
        };
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "ChangeId cannot be annotated with both @Disabled and @EnabledAfter.");
    }


    @Test
    public void testBothLoggingOnlyAndEnabledAfter() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "import android.compat.annotation.LoggingOnly;",
                        "public class Compat {",
                        "    @EnabledAfter(targetSdkVersion=29)",
                        "    @LoggingOnly",
                        "    @ChangeId",
                        "    static final long MY_CHANGE_ID = 123456789l;",
                        "}")
        };
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "ChangeId cannot be annotated with both @LoggingOnly and "
                        + "(@EnabledAfter | @Disabled).");
    }

    @Test
    public void testBothLoggingOnlyAndDisabled() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.LoggingOnly;",
                        "import android.compat.annotation.Disabled;",
                        "public class Compat {",
                        "    @LoggingOnly",
                        "    @Disabled",
                        "    @ChangeId",
                        "    static final long MY_CHANGE_ID = 123456789l;",
                        "}")
        };
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "ChangeId cannot be annotated with both @LoggingOnly and "
                        + "(@EnabledAfter | @Disabled).");
    }

    @Test
    public void testLoggingOnly() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.LoggingOnly;",
                        "public class Compat {",
                        "    @LoggingOnly",
                        "    @ChangeId",
                        "    static final long MY_CHANGE_ID = 123456789l;",
                        "}")
        };
        String expectedFile = HEADER + "<config>" +
                "<compat-change id=\"123456789\" loggingOnly=\"true\" name=\"MY_CHANGE_ID\">" +
                "<meta-data definedIn=\"libcore.util.Compat\" " +
                "sourcePosition=\"libcore/util/Compat.java:6\"/>" +
                "</compat-change></config>";
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).succeeded();
        CompilationSubject.assertThat(compilation).generatedFile(CLASS_OUTPUT, "libcore.util",
                "Compat_compat_config.xml").contentsAsString(UTF_8).isEqualTo(expectedFile);
    }

    @Test
    public void testIgnoredParams() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "android.compat.Compatibility",
                        "package android.compat;",
                        "import android.compat.annotation.ChangeId;",
                        "public final class Compatibility {",
                        "   public static void reportChange(@ChangeId long changeId) {}",
                        "   public static boolean isChangeEnabled(@ChangeId long changeId) {",
                        "       return true;",
                        "   }",
                        "}")
        };
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).succeeded();
    }

    @Test
    public void testOtherClassParams() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "android.compat.OtherClass",
                        "package android.compat;",
                        "import android.compat.annotation.ChangeId;",
                        "public final class OtherClass {",
                        "   public static void reportChange(@ChangeId long changeId) {}",
                        "}")
        };
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "Non FIELD element annotated with @ChangeId.");
    }

    @Test
    public void testOtherMethodParams() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "android.compat.Compatibility",
                        "package android.compat;",
                        "import android.compat.annotation.ChangeId;",
                        "public final class Compatibility {",
                        "   public static void otherMethod(@ChangeId long changeId) {}",
                        "}")
        };
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "Non FIELD element annotated with @ChangeId.");
    }

    @Test
    public void testNonFinal() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "public class Compat {",
                        "    @EnabledAfter(targetSdkVersion=29)",
                        "    @ChangeId",
                        "    static long MY_CHANGE_ID = 123456789l;",
                        "}")
        };
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "Non constant/final variable annotated with @ChangeId.");
    }

    @Test
    public void testNonLong() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "public class Compat {",
                        "    @EnabledAfter(targetSdkVersion=29)",
                        "    @ChangeId",
                        "    static final int MY_CHANGE_ID = 12345;",
                        "}")
        };
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "Variables annotated with @ChangeId must be of type long.");
    }

    @Test
    public void testNonStatic() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "public class Compat {",
                        "    @EnabledAfter(targetSdkVersion=29)",
                        "    @ChangeId",
                        "    final long MY_CHANGE_ID = 123456789l;",
                        "}")
        };
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "Non static variable annotated with @ChangeId.");
    }

    @Test
    public void testCompatConfigXmlOutput_hideTag() {
        JavaFileObject[] source = {
                JavaFileObjects.forSourceLines(
                        "libcore.util.Compat",
                        "package libcore.util;",
                        "import android.compat.annotation.ChangeId;",
                        "import android.compat.annotation.EnabledAfter;",
                        "import android.compat.annotation.Disabled;",
                        "public class Compat {",
                        "    public class Inner {",
                        "        /**",
                        "        * description of MY_CHANGE_ID.",
                        "        * @hide",
                        "        */",
                        "        @ChangeId",
                        "        static final long MY_CHANGE_ID = 123456789l;",
                        "    }",
                        "}"),
        };
        String expectedFile = HEADER + "<config>" +
                "<compat-change description=\"description of MY_CHANGE_ID.\" "
                + "id=\"123456789\" name=\"MY_CHANGE_ID\"><meta-data definedIn=\"libcore.util"
                + ".Compat.Inner\" sourcePosition=\"libcore/util/Compat.java:11\"/>"
                + "</compat-change></config>";
        Compilation compilation =
                Compiler.javac()
                        .withProcessors(new ChangeIdProcessor())
                        .compile(ObjectArrays.concat(mAnnotations, source, JavaFileObject.class));
        CompilationSubject.assertThat(compilation).succeeded();
        CompilationSubject.assertThat(compilation).generatedFile(CLASS_OUTPUT, "libcore.util",
                "Compat.Inner_compat_config.xml").contentsAsString(UTF_8).isEqualTo(expectedFile);
    }

}
