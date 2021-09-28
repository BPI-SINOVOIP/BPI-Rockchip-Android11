// Copyright 2015 Google Inc. All rights reserved.
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

package android

import (
	"encoding"
	"fmt"
	"reflect"
	"runtime"
	"strconv"
	"strings"

	"github.com/google/blueprint"
	"github.com/google/blueprint/proptools"
)

const COMMON_VARIANT = "common"

var (
	archTypeList []ArchType

	Arm    = newArch("arm", "lib32")
	Arm64  = newArch("arm64", "lib64")
	Mips   = newArch("mips", "lib32")
	Mips64 = newArch("mips64", "lib64")
	X86    = newArch("x86", "lib32")
	X86_64 = newArch("x86_64", "lib64")

	Common = ArchType{
		Name: COMMON_VARIANT,
	}
)

var archTypeMap = map[string]ArchType{
	"arm":    Arm,
	"arm64":  Arm64,
	"mips":   Mips,
	"mips64": Mips64,
	"x86":    X86,
	"x86_64": X86_64,
}

/*
Example blueprints file containing all variant property groups, with comment listing what type
of variants get properties in that group:

module {
    arch: {
        arm: {
            // Host or device variants with arm architecture
        },
        arm64: {
            // Host or device variants with arm64 architecture
        },
        mips: {
            // Host or device variants with mips architecture
        },
        mips64: {
            // Host or device variants with mips64 architecture
        },
        x86: {
            // Host or device variants with x86 architecture
        },
        x86_64: {
            // Host or device variants with x86_64 architecture
        },
    },
    multilib: {
        lib32: {
            // Host or device variants for 32-bit architectures
        },
        lib64: {
            // Host or device variants for 64-bit architectures
        },
    },
    target: {
        android: {
            // Device variants
        },
        host: {
            // Host variants
        },
        linux_glibc: {
            // Linux host variants
        },
        darwin: {
            // Darwin host variants
        },
        windows: {
            // Windows host variants
        },
        not_windows: {
            // Non-windows host variants
        },
    },
}
*/

var archVariants = map[ArchType][]string{
	Arm: {
		"armv7-a",
		"armv7-a-neon",
		"armv8-a",
		"armv8-2a",
		"cortex-a7",
		"cortex-a8",
		"cortex-a9",
		"cortex-a15",
		"cortex-a53",
		"cortex-a53-a57",
		"cortex-a55",
		"cortex-a72",
		"cortex-a73",
		"cortex-a75",
		"cortex-a76",
		"krait",
		"kryo",
		"kryo385",
		"exynos-m1",
		"exynos-m2",
	},
	Arm64: {
		"armv8_a",
		"armv8_2a",
		"cortex-a53",
		"cortex-a55",
		"cortex-a72",
		"cortex-a73",
		"cortex-a75",
		"cortex-a76",
		"kryo",
		"kryo385",
		"exynos-m1",
		"exynos-m2",
	},
	Mips: {
		"mips32_fp",
		"mips32r2_fp",
		"mips32r2_fp_xburst",
		"mips32r2dsp_fp",
		"mips32r2dspr2_fp",
		"mips32r6",
	},
	Mips64: {
		"mips64r2",
		"mips64r6",
	},
	X86: {
		"amberlake",
		"atom",
		"broadwell",
		"haswell",
		"icelake",
		"ivybridge",
		"kabylake",
		"sandybridge",
		"silvermont",
		"skylake",
		"stoneyridge",
		"tigerlake",
		"whiskeylake",
		"x86_64",
	},
	X86_64: {
		"amberlake",
		"broadwell",
		"haswell",
		"icelake",
		"ivybridge",
		"kabylake",
		"sandybridge",
		"silvermont",
		"skylake",
		"stoneyridge",
		"tigerlake",
		"whiskeylake",
	},
}

var archFeatures = map[ArchType][]string{
	Arm: {
		"neon",
	},
	Mips: {
		"dspr2",
		"rev6",
		"msa",
	},
	Mips64: {
		"rev6",
		"msa",
	},
	X86: {
		"ssse3",
		"sse4",
		"sse4_1",
		"sse4_2",
		"aes_ni",
		"avx",
		"avx2",
		"avx512",
		"popcnt",
		"movbe",
	},
	X86_64: {
		"ssse3",
		"sse4",
		"sse4_1",
		"sse4_2",
		"aes_ni",
		"avx",
		"avx2",
		"avx512",
		"popcnt",
	},
}

var archFeatureMap = map[ArchType]map[string][]string{
	Arm: {
		"armv7-a-neon": {
			"neon",
		},
		"armv8-a": {
			"neon",
		},
		"armv8-2a": {
			"neon",
		},
	},
	Mips: {
		"mips32r2dspr2_fp": {
			"dspr2",
		},
		"mips32r6": {
			"rev6",
		},
	},
	Mips64: {
		"mips64r6": {
			"rev6",
		},
	},
	X86: {
		"amberlake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"aes_ni",
			"popcnt",
		},
		"atom": {
			"ssse3",
			"movbe",
		},
		"broadwell": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"aes_ni",
			"popcnt",
		},
		"haswell": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"aes_ni",
			"avx",
			"popcnt",
			"movbe",
		},
		"icelake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"avx512",
			"aes_ni",
			"popcnt",
		},
		"ivybridge": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"aes_ni",
			"avx",
			"popcnt",
		},
		"kabylake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"aes_ni",
			"popcnt",
		},
		"sandybridge": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"popcnt",
		},
		"silvermont": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"aes_ni",
			"popcnt",
			"movbe",
		},
		"skylake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"avx512",
			"aes_ni",
			"popcnt",
		},
		"stoneyridge": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"aes_ni",
			"avx",
			"avx2",
			"popcnt",
			"movbe",
		},
		"tigerlake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"avx512",
			"aes_ni",
			"popcnt",
		},
		"whiskeylake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"avx512",
			"aes_ni",
			"popcnt",
		},
		"x86_64": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"popcnt",
		},
	},
	X86_64: {
		"amberlake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"aes_ni",
			"popcnt",
		},
		"broadwell": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"aes_ni",
			"popcnt",
		},
		"haswell": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"aes_ni",
			"avx",
			"popcnt",
		},
		"icelake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"avx512",
			"aes_ni",
			"popcnt",
		},
		"ivybridge": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"aes_ni",
			"avx",
			"popcnt",
		},
		"kabylake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"aes_ni",
			"popcnt",
		},
		"sandybridge": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"popcnt",
		},
		"silvermont": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"aes_ni",
			"popcnt",
		},
		"skylake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"avx512",
			"aes_ni",
			"popcnt",
		},
		"stoneyridge": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"aes_ni",
			"avx",
			"avx2",
			"popcnt",
		},
		"tigerlake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"avx512",
			"aes_ni",
			"popcnt",
		},
		"whiskeylake": {
			"ssse3",
			"sse4",
			"sse4_1",
			"sse4_2",
			"avx",
			"avx2",
			"avx512",
			"aes_ni",
			"popcnt",
		},
	},
}

