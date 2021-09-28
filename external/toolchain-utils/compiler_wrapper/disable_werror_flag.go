// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"bytes"
	"encoding/json"
	"io/ioutil"
	"os"
	"strings"
	"syscall"
)

func shouldForceDisableWError(env env) bool {
	value, _ := env.getenv("FORCE_DISABLE_WERROR")
	return value != ""
}

func doubleBuildWithWNoError(env env, cfg *config, originalCmd *command) (exitCode int, err error) {
	originalStdoutBuffer := &bytes.Buffer{}
	originalStderrBuffer := &bytes.Buffer{}
	// TODO: This is a bug in the old wrapper that it drops the ccache path
	// during double build. Fix this once we don't compare to the old wrapper anymore.
	if originalCmd.Path == "/usr/bin/ccache" {
		originalCmd.Path = "ccache"
	}
	originalStdinBuffer := &bytes.Buffer{}
	originalExitCode, err := wrapSubprocessErrorWithSourceLoc(originalCmd,
		env.run(originalCmd, teeStdinIfNeeded(env, originalCmd, originalStdinBuffer), originalStdoutBuffer, originalStderrBuffer))
	if err != nil {
		return 0, err
	}
	// The only way we can do anything useful is if it looks like the failure
	// was -Werror-related.
	if originalExitCode == 0 || !strings.Contains(originalStderrBuffer.String(), "-Werror") {
		originalStdoutBuffer.WriteTo(env.stdout())
		originalStderrBuffer.WriteTo(env.stderr())
		return originalExitCode, nil
	}

	retryStdoutBuffer := &bytes.Buffer{}
	retryStderrBuffer := &bytes.Buffer{}
	retryCommand := &command{
		Path:       originalCmd.Path,
		Args:       append(originalCmd.Args, "-Wno-error"),
		EnvUpdates: originalCmd.EnvUpdates,
	}
	retryExitCode, err := wrapSubprocessErrorWithSourceLoc(retryCommand,
		env.run(retryCommand, bytes.NewReader(originalStdinBuffer.Bytes()), retryStdoutBuffer, retryStderrBuffer))
	if err != nil {
		return 0, err
	}
	// If -Wno-error fixed us, pretend that we never ran without -Wno-error.
	// Otherwise, pretend that we never ran the second invocation. Since -Werror
	// is an issue, log in either case.
	if retryExitCode == 0 {
		retryStdoutBuffer.WriteTo(env.stdout())
		retryStderrBuffer.WriteTo(env.stderr())
	} else {
		originalStdoutBuffer.WriteTo(env.stdout())
		originalStderrBuffer.WriteTo(env.stderr())
	}

	// All of the below is basically logging. If we fail at any point, it's
	// reasonable for that to fail the build. This is all meant for FYI-like
	// builders in the first place.

	// Buildbots use a nonzero umask, which isn't quite what we want: these directories should
	// be world-readable and world-writable.
	oldMask := syscall.Umask(0)
	defer syscall.Umask(oldMask)

	// Allow root and regular users to write to this without issue.
	if err := os.MkdirAll(cfg.newWarningsDir, 0777); err != nil {
		return 0, wrapErrorwithSourceLocf(err, "error creating warnings directory %s", cfg.newWarningsDir)
	}

	// Have some tag to show that files aren't fully written. It would be sad if
	// an interrupted build (or out of disk space, or similar) caused tools to
	// have to be overly-defensive.
	incompleteSuffix := ".incomplete"

	// Coming up with a consistent name for this is difficult (compiler command's
	// SHA can clash in the case of identically named files in different
	// directories, or similar); let's use a random one.
	tmpFile, err := ioutil.TempFile(cfg.newWarningsDir, "warnings_report*.json"+incompleteSuffix)
	if err != nil {
		return 0, wrapErrorwithSourceLocf(err, "error creating warnings file")
	}

	if err := tmpFile.Chmod(0666); err != nil {
		return 0, wrapErrorwithSourceLocf(err, "error chmoding the file to be world-readable/writeable")
	}

	lines := []string{}
	if originalStderrBuffer.Len() > 0 {
		lines = append(lines, originalStderrBuffer.String())
	}
	if originalStdoutBuffer.Len() > 0 {
		lines = append(lines, originalStdoutBuffer.String())
	}
	outputToLog := strings.Join(lines, "\n")

	jsonData := warningsJSONData{
		Cwd:     env.getwd(),
		Command: append([]string{originalCmd.Path}, originalCmd.Args...),
		Stdout:  outputToLog,
	}
	enc := json.NewEncoder(tmpFile)
	if err := enc.Encode(jsonData); err != nil {
		_ = tmpFile.Close()
		return 0, wrapErrorwithSourceLocf(err, "error writing warnings data")
	}

	if err := tmpFile.Close(); err != nil {
		return 0, wrapErrorwithSourceLocf(err, "error closing warnings file")
	}

	if err := os.Rename(tmpFile.Name(), tmpFile.Name()[:len(tmpFile.Name())-len(incompleteSuffix)]); err != nil {
		return 0, wrapErrorwithSourceLocf(err, "error removing incomplete suffix from warnings file")
	}

	return retryExitCode, nil
}

// Struct used to write JSON. Fileds have to be uppercase for the json
// encoder to read them.
type warningsJSONData struct {
	Cwd     string   `json:"cwd"`
	Command []string `json:"command"`
	Stdout  string   `json:"stdout"`
}
