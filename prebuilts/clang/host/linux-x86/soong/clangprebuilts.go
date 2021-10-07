//
// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

package clangprebuilts

import (
	"fmt"
	"path"
	"strings"

	"github.com/google/blueprint/proptools"

	"android/soong/android"
	"android/soong/cc"
	"android/soong/cc/config"
	"android/soong/genrule"
)

const libLLVMSoFormat = "libLLVM-%sgit.so"
const libclangSoFormat = "libclang.so.%sgit"
const libclangCxxSoFormat = "libclang_cxx.so.%sgit"
const libcxxSoName = "libc++.so.1"
const libcxxabiSoName = "libc++abi.so.1"

// This module is used to generate libfuzzer, libomp static libraries and
// libclang_rt.* shared libraries. When LLVM_PREBUILTS_VERSION and
// LLVM_RELEASE_VERSION are set, the library will generated from the given
// path.
func init() {
	android.RegisterModuleType("llvm_host_defaults",
		llvmHostDefaultsFactory)
	android.RegisterModuleType("llvm_host_prebuilt_library_shared",
		llvmHostPrebuiltLibrarySharedFactory)
	android.RegisterModuleType("llvm_prebuilt_library_static",
		llvmPrebuiltLibraryStaticFactory)
	android.RegisterModuleType("libclang_rt_prebuilt_library_shared",
		libClangRtPrebuiltLibrarySharedFactory)
	android.RegisterModuleType("libclang_rt_prebuilt_library_static",
		libClangRtPrebuiltLibraryStaticFactory)
	android.RegisterModuleType("libclang_rt_llndk_library",
		libClangRtLLndkLibraryFactory)
	android.RegisterModuleType("llvm_darwin_filegroup",
		llvmDarwinFileGroupFactory)
	android.RegisterModuleType("clang_builtin_headers",
		clangBuiltinHeadersFactory)
}

func getClangPrebuiltDir(ctx android.LoadHookContext) string {
	return path.Join(
		"./",
		ctx.AConfig().GetenvWithDefault("LLVM_PREBUILTS_VERSION", config.ClangDefaultVersion),
	)
}

func getClangResourceDir(ctx android.LoadHookContext) string {
	clangDir := getClangPrebuiltDir(ctx)
	releaseVersion := ctx.AConfig().GetenvWithDefault("LLVM_RELEASE_VERSION",
		config.ClangDefaultShortVersion)
	return path.Join(clangDir, "lib64", "clang", releaseVersion, "lib", "linux")
}

func getSymbolFilePath(ctx android.LoadHookContext) string {
	libDir := getClangResourceDir(ctx)
	return path.Join(libDir, strings.TrimSuffix(ctx.ModuleName(), ".llndk")+".map.txt")
}

func trimVersionNumbers(ver string, retain int) string {
	sep := "."
	versions := strings.Split(ver, sep)
	return strings.Join(versions[0:retain], sep)
}

func getHostLibrary(ctx android.LoadHookContext) string {
	releaseVersion := ctx.AConfig().GetenvWithDefault("LLVM_RELEASE_VERSION",
		config.ClangDefaultShortVersion)

	switch ctx.ModuleName() {
	case "prebuilt_libLLVM_host":
		versionStr := trimVersionNumbers(releaseVersion, 1)
		return fmt.Sprintf(libLLVMSoFormat, versionStr)
	case "prebuilt_libclang_host":
		versionStr := trimVersionNumbers(releaseVersion, 1)
		return fmt.Sprintf(libclangSoFormat, versionStr)
	case "prebuilt_libclang_cxx_host":
		versionStr := trimVersionNumbers(releaseVersion, 1)
		return fmt.Sprintf(libclangCxxSoFormat, versionStr)
	case "prebuilt_libc++_host":
		return libcxxSoName
	case "prebuilt_libc++abi_host":
		return libcxxabiSoName
	default:
		ctx.ModuleErrorf("unsupported host LLVM module: " + ctx.ModuleName())
		return ""
	}
}

