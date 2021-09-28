// Copyright (C) 2018 The Android Open Source Project
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
	"android/soong/android"
	"android/soong/cc"
	"android/soong/genrule"
	"android/soong/java"
	"android/soong/phony"

	"fmt"
	"io"
	"path/filepath"
	"strconv"
	"strings"
	"sync"

	"github.com/google/blueprint"
	"github.com/google/blueprint/pathtools"
	"github.com/google/blueprint/proptools"
)

const (
	aidlInterfaceSuffix       = "_interface"
	aidlMetadataSingletonName = "aidl_metadata_json"
	aidlApiDir                = "aidl_api"
	aidlApiSuffix             = "-api"
	langCpp                   = "cpp"
	langJava                  = "java"
	langNdk                   = "ndk"
	langNdkPlatform           = "ndk_platform"

	currentVersion = "current"
)

var (
	pctx = android.NewPackageContext("android/aidl")

	aidlDirPrepareRule = pctx.StaticRule("aidlDirPrepareRule", blueprint.RuleParams{
		Command: `rm -rf "${outDir}" && mkdir -p "${outDir}" && ` +
			`touch ${out} # ${in}`,
		Description: "create ${out}",
	}, "outDir")

	aidlCppRule = pctx.StaticRule("aidlCppRule", blueprint.RuleParams{
		Command: `mkdir -p "${headerDir}" && ` +
			`${aidlCmd} --lang=${lang} ${optionalFlags} --structured --ninja -d ${out}.d ` +
			`-h ${headerDir} -o ${outDir} ${imports} ${in}`,
		Depfile:     "${out}.d",
		Deps:        blueprint.DepsGCC,
		CommandDeps: []string{"${aidlCmd}"},
		Description: "AIDL ${lang} ${in}",
	}, "imports", "lang", "headerDir", "outDir", "optionalFlags")

	aidlJavaRule = pctx.StaticRule("aidlJavaRule", blueprint.RuleParams{
		Command: `${aidlCmd} --lang=java ${optionalFlags} --structured --ninja -d ${out}.d ` +
			`-o ${outDir} ${imports} ${in}`,
		Depfile:     "${out}.d",
		Deps:        blueprint.DepsGCC,
		CommandDeps: []string{"${aidlCmd}"},
		Description: "AIDL Java ${in}",
	}, "imports", "outDir", "optionalFlags")

	aidlDumpApiRule = pctx.StaticRule("aidlDumpApiRule", blueprint.RuleParams{
		Command: `rm -rf "${outDir}" && mkdir -p "${outDir}" && ` +
			`${aidlCmd} --dumpapi --structured ${imports} ${optionalFlags} --out ${outDir} ${in} && ` +
			`(cd ${outDir} && find ./ -name "*.aidl" -print0 | LC_ALL=C sort -z | xargs -0 sha1sum && echo ${latestVersion}) | sha1sum | cut -d " " -f 1 > ${hashFile} `,
		CommandDeps: []string{"${aidlCmd}"},
	}, "optionalFlags", "imports", "outDir", "hashFile", "latestVersion")

	aidlMetadataRule = pctx.StaticRule("aidlMetadataRule", blueprint.RuleParams{
		Command: `rm -f ${out} && { ` +
			`echo '{' && ` +
			`echo "\"name\": \"${name}\"," && ` +
			`echo "\"stability\": \"${stability}\"," && ` +
			`echo "\"types\": [${types}]," && ` +
			`echo "\"hashes\": [${hashes}]" && ` +
			`echo '}' ` +
			`;} >> ${out}`,
		Description: "AIDL metadata: ${out}",
	}, "name", "stability", "types", "hashes")

	aidlDumpMappingsRule = pctx.StaticRule("aidlDumpMappingsRule", blueprint.RuleParams{
		Command: `rm -rf "${outDir}" && mkdir -p "${outDir}" && ` +
			`${aidlCmd} --apimapping ${outDir}/intermediate.txt ${in} ${imports} && ` +
			`${aidlToJniCmd} ${outDir}/intermediate.txt ${out}`,
		CommandDeps: []string{"${aidlCmd}"},
	}, "imports", "outDir")

	aidlCheckApiRule = pctx.StaticRule("aidlCheckApiRule", blueprint.RuleParams{
		Command: `(${aidlCmd} ${optionalFlags} --checkapi ${old} ${new} && touch ${out}) || ` +
			`(cat ${messageFile} && exit 1)`,
		CommandDeps: []string{"${aidlCmd}"},
		Description: "AIDL CHECK API: ${new} against ${old}",
	}, "optionalFlags", "old", "new", "messageFile")

	aidlDiffApiRule = pctx.StaticRule("aidlDiffApiRule", blueprint.RuleParams{
		Command: `if diff -r -B -I '//.*' -x '${hashFile}' '${old}' '${new}'; then touch '${out}'; else ` +
			`cat '${messageFile}' && exit 1; fi`,
		Description: "Check equality of ${new} and ${old}",
	}, "old", "new", "hashFile", "messageFile")

	aidlVerifyHashRule = pctx.StaticRule("aidlVerifyHashRule", blueprint.RuleParams{
		Command: `if [ $$(cd '${apiDir}' && { find ./ -name "*.aidl" -print0 | LC_ALL=C sort -z | xargs -0 sha1sum && echo ${version}; } | sha1sum | cut -d " " -f 1) = $$(read -r <'${hashFile}' hash extra; printf %s $$hash) ]; then ` +
			`touch ${out}; else cat '${messageFile}' && exit 1; fi`,
		Description: "Verify ${apiDir} files have not been modified",
	}, "apiDir", "version", "messageFile", "hashFile")

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
	pctx.HostBinToolVariable("aidlCmd", "aidl")
	pctx.SourcePathVariable("aidlToJniCmd", "system/tools/aidl/build/aidl_to_jni.py")
	android.RegisterModuleType("aidl_interface", aidlInterfaceFactory)
	android.RegisterModuleType("aidl_mapping", aidlMappingFactory)
	android.RegisterMakeVarsProvider(pctx, allAidlInterfacesMakeVars)
	android.RegisterModuleType("aidl_interfaces_metadata", aidlInterfacesMetadataSingletonFactory)
	android.PostDepsMutators(func(ctx android.RegisterMutatorsContext) {
		ctx.BottomUp("checkUnstableModule", checkUnstableModuleMutator).Parallel()
	})
}

func checkUnstableModuleMutator(mctx android.BottomUpMutatorContext) {
	mctx.VisitDirectDepsIf(func(m android.Module) bool {
		return android.InList(m.Name(), *unstableModules(mctx.Config()))
	}, func(m android.Module) {
		if mctx.ModuleName() == m.Name() {
			return
		}
		// TODO(b/154066686): Replace it with a common method instead of listing up module types.
		// Test libraries are exempted.
		if android.InList(mctx.ModuleType(), []string{"cc_test_library", "android_test", "cc_benchmark", "cc_test"}) {
			return
		}

		mctx.ModuleErrorf(m.Name() + " is disallowed in release version because it is unstable.")
	})
}

// wrap(p, a, s) = [p + v + s for v in a]
func wrap(prefix string, strs []string, suffix string) []string {
	ret := make([]string, len(strs))
	for i, v := range strs {
		ret[i] = prefix + v + suffix
	}
	return ret
}

// concat(a...) = sum((i for i in a), [])
func concat(sstrs ...[]string) []string {
	var ret []string
	for _, v := range sstrs {
		ret = append(ret, v...)
	}
	return ret
}