var defaultArchFeatureMap = map[OsType]map[ArchType][]string{}

func RegisterDefaultArchVariantFeatures(os OsType, arch ArchType, features ...string) {
	checkCalledFromInit()

	for _, feature := range features {
		if !InList(feature, archFeatures[arch]) {
			panic(fmt.Errorf("Invalid feature %q for arch %q variant \"\"", feature, arch))
		}
	}

	if defaultArchFeatureMap[os] == nil {
		defaultArchFeatureMap[os] = make(map[ArchType][]string)
	}
	defaultArchFeatureMap[os][arch] = features
}

// An Arch indicates a single CPU architecture.
type Arch struct {
	ArchType     ArchType
	ArchVariant  string
	CpuVariant   string
	Abi          []string
	ArchFeatures []string
}

func (a Arch) String() string {
	s := a.ArchType.String()
	if a.ArchVariant != "" {
		s += "_" + a.ArchVariant
	}
	if a.CpuVariant != "" {
		s += "_" + a.CpuVariant
	}
	return s
}

type ArchType struct {
	Name     string
	Field    string
	Multilib string
}

func newArch(name, multilib string) ArchType {
	archType := ArchType{
		Name:     name,
		Field:    proptools.FieldNameForProperty(name),
		Multilib: multilib,
	}
	archTypeList = append(archTypeList, archType)
	return archType
}

func ArchTypeList() []ArchType {
	return append([]ArchType(nil), archTypeList...)
}

func (a ArchType) String() string {
	return a.Name
}

var _ encoding.TextMarshaler = ArchType{}

func (a ArchType) MarshalText() ([]byte, error) {
	return []byte(strconv.Quote(a.String())), nil
}

var _ encoding.TextUnmarshaler = &ArchType{}

func (a *ArchType) UnmarshalText(text []byte) error {
	if u, ok := archTypeMap[string(text)]; ok {
		*a = u
		return nil
	}

	return fmt.Errorf("unknown ArchType %q", text)
}

var BuildOs = func() OsType {
	switch runtime.GOOS {
	case "linux":
		return Linux
	case "darwin":
		return Darwin
	default:
		panic(fmt.Sprintf("unsupported OS: %s", runtime.GOOS))
	}
}()

var (
	OsTypeList      []OsType
	commonTargetMap = make(map[string]Target)

	NoOsType    OsType
	Linux       = NewOsType("linux_glibc", Host, false)
	Darwin      = NewOsType("darwin", Host, false)
	LinuxBionic = NewOsType("linux_bionic", Host, false)
	Windows     = NewOsType("windows", HostCross, true)
	Android     = NewOsType("android", Device, false)
	Fuchsia     = NewOsType("fuchsia", Device, false)

	// A pseudo OSType for a common os variant, which is OSType agnostic and which
	// has dependencies on all the OS variants.
	CommonOS = NewOsType("common_os", Generic, false)

	osArchTypeMap = map[OsType][]ArchType{
		Linux:       []ArchType{X86, X86_64},
		LinuxBionic: []ArchType{X86_64},
		Darwin:      []ArchType{X86_64},
		Windows:     []ArchType{X86, X86_64},
		Android:     []ArchType{Arm, Arm64, Mips, Mips64, X86, X86_64},
		Fuchsia:     []ArchType{Arm64, X86_64},
	}
)

type OsType struct {
	Name, Field string
	Class       OsClass

	DefaultDisabled bool
}

type OsClass int

const (
	Generic OsClass = iota
	Device
	Host
	HostCross
)

func (class OsClass) String() string {
	switch class {
	case Generic:
		return "generic"
	case Device:
		return "device"
	case Host:
		return "host"
	case HostCross:
		return "host cross"
	default:
		panic(fmt.Errorf("unknown class %d", class))
	}
}

func (os OsType) String() string {
	return os.Name
}

func (os OsType) Bionic() bool {
	return os == Android || os == LinuxBionic
}

func (os OsType) Linux() bool {
	return os == Android || os == Linux || os == LinuxBionic
}

func NewOsType(name string, class OsClass, defDisabled bool) OsType {
	os := OsType{
		Name:  name,
		Field: strings.Title(name),
		Class: class,

		DefaultDisabled: defDisabled,
	}
	OsTypeList = append(OsTypeList, os)

	if _, found := commonTargetMap[name]; found {
		panic(fmt.Errorf("Found Os type duplicate during OsType registration: %q", name))
	} else {
		commonTargetMap[name] = Target{Os: os, Arch: Arch{ArchType: Common}}
	}

	return os
}

func osByName(name string) OsType {
	for _, os := range OsTypeList {
		if os.Name == name {
			return os
		}
	}

	return NoOsType
}

type NativeBridgeSupport bool

const (
	NativeBridgeDisabled NativeBridgeSupport = false
	NativeBridgeEnabled  NativeBridgeSupport = true
)

type Target struct {
	Os                       OsType
	Arch                     Arch
	NativeBridge             NativeBridgeSupport
	NativeBridgeHostArchName string
	NativeBridgeRelativePath string
}

func (target Target) String() string {
	return target.OsVariation() + "_" + target.ArchVariation()
}

func (target Target) OsVariation() string {
	return target.Os.String()
}

