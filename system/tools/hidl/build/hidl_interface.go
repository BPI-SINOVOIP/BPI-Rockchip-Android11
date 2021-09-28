// Copyright (C) 2017 The Android Open Source Project
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

package hidl

import (
	"fmt"
	"sort"
	"strings"
	"sync"

	"github.com/google/blueprint"
	"github.com/google/blueprint/proptools"

	"android/soong/android"
	"android/soong/cc"
	"android/soong/genrule"
	"android/soong/java"
)

var (
	hidlInterfaceSuffix       = "_interface"
	hidlMetadataSingletonName = "hidl_metadata_json"

	pctx = android.NewPackageContext("android/hidl")

	hidl             = pctx.HostBinToolVariable("hidl", "hidl-gen")
	vtsc             = pctx.HostBinToolVariable("vtsc", "vtsc")
	hidlLint         = pctx.HostBinToolVariable("lint", "hidl-lint")
	soong_zip        = pctx.HostBinToolVariable("soong_zip", "soong_zip")
	intermediatesDir = pctx.IntermediatesPathVariable("intermediatesDir", "")

	hidlRule = pctx.StaticRule("hidlRule", blueprint.RuleParams{
		Depfile:     "${depfile}",
		Deps:        blueprint.DepsGCC,
		Command:     "rm -rf ${genDir} && ${hidl} -R -p . -d ${depfile} -o ${genDir} -L ${language} ${roots} ${fqName}",
		CommandDeps: []string{"${hidl}"},
		Description: "HIDL ${language}: ${in} => ${out}",
	}, "depfile", "fqName", "genDir", "language", "roots")

	hidlSrcJarRule = pctx.StaticRule("hidlSrcJarRule", blueprint.RuleParams{
		Depfile: "${depfile}",
		Deps:    blueprint.DepsGCC,
		Command: "rm -rf ${genDir} && " +
			"${hidl} -R -p . -d ${depfile} -o ${genDir}/srcs -L ${language} ${roots} ${fqName} && " +
			"${soong_zip} -o ${genDir}/srcs.srcjar -C ${genDir}/srcs -D ${genDir}/srcs",
		CommandDeps: []string{"${hidl}", "${soong_zip}"},
		Description: "HIDL ${language}: ${in} => srcs.srcjar",
	}, "depfile", "fqName", "genDir", "language", "roots")

	vtsRule = pctx.StaticRule("vtsRule", blueprint.RuleParams{
		Command:     "rm -rf ${genDir} && ${vtsc} -m${mode} -t${type} ${inputDir}/${packagePath} ${genDir}/${packagePath}",
		CommandDeps: []string{"${vtsc}"},
		Description: "VTS ${mode} ${type}: ${in} => ${out}",
	}, "mode", "type", "inputDir", "genDir", "packagePath")

	lintRule = pctx.StaticRule("lintRule", blueprint.RuleParams{
		Command:     "rm -f ${output} && touch ${output} && ${lint} -j -e -R -p . ${roots} ${fqName} > ${output}",
		CommandDeps: []string{"${lint}"},
		Description: "hidl-lint ${fqName}: ${out}",
	}, "output", "roots", "fqName")

	zipLintRule = pctx.StaticRule("zipLintRule", blueprint.RuleParams{
		Command:     "rm -f ${output} && ${soong_zip} -o ${output} -C ${intermediatesDir} ${files}",
		CommandDeps: []string{"${soong_zip}"},
		Description: "Zipping hidl-lints into ${output}",
	}, "output", "files")

	inheritanceHierarchyRule = pctx.StaticRule("inheritanceHierarchyRule", blueprint.RuleParams{
		Command:     "rm -f ${out} && ${hidl} -L inheritance-hierarchy ${roots} ${fqInterface} > ${out}",
		CommandDeps: []string{"${hidl}"},
		Description: "HIDL inheritance hierarchy: ${fqInterface} => ${out}",
	}, "roots", "fqInterface")

	joinJsonObjectsToArrayRule = pctx.StaticRule("joinJsonObjectsToArrayRule", blueprint.RuleParams{
		Rspfile:        "$out.rsp",
		RspfileContent: "$files",
		Command: "rm -rf ${out} && " +
			// Start the output array with an opening bracket.
			"echo '[' >> ${out} && " +
			// Append each input file and a comma to the output.
			"for file in $$(cat ${out}.rsp); do " +
			"cat $$file >> ${out}; echo ',' >> ${out}; " +
			"done && " +
			// Remove the last comma, replacing it with the closing bracket.
			"sed -i '$$d' ${out} && echo ']' >> ${out}",
		Description: "Joining JSON objects into array ${out}",
	}, "files")
)

func init() {
	android.RegisterModuleType("hidl_interface", hidlInterfaceFactory)
	android.RegisterSingletonType("all_hidl_lints", allHidlLintsFactory)
	android.RegisterMakeVarsProvider(pctx, makeVarsProvider)
	android.RegisterModuleType("hidl_interfaces_metadata", hidlInterfacesMetadataSingletonFactory)
	pctx.Import("android/soong/android")
}