func getPaths(ctx android.ModuleContext, rawSrcs []string) (paths android.Paths, imports []string) {
	srcs := android.PathsForModuleSrc(ctx, rawSrcs)

	if len(srcs) == 0 {
		ctx.PropertyErrorf("srcs", "No sources provided.")
	}

	for _, src := range srcs {
		if src.Ext() != ".aidl" {
			// Silently ignore non-aidl files as some filegroups have both java and aidl files together
			continue
		}
		baseDir := strings.TrimSuffix(src.String(), src.Rel())
		if baseDir != "" && !android.InList(baseDir, imports) {
			imports = append(imports, baseDir)
		}
	}

	return srcs, imports
}

func isRelativePath(path string) bool {
	if path == "" {
		return true
	}
	return filepath.Clean(path) == path && path != ".." &&
		!strings.HasPrefix(path, "../") && !strings.HasPrefix(path, "/")
}

type aidlGenProperties struct {
	Srcs      []string `android:"path"`
	AidlRoot  string   // base directory for the input aidl file
	Imports   []string
	Stability *string
	Lang      string // target language [java|cpp|ndk]
	BaseName  string
	GenLog    bool
	Version   string
	GenTrace  bool
	Unstable  *bool
}

type aidlGenRule struct {
	android.ModuleBase

	properties aidlGenProperties

	implicitInputs android.Paths
	importFlags    string

	// TODO(b/149952131): always have a hash file
	hashFile android.Path

	genOutDir     android.ModuleGenPath
	genHeaderDir  android.ModuleGenPath
	genHeaderDeps android.Paths
	genOutputs    android.WritablePaths
}

var _ android.SourceFileProducer = (*aidlGenRule)(nil)
var _ genrule.SourceFileGenerator = (*aidlGenRule)(nil)

func (g *aidlGenRule) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	srcs, imports := getPaths(ctx, g.properties.Srcs)

	if ctx.Failed() {
		return
	}

	genDirTimestamp := android.PathForModuleGen(ctx, "timestamp")
	g.implicitInputs = append(g.implicitInputs, genDirTimestamp)

	var importPaths []string
	importPaths = append(importPaths, imports...)
	ctx.VisitDirectDeps(func(dep android.Module) {
		if importedAidl, ok := dep.(*aidlInterface); ok {
			importPaths = append(importPaths, importedAidl.properties.Full_import_paths...)
		} else if api, ok := dep.(*aidlApi); ok {
			// When compiling an AIDL interface, also make sure that each
			// version of the interface is compatible with its previous version
			for _, path := range api.checkApiTimestamps {
				g.implicitInputs = append(g.implicitInputs, path)
			}
			for _, path := range api.checkHashTimestamps {
				g.implicitInputs = append(g.implicitInputs, path)
			}
		}
	})
	g.importFlags = strings.Join(wrap("-I", importPaths, ""), " ")

	g.genOutDir = android.PathForModuleGen(ctx)
	g.genHeaderDir = android.PathForModuleGen(ctx, "include")
	for _, src := range srcs {
		outFile, headers := g.generateBuildActionsForSingleAidl(ctx, src)
		g.genOutputs = append(g.genOutputs, outFile)
		g.genHeaderDeps = append(g.genHeaderDeps, headers...)
	}

	// This is to clean genOutDir before generating any file
	ctx.ModuleBuild(pctx, android.ModuleBuildParams{
		Rule:   aidlDirPrepareRule,
		Inputs: srcs,
		Output: genDirTimestamp,
		Args: map[string]string{
			"outDir": g.genOutDir.String(),
		},
	})
}

// baseDir is the directory where the package name starts. e.g. For an AIDL fil
// mymodule/aidl_src/com/android/IFoo.aidl, baseDir is mymodule/aidl_src given that the package name is
// com.android. The build system however don't know the package name without actually reading the AIDL file.
// Therefore, we rely on the user to correctly set the base directory via following two methods:
// 1) via the 'path' property of filegroup or
// 2) via `local_include_dir' of the aidl_interface module.
func getBaseDir(ctx android.ModuleContext, src android.Path, aidlRoot android.Path) string {
	// By default, we try to get 1) by reading Rel() of the input path.
	baseDir := strings.TrimSuffix(src.String(), src.Rel())
	// However, if 2) is set and it's more specific (i.e. deeper) than 1), we use 2).
	if strings.HasPrefix(aidlRoot.String(), baseDir) {
		baseDir = aidlRoot.String()
	}
	return baseDir
}

func (g *aidlGenRule) generateBuildActionsForSingleAidl(ctx android.ModuleContext, src android.Path) (android.WritablePath, android.Paths) {
	baseDir := getBaseDir(ctx, src, android.PathForModuleSrc(ctx, g.properties.AidlRoot))

	var ext string
	if g.properties.Lang == langJava {
		ext = "java"
	} else {
		ext = "cpp"
	}
	relPath, _ := filepath.Rel(baseDir, src.String())
	outFile := android.PathForModuleGen(ctx, pathtools.ReplaceExtension(relPath, ext))
	implicits := g.implicitInputs

	var optionalFlags []string
	if g.properties.Version != "" {
		optionalFlags = append(optionalFlags, "--version "+g.properties.Version)

		hash := "notfrozen"
		if !strings.HasPrefix(baseDir, ctx.Config().BuildDir()) {
			hashFile := android.ExistentPathForSource(ctx, baseDir, ".hash")
			if hashFile.Valid() {
				hash = "$$(read -r <" + hashFile.Path().String() + " hash extra; printf '%s' \"$$hash\")"
				implicits = append(implicits, hashFile.Path())

				g.hashFile = hashFile.Path()
			}
		}
		optionalFlags = append(optionalFlags, "--hash "+hash)
	}
	if g.properties.GenTrace {
		optionalFlags = append(optionalFlags, "-t")
	}
	if g.properties.Stability != nil {
		optionalFlags = append(optionalFlags, "--stability", *g.properties.Stability)
	}

	var headers android.WritablePaths
	if g.properties.Lang == langJava {
		ctx.ModuleBuild(pctx, android.ModuleBuildParams{
			Rule:      aidlJavaRule,
			Input:     src,
			Implicits: implicits,
			Output:    outFile,
			Args: map[string]string{
				"imports":       g.importFlags,
				"outDir":        g.genOutDir.String(),
				"optionalFlags": strings.Join(optionalFlags, " "),
			},
		})
	} else {
		typeName := strings.TrimSuffix(filepath.Base(relPath), ".aidl")
		packagePath := filepath.Dir(relPath)
		baseName := typeName
		// TODO(b/111362593): aidl_to_cpp_common.cpp uses heuristics to figure out if
		//   an interface name has a leading I. Those same heuristics have been
		//   moved here.
		if len(baseName) >= 2 && baseName[0] == 'I' &&
			strings.ToUpper(baseName)[1] == baseName[1] {
			baseName = strings.TrimPrefix(typeName, "I")
		}

		prefix := ""
		if g.properties.Lang == langNdk || g.properties.Lang == langNdkPlatform {
			prefix = "aidl"
		}

		headers = append(headers, g.genHeaderDir.Join(ctx, prefix, packagePath,
			typeName+".h"))
		headers = append(headers, g.genHeaderDir.Join(ctx, prefix, packagePath,
			"Bp"+baseName+".h"))
		headers = append(headers, g.genHeaderDir.Join(ctx, prefix, packagePath,
			"Bn"+baseName+".h"))

		if g.properties.GenLog {
			optionalFlags = append(optionalFlags, "--log")
		}

		aidlLang := g.properties.Lang
		if aidlLang == langNdkPlatform {
			aidlLang = "ndk"
		}

		ctx.ModuleBuild(pctx, android.ModuleBuildParams{
			Rule:            aidlCppRule,
			Input:           src,
			Implicits:       implicits,
			Output:          outFile,
			ImplicitOutputs: headers,
			Args: map[string]string{
				"imports":       g.importFlags,
				"lang":          aidlLang,
				"headerDir":     g.genHeaderDir.String(),
				"outDir":        g.genOutDir.String(),
				"optionalFlags": strings.Join(optionalFlags, " "),
			},
		})
	}

	return outFile, headers.Paths()
}