func (target Target) ArchVariation() string {
	var variation string
	if target.NativeBridge {
		variation = "native_bridge_"
	}
	variation += target.Arch.String()

	return variation
}

func (target Target) Variations() []blueprint.Variation {
	return []blueprint.Variation{
		{Mutator: "os", Variation: target.OsVariation()},
		{Mutator: "arch", Variation: target.ArchVariation()},
	}
}

func osMutator(mctx BottomUpMutatorContext) {
	var module Module
	var ok bool
	if module, ok = mctx.Module().(Module); !ok {
		return
	}

	base := module.base()

	if !base.ArchSpecific() {
		return
	}

	osClasses := base.OsClassSupported()

	var moduleOSList []OsType

	for _, os := range OsTypeList {
		supportedClass := false
		for _, osClass := range osClasses {
			if os.Class == osClass {
				supportedClass = true
			}
		}
		if !supportedClass {
			continue
		}

		if len(mctx.Config().Targets[os]) == 0 {
			continue
		}

		moduleOSList = append(moduleOSList, os)
	}

	if len(moduleOSList) == 0 {
		base.Disable()
		return
	}

	osNames := make([]string, len(moduleOSList))

	for i, os := range moduleOSList {
		osNames[i] = os.String()
	}

	createCommonOSVariant := base.commonProperties.CreateCommonOSVariant
	if createCommonOSVariant {
		// A CommonOS variant was requested so add it to the list of OS's variants to
		// create. It needs to be added to the end because it needs to depend on the
		// the other variants in the list returned by CreateVariations(...) and inter
		// variant dependencies can only be created from a later variant in that list to
		// an earlier one. That is because variants are always processed in the order in
		// which they are returned from CreateVariations(...).
		osNames = append(osNames, CommonOS.Name)
		moduleOSList = append(moduleOSList, CommonOS)
	}

	modules := mctx.CreateVariations(osNames...)
	for i, m := range modules {
		m.base().commonProperties.CompileOS = moduleOSList[i]
		m.base().setOSProperties(mctx)
	}

	if createCommonOSVariant {
		// A CommonOS variant was requested so add dependencies from it (the last one in
		// the list) to the OS type specific variants.
		last := len(modules) - 1
		commonOSVariant := modules[last]
		commonOSVariant.base().commonProperties.CommonOSVariant = true
		for _, module := range modules[0:last] {
			// Ignore modules that are enabled. Note, this will only avoid adding
			// dependencies on OsType variants that are explicitly disabled in their
			// properties. The CommonOS variant will still depend on disabled variants
			// if they are disabled afterwards, e.g. in archMutator if
			if module.Enabled() {
				mctx.AddInterVariantDependency(commonOsToOsSpecificVariantTag, commonOSVariant, module)
			}
		}
	}
}

// Identifies the dependency from CommonOS variant to the os specific variants.
type commonOSTag struct{ blueprint.BaseDependencyTag }

var commonOsToOsSpecificVariantTag = commonOSTag{}

// Get the OsType specific variants for the current CommonOS variant.
//
// The returned list will only contain enabled OsType specific variants of the
// module referenced in the supplied context. An empty list is returned if there
// are no enabled variants or the supplied context is not for an CommonOS
// variant.
func GetOsSpecificVariantsOfCommonOSVariant(mctx BaseModuleContext) []Module {
	var variants []Module
	mctx.VisitDirectDeps(func(m Module) {
		if mctx.OtherModuleDependencyTag(m) == commonOsToOsSpecificVariantTag {
			if m.Enabled() {
				variants = append(variants, m)
			}
		}
	})

	return variants
}

// archMutator splits a module into a variant for each Target requested by the module.  Target selection
// for a module is in three levels, OsClass, mulitlib, and then Target.
// OsClass selection is determined by:
//    - The HostOrDeviceSupported value passed in to InitAndroidArchModule by the module type factory, which selects
//      whether the module type can compile for host, device or both.
//    - The host_supported and device_supported properties on the module.
// If host is supported for the module, the Host and HostCross OsClasses are selected.  If device is supported
// for the module, the Device OsClass is selected.
// Within each selected OsClass, the multilib selection is determined by:
//    - The compile_multilib property if it set (which may be overridden by target.android.compile_multilib or
//      target.host.compile_multilib).
//    - The default multilib passed to InitAndroidArchModule if compile_multilib was not set.
// Valid multilib values include:
//    "both": compile for all Targets supported by the OsClass (generally x86_64 and x86, or arm64 and arm).
//    "first": compile for only a single preferred Target supported by the OsClass.  This is generally x86_64 or arm64,
//        but may be arm for a 32-bit only build or a build with TARGET_PREFER_32_BIT=true set.
//    "32": compile for only a single 32-bit Target supported by the OsClass.
//    "64": compile for only a single 64-bit Target supported by the OsClass.
//    "common": compile a for a single Target that will work on all Targets suported by the OsClass (for example Java).
//
// Once the list of Targets is determined, the module is split into a variant for each Target.
//
// Modules can be initialized with InitAndroidMultiTargetsArchModule, in which case they will be split by OsClass,
// but will have a common Target that is expected to handle all other selected Targets via ctx.MultiTargets().
func archMutator(mctx BottomUpMutatorContext) {
	var module Module
	var ok bool
	if module, ok = mctx.Module().(Module); !ok {
		return
	}

	base := module.base()

	if !base.ArchSpecific() {
		return
	}

	os := base.commonProperties.CompileOS
	if os == CommonOS {
		// Make sure that the target related properties are initialized for the
		// CommonOS variant.
		addTargetProperties(module, commonTargetMap[os.Name], nil, true)

		// Do not create arch specific variants for the CommonOS variant.
		return
	}

	osTargets := mctx.Config().Targets[os]
	image := base.commonProperties.ImageVariation
	// Filter NativeBridge targets unless they are explicitly supported
	// Skip creating native bridge variants for vendor modules
	if os == Android &&
		!(Bool(base.commonProperties.Native_bridge_supported) && image == CoreVariation) {

		var targets []Target
		for _, t := range osTargets {
			if !t.NativeBridge {
				targets = append(targets, t)
			}
		}

		osTargets = targets
	}

	// only the primary arch in the ramdisk / recovery partition
	if os == Android && (module.InstallInRecovery() || module.InstallInRamdisk()) {
		osTargets = []Target{osTargets[0]}
	}

	prefer32 := false
	if base.prefer32 != nil {
		prefer32 = base.prefer32(mctx, base, os.Class)
	}

	multilib, extraMultilib := decodeMultilib(base, os.Class)
	targets, err := decodeMultilibTargets(multilib, osTargets, prefer32)
	if err != nil {
		mctx.ModuleErrorf("%s", err.Error())
	}

	var multiTargets []Target
	if extraMultilib != "" {
		multiTargets, err = decodeMultilibTargets(extraMultilib, osTargets, prefer32)
		if err != nil {
			mctx.ModuleErrorf("%s", err.Error())
		}
	}

	if image == RecoveryVariation {
		primaryArch := mctx.Config().DevicePrimaryArchType()
		targets = filterToArch(targets, primaryArch)
		multiTargets = filterToArch(multiTargets, primaryArch)
	}

	if len(targets) == 0 {
		base.Disable()
		return
	}

	targetNames := make([]string, len(targets))

	for i, target := range targets {
		targetNames[i] = target.ArchVariation()
	}

	modules := mctx.CreateVariations(targetNames...)
	for i, m := range modules {
		addTargetProperties(m, targets[i], multiTargets, i == 0)
		m.(Module).base().setArchProperties(mctx)
	}
}