func hidlInterfacesMetadataSingletonFactory() android.Module {
	i := &hidlInterfacesMetadataSingleton{}
	android.InitAndroidModule(i)
	return i
}

type hidlInterfacesMetadataSingleton struct {
	android.ModuleBase

	inheritanceHierarchyPath android.OutputPath
}

var _ android.OutputFileProducer = (*hidlInterfacesMetadataSingleton)(nil)

func (m *hidlInterfacesMetadataSingleton) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	if m.Name() != hidlMetadataSingletonName {
		ctx.PropertyErrorf("name", "must be %s", hidlMetadataSingletonName)
		return
	}

	var inheritanceHierarchyOutputs android.Paths
	ctx.VisitDirectDeps(func(m android.Module) {
		if !m.ExportedToMake() {
			return
		}
		if t, ok := m.(*hidlGenRule); ok {
			if t.properties.Language == "inheritance-hierarchy" {
				inheritanceHierarchyOutputs = append(inheritanceHierarchyOutputs, t.genOutputs.Paths()...)
			}
		}
	})

	m.inheritanceHierarchyPath = android.PathForIntermediates(ctx, "hidl_inheritance_hierarchy.json")

	ctx.Build(pctx, android.BuildParams{
		Rule:   joinJsonObjectsToArrayRule,
		Inputs: inheritanceHierarchyOutputs,
		Output: m.inheritanceHierarchyPath,
		Args: map[string]string{
			"files": strings.Join(inheritanceHierarchyOutputs.Strings(), " "),
		},
	})
}

func (m *hidlInterfacesMetadataSingleton) OutputFiles(tag string) (android.Paths, error) {
	if tag != "" {
		return nil, fmt.Errorf("unsupported tag %q", tag)
	}

	return android.Paths{m.inheritanceHierarchyPath}, nil
}

func allHidlLintsFactory() android.Singleton {
	return &allHidlLintsSingleton{}
}

type allHidlLintsSingleton struct {
	outPath string
}

func (m *allHidlLintsSingleton) GenerateBuildActions(ctx android.SingletonContext) {
	var hidlLintOutputs android.Paths
	ctx.VisitAllModules(func(m android.Module) {
		if t, ok := m.(*hidlGenRule); ok {
			if t.properties.Language == "lint" {
				if len(t.genOutputs) == 1 {
					hidlLintOutputs = append(hidlLintOutputs, t.genOutputs[0])
				} else {
					panic("-hidl-lint target was not configured correctly")
				}
			}
		}
	})

	outPath := android.PathForIntermediates(ctx, "hidl-lint.zip")
	m.outPath = outPath.String()

	ctx.Build(pctx, android.BuildParams{
		Rule:   zipLintRule,
		Inputs: hidlLintOutputs,
		Output: outPath,
		Args: map[string]string{
			"output": outPath.String(),
			"files":  strings.Join(wrap("-f ", hidlLintOutputs.Strings(), ""), " "),
		},
	})
}

func (m *allHidlLintsSingleton) MakeVars(ctx android.MakeVarsContext) {
	ctx.Strict("ALL_HIDL_LINTS_ZIP", m.outPath)
}

type hidlGenProperties struct {
	Language       string
	FqName         string
	Root           string
	Interfaces     []string
	Inputs         []string
	Outputs        []string
	Apex_available []string
}

type hidlGenRule struct {
	android.ModuleBase

	properties hidlGenProperties

	genOutputDir android.Path
	genInputs    android.Paths
	genOutputs   android.WritablePaths
}

var _ android.SourceFileProducer = (*hidlGenRule)(nil)
var _ genrule.SourceFileGenerator = (*hidlGenRule)(nil)