func (g *aidlGenRule) GeneratedSourceFiles() android.Paths {
	return g.genOutputs.Paths()
}

func (g *aidlGenRule) Srcs() android.Paths {
	return g.genOutputs.Paths()
}

func (g *aidlGenRule) GeneratedDeps() android.Paths {
	return g.genHeaderDeps
}

func (g *aidlGenRule) GeneratedHeaderDirs() android.Paths {
	return android.Paths{g.genHeaderDir}
}

func (g *aidlGenRule) DepsMutator(ctx android.BottomUpMutatorContext) {
	ctx.AddDependency(ctx.Module(), nil, wrap("", g.properties.Imports, aidlInterfaceSuffix)...)
	if !proptools.Bool(g.properties.Unstable) {
		ctx.AddDependency(ctx.Module(), nil, g.properties.BaseName+aidlApiSuffix)
	}

	ctx.AddReverseDependency(ctx.Module(), nil, aidlMetadataSingletonName)
}

func aidlGenFactory() android.Module {
	g := &aidlGenRule{}
	g.AddProperties(&g.properties)
	android.InitAndroidModule(g)
	return g
}

type aidlApiProperties struct {
	BaseName  string
	Srcs      []string `android:"path"`
	AidlRoot  string   // base directory for the input aidl file
	Stability *string
	Imports   []string
	Versions  []string
}

type aidlApi struct {
	android.ModuleBase

	properties aidlApiProperties

	// for triggering api check for version X against version X-1
	checkApiTimestamps android.WritablePaths

	// for triggering updating current API
	updateApiTimestamp android.WritablePath

	// for triggering check that files have not been modified
	checkHashTimestamps android.WritablePaths

	// for triggering freezing API as the new version
	freezeApiTimestamp android.WritablePath
}

func (m *aidlApi) apiDir() string {
	return filepath.Join(aidlApiDir, m.properties.BaseName)
}

// `m <iface>-freeze-api` will freeze ToT as this version
func (m *aidlApi) nextVersion(ctx android.ModuleContext) string {
	if len(m.properties.Versions) == 0 {
		return "1"
	} else {
		latestVersion := m.properties.Versions[len(m.properties.Versions)-1]

		i, err := strconv.Atoi(latestVersion)
		if err != nil {
			panic(err)
		}

		return strconv.Itoa(i + 1)
	}
}

type apiDump struct {
	dir      android.Path
	files    android.Paths
	hashFile android.OptionalPath
}

func (m *aidlApi) createApiDumpFromSource(ctx android.ModuleContext) apiDump {
	srcs, imports := getPaths(ctx, m.properties.Srcs)

	if ctx.Failed() {
		return apiDump{}
	}

	var importPaths []string
	importPaths = append(importPaths, imports...)
	ctx.VisitDirectDeps(func(dep android.Module) {
		if importedAidl, ok := dep.(*aidlInterface); ok {
			importPaths = append(importPaths, importedAidl.properties.Full_import_paths...)
		}
	})

	var apiDir android.WritablePath
	var apiFiles android.WritablePaths
	var hashFile android.WritablePath

	apiDir = android.PathForModuleOut(ctx, "dump")
	aidlRoot := android.PathForModuleSrc(ctx, m.properties.AidlRoot)
	for _, src := range srcs {
		baseDir := getBaseDir(ctx, src, aidlRoot)
		relPath, _ := filepath.Rel(baseDir, src.String())
		outFile := android.PathForModuleOut(ctx, "dump", relPath)
		apiFiles = append(apiFiles, outFile)
	}
	hashFile = android.PathForModuleOut(ctx, "dump", ".hash")
	latestVersion := "latest-version"
	if len(m.properties.Versions) >= 1 {
		latestVersion = m.properties.Versions[len(m.properties.Versions)-1]
	}

	var optionalFlags []string
	if m.properties.Stability != nil {
		optionalFlags = append(optionalFlags, "--stability", *m.properties.Stability)
	}

	ctx.ModuleBuild(pctx, android.ModuleBuildParams{
		Rule:    aidlDumpApiRule,
		Outputs: append(apiFiles, hashFile),
		Inputs:  srcs,
		Args: map[string]string{
			"optionalFlags": strings.Join(optionalFlags, " "),
			"imports":       strings.Join(wrap("-I", importPaths, ""), " "),
			"outDir":        apiDir.String(),
			"hashFile":      hashFile.String(),
			"latestVersion": latestVersion,
		},
	})
	return apiDump{apiDir, apiFiles.Paths(), android.OptionalPathForPath(hashFile)}
}

func (m *aidlApi) makeApiDumpAsVersion(ctx android.ModuleContext, dump apiDump, version string) android.WritablePath {
	timestampFile := android.PathForModuleOut(ctx, "updateapi_"+version+".timestamp")

	modulePath := android.PathForModuleSrc(ctx).String()

	targetDir := filepath.Join(modulePath, m.apiDir(), version)
	rb := android.NewRuleBuilder()
	// Wipe the target directory and then copy the API dump into the directory
	rb.Command().Text("mkdir -p " + targetDir)
	rb.Command().Text("rm -rf " + targetDir + "/*")
	if version != currentVersion {
		rb.Command().Text("cp -rf " + dump.dir.String() + "/. " + targetDir).Implicits(dump.files)
		// If this is making a new frozen (i.e. non-current) version of the interface,
		// modify Android.bp file to add the new version to the 'versions' property.
		rb.Command().BuiltTool(ctx, "bpmodify").
			Text("-w -m " + m.properties.BaseName).
			Text("-parameter versions -a " + version).
			Text(android.PathForModuleSrc(ctx, "Android.bp").String())
	} else {
		// In this case (unfrozen interface), don't copy .hash
		rb.Command().Text("cp -rf " + dump.dir.String() + "/* " + targetDir).Implicits(dump.files)
	}
	rb.Command().Text("touch").Output(timestampFile)

	rb.Build(pctx, ctx, "dump_aidl_api"+m.properties.BaseName+"_"+version,
		"Making AIDL API of "+m.properties.BaseName+" as version "+version)
	return timestampFile
}

func (m *aidlApi) checkCompatibility(ctx android.ModuleContext, oldDump apiDump, newDump apiDump) android.WritablePath {
	newVersion := newDump.dir.Base()
	timestampFile := android.PathForModuleOut(ctx, "checkapi_"+newVersion+".timestamp")
	messageFile := android.PathForSource(ctx, "system/tools/aidl/build/message_check_compatibility.txt")

	var optionalFlags []string
	if m.properties.Stability != nil {
		optionalFlags = append(optionalFlags, "--stability", *m.properties.Stability)
	}

	var implicits android.Paths
	implicits = append(implicits, oldDump.files...)
	implicits = append(implicits, newDump.files...)
	implicits = append(implicits, messageFile)
	ctx.ModuleBuild(pctx, android.ModuleBuildParams{
		Rule:      aidlCheckApiRule,
		Implicits: implicits,
		Output:    timestampFile,
		Args: map[string]string{
			"optionalFlags": strings.Join(optionalFlags, " "),
			"old":           oldDump.dir.String(),
			"new":           newDump.dir.String(),
			"messageFile":   messageFile.String(),
		},
	})
	return timestampFile
}

