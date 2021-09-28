// Copyright (C) 2019 The Android Open Source Project
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

package aidl

import (
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/google/blueprint"
	"github.com/google/blueprint/proptools"

	"android/soong/android"
	"android/soong/cc"
	"android/soong/java"
)

var buildDir string

func setUp() {
	var err error
	buildDir, err = ioutil.TempDir("", "soong_aidl_test")
	if err != nil {
		panic(err)
	}
}

func tearDown() {
	os.RemoveAll(buildDir)
}

func TestMain(m *testing.M) {
	run := func() int {
		setUp()
		defer tearDown()

		return m.Run()
	}

	os.Exit(run())
}

type testCustomizer func(fs map[string][]byte, config android.Config)

func withFiles(files map[string][]byte) testCustomizer {
	return func(fs map[string][]byte, config android.Config) {
		for k, v := range files {
			fs[k] = v
		}
	}
}

func setReleaseEnv() testCustomizer {
	return func(_ map[string][]byte, config android.Config) {
		config.TestProductVariables.Platform_sdk_codename = proptools.StringPtr("REL")
		config.TestProductVariables.Platform_sdk_final = proptools.BoolPtr(true)
	}
}

func _testAidl(t *testing.T, bp string, customizers ...testCustomizer) (*android.TestContext, android.Config) {
	t.Helper()

	bp = bp + java.GatherRequiredDepsForTest()
	bp = bp + cc.GatherRequiredDepsForTest(android.Android)
	bp = bp + `
		java_defaults {
			name: "aidl-java-module-defaults",
		}
		cc_defaults {
			name: "aidl-cpp-module-defaults",
		}
		cc_library {
			name: "libbinder",
		}
		cc_library {
			name: "libutils",
		}
		cc_library {
			name: "libcutils",
		}
		cc_library {
			name: "libbinder_ndk",
		}
		ndk_library {
			name: "libbinder_ndk",
			symbol_file: "libbinder_ndk.map.txt",
			first_version: "29",
		}
		aidl_interfaces_metadata {
			name: "aidl_metadata_json",
		}
	`
	fs := map[string][]byte{}

	cc.GatherRequiredFilesForTest(fs)

	for _, c := range customizers {
		// The fs now needs to be populated before creating the config, call customizers twice
		// for now, once to get any fs changes, and later after the config was created to
		// set product variables or targets.
		tempConfig := android.TestArchConfig(buildDir, nil, bp, fs)
		c(fs, tempConfig)
	}

	config := android.TestArchConfig(buildDir, nil, bp, fs)

	// To keep tests stable, fix Platform_sdk_codename and Platform_sdk_final
	// Use setReleaseEnv() to test release version
	config.TestProductVariables.Platform_sdk_codename = proptools.StringPtr("Q")
	config.TestProductVariables.Platform_sdk_final = proptools.BoolPtr(false)

	for _, c := range customizers {
		// The fs now needs to be populated before creating the config, call customizers twice
		// for now, earlier to get any fs changes, and now after the config was created to
		// set product variables or targets.
		tempFS := map[string][]byte{}
		c(tempFS, config)
	}

	ctx := android.NewTestArchContext()
	cc.RegisterRequiredBuildComponentsForTest(ctx)
	ctx.RegisterModuleType("aidl_interface", aidlInterfaceFactory)
	ctx.RegisterModuleType("aidl_interfaces_metadata", aidlInterfacesMetadataSingletonFactory)
	ctx.RegisterModuleType("android_app", java.AndroidAppFactory)
	ctx.RegisterModuleType("java_defaults", func() android.Module {
		return java.DefaultsFactory()
	})
	ctx.RegisterModuleType("java_library_static", java.LibraryStaticFactory)
	ctx.RegisterModuleType("java_library", java.LibraryFactory)
	ctx.RegisterModuleType("java_system_modules", java.SystemModulesFactory)
	ctx.RegisterModuleType("ndk_library", cc.NdkLibraryFactory)

	ctx.PreArchMutators(android.RegisterDefaultsPreArchMutators)
	ctx.PostDepsMutators(android.RegisterOverridePostDepsMutators)
	ctx.PostDepsMutators(func(ctx android.RegisterMutatorsContext) {
		ctx.BottomUp("checkUnstableModule", checkUnstableModuleMutator).Parallel()
	})
	ctx.Register(config)

	return ctx, config
}