func addTargetProperties(m Module, target Target, multiTargets []Target, primaryTarget bool) {
	m.base().commonProperties.CompileTarget = target
	m.base().commonProperties.CompileMultiTargets = multiTargets
	m.base().commonProperties.CompilePrimary = primaryTarget
}

func decodeMultilib(base *ModuleBase, class OsClass) (multilib, extraMultilib string) {
	switch class {
	case Device:
		multilib = String(base.commonProperties.Target.Android.Compile_multilib)
	case Host, HostCross:
		multilib = String(base.commonProperties.Target.Host.Compile_multilib)
	}
	if multilib == "" {
		multilib = String(base.commonProperties.Compile_multilib)
	}
	if multilib == "" {
		multilib = base.commonProperties.Default_multilib
	}

	if base.commonProperties.UseTargetVariants {
		return multilib, ""
	} else {
		// For app modules a single arch variant will be created per OS class which is expected to handle all the
		// selected arches.  Return the common-type as multilib and any Android.bp provided multilib as extraMultilib
		if multilib == base.commonProperties.Default_multilib {
			multilib = "first"
		}
		return base.commonProperties.Default_multilib, multilib
	}
}

func filterToArch(targets []Target, arch ArchType) []Target {
	for i := 0; i < len(targets); i++ {
		if targets[i].Arch.ArchType != arch {
			targets = append(targets[:i], targets[i+1:]...)
			i--
		}
	}
	return targets
}

type archPropTypeDesc struct {
	arch, multilib, target reflect.Type
}

type archPropRoot struct {
	Arch, Multilib, Target interface{}
}

// createArchPropTypeDesc takes a reflect.Type that is either a struct or a pointer to a struct, and
// returns lists of reflect.Types that contains the arch-variant properties inside structs for each
// arch, multilib and target property.
func createArchPropTypeDesc(props reflect.Type) []archPropTypeDesc {
	// Each property struct shard will be nested many times under the runtime generated arch struct,
	// which can hit the limit of 64kB for the name of runtime generated structs.  They are nested
	// 97 times now, which may grow in the future, plus there is some overhead for the containing
	// type.  This number may need to be reduced if too many are added, but reducing it too far
	// could cause problems if a single deeply nested property no longer fits in the name.
	const maxArchTypeNameSize = 500

	propShards, _ := proptools.FilterPropertyStructSharded(props, maxArchTypeNameSize, filterArchStruct)
	if len(propShards) == 0 {
		return nil
	}

	var ret []archPropTypeDesc
	for _, props := range propShards {

		variantFields := func(names []string) []reflect.StructField {
			ret := make([]reflect.StructField, len(names))

			for i, name := range names {
				ret[i].Name = name
				ret[i].Type = props
			}

			return ret
		}

		archFields := make([]reflect.StructField, len(archTypeList))
		for i, arch := range archTypeList {
			variants := []string{}

			for _, archVariant := range archVariants[arch] {
				archVariant := variantReplacer.Replace(archVariant)
				variants = append(variants, proptools.FieldNameForProperty(archVariant))
			}
			for _, feature := range archFeatures[arch] {
				feature := variantReplacer.Replace(feature)
				variants = append(variants, proptools.FieldNameForProperty(feature))
			}

			fields := variantFields(variants)

			fields = append([]reflect.StructField{{
				Name:      "BlueprintEmbed",
				Type:      props,
				Anonymous: true,
			}}, fields...)

			archFields[i] = reflect.StructField{
				Name: arch.Field,
				Type: reflect.StructOf(fields),
			}
		}
		archType := reflect.StructOf(archFields)

		multilibType := reflect.StructOf(variantFields([]string{"Lib32", "Lib64"}))

		targets := []string{
			"Host",
			"Android64",
			"Android32",
			"Bionic",
			"Linux",
			"Not_windows",
			"Arm_on_x86",
			"Arm_on_x86_64",
			"Native_bridge",
		}
		for _, os := range OsTypeList {
			targets = append(targets, os.Field)

			for _, archType := range osArchTypeMap[os] {
				targets = append(targets, os.Field+"_"+archType.Name)

				if os.Linux() {
					target := "Linux_" + archType.Name
					if !InList(target, targets) {
						targets = append(targets, target)
					}
				}
				if os.Bionic() {
					target := "Bionic_" + archType.Name
					if !InList(target, targets) {
						targets = append(targets, target)
					}
				}
			}
		}

		targetType := reflect.StructOf(variantFields(targets))

		ret = append(ret, archPropTypeDesc{
			arch:     reflect.PtrTo(archType),
			multilib: reflect.PtrTo(multilibType),
			target:   reflect.PtrTo(targetType),
		})
	}
	return ret
}