func llvmHostPrebuiltLibraryShared(ctx android.LoadHookContext) {
	moduleName := ctx.ModuleName()
	enabled := ctx.AConfig().IsEnvTrue("LLVM_BUILD_HOST_TOOLS")

	clangDir := getClangPrebuiltDir(ctx)

	headerDir := path.Join(clangDir, "include")
	if moduleName == "prebuilt_libc++_host" {
		headerDir = path.Join(headerDir, "c++", "v1")
	}

	linuxLibrary := path.Join(clangDir, "lib64", getHostLibrary(ctx))
	darwinFileGroup := strings.TrimSuffix(strings.TrimPrefix(
		moduleName, "prebuilt_"), "_host") + "_darwin"

	type props struct {
		Enabled             *bool
		Export_include_dirs []string
		Target              struct {
			Linux_glibc_x86_64 struct {
				Srcs []string
			}
			Darwin_x86_64 struct {
				Srcs []string
			}
			Windows struct {
				Enabled *bool
			}
		}
		Stl *string
	}

	p := &props{}
	p.Enabled = proptools.BoolPtr(enabled)
	p.Export_include_dirs = []string{headerDir}
	p.Target.Linux_glibc_x86_64.Srcs = []string{linuxLibrary}
	p.Target.Darwin_x86_64.Srcs = []string{":" + darwinFileGroup}
	p.Target.Windows.Enabled = proptools.BoolPtr(false)
	p.Stl = proptools.StringPtr("none")
	ctx.AppendProperties(p)
}

type archProps struct {
	Android_arm struct {
		Srcs []string
	}
	Android_arm64 struct {
		Srcs []string
	}
	Android_x86 struct {
		Srcs []string
	}
	Android_x86_64 struct {
		Srcs []string
	}
}

func llvmPrebuiltLibraryStatic(ctx android.LoadHookContext) {
	libDir := getClangResourceDir(ctx)
	name := strings.TrimPrefix(ctx.ModuleName(), "prebuilt_") + ".a"

	type props struct {
		Export_include_dirs []string
		Target              archProps
	}

	p := &props{}

	if name == "libFuzzer.a" {
		headerDir := path.Join(getClangPrebuiltDir(ctx), "prebuilt_include", "llvm", "lib", "Fuzzer")
		p.Export_include_dirs = []string{headerDir}
	}

	p.Target.Android_arm.Srcs = []string{path.Join(libDir, "arm", name)}
	p.Target.Android_arm64.Srcs = []string{path.Join(libDir, "aarch64", name)}
	p.Target.Android_x86.Srcs = []string{path.Join(libDir, "i386", name)}
	p.Target.Android_x86_64.Srcs = []string{path.Join(libDir, "x86_64", name)}
	ctx.AppendProperties(p)
}

type prebuiltLibrarySharedProps struct {
	Has_stubs *bool

	Shared_libs []string
}

func libClangRtPrebuiltLibraryShared(ctx android.LoadHookContext, in *prebuiltLibrarySharedProps) {
	if ctx.AConfig().IsEnvTrue("FORCE_BUILD_SANITIZER_SHARED_OBJECTS") {
		return
	}

	libDir := getClangResourceDir(ctx)

	type props struct {
		Srcs               []string
		System_shared_libs []string
		No_libcrt          *bool
		Sanitize           struct {
			Never *bool
		}
		Strip struct {
			None *bool
		}
		Pack_relocations *bool
		Stl              *string
		Stubs            struct {
			Symbol_file *string
			Versions    []string
		}
	}

	p := &props{}

	name := strings.TrimPrefix(ctx.ModuleName(), "prebuilt_")

	p.Srcs = []string{path.Join(libDir, name+".so")}
	p.System_shared_libs = []string{}
	p.No_libcrt = proptools.BoolPtr(true)
	p.Sanitize.Never = proptools.BoolPtr(true)
	p.Strip.None = proptools.BoolPtr(true)
	disable := false
	p.Pack_relocations = &disable
	p.Stl = proptools.StringPtr("none")

	if proptools.Bool(in.Has_stubs) {
		p.Stubs.Versions = []string{"10000"}
		p.Stubs.Symbol_file = proptools.StringPtr(getSymbolFilePath(ctx))
	}

	ctx.AppendProperties(p)
}

func libClangRtPrebuiltLibraryStatic(ctx android.LoadHookContext) {
	libDir := getClangResourceDir(ctx)

	type props struct {
		Srcs               []string
		System_shared_libs []string
		No_libcrt          *bool
		Stl                *string
	}

	name := strings.TrimPrefix(ctx.ModuleName(), "prebuilt_")

	p := &props{}
	if strings.HasSuffix(name, ".static") {
		p.Srcs = []string{path.Join(libDir, strings.TrimSuffix(name, ".static")+".a")}
	} else {
		p.Srcs = []string{path.Join(libDir, name+".a")}
	}
	p.System_shared_libs = []string{}
	p.No_libcrt = proptools.BoolPtr(true)
	p.Stl = proptools.StringPtr("none")
	ctx.AppendProperties(p)
}