func testAidl(t *testing.T, bp string, customizers ...testCustomizer) (*android.TestContext, android.Config) {
	t.Helper()
	ctx, config := _testAidl(t, bp, customizers...)
	_, errs := ctx.ParseFileList(".", []string{"Android.bp"})
	android.FailIfErrored(t, errs)
	_, errs = ctx.PrepareBuildActions(config)
	android.FailIfErrored(t, errs)
	return ctx, config
}

func testAidlError(t *testing.T, pattern, bp string, customizers ...testCustomizer) {
	t.Helper()
	ctx, config := _testAidl(t, bp, customizers...)
	_, errs := ctx.ParseFileList(".", []string{"Android.bp"})
	if len(errs) > 0 {
		android.FailIfNoMatchingErrors(t, pattern, errs)
		return
	}
	_, errs = ctx.PrepareBuildActions(config)
	if len(errs) > 0 {
		android.FailIfNoMatchingErrors(t, pattern, errs)
		return
	}
	t.Fatalf("missing expected error %q (0 errors are returned)", pattern)
}

// asserts that there are expected module regardless of variants
func assertModulesExists(t *testing.T, ctx *android.TestContext, names ...string) {
	missing := []string{}
	for _, name := range names {
		variants := ctx.ModuleVariantsForTests(name)
		if len(variants) == 0 {
			missing = append(missing, name)
		}
	}
	if len(missing) > 0 {
		// find all the modules that do exist
		allModuleNames := make(map[string]bool)
		ctx.VisitAllModules(func(m blueprint.Module) {
			allModuleNames[ctx.ModuleName(m)] = true
		})
		t.Errorf("expected modules(%v) not found. all modules: %v", missing, android.SortedStringKeys(allModuleNames))
	}
}

// Vintf module must have versions in release version
func TestVintfWithoutVersionInRelease(t *testing.T) {
	vintfWithoutVersionBp := `
	aidl_interface {
		name: "foo",
		stability: "vintf",
		srcs: [
			"IFoo.aidl",
		],
	}`
	expectedError := `module "foo_interface": versions: must be set \(need to be frozen\) when "unstable" is false and PLATFORM_VERSION_CODENAME is REL`
	testAidlError(t, expectedError, vintfWithoutVersionBp, setReleaseEnv())

	ctx, _ := testAidl(t, vintfWithoutVersionBp)
	assertModulesExists(t, ctx, "foo-java", "foo-cpp", "foo-ndk", "foo-ndk_platform")
}

// Check if using unstable version in release cause an error.
func TestUnstableVersionUsageInRelease(t *testing.T) {
	unstableVersionUsageInJavaBp := `
	aidl_interface {
		name: "foo",
		versions: [
			"1",
		],
		srcs: [
			"IFoo.aidl",
		],
	}
	java_library {
		name: "bar",
		libs: ["foo-unstable-java"],
	}`

	expectedError := `unstable-java is disallowed in release version because it is unstable.`
	testAidlError(t, expectedError, unstableVersionUsageInJavaBp, setReleaseEnv(), withFiles(map[string][]byte{
		"aidl_api/foo/1/foo.1.aidl": nil,
	}))

	testAidl(t, unstableVersionUsageInJavaBp, withFiles(map[string][]byte{
		"aidl_api/foo/1/foo.1.aidl": nil,
	}))

	// A stable version can be used in release version
	stableVersionUsageInJavaBp := `
	aidl_interface {
		name: "foo",
		versions: [
			"1",
		],
		srcs: [
			"IFoo.aidl",
		],
	}
	java_library {
		name: "bar",
		libs: ["foo-java"],
	}`

	testAidl(t, stableVersionUsageInJavaBp, setReleaseEnv(), withFiles(map[string][]byte{
		"aidl_api/foo/1/foo.1.aidl": nil,
	}))

	testAidl(t, stableVersionUsageInJavaBp, withFiles(map[string][]byte{
		"aidl_api/foo/1/foo.1.aidl": nil,
	}))
}