func filterArchStruct(field reflect.StructField, prefix string) (bool, reflect.StructField) {
	if proptools.HasTag(field, "android", "arch_variant") {
		// The arch_variant field isn't necessary past this point
		// Instead of wasting space, just remove it. Go also has a
		// 16-bit limit on structure name length. The name is constructed
		// based on the Go source representation of the structure, so
		// the tag names count towards that length.

		androidTag := field.Tag.Get("android")
		values := strings.Split(androidTag, ",")

		if string(field.Tag) != `android:"`+strings.Join(values, ",")+`"` {
			panic(fmt.Errorf("unexpected tag format %q", field.Tag))
		}
		// these tags don't need to be present in the runtime generated struct type.
		values = RemoveListFromList(values, []string{"arch_variant", "variant_prepend", "path"})
		if len(values) > 0 {
			panic(fmt.Errorf("unknown tags %q in field %q", values, prefix+field.Name))
		}

		field.Tag = ""
		return true, field
	}
	return false, field
}

var archPropTypeMap OncePer

func InitArchModule(m Module) {

	base := m.base()

	base.generalProperties = m.GetProperties()

	for _, properties := range base.generalProperties {
		propertiesValue := reflect.ValueOf(properties)
		t := propertiesValue.Type()
		if propertiesValue.Kind() != reflect.Ptr {
			panic(fmt.Errorf("properties must be a pointer to a struct, got %T",
				propertiesValue.Interface()))
		}

		propertiesValue = propertiesValue.Elem()
		if propertiesValue.Kind() != reflect.Struct {
			panic(fmt.Errorf("properties must be a pointer to a struct, got %T",
				propertiesValue.Interface()))
		}

		archPropTypes := archPropTypeMap.Once(NewCustomOnceKey(t), func() interface{} {
			return createArchPropTypeDesc(t)
		}).([]archPropTypeDesc)

		var archProperties []interface{}
		for _, t := range archPropTypes {
			archProperties = append(archProperties, &archPropRoot{
				Arch:     reflect.Zero(t.arch).Interface(),
				Multilib: reflect.Zero(t.multilib).Interface(),
				Target:   reflect.Zero(t.target).Interface(),
			})
		}
		base.archProperties = append(base.archProperties, archProperties)
		m.AddProperties(archProperties...)
	}

	base.customizableProperties = m.GetProperties()
}

var variantReplacer = strings.NewReplacer("-", "_", ".", "_")

func (m *ModuleBase) appendProperties(ctx BottomUpMutatorContext,
	dst interface{}, src reflect.Value, field, srcPrefix string) reflect.Value {

	if src.Kind() == reflect.Ptr {
		if src.IsNil() {
			return src
		}
		src = src.Elem()
	}

	src = src.FieldByName(field)
	if !src.IsValid() {
		ctx.ModuleErrorf("field %q does not exist", srcPrefix)
		return src
	}

	ret := src

	if src.Kind() == reflect.Struct {
		src = src.FieldByName("BlueprintEmbed")
	}

	order := func(property string,
		dstField, srcField reflect.StructField,
		dstValue, srcValue interface{}) (proptools.Order, error) {
		if proptools.HasTag(dstField, "android", "variant_prepend") {
			return proptools.Prepend, nil
		} else {
			return proptools.Append, nil
		}
	}

	err := proptools.ExtendMatchingProperties([]interface{}{dst}, src.Interface(), nil, order)
	if err != nil {
		if propertyErr, ok := err.(*proptools.ExtendPropertyError); ok {
			ctx.PropertyErrorf(propertyErr.Property, "%s", propertyErr.Err.Error())
		} else {
			panic(err)
		}
	}

	return ret
}

// Rewrite the module's properties structs to contain os-specific values.
func (m *ModuleBase) setOSProperties(ctx BottomUpMutatorContext) {
	os := m.commonProperties.CompileOS

	for i := range m.generalProperties {
		genProps := m.generalProperties[i]
		if m.archProperties[i] == nil {
			continue
		}
		for _, archProperties := range m.archProperties[i] {
			archPropValues := reflect.ValueOf(archProperties).Elem()

			targetProp := archPropValues.FieldByName("Target").Elem()

			// Handle host-specific properties in the form:
			// target: {
			//     host: {
			//         key: value,
			//     },
			// },
			if os.Class == Host || os.Class == HostCross {
				field := "Host"
				prefix := "target.host"
				m.appendProperties(ctx, genProps, targetProp, field, prefix)
			}

			// Handle target OS generalities of the form:
			// target: {
			//     bionic: {
			//         key: value,
			//     },
			// }
			if os.Linux() {
				field := "Linux"
				prefix := "target.linux"
				m.appendProperties(ctx, genProps, targetProp, field, prefix)
			}

			if os.Bionic() {
				field := "Bionic"
				prefix := "target.bionic"
				m.appendProperties(ctx, genProps, targetProp, field, prefix)
			}

			// Handle target OS properties in the form:
			// target: {
			//     linux_glibc: {
			//         key: value,
			//     },
			//     not_windows: {
			//         key: value,
			//     },
			//     android {
			//         key: value,
			//     },
			// },
			field := os.Field
			prefix := "target." + os.Name
			m.appendProperties(ctx, genProps, targetProp, field, prefix)

			if (os.Class == Host || os.Class == HostCross) && os != Windows {
				field := "Not_windows"
				prefix := "target.not_windows"
				m.appendProperties(ctx, genProps, targetProp, field, prefix)
			}

			// Handle 64-bit device properties in the form:
			// target {
			//     android64 {
			//         key: value,
			//     },
			//     android32 {
			//         key: value,
			//     },
			// },
			// WARNING: this is probably not what you want to use in your blueprints file, it selects
			// options for all targets on a device that supports 64-bit binaries, not just the targets
			// that are being compiled for 64-bit.  Its expected use case is binaries like linker and
			// debuggerd that need to know when they are a 32-bit process running on a 64-bit device
			if os.Class == Device {
				if ctx.Config().Android64() {
					field := "Android64"
					prefix := "target.android64"
					m.appendProperties(ctx, genProps, targetProp, field, prefix)
				} else {
					field := "Android32"
					prefix := "target.android32"
					m.appendProperties(ctx, genProps, targetProp, field, prefix)
				}
			}
		}
	}
}