func (m *aidlApi) checkEquality(ctx android.ModuleContext, oldDump apiDump, newDump apiDump) android.WritablePath {
	newVersion := newDump.dir.Base()
	timestampFile := android.PathForModuleOut(ctx, "checkapi_"+newVersion+".timestamp")

	// Use different messages depending on whether platform SDK is finalized or not.
	// In case when it is finalized, we should never allow updating the already frozen API.
	// If it's not finalized, we let users to update the current version by invoking
	// `m <name>-update-api`.
	messageFile := android.PathForSource(ctx, "system/tools/aidl/build/message_check_equality.txt")
	sdkIsFinal := ctx.Config().DefaultAppTargetSdkInt() != android.FutureApiLevel
	if sdkIsFinal {
		messageFile = android.PathForSource(ctx, "system/tools/aidl/build/message_check_equality_release.txt")
	}
	formattedMessageFile := android.PathForModuleOut(ctx, "message_check_equality.txt")
	rb := android.NewRuleBuilder()
	rb.Command().Text("sed").Flag(" s/%s/" + m.properties.BaseName + "/ ").Input(messageFile).Text(" > ").Output(formattedMessageFile)
	rb.Build(pctx, ctx, "format_message_"+m.properties.BaseName, "")

	var implicits android.Paths
	implicits = append(implicits, oldDump.files...)
	implicits = append(implicits, newDump.files...)
	implicits = append(implicits, formattedMessageFile)
	ctx.ModuleBuild(pctx, android.ModuleBuildParams{
		Rule:      aidlDiffApiRule,
		Implicits: implicits,
		Output:    timestampFile,
		Args: map[string]string{
			"old":         oldDump.dir.String(),
			"new":         newDump.dir.String(),
			"hashFile":    newDump.hashFile.Path().Base(),
			"messageFile": formattedMessageFile.String(),
		},
	})
	return timestampFile
}

func (m *aidlApi) checkIntegrity(ctx android.ModuleContext, dump apiDump) android.WritablePath {
	version := dump.dir.Base()
	timestampFile := android.PathForModuleOut(ctx, "checkhash_"+version+".timestamp")
	messageFile := android.PathForSource(ctx, "system/tools/aidl/build/message_check_integrity.txt")

	i, _ := strconv.Atoi(version)
	if i == 1 {
		version = "latest-version"
	} else {
		version = strconv.Itoa(i - 1)
	}

	var implicits android.Paths
	implicits = append(implicits, dump.files...)
	implicits = append(implicits, dump.hashFile.Path())
	implicits = append(implicits, messageFile)
	ctx.ModuleBuild(pctx, android.ModuleBuildParams{
		Rule:      aidlVerifyHashRule,
		Implicits: implicits,
		Output:    timestampFile,
		Args: map[string]string{
			"apiDir":      dump.dir.String(),
			"version":     version,
			"hashFile":    dump.hashFile.Path().String(),
			"messageFile": messageFile.String(),
		},
	})
	return timestampFile
}

func (m *aidlApi) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	// An API dump is created from source and it is compared against the API dump of the
	// 'current' (yet-to-be-finalized) version. By checking this we enforce that any change in
	// the AIDL interface is gated by the AIDL API review even before the interface is frozen as
	// a new version.
	totApiDump := m.createApiDumpFromSource(ctx)
	currentApiDir := android.ExistentPathForSource(ctx, ctx.ModuleDir(), m.apiDir(), currentVersion)
	var currentApiDump apiDump
	if currentApiDir.Valid() {
		currentApiDump = apiDump{
			dir:      currentApiDir.Path(),
			files:    ctx.Glob(filepath.Join(currentApiDir.Path().String(), "**/*.aidl"), nil),
			hashFile: android.ExistentPathForSource(ctx, ctx.ModuleDir(), m.apiDir(), currentVersion, ".hash"),
		}
		checked := m.checkEquality(ctx, currentApiDump, totApiDump)
		m.checkApiTimestamps = append(m.checkApiTimestamps, checked)
	} else {
		// The "current" directory might not exist, in case when the interface is first created.
		// Instruct user to create one by executing `m <name>-update-api`.
		rb := android.NewRuleBuilder()
		ifaceName := m.properties.BaseName
		rb.Command().Text(fmt.Sprintf(`echo "API dump for the current version of AIDL interface %s does not exist."`, ifaceName))
		rb.Command().Text(fmt.Sprintf(`echo Run "m %s-update-api", or add "unstable: true" to the build rule `+
			`for the interface if it does not need to be versioned`, ifaceName))
		// This file will never be created. Otherwise, the build will pass simply by running 'm; m'.
		alwaysChecked := android.PathForModuleOut(ctx, "checkapi_current.timestamp")
		rb.Command().Text("false").ImplicitOutput(alwaysChecked)
		rb.Build(pctx, ctx, "check_current_aidl_api", "")
		m.checkApiTimestamps = append(m.checkApiTimestamps, alwaysChecked)
	}

	// Also check that version X is backwards compatible with version X-1.
	// "current" is checked against the latest version.
	var dumps []apiDump
	for _, ver := range m.properties.Versions {
		apiDir := filepath.Join(ctx.ModuleDir(), m.apiDir(), ver)
		apiDirPath := android.ExistentPathForSource(ctx, apiDir)
		if apiDirPath.Valid() {
			dumps = append(dumps, apiDump{
				dir:      apiDirPath.Path(),
				files:    ctx.Glob(filepath.Join(apiDirPath.String(), "**/*.aidl"), nil),
				hashFile: android.ExistentPathForSource(ctx, ctx.ModuleDir(), m.apiDir(), ver, ".hash"),
			})
		} else if ctx.Config().AllowMissingDependencies() {
			ctx.AddMissingDependencies([]string{apiDir})
		} else {
			ctx.ModuleErrorf("API version %s path %s does not exist", ver, apiDir)
		}
	}
	if currentApiDir.Valid() {
		dumps = append(dumps, currentApiDump)
	}
	for i, _ := range dumps {
		if dumps[i].hashFile.Valid() {
			checkHashTimestamp := m.checkIntegrity(ctx, dumps[i])
			m.checkHashTimestamps = append(m.checkHashTimestamps, checkHashTimestamp)
		}

		if i == 0 {
			continue
		}
		checked := m.checkCompatibility(ctx, dumps[i-1], dumps[i])
		m.checkApiTimestamps = append(m.checkApiTimestamps, checked)
	}

	// API dump from source is updated to the 'current' version. Triggered by `m <name>-update-api`
	m.updateApiTimestamp = m.makeApiDumpAsVersion(ctx, totApiDump, currentVersion)

	// API dump from source is frozen as the next stable version. Triggered by `m <name>-freeze-api`
	nextVersion := m.nextVersion(ctx)
	m.freezeApiTimestamp = m.makeApiDumpAsVersion(ctx, totApiDump, nextVersion)
}

func (m *aidlApi) AndroidMk() android.AndroidMkData {
	return android.AndroidMkData{
		Custom: func(w io.Writer, name, prefix, moduleDir string, data android.AndroidMkData) {
			android.WriteAndroidMkData(w, data)
			targetName := m.properties.BaseName + "-freeze-api"
			fmt.Fprintln(w, ".PHONY:", targetName)
			fmt.Fprintln(w, targetName+":", m.freezeApiTimestamp.String())

			targetName = m.properties.BaseName + "-update-api"
			fmt.Fprintln(w, ".PHONY:", targetName)
			fmt.Fprintln(w, targetName+":", m.updateApiTimestamp.String())
		},
	}
}