// The module which has never been frozen and is not "unstable" is not allowed in release version.
func TestNonVersionedModuleUsageInRelease(t *testing.T) {
	nonVersionedModuleUsageInJavaBp := `
	aidl_interface {
		name: "foo",
		srcs: [
			"IFoo.aidl",
		],
	}

	java_library {
		name: "bar",
		libs: ["foo-java"],
	}`

	expectedError := `"foo_interface": versions: must be set \(need to be frozen\) when "unstable" is false and PLATFORM_VERSION_CODENAME is REL.`
	testAidlError(t, expectedError, nonVersionedModuleUsageInJavaBp, setReleaseEnv())
	testAidl(t, nonVersionedModuleUsageInJavaBp)

	nonVersionedUnstableModuleUsageInJavaBp := `
	aidl_interface {
		name: "foo",
		srcs: [
			"IFoo.aidl",
		],
		unstable: true,
	}

	java_library {
		name: "bar",
		libs: ["foo-java"],
	}`

	testAidl(t, nonVersionedUnstableModuleUsageInJavaBp, setReleaseEnv())
	testAidl(t, nonVersionedUnstableModuleUsageInJavaBp)
}

func TestUnstableModules(t *testing.T) {
	testAidlError(t, `module "foo_interface": stability: must be empty when "unstable" is true`, `
		aidl_interface {
			name: "foo",
			stability: "vintf",
			unstable: true,
			srcs: [
				"IFoo.aidl",
			],
		}
	`)

	testAidlError(t, `module "foo_interface": versions: cannot have versions for an unstable interface`, `
		aidl_interface {
			name: "foo",
			versions: [
				"1",
			],
			unstable: true,
			srcs: [
				"IFoo.aidl",
			],
		}
	`)

	ctx, _ := testAidl(t, `
		aidl_interface {
			name: "foo",
			unstable: true,
			srcs: [
				"IFoo.aidl",
			],
		}
	`)

	assertModulesExists(t, ctx, "foo-java", "foo-cpp", "foo-ndk", "foo-ndk_platform")
}

