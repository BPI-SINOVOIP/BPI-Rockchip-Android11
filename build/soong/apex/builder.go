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

package apex

import (
	"encoding/json"
	"fmt"
	"path/filepath"
	"runtime"
	"sort"
	"strconv"
	"strings"

	"android/soong/android"
	"android/soong/java"

	"github.com/google/blueprint"
	"github.com/google/blueprint/proptools"
)

var (
	pctx = android.NewPackageContext("android/apex")
)

func init() {
	pctx.Import("android/soong/android")
	pctx.Import("android/soong/java")
	pctx.HostBinToolVariable("apexer", "apexer")
	// ART minimal builds (using the master-art manifest) do not have the "frameworks/base"
	// projects, and hence cannot built 'aapt2'. Use the SDK prebuilt instead.
	hostBinToolVariableWithPrebuilt := func(name, prebuiltDir, tool string) {
		pctx.VariableFunc(name, func(ctx android.PackageVarContext) string {
			if !ctx.Config().FrameworksBaseDirExists(ctx) {
				return filepath.Join(prebuiltDir, runtime.GOOS, "bin", tool)
			} else {
				return ctx.Config().HostToolPath(ctx, tool).String()
			}
		})
	}
	hostBinToolVariableWithPrebuilt("aapt2", "prebuilts/sdk/tools", "aapt2")
	pctx.HostBinToolVariable("avbtool", "avbtool")
	pctx.HostBinToolVariable("e2fsdroid", "e2fsdroid")
	pctx.HostBinToolVariable("merge_zips", "merge_zips")
	pctx.HostBinToolVariable("mke2fs", "mke2fs")
	pctx.HostBinToolVariable("resize2fs", "resize2fs")
	pctx.HostBinToolVariable("sefcontext_compile", "sefcontext_compile")
	pctx.HostBinToolVariable("soong_zip", "soong_zip")
	pctx.HostBinToolVariable("zip2zip", "zip2zip")
	pctx.HostBinToolVariable("zipalign", "zipalign")
	pctx.HostBinToolVariable("jsonmodify", "jsonmodify")
	pctx.HostBinToolVariable("conv_apex_manifest", "conv_apex_manifest")
	pctx.HostBinToolVariable("extract_apks", "extract_apks")
}