// Rewrite the module's properties structs to contain arch-specific values.
func (m *ModuleBase) setArchProperties(ctx BottomUpMutatorContext) {
	arch := m.Arch()
	os := m.Os()

	for i := range m.generalProperties {
		genProps := m.generalProperties[i]
		if m.archProperties[i] == nil {
			continue
		}
		for _, archProperties := range m.archProperties[i] {
			archPropValues := reflect.ValueOf(archProperties).Elem()

			archProp := archPropValues.FieldByName("Arch").Elem()
			multilibProp := archPropValues.FieldByName("Multilib").Elem()
			targetProp := archPropValues.FieldByName("Target").Elem()

			// Handle arch-specific properties in the form:
			// arch: {
			//     arm64: {
			//         key: value,
			//     },
			// },
			t := arch.ArchType

			if arch.ArchType != Common {
				field := proptools.FieldNameForProperty(t.Name)
				prefix := "arch." + t.Name
				archStruct := m.appendProperties(ctx, genProps, archProp, field, prefix)

				// Handle arch-variant-specific properties in the form:
				// arch: {
				//     variant: {
				//         key: value,
				//     },
				// },
				v := variantReplacer.Replace(arch.ArchVariant)
				if v != "" {
					field := proptools.FieldNameForProperty(v)
					prefix := "arch." + t.Name + "." + v
					m.appendProperties(ctx, genProps, archStruct, field, prefix)
				}

				// Handle cpu-variant-specific properties in the form:
				// arch: {
				//     variant: {
				//         key: value,
				//     },
				// },
				if arch.CpuVariant != arch.ArchVariant {
					c := variantReplacer.Replace(arch.CpuVariant)
					if c != "" {
						field := proptools.FieldNameForProperty(c)
						prefix := "arch." + t.Name + "." + c
						m.appendProperties(ctx, genProps, archStruct, field, prefix)
					}
				}

				// Handle arch-feature-specific properties in the form:
				// arch: {
				//     feature: {
				//         key: value,
				//     },
				// },
				for _, feature := range arch.ArchFeatures {
					field := proptools.FieldNameForProperty(feature)
					prefix := "arch." + t.Name + "." + feature
					m.appendProperties(ctx, genProps, archStruct, field, prefix)
				}

				// Handle multilib-specific properties in the form:
				// multilib: {
				//     lib32: {
				//         key: value,
				//     },
				// },
				field = proptools.FieldNameForProperty(t.Multilib)
				prefix = "multilib." + t.Multilib
				m.appendProperties(ctx, genProps, multilibProp, field, prefix)
			}

			// Handle combined OS-feature and arch specific properties in the form:
			// target: {
			//     bionic_x86: {
			//         key: value,
			//     },
			// }
			if os.Linux() && arch.ArchType != Common {
				field := "Linux_" + arch.ArchType.Name
				prefix := "target.linux_" + arch.ArchType.Name
				m.appendProperties(ctx, genProps, targetProp, field, prefix)
			}

			if os.Bionic() && arch.ArchType != Common {
				field := "Bionic_" + t.Name
				prefix := "target.bionic_" + t.Name
				m.appendProperties(ctx, genProps, targetProp, field, prefix)
			}

			// Handle combined OS and arch specific properties in the form:
			// target: {
			//     linux_glibc_x86: {
			//         key: value,
			//     },
			//     linux_glibc_arm: {
			//         key: value,
			//     },
			//     android_arm {
			//         key: value,
			//     },
			//     android_x86 {
			//         key: value,
			//     },
			// },
			if arch.ArchType != Common {
				field := os.Field + "_" + t.Name
				prefix := "target." + os.Name + "_" + t.Name
				m.appendProperties(ctx, genProps, targetProp, field, prefix)
			}

			// Handle arm on x86 properties in the form:
			// target {
			//     arm_on_x86 {
			//         key: value,
			//     },
			//     arm_on_x86_64 {
			//         key: value,
			//     },
			// },
			if os.Class == Device {
				if arch.ArchType == X86 && (hasArmAbi(arch) ||
					hasArmAndroidArch(ctx.Config().Targets[Android])) {
					field := "Arm_on_x86"
					prefix := "target.arm_on_x86"
					m.appendProperties(ctx, genProps, targetProp, field, prefix)
				}
				if arch.ArchType == X86_64 && (hasArmAbi(arch) ||
					hasArmAndroidArch(ctx.Config().Targets[Android])) {
					field := "Arm_on_x86_64"
					prefix := "target.arm_on_x86_64"
					m.appendProperties(ctx, genProps, targetProp, field, prefix)
				}
				if os == Android && m.Target().NativeBridge == NativeBridgeEnabled {
					field := "Native_bridge"
					prefix := "target.native_bridge"
					m.appendProperties(ctx, genProps, targetProp, field, prefix)
				}
			}
		}
	}
}

func forEachInterface(v reflect.Value, f func(reflect.Value)) {
	switch v.Kind() {
	case reflect.Interface:
		f(v)
	case reflect.Struct:
		for i := 0; i < v.NumField(); i++ {
			forEachInterface(v.Field(i), f)
		}
	case reflect.Ptr:
		forEachInterface(v.Elem(), f)
	default:
		panic(fmt.Errorf("Unsupported kind %s", v.Kind()))
	}
}

