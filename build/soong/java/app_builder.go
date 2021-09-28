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

package java

// This file generates the final rules for compiling all Java.  All properties related to
// compiling should have been translated into javaBuilderFlags or another argument to the Transform*
// functions.

import (
	"path/filepath"
	"strings"

	"github.com/google/blueprint"
	"github.com/google/blueprint/proptools"

	"android/soong/android"
	"android/soong/remoteexec"
)

var (
	Signapk, SignapkRE = remoteexec.StaticRules(pctx, "signapk",
		blueprint.RuleParams{
			Command: `$reTemplate${config.JavaCmd} ${config.JavaVmFlags} -Djava.library.path=$$(dirname ${config.SignapkJniLibrary}) ` +
				`-jar ${config.SignapkCmd} $flags $certificates $in $out`,
			CommandDeps: []string{"${config.SignapkCmd}", "${config.SignapkJniLibrary}"},
		},
		&remoteexec.REParams{Labels: map[string]string{"type": "tool", "name": "signapk"},
			ExecStrategy:    "${config.RESignApkExecStrategy}",
			Inputs:          []string{"${config.SignapkCmd}", "$in", "$$(dirname ${config.SignapkJniLibrary})", "$implicits"},
			OutputFiles:     []string{"$outCommaList"},
			ToolchainInputs: []string{"${config.JavaCmd}"},
			Platform:        map[string]string{remoteexec.PoolKey: "${config.REJavaPool}"},
		}, []string{"flags", "certificates"}, []string{"implicits", "outCommaList"})
)

var combineApk = pctx.AndroidStaticRule("combineApk",
	blueprint.RuleParams{
		Command:     `${config.MergeZipsCmd} $out $in`,
		CommandDeps: []string{"${config.MergeZipsCmd}"},
	})

func CreateAndSignAppPackage(ctx android.ModuleContext, outputFile android.WritablePath,
	packageFile, jniJarFile, dexJarFile android.Path, certificates []Certificate, deps android.Paths, v4SignatureFile android.WritablePath, lineageFile android.Path) {

	unsignedApkName := strings.TrimSuffix(outputFile.Base(), ".apk") + "-unsigned.apk"
	unsignedApk := android.PathForModuleOut(ctx, unsignedApkName)

	var inputs android.Paths
	if dexJarFile != nil {
		inputs = append(inputs, dexJarFile)
	}
	inputs = append(inputs, packageFile)
	if jniJarFile != nil {
		inputs = append(inputs, jniJarFile)
	}

	ctx.Build(pctx, android.BuildParams{
		Rule:      combineApk,
		Inputs:    inputs,
		Output:    unsignedApk,
		Implicits: deps,
	})

	SignAppPackage(ctx, outputFile, unsignedApk, certificates, v4SignatureFile, lineageFile)
}

func SignAppPackage(ctx android.ModuleContext, signedApk android.WritablePath, unsignedApk android.Path, certificates []Certificate, v4SignatureFile android.WritablePath, lineageFile android.Path) {

	var certificateArgs []string
	var deps android.Paths
	for _, c := range certificates {
		certificateArgs = append(certificateArgs, c.Pem.String(), c.Key.String())
		deps = append(deps, c.Pem, c.Key)
	}

	outputFiles := android.WritablePaths{signedApk}
	var flags []string
	if v4SignatureFile != nil {
		outputFiles = append(outputFiles, v4SignatureFile)
		flags = append(flags, "--enable-v4")
	}

	if lineageFile != nil {
		flags = append(flags, "--lineage", lineageFile.String())
		deps = append(deps, lineageFile)
	}

	rule := Signapk
	args := map[string]string{
		"certificates": strings.Join(certificateArgs, " "),
		"flags":        strings.Join(flags, " "),
	}
	if ctx.Config().IsEnvTrue("RBE_SIGNAPK") {
		rule = SignapkRE
		args["implicits"] = strings.Join(deps.Strings(), ",")
		args["outCommaList"] = strings.Join(outputFiles.Strings(), ",")
	}
	ctx.Build(pctx, android.BuildParams{
		Rule:        rule,
		Description: "signapk",
		Outputs:     outputFiles,
		Input:       unsignedApk,
		Implicits:   deps,
		Args:        args,
	})
}

var buildAAR = pctx.AndroidStaticRule("buildAAR",
	blueprint.RuleParams{
		Command: `rm -rf ${outDir} && mkdir -p ${outDir} && ` +
			`cp ${manifest} ${outDir}/AndroidManifest.xml && ` +
			`cp ${classesJar} ${outDir}/classes.jar && ` +
			`cp ${rTxt} ${outDir}/R.txt && ` +
			`${config.SoongZipCmd} -jar -o $out -C ${outDir} -D ${outDir}`,
		CommandDeps: []string{"${config.SoongZipCmd}"},
	},
	"manifest", "classesJar", "rTxt", "outDir")