func (g *hidlGenRule) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	g.genOutputDir = android.PathForModuleGen(ctx)

	for _, input := range g.properties.Inputs {
		g.genInputs = append(g.genInputs, android.PathForModuleSrc(ctx, input))
	}

	var interfaces []string
	for _, src := range g.properties.Inputs {
		if strings.HasSuffix(src, ".hal") && strings.HasPrefix(src, "I") {
			interfaces = append(interfaces, strings.TrimSuffix(src, ".hal"))
		}
	}

	switch g.properties.Language {
	case "lint":
		g.genOutputs = append(g.genOutputs, android.PathForModuleGen(ctx, "lint.json"))
	case "inheritance-hierarchy":
		for _, intf := range interfaces {
			g.genOutputs = append(g.genOutputs, android.PathForModuleGen(ctx, intf+"_inheritance_hierarchy.json"))
		}
	default:
		for _, output := range g.properties.Outputs {
			g.genOutputs = append(g.genOutputs, android.PathForModuleGen(ctx, output))
		}
	}

	if g.properties.Language == "vts" && isVtsSpecPackage(ctx.ModuleName()) {
		vtsList := vtsList(ctx.AConfig())
		vtsListMutex.Lock()
		*vtsList = append(*vtsList, g.genOutputs.Paths()...)
		vtsListMutex.Unlock()
	}

	var fullRootOptions []string
	var currentPath android.OptionalPath
	ctx.VisitDirectDeps(func(dep android.Module) {
		switch t := dep.(type) {
		case *hidlInterface:
			fullRootOptions = append(fullRootOptions, t.properties.Full_root_option)
		case *hidlPackageRoot:
			if currentPath.Valid() {
				panic(fmt.Sprintf("Expecting only one path, but found %v %v", currentPath, t.getCurrentPath()))
			}

			currentPath = t.getCurrentPath()
		default:
			panic(fmt.Sprintf("Unrecognized hidlGenProperties dependency: %T", t))
		}
	})

	fullRootOptions = android.FirstUniqueStrings(fullRootOptions)

	inputs := g.genInputs
	if currentPath.Valid() {
		inputs = append(inputs, currentPath.Path())
	}

	rule := hidlRule
	if g.properties.Language == "java" {
		rule = hidlSrcJarRule
	}

	if g.properties.Language == "lint" {
		ctx.Build(pctx, android.BuildParams{
			Rule:   lintRule,
			Inputs: inputs,
			Output: g.genOutputs[0],
			Args: map[string]string{
				"output": g.genOutputs[0].String(),
				"fqName": g.properties.FqName,
				"roots":  strings.Join(fullRootOptions, " "),
			},
		})

		return
	}

	if g.properties.Language == "inheritance-hierarchy" {
		for i, intf := range interfaces {
			ctx.Build(pctx, android.BuildParams{
				Rule:   inheritanceHierarchyRule,
				Inputs: inputs,
				Output: g.genOutputs[i],
				Args: map[string]string{
					"fqInterface": g.properties.FqName + "::" + intf,
					"roots":       strings.Join(fullRootOptions, " "),
				},
			})
		}

		return
	}

	ctx.ModuleBuild(pctx, android.ModuleBuildParams{
		Rule:            rule,
		Inputs:          inputs,
		Output:          g.genOutputs[0],
		ImplicitOutputs: g.genOutputs[1:],
		Args: map[string]string{
			"depfile":  g.genOutputs[0].String() + ".d",
			"genDir":   g.genOutputDir.String(),
			"fqName":   g.properties.FqName,
			"language": g.properties.Language,
			"roots":    strings.Join(fullRootOptions, " "),
		},
	})
}

func (g *hidlGenRule) GeneratedSourceFiles() android.Paths {
	return g.genOutputs.Paths()
}

func (g *hidlGenRule) Srcs() android.Paths {
	return g.genOutputs.Paths()
}

func (g *hidlGenRule) GeneratedDeps() android.Paths {
	return g.genOutputs.Paths()
}

func (g *hidlGenRule) GeneratedHeaderDirs() android.Paths {
	return android.Paths{g.genOutputDir}
}

func (g *hidlGenRule) DepsMutator(ctx android.BottomUpMutatorContext) {
	ctx.AddDependency(ctx.Module(), nil, g.properties.FqName+hidlInterfaceSuffix)
	ctx.AddDependency(ctx.Module(), nil, wrap("", g.properties.Interfaces, hidlInterfaceSuffix)...)
	ctx.AddDependency(ctx.Module(), nil, g.properties.Root)

	ctx.AddReverseDependency(ctx.Module(), nil, hidlMetadataSingletonName)
}

func hidlGenFactory() android.Module {
	g := &hidlGenRule{}
	g.AddProperties(&g.properties)
	android.InitAndroidModule(g)
	return g
}

type vtscProperties struct {
	Mode        string
	Type        string
	SpecName    string // e.g. foo-vts.spec
	Outputs     []string
	PackagePath string // e.g. android/hardware/foo/1.0/
}

type vtscRule struct {
	android.ModuleBase

	properties vtscProperties

	genOutputDir android.Path
	genInputDir  android.Path
	genInputs    android.Paths
	genOutputs   android.WritablePaths
}

var _ android.SourceFileProducer = (*vtscRule)(nil)
var _ genrule.SourceFileGenerator = (*vtscRule)(nil)

func (g *vtscRule) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	g.genOutputDir = android.PathForModuleGen(ctx)

	ctx.VisitDirectDeps(func(dep android.Module) {
		if specs, ok := dep.(*hidlGenRule); ok {
			g.genInputDir = specs.genOutputDir
			g.genInputs = specs.genOutputs.Paths()
		}
	})

	for _, output := range g.properties.Outputs {
		g.genOutputs = append(g.genOutputs, android.PathForModuleGen(ctx, output))
	}

	ctx.ModuleBuild(pctx, android.ModuleBuildParams{
		Rule:    vtsRule,
		Inputs:  g.genInputs,
		Outputs: g.genOutputs,
		Args: map[string]string{
			"mode":        g.properties.Mode,
			"type":        g.properties.Type,
			"inputDir":    g.genInputDir.String(),
			"genDir":      g.genOutputDir.String(),
			"packagePath": g.properties.PackagePath,
		},
	})
}