// Convert the arch product variables into a list of targets for each os class structs
func decodeTargetProductVariables(config *config) (map[OsType][]Target, error) {
	variables := config.productVariables

	targets := make(map[OsType][]Target)
	var targetErr error

	addTarget := func(os OsType, archName string, archVariant, cpuVariant *string, abi []string,
		nativeBridgeEnabled NativeBridgeSupport, nativeBridgeHostArchName *string,
		nativeBridgeRelativePath *string) {
		if targetErr != nil {
			return
		}

		arch, err := decodeArch(os, archName, archVariant, cpuVariant, abi)
		if err != nil {
			targetErr = err
			return
		}
		nativeBridgeRelativePathStr := String(nativeBridgeRelativePath)
		nativeBridgeHostArchNameStr := String(nativeBridgeHostArchName)

		// Use guest arch as relative install path by default
		if nativeBridgeEnabled && nativeBridgeRelativePathStr == "" {
			nativeBridgeRelativePathStr = arch.ArchType.String()
		}

		targets[os] = append(targets[os],
			Target{
				Os:                       os,
				Arch:                     arch,
				NativeBridge:             nativeBridgeEnabled,
				NativeBridgeHostArchName: nativeBridgeHostArchNameStr,
				NativeBridgeRelativePath: nativeBridgeRelativePathStr,
			})
	}

	if variables.HostArch == nil {
		return nil, fmt.Errorf("No host primary architecture set")
	}

	addTarget(BuildOs, *variables.HostArch, nil, nil, nil, NativeBridgeDisabled, nil, nil)

	if variables.HostSecondaryArch != nil && *variables.HostSecondaryArch != "" {
		addTarget(BuildOs, *variables.HostSecondaryArch, nil, nil, nil, NativeBridgeDisabled, nil, nil)
	}

	if Bool(config.Host_bionic) {
		addTarget(LinuxBionic, "x86_64", nil, nil, nil, NativeBridgeDisabled, nil, nil)
	}

	if String(variables.CrossHost) != "" {
		crossHostOs := osByName(*variables.CrossHost)
		if crossHostOs == NoOsType {
			return nil, fmt.Errorf("Unknown cross host OS %q", *variables.CrossHost)
		}

		if String(variables.CrossHostArch) == "" {
			return nil, fmt.Errorf("No cross-host primary architecture set")
		}

		addTarget(crossHostOs, *variables.CrossHostArch, nil, nil, nil, NativeBridgeDisabled, nil, nil)

		if variables.CrossHostSecondaryArch != nil && *variables.CrossHostSecondaryArch != "" {
			addTarget(crossHostOs, *variables.CrossHostSecondaryArch, nil, nil, nil, NativeBridgeDisabled, nil, nil)
		}
	}

	if variables.DeviceArch != nil && *variables.DeviceArch != "" {
		var target = Android
		if Bool(variables.Fuchsia) {
			target = Fuchsia
		}

		addTarget(target, *variables.DeviceArch, variables.DeviceArchVariant,
			variables.DeviceCpuVariant, variables.DeviceAbi, NativeBridgeDisabled, nil, nil)

		if variables.DeviceSecondaryArch != nil && *variables.DeviceSecondaryArch != "" {
			addTarget(Android, *variables.DeviceSecondaryArch,
				variables.DeviceSecondaryArchVariant, variables.DeviceSecondaryCpuVariant,
				variables.DeviceSecondaryAbi, NativeBridgeDisabled, nil, nil)
		}

		if variables.NativeBridgeArch != nil && *variables.NativeBridgeArch != "" {
			addTarget(Android, *variables.NativeBridgeArch,
				variables.NativeBridgeArchVariant, variables.NativeBridgeCpuVariant,
				variables.NativeBridgeAbi, NativeBridgeEnabled, variables.DeviceArch,
				variables.NativeBridgeRelativePath)
		}

		if variables.DeviceSecondaryArch != nil && *variables.DeviceSecondaryArch != "" &&
			variables.NativeBridgeSecondaryArch != nil && *variables.NativeBridgeSecondaryArch != "" {
			addTarget(Android, *variables.NativeBridgeSecondaryArch,
				variables.NativeBridgeSecondaryArchVariant,
				variables.NativeBridgeSecondaryCpuVariant,
				variables.NativeBridgeSecondaryAbi,
				NativeBridgeEnabled,
				variables.DeviceSecondaryArch,
				variables.NativeBridgeSecondaryRelativePath)
		}
	}

	if targetErr != nil {
		return nil, targetErr
	}

	return targets, nil
}

// hasArmAbi returns true if arch has at least one arm ABI
func hasArmAbi(arch Arch) bool {
	return PrefixInList(arch.Abi, "arm")
}

// hasArmArch returns true if targets has at least non-native_bridge arm Android arch
func hasArmAndroidArch(targets []Target) bool {
	for _, target := range targets {
		if target.Os == Android && target.Arch.ArchType == Arm {
			return true
		}
	}
	return false
}

type archConfig struct {
	arch        string
	archVariant string
	cpuVariant  string
	abi         []string
}

func getMegaDeviceConfig() []archConfig {
	return []archConfig{
		{"arm", "armv7-a", "generic", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "generic", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "cortex-a7", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "cortex-a8", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "cortex-a9", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "cortex-a15", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "cortex-a53", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "cortex-a53.a57", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "cortex-a72", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "cortex-a73", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "cortex-a75", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "cortex-a76", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "krait", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "kryo", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "kryo385", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "exynos-m1", []string{"armeabi-v7a"}},
		{"arm", "armv7-a-neon", "exynos-m2", []string{"armeabi-v7a"}},
		{"arm64", "armv8-a", "cortex-a53", []string{"arm64-v8a"}},
		{"arm64", "armv8-a", "cortex-a72", []string{"arm64-v8a"}},
		{"arm64", "armv8-a", "cortex-a73", []string{"arm64-v8a"}},
		{"arm64", "armv8-a", "kryo", []string{"arm64-v8a"}},
		{"arm64", "armv8-a", "exynos-m1", []string{"arm64-v8a"}},
		{"arm64", "armv8-a", "exynos-m2", []string{"arm64-v8a"}},
		{"arm64", "armv8-2a", "cortex-a75", []string{"arm64-v8a"}},
		{"arm64", "armv8-2a", "cortex-a76", []string{"arm64-v8a"}},
		{"arm64", "armv8-2a", "kryo385", []string{"arm64-v8a"}},
		{"mips", "mips32-fp", "", []string{"mips"}},
		{"mips", "mips32r2-fp", "", []string{"mips"}},
		{"mips", "mips32r2-fp-xburst", "", []string{"mips"}},
		//{"mips", "mips32r6", "", []string{"mips"}},
		{"mips", "mips32r2dsp-fp", "", []string{"mips"}},
		{"mips", "mips32r2dspr2-fp", "", []string{"mips"}},
		// mips64r2 is mismatching 64r2 and 64r6 libraries during linking to libgcc
		//{"mips64", "mips64r2", "", []string{"mips64"}},
		{"mips64", "mips64r6", "", []string{"mips64"}},
		{"x86", "", "", []string{"x86"}},
		{"x86", "atom", "", []string{"x86"}},
		{"x86", "haswell", "", []string{"x86"}},
		{"x86", "ivybridge", "", []string{"x86"}},
		{"x86", "sandybridge", "", []string{"x86"}},
		{"x86", "silvermont", "", []string{"x86"}},
		{"x86", "stoneyridge", "", []string{"x86"}},
		{"x86", "x86_64", "", []string{"x86"}},
		{"x86_64", "", "", []string{"x86_64"}},
		{"x86_64", "haswell", "", []string{"x86_64"}},
		{"x86_64", "ivybridge", "", []string{"x86_64"}},
		{"x86_64", "sandybridge", "", []string{"x86_64"}},
		{"x86_64", "silvermont", "", []string{"x86_64"}},
		{"x86_64", "stoneyridge", "", []string{"x86_64"}},
	}
}