func TestCreatesModulesWithNoVersions(t *testing.T) {
	ctx, _ := testAidl(t, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
		}
	`)

	assertModulesExists(t, ctx, "foo-java", "foo-cpp", "foo-ndk", "foo-ndk_platform")
}

func TestCreatesModulesWithFrozenVersions(t *testing.T) {
	// Each version should be under aidl_api/<name>/<ver>
	testAidlError(t, `aidl_api/foo/1`, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
			versions: [
				"1",
			],
		}
	`)

	ctx, _ := testAidl(t, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
			versions: [
				"1",
			],
		}
	`, withFiles(map[string][]byte{
		"aidl_api/foo/1/foo.1.aidl": nil,
	}))

	// For alias for the latest frozen version (=1)
	assertModulesExists(t, ctx, "foo-java", "foo-cpp", "foo-ndk", "foo-ndk_platform")

	// For frozen version "1"
	// Note that it is not yet implemented to generate native modules for latest frozen version
	assertModulesExists(t, ctx, "foo-V1-java")

	// For ToT (current)
	assertModulesExists(t, ctx, "foo-unstable-java", "foo-unstable-cpp", "foo-unstable-ndk", "foo-unstable-ndk_platform")
}

const (
	androidVariant = "android_common"
	nativeVariant  = "android_arm_armv7-a-neon_shared"
)

func TestNativeOutputIsAlwaysVersioned(t *testing.T) {
	var ctx *android.TestContext
	assertOutput := func(moduleName, variant, outputFilename string) {
		t.Helper()
		producer, ok := ctx.ModuleForTests(moduleName, variant).Module().(android.OutputFileProducer)
		if !ok {
			t.Errorf("%s(%s): should be OutputFileProducer.", moduleName, variant)
		}
		paths, err := producer.OutputFiles("")
		if err != nil {
			t.Errorf("%s(%s): failed to get OutputFiles: %v", moduleName, variant, err)
		}
		if len(paths) != 1 || paths[0].Base() != outputFilename {
			t.Errorf("%s(%s): expected output %q, but got %v", moduleName, variant, outputFilename, paths)
		}
	}

	// No versions
	ctx, _ = testAidl(t, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
		}
	`)

	assertOutput("foo-java", androidVariant, "foo-java.jar")
	assertOutput("foo-cpp", nativeVariant, "foo-V1-cpp.so")

	// With versions: "1", "2"
	ctx, _ = testAidl(t, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
			versions: [
				"1", "2",
			],
		}
	`, withFiles(map[string][]byte{
		"aidl_api/foo/1/foo.1.aidl": nil,
		"aidl_api/foo/2/foo.2.aidl": nil,
	}))

	// alias for the latest frozen version (=2)
	assertOutput("foo-java", androidVariant, "foo-java.jar")
	assertOutput("foo-cpp", nativeVariant, "foo-V2-cpp.so")

	// frozen "1"
	assertOutput("foo-V1-java", androidVariant, "foo-V1-java.jar")
	assertOutput("foo-V1-cpp", nativeVariant, "foo-V1-cpp.so")

	// tot
	assertOutput("foo-unstable-java", androidVariant, "foo-unstable-java.jar")
	assertOutput("foo-unstable-cpp", nativeVariant, "foo-V3-cpp.so")

	// skip ndk/ndk_platform since they follow the same rule with cpp
}

func TestGenLogForNativeBackendRequiresJson(t *testing.T) {
	testAidlError(t, `"foo-cpp" depends on .*"libjsoncpp"`, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
			backend: {
				cpp: {
					gen_log: true,
				},
			},
		}
	`)
	testAidl(t, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
			backend: {
				cpp: {
					gen_log: true,
				},
			},
		}
		cc_library {
			name: "libjsoncpp",
		}
	`)
}

func TestImports(t *testing.T) {
	testAidlError(t, `Import does not exist:`, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
			imports: [
				"bar",
			]
		}
	`)

	testAidlError(t, `backend.java.enabled: Java backend not enabled in the imported AIDL interface "bar"`, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
			imports: [
				"bar",
			]
		}
		aidl_interface {
			name: "bar",
			srcs: [
				"IBar.aidl",
			],
			backend: {
				java: {
					enabled: false,
				},
			},
		}
	`)

	testAidlError(t, `backend.cpp.enabled: C\+\+ backend not enabled in the imported AIDL interface "bar"`, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
			imports: [
				"bar",
			]
		}
		aidl_interface {
			name: "bar",
			srcs: [
				"IBar.aidl",
			],
			backend: {
				cpp: {
					enabled: false,
				},
			},
		}
	`)

	ctx, _ := testAidl(t, `
		aidl_interface {
			name: "foo",
			srcs: [
				"IFoo.aidl",
			],
			imports: [
				"bar",
			]
		}
		aidl_interface {
			name: "bar",
			srcs: [
				"IBar.aidl",
			],
		}
	`)

	ldRule := ctx.ModuleForTests("foo-cpp", nativeVariant).Rule("ld")
	libFlags := ldRule.Args["libFlags"]
	libBar := filepath.Join("bar-cpp", nativeVariant, "bar-V1-cpp.so")
	if !strings.Contains(libFlags, libBar) {
		t.Errorf("%q is not found in %q", libBar, libFlags)
	}
}
