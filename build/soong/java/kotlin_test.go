// Copyright 2017 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package java

import (
	"android/soong/android"
	"strconv"
	"strings"
	"testing"
)

func TestKotlin(t *testing.T) {
	ctx, _ := testJava(t, `
		java_library {
			name: "foo",
			srcs: ["a.java", "b.kt"],
		}

		java_library {
			name: "bar",
			srcs: ["b.kt"],
			libs: ["foo"],
			static_libs: ["baz"],
		}

		java_library {
			name: "baz",
			srcs: ["c.java"],
		}
		`)

	fooKotlinc := ctx.ModuleForTests("foo", "android_common").Rule("kotlinc")
	fooJavac := ctx.ModuleForTests("foo", "android_common").Rule("javac")
	fooJar := ctx.ModuleForTests("foo", "android_common").Output("combined/foo.jar")

	if len(fooKotlinc.Inputs) != 2 || fooKotlinc.Inputs[0].String() != "a.java" ||
		fooKotlinc.Inputs[1].String() != "b.kt" {
		t.Errorf(`foo kotlinc inputs %v != ["a.java", "b.kt"]`, fooKotlinc.Inputs)
	}

	if len(fooJavac.Inputs) != 1 || fooJavac.Inputs[0].String() != "a.java" {
		t.Errorf(`foo inputs %v != ["a.java"]`, fooJavac.Inputs)
	}

	if !strings.Contains(fooJavac.Args["classpath"], fooKotlinc.Output.String()) {
		t.Errorf("foo classpath %v does not contain %q",
			fooJavac.Args["classpath"], fooKotlinc.Output.String())
	}

	if !inList(fooKotlinc.Output.String(), fooJar.Inputs.Strings()) {
		t.Errorf("foo jar inputs %v does not contain %q",
			fooJar.Inputs.Strings(), fooKotlinc.Output.String())
	}

	fooHeaderJar := ctx.ModuleForTests("foo", "android_common").Output("turbine-combined/foo.jar")
	bazHeaderJar := ctx.ModuleForTests("baz", "android_common").Output("turbine-combined/baz.jar")
	barKotlinc := ctx.ModuleForTests("bar", "android_common").Rule("kotlinc")

	if len(barKotlinc.Inputs) != 1 || barKotlinc.Inputs[0].String() != "b.kt" {
		t.Errorf(`bar kotlinc inputs %v != ["b.kt"]`, barKotlinc.Inputs)
	}

	if !inList(fooHeaderJar.Output.String(), barKotlinc.Implicits.Strings()) {
		t.Errorf(`expected %q in bar implicits %v`,
			fooHeaderJar.Output.String(), barKotlinc.Implicits.Strings())
	}

	if !inList(bazHeaderJar.Output.String(), barKotlinc.Implicits.Strings()) {
		t.Errorf(`expected %q in bar implicits %v`,
			bazHeaderJar.Output.String(), barKotlinc.Implicits.Strings())
	}
}