func (g *vtscRule) GeneratedSourceFiles() android.Paths {
	return g.genOutputs.Paths()
}

func (g *vtscRule) Srcs() android.Paths {
	return g.genOutputs.Paths()
}

func (g *vtscRule) GeneratedDeps() android.Paths {
	return g.genOutputs.Paths()
}

func (g *vtscRule) GeneratedHeaderDirs() android.Paths {
	return android.Paths{g.genOutputDir}
}

func (g *vtscRule) DepsMutator(ctx android.BottomUpMutatorContext) {
	ctx.AddDependency(ctx.Module(), nil, g.properties.SpecName)
}

func vtscFactory() android.Module {
	g := &vtscRule{}
	g.AddProperties(&g.properties)
	android.InitAndroidModule(g)
	return g
}

type hidlInterfaceProperties struct {
	// Vndk properties for interface library only.
	cc.VndkProperties

	// List of .hal files which compose this interface.
	Srcs []string

	// List of hal interface packages that this library depends on.
	Interfaces []string

	// Package root for this package, must be a prefix of name
	Root string

	// Unused/deprecated: List of non-TypeDef types declared in types.hal.
	Types []string

	// Whether to generate the Java library stubs.
	// Default: true
	Gen_java *bool

	// Whether to generate a Java library containing constants
	// expressed by @export annotations in the hal files.
	Gen_java_constants bool

	// Whether to generate VTS-related testing libraries.
	Gen_vts *bool

	// example: -randroid.hardware:hardware/interfaces
	Full_root_option string `blueprint:"mutated"`

	// Whether this interface library should be installed on product partition.
	// TODO(b/150902910): remove, since this should be an inherited property.
	Product_specific *bool

	// List of APEX modules this interface can be used in.
	//
	// WARNING: HIDL is not fully supported in APEX since VINTF currently doesn't
	// read files from APEXes (b/130058564).
	//
	// "//apex_available:anyapex" is a pseudo APEX name that matches to any APEX.
	// "//apex_available:platform" refers to non-APEX partitions like "system.img"
	//
	// Note, this only applies to C++ libs, Java libs, and Java constant libs. It
	// does  not apply to VTS targets/adapter targets/fuzzers since these components
	// should not be shipped on device.
	Apex_available []string
}

type hidlInterface struct {
	android.ModuleBase

	properties hidlInterfaceProperties
}

func processSources(mctx android.LoadHookContext, srcs []string) ([]string, []string, bool) {
	var interfaces []string
	var types []string // hidl-gen only supports types.hal, but don't assume that here

	hasError := false

	for _, v := range srcs {
		if !strings.HasSuffix(v, ".hal") {
			mctx.PropertyErrorf("srcs", "Source must be a .hal file: "+v)
			hasError = true
			continue
		}

		name := strings.TrimSuffix(v, ".hal")

		if strings.HasPrefix(name, "I") {
			baseName := strings.TrimPrefix(name, "I")
			interfaces = append(interfaces, baseName)
		} else {
			types = append(types, name)
		}
	}

	return interfaces, types, !hasError
}

func processDependencies(mctx android.LoadHookContext, interfaces []string) ([]string, []string, bool) {
	var dependencies []string
	var javaDependencies []string

	hasError := false

	for _, v := range interfaces {
		name, err := parseFqName(v)
		if err != nil {
			mctx.PropertyErrorf("interfaces", err.Error())
			hasError = true
			continue
		}
		dependencies = append(dependencies, name.string())
		javaDependencies = append(javaDependencies, name.javaName())
	}

	return dependencies, javaDependencies, !hasError
}

func removeCoreDependencies(mctx android.LoadHookContext, dependencies []string) []string {
	var ret []string

	for _, i := range dependencies {
		if !isCorePackage(i) {
			ret = append(ret, i)
		}
	}

	return ret
}