func (m *aidlApi) DepsMutator(ctx android.BottomUpMutatorContext) {
	ctx.AddDependency(ctx.Module(), nil, wrap("", m.properties.Imports, aidlInterfaceSuffix)...)
}

func aidlApiFactory() android.Module {
	m := &aidlApi{}
	m.AddProperties(&m.properties)
	android.InitAndroidModule(m)
	return m
}

type CommonBackendProperties struct {
	// Whether to generate code in the corresponding backend.
	// Default: true
	Enabled        *bool
	Apex_available []string

	// The minimum version of the sdk that the compiled artifacts will run against
	// For native modules, the property needs to be set when a module is a part of mainline modules(APEX).
	// Forwarded to generated java/native module.
	Min_sdk_version *string
}

type CommonNativeBackendProperties struct {
	CommonBackendProperties
	// Whether to generate additional code for gathering information
	// about the transactions.
	// Default: false
	Gen_log *bool

	// VNDK properties for correspdoning backend.
	cc.VndkProperties
}

type aidlInterfaceProperties struct {
	// Vndk properties for C++/NDK libraries only (preferred to use backend-specific settings)
	cc.VndkProperties

	// Whether the library can be installed on the vendor image.
	Vendor_available *bool

	// Whether the library can be used on host
	Host_supported *bool

	// Whether tracing should be added to the interface.
	Gen_trace *bool

	// Top level directories for includes.
	// TODO(b/128940869): remove it if aidl_interface can depend on framework.aidl
	Include_dirs []string
	// Relative path for includes. By default assumes AIDL path is relative to current directory.
	Local_include_dir string

	// List of .aidl files which compose this interface.
	Srcs []string `android:"path"`

	// List of aidl_interface modules that this uses. If one of your AIDL interfaces uses an
	// interface or parcelable from another aidl_interface, you should put its name here.
	Imports []string

	// Used by gen dependency to fill out aidl include path
	Full_import_paths []string `blueprint:"mutated"`

	// Stability promise. Currently only supports "vintf".
	// If this is unset, this corresponds to an interface with stability within
	// this compilation context (so an interface loaded here can only be used
	// with things compiled together, e.g. on the system.img).
	// If this is set to "vintf", this corresponds to a stability promise: the
	// interface must be kept stable as long as it is used.
	Stability *string

	// Previous API versions that are now frozen. The version that is last in
	// the list is considered as the most recent version.
	Versions []string

	Backend struct {
		// Backend of the compiler generating code for Java clients.
		Java struct {
			CommonBackendProperties
			// Set to the version of the sdk to compile against
			// Default: system_current
			Sdk_version *string
			// Whether to compile against platform APIs instead of
			// an SDK.
			Platform_apis *bool
		}
		// Backend of the compiler generating code for C++ clients using
		// libbinder (unstable C++ interface)
		Cpp struct {
			CommonNativeBackendProperties
		}
		// Backend of the compiler generating code for C++ clients using
		// libbinder_ndk (stable C interface to system's libbinder)
		Ndk struct {
			CommonNativeBackendProperties
		}
	}

	// Marks that this interface does not need to be stable. When set to true, the build system
	// doesn't create the API dump and require it to be updated. Default is false.
	Unstable *bool
}

type aidlInterface struct {
	android.ModuleBase

	properties aidlInterfaceProperties

	computedTypes []string
}

func (i *aidlInterface) shouldGenerateJavaBackend() bool {
	// explicitly true if not specified to give early warning to devs
	return i.properties.Backend.Java.Enabled == nil || *i.properties.Backend.Java.Enabled
}

func (i *aidlInterface) shouldGenerateCppBackend() bool {
	// explicitly true if not specified to give early warning to devs
	return i.properties.Backend.Cpp.Enabled == nil || *i.properties.Backend.Cpp.Enabled
}

func (i *aidlInterface) shouldGenerateNdkBackend() bool {
	// explicitly true if not specified to give early warning to devs
	return i.properties.Backend.Ndk.Enabled == nil || *i.properties.Backend.Ndk.Enabled
}

func (i *aidlInterface) gatherInterface(mctx android.LoadHookContext) {
	aidlInterfaces := aidlInterfaces(mctx.Config())
	aidlInterfaceMutex.Lock()
	defer aidlInterfaceMutex.Unlock()
	*aidlInterfaces = append(*aidlInterfaces, i)
}

func addUnstableModule(mctx android.LoadHookContext, moduleName string) {
	unstableModules := unstableModules(mctx.Config())
	unstableModuleMutex.Lock()
	defer unstableModuleMutex.Unlock()
	*unstableModules = append(*unstableModules, moduleName)
}

func (i *aidlInterface) checkImports(mctx android.BaseModuleContext) {
	for _, anImport := range i.properties.Imports {
		other := lookupInterface(anImport, mctx.Config())

		if other == nil {
			mctx.PropertyErrorf("imports", "Import does not exist: "+anImport)
		}

		if i.shouldGenerateJavaBackend() && !other.shouldGenerateJavaBackend() {
			mctx.PropertyErrorf("backend.java.enabled",
				"Java backend not enabled in the imported AIDL interface %q", anImport)
		}

		if i.shouldGenerateCppBackend() && !other.shouldGenerateCppBackend() {
			mctx.PropertyErrorf("backend.cpp.enabled",
				"C++ backend not enabled in the imported AIDL interface %q", anImport)
		}

		if i.shouldGenerateNdkBackend() && !other.shouldGenerateNdkBackend() {
			mctx.PropertyErrorf("backend.ndk.enabled",
				"NDK backend not enabled in the imported AIDL interface %q", anImport)
		}
	}
}

func (i *aidlInterface) checkStability(mctx android.LoadHookContext) {
	if i.properties.Stability == nil {
		return
	}

	if proptools.Bool(i.properties.Unstable) {
		mctx.PropertyErrorf("stability", "must be empty when \"unstable\" is true")
	}

	// TODO(b/136027762): should we allow more types of stability (e.g. for APEX) or
	// should we switch this flag to be something like "vintf { enabled: true }"
	isVintf := "vintf" == proptools.String(i.properties.Stability)
	if !isVintf {
		mctx.PropertyErrorf("stability", "must be empty or \"vintf\"")
	}
}
func (i *aidlInterface) checkVersions(mctx android.LoadHookContext) {
	for _, ver := range i.properties.Versions {
		_, err := strconv.Atoi(ver)
		if err != nil {
			mctx.PropertyErrorf("versions", "%q is not an integer", ver)
			continue
		}
	}
}

func (i *aidlInterface) currentVersion(ctx android.LoadHookContext) string {
	if !i.hasVersion() {
		return ""
	} else {
		ver := i.latestVersion()
		i, err := strconv.Atoi(ver)
		if err != nil {
			panic(err)
		}

		return strconv.Itoa(i + 1)
	}
}

func (i *aidlInterface) latestVersion() string {
	if !i.hasVersion() {
		return "0"
	}
	return i.properties.Versions[len(i.properties.Versions)-1]
}
func (i *aidlInterface) isLatestVersion(version string) bool {
	if !i.hasVersion() {
		return true
	}
	return version == i.latestVersion()
}
func (i *aidlInterface) hasVersion() bool {
	return len(i.properties.Versions) > 0
}

func (i *aidlInterface) isCurrentVersion(ctx android.LoadHookContext, version string) bool {
	return version == i.currentVersion(ctx)
}