func getNdkAbisConfig() []archConfig {
	return []archConfig{
		{"arm", "armv7-a", "", []string{"armeabi-v7a"}},
		{"arm64", "armv8-a", "", []string{"arm64-v8a"}},
		{"x86", "", "", []string{"x86"}},
		{"x86_64", "", "", []string{"x86_64"}},
	}
}

func getAmlAbisConfig() []archConfig {
	return []archConfig{
		{"arm", "armv7-a", "", []string{"armeabi-v7a"}},
		{"arm64", "armv8-a", "", []string{"arm64-v8a"}},
		{"x86", "", "", []string{"x86"}},
		{"x86_64", "", "", []string{"x86_64"}},
	}
}

func decodeArchSettings(os OsType, archConfigs []archConfig) ([]Target, error) {
	var ret []Target

	for _, config := range archConfigs {
		arch, err := decodeArch(os, config.arch, &config.archVariant,
			&config.cpuVariant, config.abi)
		if err != nil {
			return nil, err
		}

		ret = append(ret, Target{
			Os:   Android,
			Arch: arch,
		})
	}

	return ret, nil
}

// Convert a set of strings from product variables into a single Arch struct
func decodeArch(os OsType, arch string, archVariant, cpuVariant *string, abi []string) (Arch, error) {
	stringPtr := func(p *string) string {
		if p != nil {
			return *p
		}
		return ""
	}

	archType, ok := archTypeMap[arch]
	if !ok {
		return Arch{}, fmt.Errorf("unknown arch %q", arch)
	}

	a := Arch{
		ArchType:    archType,
		ArchVariant: stringPtr(archVariant),
		CpuVariant:  stringPtr(cpuVariant),
		Abi:         abi,
	}

	if a.ArchVariant == a.ArchType.Name || a.ArchVariant == "generic" {
		a.ArchVariant = ""
	}

	if a.CpuVariant == a.ArchType.Name || a.CpuVariant == "generic" {
		a.CpuVariant = ""
	}

	for i := 0; i < len(a.Abi); i++ {
		if a.Abi[i] == "" {
			a.Abi = append(a.Abi[:i], a.Abi[i+1:]...)
			i--
		}
	}

	if a.ArchVariant == "" {
		if featureMap, ok := defaultArchFeatureMap[os]; ok {
			a.ArchFeatures = featureMap[archType]
		}
	} else {
		if featureMap, ok := archFeatureMap[archType]; ok {
			a.ArchFeatures = featureMap[a.ArchVariant]
		}
	}

	return a, nil
}

func filterMultilibTargets(targets []Target, multilib string) []Target {
	var ret []Target
	for _, t := range targets {
		if t.Arch.ArchType.Multilib == multilib {
			ret = append(ret, t)
		}
	}
	return ret
}

// Return the set of Os specific common architecture targets for each Os in a list of
// targets.
func getCommonTargets(targets []Target) []Target {
	var ret []Target
	set := make(map[string]bool)

	for _, t := range targets {
		if _, found := set[t.Os.String()]; !found {
			set[t.Os.String()] = true
			ret = append(ret, commonTargetMap[t.Os.String()])
		}
	}

	return ret
}

func firstTarget(targets []Target, filters ...string) []Target {
	for _, filter := range filters {
		buildTargets := filterMultilibTargets(targets, filter)
		if len(buildTargets) > 0 {
			return buildTargets[:1]
		}
	}
	return nil
}

// Use the module multilib setting to select one or more targets from a target list
func decodeMultilibTargets(multilib string, targets []Target, prefer32 bool) ([]Target, error) {
	buildTargets := []Target{}

	switch multilib {
	case "common":
		buildTargets = getCommonTargets(targets)
	case "common_first":
		buildTargets = getCommonTargets(targets)
		if prefer32 {
			buildTargets = append(buildTargets, firstTarget(targets, "lib32", "lib64")...)
		} else {
			buildTargets = append(buildTargets, firstTarget(targets, "lib64", "lib32")...)
		}
	case "both":
		if prefer32 {
			buildTargets = append(buildTargets, filterMultilibTargets(targets, "lib32")...)
			buildTargets = append(buildTargets, filterMultilibTargets(targets, "lib64")...)
		} else {
			buildTargets = append(buildTargets, filterMultilibTargets(targets, "lib64")...)
			buildTargets = append(buildTargets, filterMultilibTargets(targets, "lib32")...)
		}
	case "32":
		buildTargets = filterMultilibTargets(targets, "lib32")
	case "64":
		buildTargets = filterMultilibTargets(targets, "lib64")
	case "first":
		if prefer32 {
			buildTargets = firstTarget(targets, "lib32", "lib64")
		} else {
			buildTargets = firstTarget(targets, "lib64", "lib32")
		}
	case "prefer32":
		buildTargets = filterMultilibTargets(targets, "lib32")
		if len(buildTargets) == 0 {
			buildTargets = filterMultilibTargets(targets, "lib64")
		}
	default:
		return nil, fmt.Errorf(`compile_multilib must be "both", "first", "32", "64", or "prefer32" found %q`,
			multilib)
	}

	return buildTargets, nil
}
