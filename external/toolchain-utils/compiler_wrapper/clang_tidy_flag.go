// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"fmt"
	"path/filepath"
	"strings"
)

func processClangTidyFlags(builder *commandBuilder) (cSrcFile string, useClangTidy bool) {
	withTidy, _ := builder.env.getenv("WITH_TIDY")
	if withTidy == "" {
		return "", false
	}
	srcFileSuffixes := []string{
		".c",
		".cc",
		".cpp",
		".C",
		".cxx",
		".c++",
	}
	cSrcFile = ""
	lastArg := ""
	for _, arg := range builder.args {
		if hasAtLeastOneSuffix(arg.value, srcFileSuffixes) && lastArg != "-o" {
			cSrcFile = arg.value
		}
		lastArg = arg.value
	}
	useClangTidy = cSrcFile != ""
	return cSrcFile, useClangTidy
}

func runClangTidy(env env, clangCmd *command, cSrcFile string) error {
	defaultTidyChecks := strings.Join([]string{
		"*",
		"google*",
		"-bugprone-narrowing-conversions",
		"-cppcoreguidelines-*",
		"-fuchsia-*",
		"-google-build-using-namespace",
		"-google-default-arguments",
		"-google-explicit-constructor",
		"-google-readability*",
		"-google-runtime-int",
		"-google-runtime-references",
		"-hicpp-avoid-c-arrays",
		"-hicpp-braces-around-statements",
		"-hicpp-no-array-decay",
		"-hicpp-signed-bitwise",
		"-hicpp-uppercase-literal-suffix",
		"-hicpp-use-auto",
		"-llvm-namespace-comment",
		"-misc-non-private-member-variables-in-classes",
		"-misc-unused-parameters",
		"-modernize-*",
		"-readability-*",
	}, ",")

	resourceDir, err := getClangResourceDir(env, clangCmd.Path)
	if err != nil {
		return err
	}

	clangTidyPath := filepath.Join(filepath.Dir(clangCmd.Path), "clang-tidy")
	clangTidyCmd := &command{
		Path: clangTidyPath,
		Args: append([]string{
			"-checks=" + defaultTidyChecks,
			cSrcFile,
			"--",
			"-resource-dir=" + resourceDir,
		}, clangCmd.Args...),
		EnvUpdates: clangCmd.EnvUpdates,
	}

	// Note: We pass nil as stdin as we checked before that the compiler
	// was invoked with a source file argument.
	exitCode, err := wrapSubprocessErrorWithSourceLoc(clangTidyCmd,
		env.run(clangTidyCmd, nil, env.stdout(), env.stderr()))
	if err == nil && exitCode != 0 {
		// Note: We continue on purpose when clang-tidy fails
		// to maintain compatibility with the previous wrapper.
		fmt.Fprint(env.stderr(), "clang-tidy failed")
	}
	return err
}

func hasAtLeastOneSuffix(s string, suffixes []string) bool {
	for _, suffix := range suffixes {
		if strings.HasSuffix(s, suffix) {
			return true
		}
	}
	return false
}