// This function returns module name with version. Assume that there is foo of which latest version is 2
// Version -> Module name
// "1"->foo-V1
// "2"->foo-V2
// "3"(unfrozen)->foo-unstable
// ""-> foo
func (i *aidlInterface) versionedName(ctx android.LoadHookContext, version string) string {
	name := i.ModuleBase.Name()
	if version == "" {
		return name
	}
	if i.isCurrentVersion(ctx, version) {
		return name + "-unstable"
	}
	return name + "-V" + version
}

// This function returns C++ artifact's name. Mostly, it returns same as versionedName(),
// But, it returns different value only if it is the case below.
// Assume that there is foo of which latest version is 2
// foo-unstable -> foo-V3
// foo -> foo-V2 (latest frozen version)
// Assume that there is bar of which version hasn't been defined yet.
// bar -> bar-V1
func (i *aidlInterface) cppOutputName(version string) string {
	name := i.ModuleBase.Name()
	// Even if the module doesn't have version, it returns with version(-V1)
	if !i.hasVersion() {
		// latestVersion() always returns "0"
		i, err := strconv.Atoi(i.latestVersion())
		if err != nil {
			panic(err)
		}
		return name + "-V" + strconv.Itoa(i+1)
	}
	if version == "" {
		version = i.latestVersion()
	}
	return name + "-V" + version
}

func (i *aidlInterface) srcsForVersion(mctx android.LoadHookContext, version string) (srcs []string, aidlRoot string) {
	if i.isCurrentVersion(mctx, version) {
		return i.properties.Srcs, i.properties.Local_include_dir
	} else {
		aidlRoot = filepath.Join(aidlApiDir, i.ModuleBase.Name(), version)
		full_paths, err := mctx.GlobWithDeps(filepath.Join(mctx.ModuleDir(), aidlRoot, "**/*.aidl"), nil)
		if err != nil {
			panic(err)
		}
		for _, path := range full_paths {
			// Here, we need path local to the module
			srcs = append(srcs, strings.TrimPrefix(path, mctx.ModuleDir()+"/"))
		}
		return srcs, aidlRoot
	}
}

func aidlInterfaceHook(mctx android.LoadHookContext, i *aidlInterface) {
	if !isRelativePath(i.properties.Local_include_dir) {
		mctx.PropertyErrorf("local_include_dir", "must be relative path: "+i.properties.Local_include_dir)
	}
	var importPaths []string
	importPaths = append(importPaths, filepath.Join(mctx.ModuleDir(), i.properties.Local_include_dir))
	importPaths = append(importPaths, i.properties.Include_dirs...)

	i.properties.Full_import_paths = importPaths

	i.gatherInterface(mctx)
	i.checkStability(mctx)
	i.checkVersions(mctx)

	if mctx.Failed() {
		return
	}

	var libs []string

	currentVersion := i.currentVersion(mctx)

	versionsForCpp := make([]string, len(i.properties.Versions))

	sdkIsFinal := mctx.Config().DefaultAppTargetSdkInt() != android.FutureApiLevel

	needToCheckUnstableVersion := sdkIsFinal && i.hasVersion() && i.Owner() == ""
	copy(versionsForCpp, i.properties.Versions)
	if i.hasVersion() {
		// In C++ library, AIDL doesn't create the module of which name is with latest version,
		// instead of it, there is a module without version.
		versionsForCpp[len(i.properties.Versions)-1] = ""
	}
	if i.shouldGenerateCppBackend() {
		unstableLib := addCppLibrary(mctx, i, currentVersion, langCpp)
		if needToCheckUnstableVersion {
			addUnstableModule(mctx, unstableLib)
		}
		libs = append(libs, unstableLib)
		for _, version := range versionsForCpp {
			addCppLibrary(mctx, i, version, langCpp)
		}
	}

	if i.shouldGenerateNdkBackend() {
		if !proptools.Bool(i.properties.Vendor_available) {
			unstableLib := addCppLibrary(mctx, i, currentVersion, langNdk)
			if needToCheckUnstableVersion {
				addUnstableModule(mctx, unstableLib)
			}
			libs = append(libs, unstableLib)
			for _, version := range versionsForCpp {
				addCppLibrary(mctx, i, version, langNdk)
			}
		}
		// TODO(b/121157555): combine with '-ndk' variant
		unstableLib := addCppLibrary(mctx, i, currentVersion, langNdkPlatform)
		if needToCheckUnstableVersion {
			addUnstableModule(mctx, unstableLib)
		}
		libs = append(libs, unstableLib)
		for _, version := range versionsForCpp {
			addCppLibrary(mctx, i, version, langNdkPlatform)
		}
	}
	versionsForJava := i.properties.Versions
	if i.hasVersion() {
		versionsForJava = append(i.properties.Versions, "")
	}
	if i.shouldGenerateJavaBackend() {
		unstableLib := addJavaLibrary(mctx, i, currentVersion)
		if needToCheckUnstableVersion {
			addUnstableModule(mctx, unstableLib)
		}
		libs = append(libs, unstableLib)
		for _, version := range versionsForJava {
			addJavaLibrary(mctx, i, version)
		}
	}

	if proptools.Bool(i.properties.Unstable) {
		if i.hasVersion() {
			mctx.PropertyErrorf("versions", "cannot have versions for an unstable interface")
		}
		apiDirRoot := filepath.Join(aidlApiDir, i.ModuleBase.Name())
		aidlDumps, _ := mctx.GlobWithDeps(filepath.Join(mctx.ModuleDir(), apiDirRoot, "**/*.aidl"), nil)
		if len(aidlDumps) != 0 {
			mctx.PropertyErrorf("unstable", "The interface is configured as unstable, "+
				"but API dumps exist under %q. Unstable interface cannot have dumps.", apiDirRoot)
		}
		if i.properties.Stability != nil {
			mctx.ModuleErrorf("unstable:true and stability:%q cannot happen at the same time", i.properties.Stability)
		}
	} else {
		sdkIsFinal := mctx.Config().DefaultAppTargetSdkInt() != android.FutureApiLevel
		if sdkIsFinal && !i.hasVersion() && i.Owner() == "" {
			mctx.PropertyErrorf("versions", "must be set (need to be frozen) when \"unstable\" is false and PLATFORM_VERSION_CODENAME is REL.")
		}
		addApiModule(mctx, i)
	}

	// Reserve this module name for future use
	mctx.CreateModule(phony.PhonyFactory, &phonyProperties{
		Name:     proptools.StringPtr(i.ModuleBase.Name()),
		Required: libs,
	})
}