func BuildAAR(ctx android.ModuleContext, outputFile android.WritablePath,
	classesJar, manifest, rTxt android.Path, res android.Paths) {

	// TODO(ccross): uniquify and copy resources with dependencies

	deps := android.Paths{manifest, rTxt}
	classesJarPath := ""
	if classesJar != nil {
		deps = append(deps, classesJar)
		classesJarPath = classesJar.String()
	}

	ctx.Build(pctx, android.BuildParams{
		Rule:        buildAAR,
		Description: "aar",
		Implicits:   deps,
		Output:      outputFile,
		Args: map[string]string{
			"manifest":   manifest.String(),
			"classesJar": classesJarPath,
			"rTxt":       rTxt.String(),
			"outDir":     android.PathForModuleOut(ctx, "aar").String(),
		},
	})
}

var buildBundleModule = pctx.AndroidStaticRule("buildBundleModule",
	blueprint.RuleParams{
		Command:     `${config.MergeZipsCmd} ${out} ${in}`,
		CommandDeps: []string{"${config.MergeZipsCmd}"},
	})

var bundleMungePackage = pctx.AndroidStaticRule("bundleMungePackage",
	blueprint.RuleParams{
		Command:     `${config.Zip2ZipCmd} -i ${in} -o ${out} AndroidManifest.xml:manifest/AndroidManifest.xml resources.pb "res/**/*" "assets/**/*"`,
		CommandDeps: []string{"${config.Zip2ZipCmd}"},
	})

var bundleMungeDexJar = pctx.AndroidStaticRule("bundleMungeDexJar",
	blueprint.RuleParams{
		Command: `${config.Zip2ZipCmd} -i ${in} -o ${out} "classes*.dex:dex/" && ` +
			`${config.Zip2ZipCmd} -i ${in} -o ${resJar} -x "classes*.dex" "**/*:root/"`,
		CommandDeps: []string{"${config.Zip2ZipCmd}"},
	}, "resJar")

// Builds an app into a module suitable for input to bundletool
func BuildBundleModule(ctx android.ModuleContext, outputFile android.WritablePath,
	packageFile, jniJarFile, dexJarFile android.Path) {

	protoResJarFile := android.PathForModuleOut(ctx, "package-res.pb.apk")
	aapt2Convert(ctx, protoResJarFile, packageFile)

	var zips android.Paths

	mungedPackage := android.PathForModuleOut(ctx, "bundle", "apk.zip")
	ctx.Build(pctx, android.BuildParams{
		Rule:        bundleMungePackage,
		Input:       protoResJarFile,
		Output:      mungedPackage,
		Description: "bundle apk",
	})
	zips = append(zips, mungedPackage)

	if dexJarFile != nil {
		mungedDexJar := android.PathForModuleOut(ctx, "bundle", "dex.zip")
		mungedResJar := android.PathForModuleOut(ctx, "bundle", "res.zip")
		ctx.Build(pctx, android.BuildParams{
			Rule:           bundleMungeDexJar,
			Input:          dexJarFile,
			Output:         mungedDexJar,
			ImplicitOutput: mungedResJar,
			Description:    "bundle dex",
			Args: map[string]string{
				"resJar": mungedResJar.String(),
			},
		})
		zips = append(zips, mungedDexJar, mungedResJar)
	}
	if jniJarFile != nil {
		zips = append(zips, jniJarFile)
	}

	ctx.Build(pctx, android.BuildParams{
		Rule:        buildBundleModule,
		Inputs:      zips,
		Output:      outputFile,
		Description: "bundle",
	})
}

func TransformJniLibsToJar(ctx android.ModuleContext, outputFile android.WritablePath,
	jniLibs []jniLib, uncompressJNI bool) {

	var deps android.Paths
	jarArgs := []string{
		"-j", // junk paths, they will be added back with -P arguments
	}

	if uncompressJNI {
		jarArgs = append(jarArgs, "-L", "0")
	}

	for _, j := range jniLibs {
		deps = append(deps, j.path)
		jarArgs = append(jarArgs,
			"-P", targetToJniDir(j.target),
			"-f", j.path.String())
	}

	rule := zip
	args := map[string]string{
		"jarArgs": strings.Join(proptools.NinjaAndShellEscapeList(jarArgs), " "),
	}
	if ctx.Config().IsEnvTrue("RBE_ZIP") {
		rule = zipRE
		args["implicits"] = strings.Join(deps.Strings(), ",")
	}
	ctx.Build(pctx, android.BuildParams{
		Rule:        rule,
		Description: "zip jni libs",
		Output:      outputFile,
		Implicits:   deps,
		Args:        args,
	})
}

func targetToJniDir(target android.Target) string {
	return filepath.Join("lib", target.Arch.Abi[0])
}
