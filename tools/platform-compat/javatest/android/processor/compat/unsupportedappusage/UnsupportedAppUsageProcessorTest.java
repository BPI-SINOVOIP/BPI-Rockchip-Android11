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

package android.processor.compat.unsupportedappusage;

import static com.google.common.truth.Truth.assertThat;

import com.google.testing.compile.Compilation;
import com.google.testing.compile.CompilationSubject;
import com.google.testing.compile.Compiler;
import com.google.testing.compile.JavaFileObjects;

import org.junit.Test;

import java.io.IOException;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.stream.Collectors;

import javax.tools.JavaFileObject;
import javax.tools.StandardLocation;

public class UnsupportedAppUsageProcessorTest {

    private static final JavaFileObject ANNOTATION = JavaFileObjects.forSourceLines(
            "android.compat.annotation.UnsupportedAppUsage",
            "package android.compat.annotation;",
            "public @interface UnsupportedAppUsage {",
            "    String expectedSignature() default \"\";\n",
            "    String someProperty() default \"\";",
            "    String overrideSourcePosition() default \"\";",
            "    String implicitMember() default \"\";",
            "}");

    private Compilation compile(JavaFileObject source) {
        Compilation compilation =
                Compiler.javac().withProcessors(new UnsupportedAppUsageProcessor())
                        .compile(ANNOTATION, source);
        CompilationSubject.assertThat(compilation).succeeded();
        return compilation;
    }

    private CsvReader compileAndReadCsv(JavaFileObject source, String filename) throws IOException {
        Compilation compilation = compile(source);
        Optional<JavaFileObject> csv = compilation.generatedFile(
                StandardLocation.CLASS_OUTPUT, filename);
        assertThat(csv.isPresent()).isTrue();

        return new CsvReader(csv.get().openInputStream());
    }

    @Test
    public void testSignatureFormat() throws Exception {
        JavaFileObject src = JavaFileObjects.forSourceLines("a.b.Class",
                "package a.b;",
                "import android.compat.annotation.UnsupportedAppUsage;",
                "public class Class {",
                "  @UnsupportedAppUsage",
                "  public void method() {}",
                "}");
        assertThat(compileAndReadCsv(src, "a/b/Class.uau").getContents().get(0)).containsEntry(
                "signature", "La/b/Class;->method()V"
        );
    }

    @Test
    public void testSourcePosition() throws Exception {
        JavaFileObject src = JavaFileObjects.forSourceLines("a.b.Class",
                "package a.b;", // 1
                "import android.compat.annotation.UnsupportedAppUsage;", // 2
                "public class Class {", // 3
                "  @UnsupportedAppUsage", // 4
                "  public void method() {}", // 5
                "}");
        Map<String, String> row = compileAndReadCsv(src, "a/b/Class.uau").getContents().get(0);
        assertThat(row).containsEntry("startline", "4");
        assertThat(row).containsEntry("startcol", "3");
        assertThat(row).containsEntry("endline", "4");
        assertThat(row).containsEntry("endcol", "23");
    }

    @Test
    public void testAnnotationProperties() throws Exception {
        JavaFileObject src = JavaFileObjects.forSourceLines("a.b.Class",
                "package a.b;", // 1
                "import android.compat.annotation.UnsupportedAppUsage;", // 2
                "public class Class {", // 3
                "  @UnsupportedAppUsage(someProperty=\"value\")", // 4
                "  public void method() {}", // 5
                "}");
        assertThat(compileAndReadCsv(src, "a/b/Class.uau").getContents().get(0)).containsEntry(
                "properties", "someProperty=%22value%22");
    }

    @Test
    public void testSourcePositionOverride() throws Exception {
        JavaFileObject src = JavaFileObjects.forSourceLines("a.b.Class",
                "package a.b;", // 1
                "import android.compat.annotation.UnsupportedAppUsage;", // 2
                "public class Class {", // 3
                "  @UnsupportedAppUsage(overrideSourcePosition=\"otherfile.aidl:30:10:31:20\")",
                "  public void method() {}", // 5
                "}");
        Map<String, String> row = compileAndReadCsv(src, "a/b/Class.uau").getContents().get(0);
        assertThat(row).containsEntry("file", "otherfile.aidl");
        assertThat(row).containsEntry("startline", "30");
        assertThat(row).containsEntry("startcol", "10");
        assertThat(row).containsEntry("endline", "31");
        assertThat(row).containsEntry("endcol", "20");
        assertThat(row).containsEntry("properties", "");
    }