func addCppLibrary(mctx android.LoadHookContext, i *aidlInterface, version string, lang string) string {
	cppSourceGen := i.versionedName(mctx, version) + "-" + lang + "-source"
	cppModuleGen := i.versionedName(mctx, version) + "-" + lang
	cppOutputGen := i.cppOutputName(version) + "-" + lang
	if i.hasVersion() && version == "" {
		version = i.latestVersion()
	}
	srcs, aidlRoot := i.srcsForVersion(mctx, version)
	if len(srcs) == 0 {
		// This can happen when the version is about to be frozen; the version
		// directory is created but API dump hasn't been copied there.
		// Don't create a library for the yet-to-be-frozen version.
		return ""
	}

	// For an interface with no versions, this is the ToT interface.
	// For an interface w/ versions, this is that latest version.
	isLatest := !i.hasVersion() || version == i.latestVersion()

	var overrideVndkProperties cc.VndkProperties
	if !isLatest {
		// We only want the VNDK to include the latest interface. For interfaces in
		// development, they will be frozen, so we put their latest version in the
		// VNDK. For interfaces which are already frozen, we put their latest version
		// in the VNDK, and when that version is frozen, the version in the VNDK can
		// be updated. Otherwise, we remove this library from the VNDK, to avoid adding
		// multiple versions of the same library to the VNDK.
		overrideVndkProperties.Vndk.Enabled = proptools.BoolPtr(false)
		overrideVndkProperties.Vndk.Support_system_process = proptools.BoolPtr(false)
	}

	var commonProperties *CommonNativeBackendProperties
	if lang == langCpp {
		commonProperties = &i.properties.Backend.Cpp.CommonNativeBackendProperties
	} else if lang == langNdk || lang == langNdkPlatform {
		commonProperties = &i.properties.Backend.Ndk.CommonNativeBackendProperties
	}

	genLog := proptools.Bool(commonProperties.Gen_log)
	genTrace := proptools.Bool(i.properties.Gen_trace)

	mctx.CreateModule(aidlGenFactory, &nameProperties{
		Name: proptools.StringPtr(cppSourceGen),
	}, &aidlGenProperties{
		Srcs:      srcs,
		AidlRoot:  aidlRoot,
		Imports:   concat(i.properties.Imports, []string{i.ModuleBase.Name()}),
		Stability: i.properties.Stability,
		Lang:      lang,
		BaseName:  i.ModuleBase.Name(),
		GenLog:    genLog,
		Version:   version,
		GenTrace:  genTrace,
		Unstable:  i.properties.Unstable,
	})

	importExportDependencies := wrap("", i.properties.Imports, "-"+lang)
	var libJSONCppDependency []string
	var staticLibDependency []string
	var sdkVersion *string
	var minSdkVersion *string
	var stl *string
	var cpp_std *string
	var hostSupported *bool
	var addCflags []string

	if lang == langCpp {
		importExportDependencies = append(importExportDependencies, "libbinder", "libutils")
		if genLog {
			libJSONCppDependency = []string{"libjsoncpp"}
		}
		if genTrace {
			importExportDependencies = append(importExportDependencies, "libcutils")
		}
		hostSupported = i.properties.Host_supported
		minSdkVersion = i.properties.Backend.Cpp.Min_sdk_version
	} else if lang == langNdk {
		importExportDependencies = append(importExportDependencies, "libbinder_ndk")
		if genLog {
			staticLibDependency = []string{"libjsoncpp_ndk"}
		}
		sdkVersion = proptools.StringPtr("current")
		stl = proptools.StringPtr("c++_shared")
		minSdkVersion = i.properties.Backend.Ndk.Min_sdk_version
	} else if lang == langNdkPlatform {
		importExportDependencies = append(importExportDependencies, "libbinder_ndk")
		if genLog {
			libJSONCppDependency = []string{"libjsoncpp"}
		}
		hostSupported = i.properties.Host_supported
		addCflags = append(addCflags, "-DBINDER_STABILITY_SUPPORT")
		minSdkVersion = i.properties.Backend.Ndk.Min_sdk_version
	} else {
		panic("Unrecognized language: " + lang)
	}

	vendorAvailable := i.properties.Vendor_available
	if lang == langCpp && "vintf" == proptools.String(i.properties.Stability) {
		// Vendors cannot use the libbinder (cpp) backend of AIDL in a way that is stable.
		// So, in order to prevent accidental usage of these library by vendor, forcibly
		// disabling this version of the library.
		//
		// It may be the case in the future that we will want to enable this (if some generic
		// helper should be used by both libbinder vendor things using /dev/vndbinder as well
		// as those things using /dev/binder + libbinder_ndk to talk to stable interfaces).
		vendorAvailable = proptools.BoolPtr(false)
	}

	mctx.CreateModule(cc.LibraryFactory, &ccProperties{
		Name:                      proptools.StringPtr(cppModuleGen),
		Vendor_available:          vendorAvailable,
		Host_supported:            hostSupported,
		Defaults:                  []string{"aidl-cpp-module-defaults"},
		Generated_sources:         []string{cppSourceGen},
		Generated_headers:         []string{cppSourceGen},
		Export_generated_headers:  []string{cppSourceGen},
		Static:                    staticLib{Whole_static_libs: libJSONCppDependency},
		Shared:                    sharedLib{Shared_libs: libJSONCppDependency, Export_shared_lib_headers: libJSONCppDependency},
		Static_libs:               staticLibDependency,
		Shared_libs:               importExportDependencies,
		Export_shared_lib_headers: importExportDependencies,
		Sdk_version:               sdkVersion,
		Stl:                       stl,
		Cpp_std:                   cpp_std,
		Cflags:                    append(addCflags, "-Wextra", "-Wall", "-Werror"),
		Stem:                      proptools.StringPtr(cppOutputGen),
		Apex_available:            commonProperties.Apex_available,
		Min_sdk_version:           minSdkVersion,
	}, &i.properties.VndkProperties, &commonProperties.VndkProperties, &overrideVndkProperties)

	return cppModuleGen
}

func addJavaLibrary(mctx android.LoadHookContext, i *aidlInterface, version string) string {
	javaSourceGen := i.versionedName(mctx, version) + "-java-source"
	javaModuleGen := i.versionedName(mctx, version) + "-java"
	if i.hasVersion() && version == "" {
		version = i.latestVersion()
	}
	srcs, aidlRoot := i.srcsForVersion(mctx, version)
	if len(srcs) == 0 {
		// This can happen when the version is about to be frozen; the version
		// directory is created but API dump hasn't been copied there.
		// Don't create a library for the yet-to-be-frozen version.
		return ""
	}

	sdkVersion := i.properties.Backend.Java.Sdk_version
	if !proptools.Bool(i.properties.Backend.Java.Platform_apis) && sdkVersion == nil {
		// platform apis requires no default
		sdkVersion = proptools.StringPtr("system_current")
	}

	mctx.CreateModule(aidlGenFactory, &nameProperties{
		Name: proptools.StringPtr(javaSourceGen),
	}, &aidlGenProperties{
		Srcs:      srcs,
		AidlRoot:  aidlRoot,
		Imports:   concat(i.properties.Imports, []string{i.ModuleBase.Name()}),
		Stability: i.properties.Stability,
		Lang:      langJava,
		BaseName:  i.ModuleBase.Name(),
		Version:   version,
		Unstable:  i.properties.Unstable,
	})

	mctx.CreateModule(java.LibraryFactory, &javaProperties{
		Name:            proptools.StringPtr(javaModuleGen),
		Installable:     proptools.BoolPtr(true),
		Defaults:        []string{"aidl-java-module-defaults"},
		Sdk_version:     sdkVersion,
		Platform_apis:   i.properties.Backend.Java.Platform_apis,
		Static_libs:     wrap("", i.properties.Imports, "-java"),
		Srcs:            []string{":" + javaSourceGen},
		Apex_available:  i.properties.Backend.Java.Apex_available,
		Min_sdk_version: i.properties.Backend.Java.Min_sdk_version,
	})

	return javaModuleGen
}

func addApiModule(mctx android.LoadHookContext, i *aidlInterface) string {
	apiModule := i.ModuleBase.Name() + aidlApiSuffix
	srcs, aidlRoot := i.srcsForVersion(mctx, i.currentVersion(mctx))
	mctx.CreateModule(aidlApiFactory, &nameProperties{
		Name: proptools.StringPtr(apiModule),
	}, &aidlApiProperties{
		BaseName:  i.ModuleBase.Name(),
		Srcs:      srcs,
		AidlRoot:  aidlRoot,
		Stability: i.properties.Stability,
		Imports:   concat(i.properties.Imports, []string{i.ModuleBase.Name()}),
		Versions:  i.properties.Versions,
	})
	return apiModule
}