func hidlInterfaceMutator(mctx android.LoadHookContext, i *hidlInterface) {
	name, err := parseFqName(i.ModuleBase.Name())
	if err != nil {
		mctx.PropertyErrorf("name", err.Error())
	}

	if !name.inPackage(i.properties.Root) {
		mctx.PropertyErrorf("root", i.properties.Root+" must be a prefix of  "+name.string()+".")
	}
	if lookupPackageRoot(i.properties.Root) == nil {
		mctx.PropertyErrorf("interfaces", `Cannot find package root specification for package `+
			`root '%s' needed for module '%s'. Either this is a mispelling of the package `+
			`root, or a new hidl_package_root module needs to be added. For example, you can `+
			`fix this error by adding the following to <some path>/Android.bp:

hidl_package_root {
name: "%s",
// if you want to require <some path>/current.txt for interface versioning
use_current: true,
}

This corresponds to the "-r%s:<some path>" option that would be passed into hidl-gen.`,
			i.properties.Root, name, i.properties.Root, i.properties.Root)
	}

	interfaces, types, _ := processSources(mctx, i.properties.Srcs)

	if len(interfaces) == 0 && len(types) == 0 {
		mctx.PropertyErrorf("srcs", "No sources provided.")
	}

	dependencies, javaDependencies, _ := processDependencies(mctx, i.properties.Interfaces)
	cppDependencies := removeCoreDependencies(mctx, dependencies)

	if mctx.Failed() {
		return
	}

	shouldGenerateLibrary := !isCorePackage(name.string())
	// explicitly true if not specified to give early warning to devs
	shouldGenerateJava := proptools.BoolDefault(i.properties.Gen_java, true)
	shouldGenerateJavaConstants := i.properties.Gen_java_constants
	shouldGenerateVts := shouldGenerateLibrary && proptools.BoolDefault(i.properties.Gen_vts, true)

	// TODO(b/150902910): re-enable VTS builds for product things
	shouldGenerateVts = shouldGenerateVts && !proptools.Bool(i.properties.Product_specific)

	var libraryIfExists []string
	if shouldGenerateLibrary {
		libraryIfExists = []string{name.string()}
	}

	// TODO(b/69002743): remove filegroups
	mctx.CreateModule(android.FileGroupFactory, &fileGroupProperties{
		Name: proptools.StringPtr(name.fileGroupName()),
		Srcs: i.properties.Srcs,
	})

	mctx.CreateModule(hidlGenFactory, &nameProperties{
		Name: proptools.StringPtr(name.sourcesName()),
	}, &hidlGenProperties{
		Language:   "c++-sources",
		FqName:     name.string(),
		Root:       i.properties.Root,
		Interfaces: i.properties.Interfaces,
		Inputs:     i.properties.Srcs,
		Outputs:    concat(wrap(name.dir(), interfaces, "All.cpp"), wrap(name.dir(), types, ".cpp")),
	})
	mctx.CreateModule(hidlGenFactory, &nameProperties{
		Name: proptools.StringPtr(name.headersName()),
	}, &hidlGenProperties{
		Language:   "c++-headers",
		FqName:     name.string(),
		Root:       i.properties.Root,
		Interfaces: i.properties.Interfaces,
		Inputs:     i.properties.Srcs,
		Outputs: concat(wrap(name.dir()+"I", interfaces, ".h"),
			wrap(name.dir()+"Bs", interfaces, ".h"),
			wrap(name.dir()+"BnHw", interfaces, ".h"),
			wrap(name.dir()+"BpHw", interfaces, ".h"),
			wrap(name.dir()+"IHw", interfaces, ".h"),
			wrap(name.dir(), types, ".h"),
			wrap(name.dir()+"hw", types, ".h")),
	})

	if shouldGenerateLibrary {
		mctx.CreateModule(cc.LibraryFactory, &ccProperties{
			Name:               proptools.StringPtr(name.string()),
			Host_supported:     proptools.BoolPtr(true),
			Recovery_available: proptools.BoolPtr(true),
			Vendor_available:   proptools.BoolPtr(true),
			Double_loadable:    proptools.BoolPtr(isDoubleLoadable(name.string())),
			Defaults:           []string{"hidl-module-defaults"},
			Generated_sources:  []string{name.sourcesName()},
			Generated_headers:  []string{name.headersName()},
			Shared_libs: concat(cppDependencies, []string{
				"libhidlbase",
				"liblog",
				"libutils",
				"libcutils",
			}),
			Export_shared_lib_headers: concat(cppDependencies, []string{
				"libhidlbase",
				"libutils",
			}),
			Export_generated_headers: []string{name.headersName()},
			Apex_available:           i.properties.Apex_available,
			Min_sdk_version:          getMinSdkVersion(name.string()),
		}, &i.properties.VndkProperties)
	}

	if shouldGenerateJava {
		mctx.CreateModule(hidlGenFactory, &nameProperties{
			Name: proptools.StringPtr(name.javaSourcesName()),
		}, &hidlGenProperties{
			Language:   "java",
			FqName:     name.string(),
			Root:       i.properties.Root,
			Interfaces: i.properties.Interfaces,
			Inputs:     i.properties.Srcs,
			Outputs:    []string{"srcs.srcjar"},
		})

		commonJavaProperties := javaProperties{
			Defaults:    []string{"hidl-java-module-defaults"},
			Installable: proptools.BoolPtr(true),
			Srcs:        []string{":" + name.javaSourcesName()},

			// This should ideally be system_current, but android.hidl.base-V1.0-java is used
			// to build framework, which is used to build system_current.  Use core_current
			// plus hwbinder.stubs, which together form a subset of system_current that does
			// not depend on framework.
			Sdk_version:    proptools.StringPtr("core_current"),
			Libs:           []string{"hwbinder.stubs"},
			Apex_available: i.properties.Apex_available,
		}

		mctx.CreateModule(java.LibraryFactory, &javaProperties{
			Name:        proptools.StringPtr(name.javaName()),
			Static_libs: javaDependencies,
		}, &commonJavaProperties)
		mctx.CreateModule(java.LibraryFactory, &javaProperties{
			Name: proptools.StringPtr(name.javaSharedName()),
			Libs: javaDependencies,
		}, &commonJavaProperties)
	}

	if shouldGenerateJavaConstants {
		mctx.CreateModule(hidlGenFactory, &nameProperties{
			Name: proptools.StringPtr(name.javaConstantsSourcesName()),
		}, &hidlGenProperties{
			Language:   "java-constants",
			FqName:     name.string(),
			Root:       i.properties.Root,
			Interfaces: i.properties.Interfaces,
			Inputs:     i.properties.Srcs,
			Outputs:    []string{name.sanitizedDir() + "Constants.java"},
		})
		mctx.CreateModule(java.LibraryFactory, &javaProperties{
			Name:           proptools.StringPtr(name.javaConstantsName()),
			Defaults:       []string{"hidl-java-module-defaults"},
			Sdk_version:    proptools.StringPtr("core_current"),
			Srcs:           []string{":" + name.javaConstantsSourcesName()},
			Apex_available: i.properties.Apex_available,
		})
	}

	mctx.CreateModule(hidlGenFactory, &nameProperties{
		Name: proptools.StringPtr(name.adapterHelperSourcesName()),
	}, &hidlGenProperties{
		Language:   "c++-adapter-sources",
		FqName:     name.string(),
		Root:       i.properties.Root,
		Interfaces: i.properties.Interfaces,
		Inputs:     i.properties.Srcs,
		Outputs:    wrap(name.dir()+"A", concat(interfaces, types), ".cpp"),
	})
	mctx.CreateModule(hidlGenFactory, &nameProperties{
		Name: proptools.StringPtr(name.adapterHelperHeadersName()),
	}, &hidlGenProperties{
		Language:   "c++-adapter-headers",
		FqName:     name.string(),
		Root:       i.properties.Root,
		Interfaces: i.properties.Interfaces,
		Inputs:     i.properties.Srcs,
		Outputs:    wrap(name.dir()+"A", concat(interfaces, types), ".h"),
	})

	mctx.CreateModule(cc.LibraryFactory, &ccProperties{
		Name:              proptools.StringPtr(name.adapterHelperName()),
		Vendor_available:  proptools.BoolPtr(true),
		Defaults:          []string{"hidl-module-defaults"},
		Generated_sources: []string{name.adapterHelperSourcesName()},
		Generated_headers: []string{name.adapterHelperHeadersName()},
		Shared_libs: []string{
			"libbase",
			"libcutils",
			"libhidlbase",
			"liblog",
			"libutils",
		},
		Static_libs: concat([]string{
			"libhidladapter",
		}, wrap("", dependencies, "-adapter-helper"), cppDependencies, libraryIfExists),
		Export_shared_lib_headers: []string{
			"libhidlbase",
		},
		Export_static_lib_headers: concat([]string{
			"libhidladapter",
		}, wrap("", dependencies, "-adapter-helper"), cppDependencies, libraryIfExists),
		Export_generated_headers: []string{name.adapterHelperHeadersName()},
		Group_static_libs:        proptools.BoolPtr(true),
	})
	mctx.CreateModule(hidlGenFactory, &nameProperties{
		Name: proptools.StringPtr(name.adapterSourcesName()),
	}, &hidlGenProperties{
		Language:   "c++-adapter-main",
		FqName:     name.string(),
		Root:       i.properties.Root,
		Interfaces: i.properties.Interfaces,
		Inputs:     i.properties.Srcs,
		Outputs:    []string{"main.cpp"},
	})
	mctx.CreateModule(cc.TestFactory, &ccProperties{
		Name:              proptools.StringPtr(name.adapterName()),
		Generated_sources: []string{name.adapterSourcesName()},
		Shared_libs: []string{
			"libbase",
			"libcutils",
			"libhidlbase",
			"liblog",
			"libutils",
		},
		Static_libs: concat([]string{
			"libhidladapter",
			name.adapterHelperName(),
		}, wrap("", dependencies, "-adapter-helper"), cppDependencies, libraryIfExists),
		Group_static_libs: proptools.BoolPtr(true),
	})

	if shouldGenerateVts {
		vtsSpecs := concat(wrap(name.dir(), interfaces, ".vts"), wrap(name.dir(), types, ".vts"))

		mctx.CreateModule(hidlGenFactory, &nameProperties{
			Name: proptools.StringPtr(name.vtsSpecName()),
		}, &hidlGenProperties{
			Language:   "vts",
			FqName:     name.string(),
			Root:       i.properties.Root,
			Interfaces: i.properties.Interfaces,
			Inputs:     i.properties.Srcs,
			Outputs:    vtsSpecs,
		})

		mctx.CreateModule(vtscFactory, &nameProperties{
			Name: proptools.StringPtr(name.vtsDriverSourcesName()),
		}, &vtscProperties{
			Mode:        "DRIVER",
			Type:        "SOURCE",
			SpecName:    name.vtsSpecName(),
			Outputs:     wrap("", vtsSpecs, ".cpp"),
			PackagePath: name.dir(),
		})
		mctx.CreateModule(vtscFactory, &nameProperties{
			Name: proptools.StringPtr(name.vtsDriverHeadersName()),
		}, &vtscProperties{
			Mode:        "DRIVER",
			Type:        "HEADER",
			SpecName:    name.vtsSpecName(),
			Outputs:     wrap("", vtsSpecs, ".h"),
			PackagePath: name.dir(),
		})
		mctx.CreateModule(cc.LibraryFactory, &ccProperties{
			Name:                      proptools.StringPtr(name.vtsDriverName()),
			Defaults:                  []string{"VtsHalDriverDefaults"},
			Generated_sources:         []string{name.vtsDriverSourcesName()},
			Generated_headers:         []string{name.vtsDriverHeadersName()},
			Export_generated_headers:  []string{name.vtsDriverHeadersName()},
			Shared_libs:               wrap("", cppDependencies, "-vts.driver"),
			Export_shared_lib_headers: wrap("", cppDependencies, "-vts.driver"),
			Static_libs:               concat(cppDependencies, libraryIfExists),

			// TODO(b/126244142)
			Cflags: []string{"-Wno-unused-variable"},
		})

		mctx.CreateModule(vtscFactory, &nameProperties{
			Name: proptools.StringPtr(name.vtsProfilerSourcesName()),
		}, &vtscProperties{
			Mode:        "PROFILER",
			Type:        "SOURCE",
			SpecName:    name.vtsSpecName(),
			Outputs:     wrap("", vtsSpecs, ".cpp"),
			PackagePath: name.dir(),
		})
		mctx.CreateModule(vtscFactory, &nameProperties{
			Name: proptools.StringPtr(name.vtsProfilerHeadersName()),
		}, &vtscProperties{
			Mode:        "PROFILER",
			Type:        "HEADER",
			SpecName:    name.vtsSpecName(),
			Outputs:     wrap("", vtsSpecs, ".h"),
			PackagePath: name.dir(),
		})
		mctx.CreateModule(cc.LibraryFactory, &ccProperties{
			Name:                      proptools.StringPtr(name.vtsProfilerName()),
			Defaults:                  []string{"VtsHalProfilerDefaults"},
			Generated_sources:         []string{name.vtsProfilerSourcesName()},
			Generated_headers:         []string{name.vtsProfilerHeadersName()},
			Export_generated_headers:  []string{name.vtsProfilerHeadersName()},
			Shared_libs:               wrap("", cppDependencies, "-vts.profiler"),
			Export_shared_lib_headers: wrap("", cppDependencies, "-vts.profiler"),
			Static_libs:               concat(cppDependencies, libraryIfExists),

			// TODO(b/126244142)
			Cflags: []string{"-Wno-unused-variable"},
		})

		specDependencies := append(cppDependencies, name.string())
		mctx.CreateModule(cc.FuzzFactory, &ccProperties{
			Name:        proptools.StringPtr(name.vtsFuzzerName()),
			Defaults:    []string{"vts_proto_fuzzer_default"},
			Shared_libs: []string{name.vtsDriverName()},
			Cflags: []string{
				"-DSTATIC_TARGET_FQ_NAME=" + name.string(),
				"-DSTATIC_SPEC_DATA=" + strings.Join(specDependencies, ":"),
			},
		}, &fuzzProperties{
			Data: wrap(":", specDependencies, "-vts.spec"),
			Fuzz_config: &fuzzConfig{
				Fuzz_on_haiku_device: proptools.BoolPtr(isFuzzerEnabled(name.vtsFuzzerName())),
			},
		})
	}

	mctx.CreateModule(hidlGenFactory, &nameProperties{
		Name: proptools.StringPtr(name.lintName()),
	}, &hidlGenProperties{
		Language:   "lint",
		FqName:     name.string(),
		Root:       i.properties.Root,
		Interfaces: i.properties.Interfaces,
		Inputs:     i.properties.Srcs,
	})

	mctx.CreateModule(hidlGenFactory, &nameProperties{
		Name: proptools.StringPtr(name.inheritanceHierarchyName()),
	}, &hidlGenProperties{
		Language:   "inheritance-hierarchy",
		FqName:     name.string(),
		Root:       i.properties.Root,
		Interfaces: i.properties.Interfaces,
		Inputs:     i.properties.Srcs,
	})
}