var (
	// Create a canned fs config file where all files and directories are
	// by default set to (uid/gid/mode) = (1000/1000/0644)
	// TODO(b/113082813) make this configurable using config.fs syntax
	generateFsConfig = pctx.StaticRule("generateFsConfig", blueprint.RuleParams{
		Command: `( echo '/ 1000 1000 0755' ` +
			`&& for i in ${ro_paths}; do echo "/$$i 1000 1000 0644"; done ` +
			`&& for i in  ${exec_paths}; do echo "/$$i 0 2000 0755"; done ` +
			`&& ( tr ' ' '\n' <${out}.apklist | for i in ${apk_paths}; do read apk; echo "/$$i 0 2000 0755"; zipinfo -1 $$apk | sed "s:\(.*\):/$$i/\1 1000 1000 0644:"; done ) ) > ${out}`,
		Description:    "fs_config ${out}",
		Rspfile:        "$out.apklist",
		RspfileContent: "$in",
	}, "ro_paths", "exec_paths", "apk_paths")

	apexManifestRule = pctx.StaticRule("apexManifestRule", blueprint.RuleParams{
		Command: `rm -f $out && ${jsonmodify} $in ` +
			`-a provideNativeLibs ${provideNativeLibs} ` +
			`-a requireNativeLibs ${requireNativeLibs} ` +
			`${opt} ` +
			`-o $out`,
		CommandDeps: []string{"${jsonmodify}"},
		Description: "prepare ${out}",
	}, "provideNativeLibs", "requireNativeLibs", "opt")

	stripApexManifestRule = pctx.StaticRule("stripApexManifestRule", blueprint.RuleParams{
		Command:     `rm -f $out && ${conv_apex_manifest} strip $in -o $out`,
		CommandDeps: []string{"${conv_apex_manifest}"},
		Description: "strip ${in}=>${out}",
	})

	pbApexManifestRule = pctx.StaticRule("pbApexManifestRule", blueprint.RuleParams{
		Command:     `rm -f $out && ${conv_apex_manifest} proto $in -o $out`,
		CommandDeps: []string{"${conv_apex_manifest}"},
		Description: "convert ${in}=>${out}",
	})

	// TODO(b/113233103): make sure that file_contexts is sane, i.e., validate
	// against the binary policy using sefcontext_compiler -p <policy>.

	// TODO(b/114327326): automate the generation of file_contexts
	apexRule = pctx.StaticRule("apexRule", blueprint.RuleParams{
		Command: `rm -rf ${image_dir} && mkdir -p ${image_dir} && ` +
			`(. ${out}.copy_commands) && ` +
			`APEXER_TOOL_PATH=${tool_path} ` +
			`${apexer} --force --manifest ${manifest} ` +
			`--file_contexts ${file_contexts} ` +
			`--canned_fs_config ${canned_fs_config} ` +
			`--include_build_info ` +
			`--payload_type image ` +
			`--key ${key} ${opt_flags} ${image_dir} ${out} `,
		CommandDeps: []string{"${apexer}", "${avbtool}", "${e2fsdroid}", "${merge_zips}",
			"${mke2fs}", "${resize2fs}", "${sefcontext_compile}",
			"${soong_zip}", "${zipalign}", "${aapt2}", "prebuilts/sdk/current/public/android.jar"},
		Rspfile:        "${out}.copy_commands",
		RspfileContent: "${copy_commands}",
		Description:    "APEX ${image_dir} => ${out}",
	}, "tool_path", "image_dir", "copy_commands", "file_contexts", "canned_fs_config", "key", "opt_flags", "manifest")

	zipApexRule = pctx.StaticRule("zipApexRule", blueprint.RuleParams{
		Command: `rm -rf ${image_dir} && mkdir -p ${image_dir} && ` +
			`(. ${out}.copy_commands) && ` +
			`APEXER_TOOL_PATH=${tool_path} ` +
			`${apexer} --force --manifest ${manifest} ` +
			`--payload_type zip ` +
			`${image_dir} ${out} `,
		CommandDeps:    []string{"${apexer}", "${merge_zips}", "${soong_zip}", "${zipalign}", "${aapt2}"},
		Rspfile:        "${out}.copy_commands",
		RspfileContent: "${copy_commands}",
		Description:    "ZipAPEX ${image_dir} => ${out}",
	}, "tool_path", "image_dir", "copy_commands", "manifest")

	apexProtoConvertRule = pctx.AndroidStaticRule("apexProtoConvertRule",
		blueprint.RuleParams{
			Command:     `${aapt2} convert --output-format proto $in -o $out`,
			CommandDeps: []string{"${aapt2}"},
		})

	apexBundleRule = pctx.StaticRule("apexBundleRule", blueprint.RuleParams{
		Command: `${zip2zip} -i $in -o $out.base ` +
			`apex_payload.img:apex/${abi}.img ` +
			`apex_build_info.pb:apex/${abi}.build_info.pb ` +
			`apex_manifest.json:root/apex_manifest.json ` +
			`apex_manifest.pb:root/apex_manifest.pb ` +
			`AndroidManifest.xml:manifest/AndroidManifest.xml ` +
			`assets/NOTICE.html.gz:assets/NOTICE.html.gz &&` +
			`${soong_zip} -o $out.config -C $$(dirname ${config}) -f ${config} && ` +
			`${merge_zips} $out $out.base $out.config`,
		CommandDeps: []string{"${zip2zip}", "${soong_zip}", "${merge_zips}"},
		Description: "app bundle",
	}, "abi", "config")

	emitApexContentRule = pctx.StaticRule("emitApexContentRule", blueprint.RuleParams{
		Command:        `rm -f ${out} && touch ${out} && (. ${out}.emit_commands)`,
		Rspfile:        "${out}.emit_commands",
		RspfileContent: "${emit_commands}",
		Description:    "Emit APEX image content",
	}, "emit_commands")

	diffApexContentRule = pctx.StaticRule("diffApexContentRule", blueprint.RuleParams{
		Command: `diff --unchanged-group-format='' \` +
			`--changed-group-format='%<' \` +
			`${image_content_file} ${allowed_files_file} || (` +
			`echo -e "New unexpected files were added to ${apex_module_name}." ` +
			` "To fix the build run following command:" && ` +
			`echo "system/apex/tools/update_allowed_list.sh ${allowed_files_file} ${image_content_file}" && ` +
			`exit 1); touch ${out}`,
		Description: "Diff ${image_content_file} and ${allowed_files_file}",
	}, "image_content_file", "allowed_files_file", "apex_module_name")
)

func (a *apexBundle) buildManifest(ctx android.ModuleContext, provideNativeLibs, requireNativeLibs []string) {
	manifestSrc := android.PathForModuleSrc(ctx, proptools.StringDefault(a.properties.Manifest, "apex_manifest.json"))

	manifestJsonFullOut := android.PathForModuleOut(ctx, "apex_manifest_full.json")

	// put dependency({provide|require}NativeLibs) in apex_manifest.json
	provideNativeLibs = android.SortedUniqueStrings(provideNativeLibs)
	requireNativeLibs = android.SortedUniqueStrings(android.RemoveListFromList(requireNativeLibs, provideNativeLibs))

	// apex name can be overridden
	optCommands := []string{}
	if a.properties.Apex_name != nil {
		optCommands = append(optCommands, "-v name "+*a.properties.Apex_name)
	}

	ctx.Build(pctx, android.BuildParams{
		Rule:   apexManifestRule,
		Input:  manifestSrc,
		Output: manifestJsonFullOut,
		Args: map[string]string{
			"provideNativeLibs": strings.Join(provideNativeLibs, " "),
			"requireNativeLibs": strings.Join(requireNativeLibs, " "),
			"opt":               strings.Join(optCommands, " "),
		},
	})

	if a.minSdkVersion(ctx) == android.SdkVersion_Android10 {
		// b/143654022 Q apexd can't understand newly added keys in apex_manifest.json
		// prepare stripped-down version so that APEX modules built from R+ can be installed to Q
		a.manifestJsonOut = android.PathForModuleOut(ctx, "apex_manifest.json")
		ctx.Build(pctx, android.BuildParams{
			Rule:   stripApexManifestRule,
			Input:  manifestJsonFullOut,
			Output: a.manifestJsonOut,
		})
	}

	// from R+, protobuf binary format (.pb) is the standard format for apex_manifest
	a.manifestPbOut = android.PathForModuleOut(ctx, "apex_manifest.pb")
	ctx.Build(pctx, android.BuildParams{
		Rule:   pbApexManifestRule,
		Input:  manifestJsonFullOut,
		Output: a.manifestPbOut,
	})
}

func (a *apexBundle) buildNoticeFiles(ctx android.ModuleContext, apexFileName string) android.NoticeOutputs {
	var noticeFiles android.Paths

	a.walkPayloadDeps(ctx, func(ctx android.ModuleContext, from blueprint.Module, to android.ApexModule, externalDep bool) bool {
		if externalDep {
			// As soon as the dependency graph crosses the APEX boundary, don't go further.
			return false
		}

		notice := to.NoticeFile()
		if notice.Valid() {
			noticeFiles = append(noticeFiles, notice.Path())
		}

		return true
	})

	if len(noticeFiles) == 0 {
		return android.NoticeOutputs{}
	}

	return android.BuildNoticeOutput(ctx, a.installDir, apexFileName, android.SortedUniquePaths(noticeFiles))
}

func (a *apexBundle) buildInstalledFilesFile(ctx android.ModuleContext, builtApex android.Path, imageDir android.Path) android.OutputPath {
	output := android.PathForModuleOut(ctx, "installed-files.txt")
	rule := android.NewRuleBuilder()
	rule.Command().
		Implicit(builtApex).
		Text("(cd " + imageDir.String() + " ; ").
		Text("find . \\( -type f -o -type l \\) -printf \"%s %p\\n\") ").
		Text(" | sort -nr > ").
		Output(output)
	rule.Build(pctx, ctx, "installed-files."+a.Name(), "Installed files")
	return output.OutputPath
}

func (a *apexBundle) buildBundleConfig(ctx android.ModuleContext) android.OutputPath {
	output := android.PathForModuleOut(ctx, "bundle_config.json")

	type ApkConfig struct {
		Package_name string `json:"package_name"`
		Apk_path     string `json:"path"`
	}
	config := struct {
		Compression struct {
			Uncompressed_glob []string `json:"uncompressed_glob"`
		} `json:"compression"`
		Apex_config struct {
			Apex_embedded_apk_config []ApkConfig `json:"apex_embedded_apk_config,omitempty"`
		} `json:"apex_config,omitempty"`
	}{}

	config.Compression.Uncompressed_glob = []string{
		"apex_payload.img",
		"apex_manifest.*",
	}

	// collect the manifest names and paths of android apps
	// if their manifest names are overridden
	for _, fi := range a.filesInfo {
		if fi.class != app && fi.class != appSet {
			continue
		}
		packageName := fi.overriddenPackageName
		if packageName != "" {
			config.Apex_config.Apex_embedded_apk_config = append(
				config.Apex_config.Apex_embedded_apk_config,
				ApkConfig{
					Package_name: packageName,
					Apk_path:     fi.Path(),
				})
		}
	}

	j, err := json.Marshal(config)
	if err != nil {
		panic(fmt.Errorf("error while marshalling to %q: %#v", output, err))
	}

	ctx.Build(pctx, android.BuildParams{
		Rule:        android.WriteFile,
		Output:      output,
		Description: "Bundle Config " + output.String(),
		Args: map[string]string{
			"content": string(j),
		},
	})

	return output.OutputPath
}

func (a *apexBundle) buildUnflattenedApex(ctx android.ModuleContext) {
	var abis []string
	for _, target := range ctx.MultiTargets() {
		if len(target.Arch.Abi) > 0 {
			abis = append(abis, target.Arch.Abi[0])
		}
	}

	abis = android.FirstUniqueStrings(abis)

	apexType := a.properties.ApexType
	suffix := apexType.suffix()
	var implicitInputs []android.Path
	unsignedOutputFile := android.PathForModuleOut(ctx, a.Name()+suffix+".unsigned")

	// TODO(jiyong): construct the copy rules using RuleBuilder
	var copyCommands []string
	for _, fi := range a.filesInfo {
		destPath := android.PathForModuleOut(ctx, "image"+suffix, fi.Path()).String()
		destPathDir := filepath.Dir(destPath)
		if fi.class == appSet {
			copyCommands = append(copyCommands, "rm -rf "+destPathDir)
		}
		copyCommands = append(copyCommands, "mkdir -p "+destPathDir)
		if a.linkToSystemLib && fi.transitiveDep && fi.AvailableToPlatform() {
			// TODO(jiyong): pathOnDevice should come from fi.module, not being calculated here
			pathOnDevice := filepath.Join("/system", fi.Path())
			copyCommands = append(copyCommands, "ln -sfn "+pathOnDevice+" "+destPath)
		} else {
			if fi.class == appSet {
				copyCommands = append(copyCommands,
					fmt.Sprintf("unzip -qDD -d %s %s", destPathDir, fi.builtFile.String()))
			} else {
				copyCommands = append(copyCommands, "cp -f "+fi.builtFile.String()+" "+destPath)
			}
			implicitInputs = append(implicitInputs, fi.builtFile)
		}
		// create additional symlinks pointing the file inside the APEX
		for _, symlinkPath := range fi.SymlinkPaths() {
			symlinkDest := android.PathForModuleOut(ctx, "image"+suffix, symlinkPath).String()
			copyCommands = append(copyCommands, "ln -sfn "+filepath.Base(destPath)+" "+symlinkDest)
		}
	}

	// TODO(jiyong): use RuleBuilder
	var emitCommands []string
	imageContentFile := android.PathForModuleOut(ctx, "content.txt")
	emitCommands = append(emitCommands, "echo ./apex_manifest.pb >> "+imageContentFile.String())
	if a.minSdkVersion(ctx) == android.SdkVersion_Android10 {
		emitCommands = append(emitCommands, "echo ./apex_manifest.json >> "+imageContentFile.String())
	}
	for _, fi := range a.filesInfo {
		emitCommands = append(emitCommands, "echo './"+fi.Path()+"' >> "+imageContentFile.String())
	}
	emitCommands = append(emitCommands, "sort -o "+imageContentFile.String()+" "+imageContentFile.String())
	implicitInputs = append(implicitInputs, a.manifestPbOut)

	if a.overridableProperties.Allowed_files != nil {
		ctx.Build(pctx, android.BuildParams{
			Rule:        emitApexContentRule,
			Implicits:   implicitInputs,
			Output:      imageContentFile,
			Description: "emit apex image content",
			Args: map[string]string{
				"emit_commands": strings.Join(emitCommands, " && "),
			},
		})
		implicitInputs = append(implicitInputs, imageContentFile)
		allowedFilesFile := android.PathForModuleSrc(ctx, proptools.String(a.overridableProperties.Allowed_files))

		phonyOutput := android.PathForModuleOut(ctx, a.Name()+"-diff-phony-output")
		ctx.Build(pctx, android.BuildParams{
			Rule:        diffApexContentRule,
			Implicits:   implicitInputs,
			Output:      phonyOutput,
			Description: "diff apex image content",
			Args: map[string]string{
				"allowed_files_file": allowedFilesFile.String(),
				"image_content_file": imageContentFile.String(),
				"apex_module_name":   a.Name(),
			},
		})

		implicitInputs = append(implicitInputs, phonyOutput)
	}

	outHostBinDir := android.PathForOutput(ctx, "host", ctx.Config().PrebuiltOS(), "bin").String()
	prebuiltSdkToolsBinDir := filepath.Join("prebuilts", "sdk", "tools", runtime.GOOS, "bin")

	imageDir := android.PathForModuleOut(ctx, "image"+suffix)
	if apexType == imageApex {
		// files and dirs that will be created in APEX
		var readOnlyPaths = []string{"apex_manifest.json", "apex_manifest.pb"}
		var executablePaths []string // this also includes dirs
		var extractedAppSetPaths android.Paths
		var extractedAppSetDirs []string
		for _, f := range a.filesInfo {
			pathInApex := f.Path()
			if f.installDir == "bin" || strings.HasPrefix(f.installDir, "bin/") {
				executablePaths = append(executablePaths, pathInApex)
				for _, s := range f.symlinks {
					executablePaths = append(executablePaths, filepath.Join(f.installDir, s))
				}
			} else if f.class == appSet {
				extractedAppSetPaths = append(extractedAppSetPaths, f.builtFile)
				extractedAppSetDirs = append(extractedAppSetDirs, f.installDir)
			} else {
				readOnlyPaths = append(readOnlyPaths, pathInApex)
			}
			dir := f.installDir
			for !android.InList(dir, executablePaths) && dir != "" {
				executablePaths = append(executablePaths, dir)
				dir, _ = filepath.Split(dir) // move up to the parent
				if len(dir) > 0 {
					// remove trailing slash
					dir = dir[:len(dir)-1]
				}
			}
		}
		sort.Strings(readOnlyPaths)
		sort.Strings(executablePaths)
		cannedFsConfig := android.PathForModuleOut(ctx, "canned_fs_config")
		ctx.Build(pctx, android.BuildParams{
			Rule:        generateFsConfig,
			Output:      cannedFsConfig,
			Description: "generate fs config",
			Inputs:      extractedAppSetPaths,
			Args: map[string]string{
				"ro_paths":   strings.Join(readOnlyPaths, " "),
				"exec_paths": strings.Join(executablePaths, " "),
				"apk_paths":  strings.Join(extractedAppSetDirs, " "),
			},
		})

		optFlags := []string{}

		// Additional implicit inputs.
		implicitInputs = append(implicitInputs, cannedFsConfig, a.fileContexts, a.private_key_file, a.public_key_file)
		optFlags = append(optFlags, "--pubkey "+a.public_key_file.String())

		manifestPackageName := a.getOverrideManifestPackageName(ctx)
		if manifestPackageName != "" {
			optFlags = append(optFlags, "--override_apk_package_name "+manifestPackageName)
		}

		if a.properties.AndroidManifest != nil {
			androidManifestFile := android.PathForModuleSrc(ctx, proptools.String(a.properties.AndroidManifest))
			implicitInputs = append(implicitInputs, androidManifestFile)
			optFlags = append(optFlags, "--android_manifest "+androidManifestFile.String())
		}

		targetSdkVersion := ctx.Config().DefaultAppTargetSdk()
		// TODO(b/157078772): propagate min_sdk_version to apexer.
		minSdkVersion := ctx.Config().DefaultAppTargetSdk()

		if a.minSdkVersion(ctx) == android.SdkVersion_Android10 {
			minSdkVersion = strconv.Itoa(a.minSdkVersion(ctx))
		}

		if java.UseApiFingerprint(ctx) {
			targetSdkVersion = ctx.Config().PlatformSdkCodename() + fmt.Sprintf(".$$(cat %s)", java.ApiFingerprintPath(ctx).String())
			implicitInputs = append(implicitInputs, java.ApiFingerprintPath(ctx))
		}
		if java.UseApiFingerprint(ctx) {
			minSdkVersion = ctx.Config().PlatformSdkCodename() + fmt.Sprintf(".$$(cat %s)", java.ApiFingerprintPath(ctx).String())
			implicitInputs = append(implicitInputs, java.ApiFingerprintPath(ctx))
		}
		optFlags = append(optFlags, "--target_sdk_version "+targetSdkVersion)
		optFlags = append(optFlags, "--min_sdk_version "+minSdkVersion)

		if a.overridableProperties.Logging_parent != "" {
			optFlags = append(optFlags, "--logging_parent ", a.overridableProperties.Logging_parent)
		}

		a.mergedNotices = a.buildNoticeFiles(ctx, a.Name()+suffix)
		if a.mergedNotices.HtmlGzOutput.Valid() {
			// If there's a NOTICE file, embed it as an asset file in the APEX.
			implicitInputs = append(implicitInputs, a.mergedNotices.HtmlGzOutput.Path())
			optFlags = append(optFlags, "--assets_dir "+filepath.Dir(a.mergedNotices.HtmlGzOutput.String()))
		}

		if ctx.ModuleDir() != "system/apex/apexd/apexd_testdata" && ctx.ModuleDir() != "system/apex/shim/build" && a.testOnlyShouldSkipHashtreeGeneration() {
			ctx.PropertyErrorf("test_only_no_hashtree", "not available")
			return
		}
		if a.minSdkVersion(ctx) > android.SdkVersion_Android10 || a.testOnlyShouldSkipHashtreeGeneration() {
			// Apexes which are supposed to be installed in builtin dirs(/system, etc)
			// don't need hashtree for activation. Therefore, by removing hashtree from
			// apex bundle (filesystem image in it, to be specific), we can save storage.
			optFlags = append(optFlags, "--no_hashtree")
		}

		if a.testOnlyShouldSkipPayloadSign() {
			optFlags = append(optFlags, "--unsigned_payload")
		}

		if a.properties.Apex_name != nil {
			// If apex_name is set, apexer can skip checking if key name matches with apex name.
			// Note that apex_manifest is also mended.
			optFlags = append(optFlags, "--do_not_check_keyname")
		}

		if a.minSdkVersion(ctx) == android.SdkVersion_Android10 {
			implicitInputs = append(implicitInputs, a.manifestJsonOut)
			optFlags = append(optFlags, "--manifest_json "+a.manifestJsonOut.String())
		}

		ctx.Build(pctx, android.BuildParams{
			Rule:        apexRule,
			Implicits:   implicitInputs,
			Output:      unsignedOutputFile,
			Description: "apex (" + apexType.name() + ")",
			Args: map[string]string{
				"tool_path":        outHostBinDir + ":" + prebuiltSdkToolsBinDir,
				"image_dir":        imageDir.String(),
				"copy_commands":    strings.Join(copyCommands, " && "),
				"manifest":         a.manifestPbOut.String(),
				"file_contexts":    a.fileContexts.String(),
				"canned_fs_config": cannedFsConfig.String(),
				"key":              a.private_key_file.String(),
				"opt_flags":        strings.Join(optFlags, " "),
			},
		})

		apexProtoFile := android.PathForModuleOut(ctx, a.Name()+".pb"+suffix)
		bundleModuleFile := android.PathForModuleOut(ctx, a.Name()+suffix+"-base.zip")
		a.bundleModuleFile = bundleModuleFile

		ctx.Build(pctx, android.BuildParams{
			Rule:        apexProtoConvertRule,
			Input:       unsignedOutputFile,
			Output:      apexProtoFile,
			Description: "apex proto convert",
		})

		bundleConfig := a.buildBundleConfig(ctx)

		ctx.Build(pctx, android.BuildParams{
			Rule:        apexBundleRule,
			Input:       apexProtoFile,
			Implicit:    bundleConfig,
			Output:      a.bundleModuleFile,
			Description: "apex bundle module",
			Args: map[string]string{
				"abi":    strings.Join(abis, "."),
				"config": bundleConfig.String(),
			},
		})
	} else {
		ctx.Build(pctx, android.BuildParams{
			Rule:        zipApexRule,
			Implicits:   implicitInputs,
			Output:      unsignedOutputFile,
			Description: "apex (" + apexType.name() + ")",
			Args: map[string]string{
				"tool_path":     outHostBinDir + ":" + prebuiltSdkToolsBinDir,
				"image_dir":     imageDir.String(),
				"copy_commands": strings.Join(copyCommands, " && "),
				"manifest":      a.manifestPbOut.String(),
			},
		})
	}

	a.outputFile = android.PathForModuleOut(ctx, a.Name()+suffix)
	rule := java.Signapk
	args := map[string]string{
		"certificates": a.container_certificate_file.String() + " " + a.container_private_key_file.String(),
		"flags":        "-a 4096", //alignment
	}
	implicits := android.Paths{
		a.container_certificate_file,
		a.container_private_key_file,
	}
	if ctx.Config().IsEnvTrue("RBE_SIGNAPK") {
		rule = java.SignapkRE
		args["implicits"] = strings.Join(implicits.Strings(), ",")
		args["outCommaList"] = a.outputFile.String()
	}
	ctx.Build(pctx, android.BuildParams{
		Rule:        rule,
		Description: "signapk",
		Output:      a.outputFile,
		Input:       unsignedOutputFile,
		Implicits:   implicits,
		Args:        args,
	})

	// Install to $OUT/soong/{target,host}/.../apex
	if a.installable() {
		ctx.InstallFile(a.installDir, a.Name()+suffix, a.outputFile)
	}
	a.buildFilesInfo(ctx)

	// installed-files.txt is dist'ed
	a.installedFilesFile = a.buildInstalledFilesFile(ctx, a.outputFile, imageDir)
}

func (a *apexBundle) buildFlattenedApex(ctx android.ModuleContext) {
	// Temporarily wrap the original `ctx` into a `flattenedApexContext` to have it
	// reply true to `InstallBypassMake()` (thus making the call
	// `android.PathForModuleInstall` below use `android.pathForInstallInMakeDir`
	// instead of `android.PathForOutput`) to return the correct path to the flattened
	// APEX (as its contents is installed by Make, not Soong).
	factx := flattenedApexContext{ctx}
	apexBundleName := a.Name()
	a.outputFile = android.PathForModuleInstall(&factx, "apex", apexBundleName)

	if a.installable() {
		installPath := android.PathForModuleInstall(ctx, "apex", apexBundleName)
		devicePath := android.InstallPathToOnDevicePath(ctx, installPath)
		addFlattenedFileContextsInfos(ctx, apexBundleName+":"+devicePath+":"+a.fileContexts.String())
	}
	a.buildFilesInfo(ctx)
}

func (a *apexBundle) setCertificateAndPrivateKey(ctx android.ModuleContext) {
	if a.container_certificate_file == nil {
		cert := String(a.properties.Certificate)
		if cert == "" {
			pem, key := ctx.Config().DefaultAppCertificate(ctx)
			a.container_certificate_file = pem
			a.container_private_key_file = key
		} else {
			defaultDir := ctx.Config().DefaultAppCertificateDir(ctx)
			a.container_certificate_file = defaultDir.Join(ctx, cert+".x509.pem")
			a.container_private_key_file = defaultDir.Join(ctx, cert+".pk8")
		}
	}
}

func (a *apexBundle) buildFilesInfo(ctx android.ModuleContext) {
	if a.installable() {
		// For flattened APEX, do nothing but make sure that APEX manifest and apex_pubkey are also copied along
		// with other ordinary files.
		a.filesInfo = append(a.filesInfo, newApexFile(ctx, a.manifestPbOut, "apex_manifest.pb", ".", etc, nil))

		// rename to apex_pubkey
		copiedPubkey := android.PathForModuleOut(ctx, "apex_pubkey")
		ctx.Build(pctx, android.BuildParams{
			Rule:   android.Cp,
			Input:  a.public_key_file,
			Output: copiedPubkey,
		})
		a.filesInfo = append(a.filesInfo, newApexFile(ctx, copiedPubkey, "apex_pubkey", ".", etc, nil))

		if a.properties.ApexType == flattenedApex {
			apexBundleName := a.Name()
			for _, fi := range a.filesInfo {
				dir := filepath.Join("apex", apexBundleName, fi.installDir)
				target := ctx.InstallFile(android.PathForModuleInstall(ctx, dir), fi.Stem(), fi.builtFile)
				for _, sym := range fi.symlinks {
					ctx.InstallSymlink(android.PathForModuleInstall(ctx, dir), sym, target)
				}
			}
		}
	}
}

func (a *apexBundle) getOverrideManifestPackageName(ctx android.ModuleContext) string {
	// For VNDK APEXes, check "com.android.vndk" in PRODUCT_MANIFEST_PACKAGE_NAME_OVERRIDES
	// to see if it should be overridden because their <apex name> is dynamically generated
	// according to its VNDK version.
	if a.vndkApex {
		overrideName, overridden := ctx.DeviceConfig().OverrideManifestPackageNameFor(vndkApexName)
		if overridden {
			return strings.Replace(*a.properties.Apex_name, vndkApexName, overrideName, 1)
		}
		return ""
	}
	if a.overridableProperties.Package_name != "" {
		return a.overridableProperties.Package_name
	}
	manifestPackageName, overridden := ctx.DeviceConfig().OverrideManifestPackageNameFor(ctx.ModuleName())
	if overridden {
		return manifestPackageName
	}
	return ""
}

func (a *apexBundle) buildApexDependencyInfo(ctx android.ModuleContext) {
	if !a.primaryApexType {
		return
	}

	if a.properties.IsCoverageVariant {
		// Otherwise, we will have duplicated rules for coverage and
		// non-coverage variants of the same APEX
		return
	}

	if ctx.Host() {
		// No need to generate dependency info for host variant
		return
	}

	depInfos := android.DepNameToDepInfoMap{}
	a.walkPayloadDeps(ctx, func(ctx android.ModuleContext, from blueprint.Module, to android.ApexModule, externalDep bool) bool {
		if from.Name() == to.Name() {
			// This can happen for cc.reuseObjTag. We are not interested in tracking this.
			// As soon as the dependency graph crosses the APEX boundary, don't go further.
			return !externalDep
		}

		if info, exists := depInfos[to.Name()]; exists {
			if !android.InList(from.Name(), info.From) {
				info.From = append(info.From, from.Name())
			}
			info.IsExternal = info.IsExternal && externalDep
			depInfos[to.Name()] = info
		} else {
			toMinSdkVersion := "(no version)"
			if m, ok := to.(interface{ MinSdkVersion() string }); ok {
				if v := m.MinSdkVersion(); v != "" {
					toMinSdkVersion = v
				}
			}

			depInfos[to.Name()] = android.ApexModuleDepInfo{
				To:            to.Name(),
				From:          []string{from.Name()},
				IsExternal:    externalDep,
				MinSdkVersion: toMinSdkVersion,
			}
		}

		// As soon as the dependency graph crosses the APEX boundary, don't go further.
		return !externalDep
	})

	a.ApexBundleDepsInfo.BuildDepsInfoLists(ctx, proptools.String(a.properties.Min_sdk_version), depInfos)

	ctx.Build(pctx, android.BuildParams{
		Rule:   android.Phony,
		Output: android.PathForPhony(ctx, a.Name()+"-deps-info"),
		Inputs: []android.Path{
			a.ApexBundleDepsInfo.FullListPath(),
			a.ApexBundleDepsInfo.FlatListPath(),
		},
	})
}

func (a *apexBundle) buildLintReports(ctx android.ModuleContext) {
	depSetsBuilder := java.NewLintDepSetBuilder()
	for _, fi := range a.filesInfo {
		depSetsBuilder.Transitive(fi.lintDepSets)
	}

	a.lintReports = java.BuildModuleLintReportZips(ctx, depSetsBuilder.Build())
}