func (i *aidlInterface) Name() string {
	return i.ModuleBase.Name() + aidlInterfaceSuffix
}
func (i *aidlInterface) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	aidlRoot := android.PathForModuleSrc(ctx, i.properties.Local_include_dir)
	for _, src := range android.PathsForModuleSrc(ctx, i.properties.Srcs) {
		baseDir := getBaseDir(ctx, src, aidlRoot)
		relPath, _ := filepath.Rel(baseDir, src.String())
		computedType := strings.TrimSuffix(strings.ReplaceAll(relPath, "/", "."), ".aidl")
		i.computedTypes = append(i.computedTypes, computedType)
	}
}
func (i *aidlInterface) DepsMutator(ctx android.BottomUpMutatorContext) {
	i.checkImports(ctx)

	ctx.AddReverseDependency(ctx.Module(), nil, aidlMetadataSingletonName)
}

var (
	aidlInterfacesKey   = android.NewOnceKey("aidlInterfaces")
	unstableModulesKey  = android.NewOnceKey("unstableModules")
	aidlInterfaceMutex  sync.Mutex
	unstableModuleMutex sync.Mutex
)

func aidlInterfaces(config android.Config) *[]*aidlInterface {
	return config.Once(aidlInterfacesKey, func() interface{} {
		return &[]*aidlInterface{}
	}).(*[]*aidlInterface)
}

func unstableModules(config android.Config) *[]string {
	return config.Once(unstableModulesKey, func() interface{} {
		return &[]string{}
	}).(*[]string)
}

func aidlInterfaceFactory() android.Module {
	i := &aidlInterface{}
	i.AddProperties(&i.properties)
	android.InitAndroidModule(i)
	android.AddLoadHook(i, func(ctx android.LoadHookContext) { aidlInterfaceHook(ctx, i) })
	return i
}

func lookupInterface(name string, config android.Config) *aidlInterface {
	for _, i := range *aidlInterfaces(config) {
		if i.ModuleBase.Name() == name {
			return i
		}
	}
	return nil
}

func aidlInterfacesMetadataSingletonFactory() android.Module {
	i := &aidlInterfacesMetadataSingleton{}
	android.InitAndroidModule(i)
	return i
}

type aidlInterfacesMetadataSingleton struct {
	android.ModuleBase

	metadataPath android.OutputPath
}

var _ android.OutputFileProducer = (*aidlInterfacesMetadataSingleton)(nil)

func (m *aidlInterfacesMetadataSingleton) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	if m.Name() != aidlMetadataSingletonName {
		ctx.PropertyErrorf("name", "must be %s", aidlMetadataSingletonName)
		return
	}

	type ModuleInfo struct {
		Stability     string
		ComputedTypes []string
		HashFiles     []string
	}

	// name -> ModuleInfo
	moduleInfos := map[string]ModuleInfo{}
	ctx.VisitDirectDeps(func(m android.Module) {
		if !m.ExportedToMake() {
			return
		}

		switch t := m.(type) {
		case *aidlInterface:
			info := moduleInfos[t.ModuleBase.Name()]
			info.Stability = proptools.StringDefault(t.properties.Stability, "")
			info.ComputedTypes = t.computedTypes
			moduleInfos[t.ModuleBase.Name()] = info
		case *aidlGenRule:
			info := moduleInfos[t.properties.BaseName]
			if t.hashFile != nil {
				info.HashFiles = append(info.HashFiles, t.hashFile.String())
			}
			moduleInfos[t.properties.BaseName] = info
		default:
			panic(fmt.Sprintf("Unrecognized module type: %v", t))
		}

	})

	var metadataOutputs android.Paths
	for name, info := range moduleInfos {
		metadataPath := android.PathForModuleOut(ctx, "metadata_"+name)
		metadataOutputs = append(metadataOutputs, metadataPath)

		// There is one aidlGenRule per-version per-backend. If we had
		// objects per version and sub-objects per backend, we could
		// avoid needing to filter out duplicates.
		info.HashFiles = android.FirstUniqueStrings(info.HashFiles)

		implicits := android.PathsForSource(ctx, info.HashFiles)

		ctx.Build(pctx, android.BuildParams{
			Rule:      aidlMetadataRule,
			Implicits: implicits,
			Output:    metadataPath,
			Args: map[string]string{
				"name":      name,
				"stability": info.Stability,
				"types":     strings.Join(wrap(`\"`, info.ComputedTypes, `\"`), ", "),
				"hashes": strings.Join(
					wrap(`\"$$(read -r < `,
						info.HashFiles,
						` hash extra; printf '%s' $$hash)\"`), ", "),
			},
		})
	}

	m.metadataPath = android.PathForModuleOut(ctx, "aidl_metadata.json").OutputPath

	ctx.Build(pctx, android.BuildParams{
		Rule:   joinJsonObjectsToArrayRule,
		Inputs: metadataOutputs,
		Output: m.metadataPath,
		Args: map[string]string{
			"files": strings.Join(metadataOutputs.Strings(), " "),
		},
	})
}

func (m *aidlInterfacesMetadataSingleton) OutputFiles(tag string) (android.Paths, error) {
	if tag != "" {
		return nil, fmt.Errorf("unsupported tag %q", tag)
	}

	return android.Paths{m.metadataPath}, nil
}

type aidlMappingProperties struct {
	// Source file of this prebuilt.
	Srcs   []string `android:"path"`
	Output string
}

type aidlMapping struct {
	android.ModuleBase
	properties     aidlMappingProperties
	outputFilePath android.WritablePath
}

func (s *aidlMapping) DepsMutator(ctx android.BottomUpMutatorContext) {
}

func (s *aidlMapping) GenerateAndroidBuildActions(ctx android.ModuleContext) {
	srcs, imports := getPaths(ctx, s.properties.Srcs)

	s.outputFilePath = android.PathForModuleOut(ctx, s.properties.Output)
	outDir := android.PathForModuleGen(ctx)
	ctx.Build(pctx, android.BuildParams{
		Rule:   aidlDumpMappingsRule,
		Inputs: srcs,
		Output: s.outputFilePath,
		Args: map[string]string{
			"imports": android.JoinWithPrefix(imports, " -I"),
			"outDir":  outDir.String(),
		},
	})
}

func InitAidlMappingModule(s *aidlMapping) {
	s.AddProperties(&s.properties)
}

func aidlMappingFactory() android.Module {
	module := &aidlMapping{}
	InitAidlMappingModule(module)
	android.InitAndroidModule(module)
	return module
}

func (m *aidlMapping) AndroidMk() android.AndroidMkData {
	return android.AndroidMkData{
		Custom: func(w io.Writer, name, prefix, moduleDir string, data android.AndroidMkData) {
			android.WriteAndroidMkData(w, data)
			targetName := m.Name()
			fmt.Fprintln(w, ".PHONY:", targetName)
			fmt.Fprintln(w, targetName+":", m.outputFilePath.String())
		},
	}
}

func allAidlInterfacesMakeVars(ctx android.MakeVarsContext) {
	names := []string{}
	ctx.VisitAllModules(func(module android.Module) {
		if ai, ok := module.(*aidlInterface); ok {
			names = append(names, ai.BaseModuleName())
		}
	})
	ctx.Strict("ALL_AIDL_INTERFACES", strings.Join(names, " "))
}