func (h *hidlInterface) Name() string {
	return h.ModuleBase.Name() + hidlInterfaceSuffix
}
func (h *hidlInterface) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	visited := false
	ctx.VisitDirectDeps(func(dep android.Module) {
		if visited {
			panic("internal error, multiple dependencies found but only one added")
		}
		visited = true
		h.properties.Full_root_option = dep.(*hidlPackageRoot).getFullPackageRoot()
	})
	if !visited {
		panic("internal error, no dependencies found but dependency added")
	}

}
func (h *hidlInterface) DepsMutator(ctx android.BottomUpMutatorContext) {
	ctx.AddDependency(ctx.Module(), nil, h.properties.Root)
}

func hidlInterfaceFactory() android.Module {
	i := &hidlInterface{}
	i.AddProperties(&i.properties)
	android.InitAndroidModule(i)
	android.AddLoadHook(i, func(ctx android.LoadHookContext) { hidlInterfaceMutator(ctx, i) })

	return i
}

var minSdkVersion = map[string]string{
	"android.hardware.audio.common@5.0":         "30",
	"android.hardware.bluetooth.a2dp@1.0":       "30",
	"android.hardware.bluetooth.audio@2.0":      "30",
	"android.hardware.bluetooth@1.0":            "30",
	"android.hardware.bluetooth@1.1":            "30",
	"android.hardware.cas.native@1.0":           "29",
	"android.hardware.cas@1.0":                  "29",
	"android.hardware.graphics.allocator@2.0":   "29",
	"android.hardware.graphics.allocator@3.0":   "29",
	"android.hardware.graphics.allocator@4.0":   "29",
	"android.hardware.graphics.bufferqueue@1.0": "29",
	"android.hardware.graphics.bufferqueue@2.0": "29",
	"android.hardware.graphics.common@1.0":      "29",
	"android.hardware.graphics.common@1.1":      "29",
	"android.hardware.graphics.common@1.2":      "29",
	"android.hardware.graphics.mapper@2.0":      "29",
	"android.hardware.graphics.mapper@2.1":      "29",
	"android.hardware.graphics.mapper@3.0":      "29",
	"android.hardware.graphics.mapper@4.0":      "29",
	"android.hardware.media.bufferpool@2.0":     "29",
	"android.hardware.media.c2@1.0":             "29",
	"android.hardware.media.c2@1.1":             "29",
	"android.hardware.media.omx@1.0":            "29",
	"android.hardware.media@1.0":                "29",
	"android.hardware.neuralnetworks@1.0":       "30",
	"android.hardware.neuralnetworks@1.1":       "30",
	"android.hardware.neuralnetworks@1.2":       "30",
	"android.hardware.neuralnetworks@1.3":       "30",
	"android.hidl.allocator@1.0":                "29",
	"android.hidl.memory.token@1.0":             "29",
	"android.hidl.memory@1.0":                   "29",
	"android.hidl.safe_union@1.0":               "29",
	"android.hidl.token@1.0":                    "29",
}