    @Test
    public void testSourcePositionOverrideWrongFormat() throws Exception {
        JavaFileObject src = JavaFileObjects.forSourceLines("a.b.Class",
                "package a.b;", // 1
                "import android.compat.annotation.UnsupportedAppUsage;", // 2
                "public class Class {", // 3
                "  @UnsupportedAppUsage(overrideSourcePosition=\"invalid\")", // 4
                "  public void method() {}", // 5
                "}");
        Compilation compilation =
                Compiler.javac().withProcessors(new UnsupportedAppUsageProcessor())
                        .compile(ANNOTATION, src);
        CompilationSubject.assertThat(compilation).failed();
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "Expected overrideSourcePosition to have format "
                        + "string:int:int:int:int").inFile(src).onLine(4);
    }

    @Test
    public void testSourcePositionOverrideInvalidInt() throws Exception {
        JavaFileObject src = JavaFileObjects.forSourceLines("a.b.Class",
                "package a.b;", // 1
                "import android.compat.annotation.UnsupportedAppUsage;", // 2
                "public class Class {", // 3
                "  @UnsupportedAppUsage(overrideSourcePosition=\"otherfile.aidl:a:b:c:d\")", // 4
                "  public void method() {}", // 5
                "}");
        Compilation compilation =
                Compiler.javac().withProcessors(new UnsupportedAppUsageProcessor())
                        .compile(ANNOTATION, src);
        CompilationSubject.assertThat(compilation).failed();
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "Expected overrideSourcePosition to have format "
                        + "string:int:int:int:int").inFile(src).onLine(4);
    }

    @Test
    public void testImplicitMemberSkipped() throws Exception {
        JavaFileObject src = JavaFileObjects.forSourceLines("a.b.Class",
                "package a.b;", // 1
                "import android.compat.annotation.UnsupportedAppUsage;", // 2
                "public class Class {", // 3
                "  @UnsupportedAppUsage(implicitMember=\"foo\")", // 4
                "  public void method() {}", // 5
                "}");
        List<JavaFileObject> generatedNonClassFiles =
                compile(src)
                        .generatedFiles()
                        .stream()
                        .filter(file -> !file.getName().endsWith(".class"))
                        .collect(Collectors.toList());
        assertThat(generatedNonClassFiles).hasSize(0);
    }

    @Test
    public void testExpectedSignatureSucceedsIfMatching() throws Exception {
        JavaFileObject src = JavaFileObjects.forSourceLines("a.b.Class",
                "package a.b;", // 1
                "import android.compat.annotation.UnsupportedAppUsage;", // 2
                "public class Class {", // 3
                "  @UnsupportedAppUsage(expectedSignature=\"La/b/Class;->method()V\")", // 4
                "  public void method() {}", // 5
                "}");
        Compilation compilation =
                Compiler.javac().withProcessors(new UnsupportedAppUsageProcessor())
                        .compile(ANNOTATION, src);
        CompilationSubject.assertThat(compilation).succeeded();
    }

    @Test
    public void testExpectedSignatureThrowsErrorIfWrong() throws Exception {
        JavaFileObject src = JavaFileObjects.forSourceLines("a.b.Class",
                "package a.b;", // 1
                "import android.compat.annotation.UnsupportedAppUsage;", // 2
                "public class Class {", // 3
                "  @UnsupportedAppUsage(expectedSignature=\"expected\")", // 4
                "  public void method() {}", // 5
                "}");
        Compilation compilation =
                Compiler.javac().withProcessors(new UnsupportedAppUsageProcessor())
                        .compile(ANNOTATION, src);
        CompilationSubject.assertThat(compilation).failed();
        CompilationSubject.assertThat(compilation).hadErrorContaining(
                "Expected signature doesn't match generated signature").inFile(src).onLine(5);
    }

}