func TestKapt(t *testing.T) {
	ctx, _ := testJava(t, `
		java_library {
			name: "foo",
			srcs: ["a.java", "b.kt"],
			plugins: ["bar", "baz"],
		}

		java_plugin {
			name: "bar",
			processor_class: "com.bar",
			srcs: ["b.java"],
		}

		java_plugin {
			name: "baz",
			processor_class: "com.baz",
			srcs: ["b.java"],
		}
		`)

	buildOS := android.BuildOs.String()

	kapt := ctx.ModuleForTests("foo", "android_common").Rule("kapt")
	kotlinc := ctx.ModuleForTests("foo", "android_common").Rule("kotlinc")
	javac := ctx.ModuleForTests("foo", "android_common").Rule("javac")

	bar := ctx.ModuleForTests("bar", buildOS+"_common").Rule("javac").Output.String()
	baz := ctx.ModuleForTests("baz", buildOS+"_common").Rule("javac").Output.String()

	// Test that the kotlin and java sources are passed to kapt and kotlinc
	if len(kapt.Inputs) != 2 || kapt.Inputs[0].String() != "a.java" || kapt.Inputs[1].String() != "b.kt" {
		t.Errorf(`foo kapt inputs %v != ["a.java", "b.kt"]`, kapt.Inputs)
	}
	if len(kotlinc.Inputs) != 2 || kotlinc.Inputs[0].String() != "a.java" || kotlinc.Inputs[1].String() != "b.kt" {
		t.Errorf(`foo kotlinc inputs %v != ["a.java", "b.kt"]`, kotlinc.Inputs)
	}

	// Test that only the java sources are passed to javac
	if len(javac.Inputs) != 1 || javac.Inputs[0].String() != "a.java" {
		t.Errorf(`foo inputs %v != ["a.java"]`, javac.Inputs)
	}

	// Test that the kapt srcjar is a dependency of kotlinc and javac rules
	if !inList(kapt.Output.String(), kotlinc.Implicits.Strings()) {
		t.Errorf("expected %q in kotlinc implicits %v", kapt.Output.String(), kotlinc.Implicits.Strings())
	}
	if !inList(kapt.Output.String(), javac.Implicits.Strings()) {
		t.Errorf("expected %q in javac implicits %v", kapt.Output.String(), javac.Implicits.Strings())
	}

	// Test that the kapt srcjar is extracted by the kotlinc and javac rules
	if kotlinc.Args["srcJars"] != kapt.Output.String() {
		t.Errorf("expected %q in kotlinc srcjars %v", kapt.Output.String(), kotlinc.Args["srcJars"])
	}
	if javac.Args["srcJars"] != kapt.Output.String() {
		t.Errorf("expected %q in javac srcjars %v", kapt.Output.String(), kotlinc.Args["srcJars"])
	}

	// Test that the processors are passed to kapt
	expectedProcessorPath := "-P plugin:org.jetbrains.kotlin.kapt3:apclasspath=" + bar +
		" -P plugin:org.jetbrains.kotlin.kapt3:apclasspath=" + baz
	if kapt.Args["kaptProcessorPath"] != expectedProcessorPath {
		t.Errorf("expected kaptProcessorPath %q, got %q", expectedProcessorPath, kapt.Args["kaptProcessorPath"])
	}
	expectedProcessor := "-P plugin:org.jetbrains.kotlin.kapt3:processors=com.bar -P plugin:org.jetbrains.kotlin.kapt3:processors=com.baz"
	if kapt.Args["kaptProcessor"] != expectedProcessor {
		t.Errorf("expected kaptProcessor %q, got %q", expectedProcessor, kapt.Args["kaptProcessor"])
	}

	// Test that the processors are not passed to javac
	if javac.Args["processorPath"] != "" {
		t.Errorf("expected processorPath '', got %q", javac.Args["processorPath"])
	}
	if javac.Args["processor"] != "-proc:none" {
		t.Errorf("expected processor '-proc:none', got %q", javac.Args["processor"])
	}
}

func TestKaptEncodeFlags(t *testing.T) {
	// Compares the kaptEncodeFlags against the results of the example implementation at
	// https://kotlinlang.org/docs/reference/kapt.html#apjavac-options-encoding
	tests := []struct {
		in  [][2]string
		out string
	}{
		{
			// empty input
			in:  [][2]string{},
			out: "rO0ABXcEAAAAAA==",
		},
		{
			// common input
			in: [][2]string{
				{"-source", "1.8"},
				{"-target", "1.8"},
			},
			out: "rO0ABXcgAAAAAgAHLXNvdXJjZQADMS44AActdGFyZ2V0AAMxLjg=",
		},
		{
			// input that serializes to a 255 byte block
			in: [][2]string{
				{"-source", "1.8"},
				{"-target", "1.8"},
				{"a", strings.Repeat("b", 218)},
			},
			out: "rO0ABXf/AAAAAwAHLXNvdXJjZQADMS44AActdGFyZ2V0AAMxLjgAAWEA2mJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJi",
		},
		{
			// input that serializes to a 256 byte block
			in: [][2]string{
				{"-source", "1.8"},
				{"-target", "1.8"},
				{"a", strings.Repeat("b", 219)},
			},
			out: "rO0ABXoAAAEAAAAAAwAHLXNvdXJjZQADMS44AActdGFyZ2V0AAMxLjgAAWEA22JiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYg==",
		},
		{
			// input that serializes to a 257 byte block
			in: [][2]string{
				{"-source", "1.8"},
				{"-target", "1.8"},
				{"a", strings.Repeat("b", 220)},
			},
			out: "rO0ABXoAAAEBAAAAAwAHLXNvdXJjZQADMS44AActdGFyZ2V0AAMxLjgAAWEA3GJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmI=",
		},
	}

	for i, test := range tests {
		t.Run(strconv.Itoa(i), func(t *testing.T) {
			got := kaptEncodeFlags(test.in)
			if got != test.out {
				t.Errorf("\nwant %q\n got %q", test.out, got)
			}
		})
	}
}