func getMinSdkVersion(name string) *string {
	if ver, ok := minSdkVersion[name]; ok {
		return proptools.StringPtr(ver)
	}
	return nil
}

var doubleLoadablePackageNames = []string{
	"android.frameworks.bufferhub@1.0",
	"android.hardware.cas@1.0",
	"android.hardware.cas.native@1.0",
	"android.hardware.configstore@",
	"android.hardware.drm@",
	"android.hardware.graphics.allocator@",
	"android.hardware.graphics.bufferqueue@",
	"android.hardware.media@",
	"android.hardware.media.bufferpool@",
	"android.hardware.media.c2@",
	"android.hardware.media.omx@",
	"android.hardware.memtrack@1.0",
	"android.hardware.neuralnetworks@",
	"android.hidl.allocator@",
	"android.hidl.token@",
	"android.system.suspend@1.0",
}

func isDoubleLoadable(name string) bool {
	for _, pkgname := range doubleLoadablePackageNames {
		if strings.HasPrefix(name, pkgname) {
			return true
		}
	}
	return false
}

// packages in libhidlbase
var coreDependencyPackageNames = []string{
	"android.hidl.base@",
	"android.hidl.manager@",
}

func isCorePackage(name string) bool {
	for _, pkgname := range coreDependencyPackageNames {
		if strings.HasPrefix(name, pkgname) {
			return true
		}
	}
	return false
}

var fuzzerPackageNameBlacklist = []string{
	"android.hardware.keymaster@", // to avoid deleteAllKeys()
}

func isFuzzerEnabled(name string) bool {
	for _, pkgname := range fuzzerPackageNameBlacklist {
		if strings.HasPrefix(name, pkgname) {
			return false
		}
	}
	return true
}

// TODO(b/126383715): centralize this logic/support filtering in core VTS build
var coreVtsSpecs = []string{
	"android.frameworks.",
	"android.hardware.",
	"android.hidl.",
	"android.system.",
}

func isVtsSpecPackage(name string) bool {
	for _, pkgname := range coreVtsSpecs {
		if strings.HasPrefix(name, pkgname) {
			return true
		}
	}
	return false
}

var vtsListKey = android.NewOnceKey("vtsList")

func vtsList(config android.Config) *android.Paths {
	return config.Once(vtsListKey, func() interface{} {
		return &android.Paths{}
	}).(*android.Paths)
}

var vtsListMutex sync.Mutex

func makeVarsProvider(ctx android.MakeVarsContext) {
	vtsList := vtsList(ctx.Config()).Strings()
	sort.Strings(vtsList)

	ctx.Strict("VTS_SPEC_FILE_LIST", strings.Join(vtsList, " "))
}