func libClangRtLLndkLibrary(ctx android.LoadHookContext) {
	type props struct {
		Symbol_file *string
	}

	p := &props{}
	p.Symbol_file = proptools.StringPtr(getSymbolFilePath(ctx))
	ctx.AppendProperties(p)
}

func llvmDarwinFileGroup(ctx android.LoadHookContext) {
	clangDir := getClangPrebuiltDir(ctx)
	libName := strings.TrimSuffix(ctx.ModuleName(), "_darwin")
	if libName == "libc++" || libName == "libc++abi" {
		libName += ".1"
	}
	lib := path.Join(clangDir, "lib64", libName+".dylib")

	type props struct {
		Srcs []string
	}

	libPath := android.ExistentPathForSource(ctx, ctx.ModuleDir(), lib)
	if libPath.Valid() {
		p := &props{}
		p.Srcs = []string{lib}
		ctx.AppendProperties(p)
	}
}

func llvmPrebuiltLibraryStaticFactory() android.Module {
	module, _ := cc.NewPrebuiltStaticLibrary(android.DeviceSupported)
	android.AddLoadHook(module, llvmPrebuiltLibraryStatic)
	return module.Init()
}

func llvmHostPrebuiltLibrarySharedFactory() android.Module {
	module, _ := cc.NewPrebuiltSharedLibrary(android.HostSupported)
	android.AddLoadHook(module, llvmHostPrebuiltLibraryShared)
	return module.Init()
}

func libClangRtPrebuiltLibrarySharedFactory() android.Module {
	module, _ := cc.NewPrebuiltSharedLibrary(android.HostAndDeviceSupported)
	props := &prebuiltLibrarySharedProps{}
	module.AddProperties(props)
	android.AddLoadHook(module, func(ctx android.LoadHookContext) {
		libClangRtPrebuiltLibraryShared(ctx, props)
	})
	return module.Init()
}

func libClangRtPrebuiltLibraryStaticFactory() android.Module {
	module, _ := cc.NewPrebuiltStaticLibrary(android.HostAndDeviceSupported)
	android.AddLoadHook(module, libClangRtPrebuiltLibraryStatic)
	return module.Init()
}

func libClangRtLLndkLibraryFactory() android.Module {
	module := cc.NewLLndkStubLibrary()
	android.AddLoadHook(module, libClangRtLLndkLibrary)
	return module.Init()
}

func llvmDarwinFileGroupFactory() android.Module {
	module := android.FileGroupFactory()
	android.AddLoadHook(module, llvmDarwinFileGroup)
	return module
}

func llvmHostDefaults(ctx android.LoadHookContext) {
	type props struct {
		Enabled *bool
	}

	p := &props{}
	if !ctx.AConfig().IsEnvTrue("LLVM_BUILD_HOST_TOOLS") {
		p.Enabled = proptools.BoolPtr(false)
	}
	ctx.AppendProperties(p)
}

func llvmHostDefaultsFactory() android.Module {
	module := cc.DefaultsFactory()
	android.AddLoadHook(module, llvmHostDefaults)
	return module
}

func clangBuiltinHeaders(ctx android.LoadHookContext) {
	type props struct {
		Cmd  *string
		Srcs []string
	}

	p := &props{}
	builtinHeadersDir := path.Join(
		getClangPrebuiltDir(ctx), "lib64", "clang",
		ctx.AConfig().GetenvWithDefault("LLVM_RELEASE_VERSION",
			config.ClangDefaultShortVersion), "include")
	s := "$(location) " + path.Join(ctx.ModuleDir(), builtinHeadersDir) + " $(in) >$(out)"
	p.Cmd = &s

	p.Srcs = []string{path.Join(builtinHeadersDir, "**", "*.h")}
	ctx.AppendProperties(p)
}

func clangBuiltinHeadersFactory() android.Module {
	module := genrule.GenRuleFactory()
	android.AddLoadHook(module, clangBuiltinHeaders)
	return module
}
