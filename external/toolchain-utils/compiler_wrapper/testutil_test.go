// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"bytes"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"testing"
)

const mainCc = "main.cc"
const clangAndroid = "./clang"
const clangX86_64 = "./x86_64-cros-linux-gnu-clang"
const gccX86_64 = "./x86_64-cros-linux-gnu-gcc"
const gccX86_64Eabi = "./x86_64-cros-eabi-gcc"
const gccArmV7 = "./armv7m-cros-linux-gnu-gcc"
const gccArmV7Eabi = "./armv7m-cros-eabi-gcc"
const gccArmV8 = "./armv8m-cros-linux-gnu-gcc"
const gccArmV8Eabi = "./armv8m-cros-eabi-gcc"

type testContext struct {
	t            *testing.T
	wd           string
	tempDir      string
	env          []string
	cfg          *config
	inputCmd     *command
	lastCmd      *command
	cmdCount     int
	cmdMock      func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error
	stdinBuffer  bytes.Buffer
	stdoutBuffer bytes.Buffer
	stderrBuffer bytes.Buffer
}

func withTestContext(t *testing.T, work func(ctx *testContext)) {
	t.Parallel()
	tempDir, err := ioutil.TempDir("", "compiler_wrapper")
	if err != nil {
		t.Fatalf("Unable to create the temp dir. Error: %s", err)
	}
	defer os.RemoveAll(tempDir)

	ctx := testContext{
		t:       t,
		wd:      tempDir,
		tempDir: tempDir,
		env:     nil,
		cfg:     &config{},
	}
	ctx.updateConfig(&config{})

	work(&ctx)
}

var _ env = (*testContext)(nil)

func (ctx *testContext) getenv(key string) (string, bool) {
	for i := len(ctx.env) - 1; i >= 0; i-- {
		entry := ctx.env[i]
		if strings.HasPrefix(entry, key+"=") {
			return entry[len(key)+1:], true
		}
	}
	return "", false
}

func (ctx *testContext) environ() []string {
	return ctx.env
}

func (ctx *testContext) getwd() string {
	return ctx.wd
}

func (ctx *testContext) stdin() io.Reader {
	return &ctx.stdinBuffer
}

func (ctx *testContext) stdout() io.Writer {
	return &ctx.stdoutBuffer
}

func (ctx *testContext) stdoutString() string {
	return ctx.stdoutBuffer.String()
}

func (ctx *testContext) stderr() io.Writer {
	return &ctx.stderrBuffer
}

func (ctx *testContext) stderrString() string {
	return ctx.stderrBuffer.String()
}

func (ctx *testContext) run(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
	ctx.cmdCount++
	ctx.lastCmd = cmd
	if ctx.cmdMock != nil {
		return ctx.cmdMock(cmd, stdin, stdout, stderr)
	}
	return nil
}

func (ctx *testContext) exec(cmd *command) error {
	ctx.cmdCount++
	ctx.lastCmd = cmd
	if ctx.cmdMock != nil {
		return ctx.cmdMock(cmd, ctx.stdin(), ctx.stdout(), ctx.stderr())
	}
	return nil
}

func (ctx *testContext) must(exitCode int) *command {
	if exitCode != 0 {
		ctx.t.Fatalf("expected no error, but got exit code %d. Stderr: %s",
			exitCode, ctx.stderrString())
	}
	return ctx.lastCmd
}

func (ctx *testContext) mustFail(exitCode int) string {
	if exitCode == 0 {
		ctx.t.Fatalf("expected an error, but got none")
	}
	return ctx.stderrString()
}

func (ctx *testContext) updateConfig(cfg *config) {
	*ctx.cfg = *cfg
	ctx.cfg.newWarningsDir = filepath.Join(ctx.tempDir, "fatal_clang_warnings")
}

func (ctx *testContext) newCommand(path string, args ...string) *command {
	// Create an empty wrapper at the given path.
	// Needed as we are resolving symlinks which stats the wrapper file.
	ctx.writeFile(path, "")
	return &command{
		Path: path,
		Args: args,
	}
}

func (ctx *testContext) writeFile(fullFileName string, fileContent string) {
	if !filepath.IsAbs(fullFileName) {
		fullFileName = filepath.Join(ctx.tempDir, fullFileName)
	}
	if err := os.MkdirAll(filepath.Dir(fullFileName), 0777); err != nil {
		ctx.t.Fatal(err)
	}
	if err := ioutil.WriteFile(fullFileName, []byte(fileContent), 0777); err != nil {
		ctx.t.Fatal(err)
	}
}

func (ctx *testContext) symlink(oldname string, newname string) {
	if !filepath.IsAbs(oldname) {
		oldname = filepath.Join(ctx.tempDir, oldname)
	}
	if !filepath.IsAbs(newname) {
		newname = filepath.Join(ctx.tempDir, newname)
	}
	if err := os.MkdirAll(filepath.Dir(newname), 0777); err != nil {
		ctx.t.Fatal(err)
	}
	if err := os.Symlink(oldname, newname); err != nil {
		ctx.t.Fatal(err)
	}
}

func (ctx *testContext) readAllString(r io.Reader) string {
	if r == nil {
		return ""
	}
	bytes, err := ioutil.ReadAll(r)
	if err != nil {
		ctx.t.Fatal(err)
	}
	return string(bytes)
}

func verifyPath(cmd *command, expectedRegex string) error {
	compiledRegex := regexp.MustCompile(matchFullString(expectedRegex))
	if !compiledRegex.MatchString(cmd.Path) {
		return fmt.Errorf("path does not match %s. Actual %s", expectedRegex, cmd.Path)
	}
	return nil
}

func verifyArgCount(cmd *command, expectedCount int, expectedRegex string) error {
	compiledRegex := regexp.MustCompile(matchFullString(expectedRegex))
	count := 0
	for _, arg := range cmd.Args {
		if compiledRegex.MatchString(arg) {
			count++
		}
	}
	if count != expectedCount {
		return fmt.Errorf("expected %d matches for arg %s. All args: %s",
			expectedCount, expectedRegex, cmd.Args)
	}
	return nil
}

func verifyArgOrder(cmd *command, expectedRegexes ...string) error {
	compiledRegexes := []*regexp.Regexp{}
	for _, regex := range expectedRegexes {
		compiledRegexes = append(compiledRegexes, regexp.MustCompile(matchFullString(regex)))
	}
	expectedArgIndex := 0
	for _, arg := range cmd.Args {
		if expectedArgIndex == len(compiledRegexes) {
			break
		} else if compiledRegexes[expectedArgIndex].MatchString(arg) {
			expectedArgIndex++
		}
	}
	if expectedArgIndex != len(expectedRegexes) {
		return fmt.Errorf("expected args %s in order. All args: %s",
			expectedRegexes, cmd.Args)
	}
	return nil
}

func verifyEnvUpdate(cmd *command, expectedRegex string) error {
	compiledRegex := regexp.MustCompile(matchFullString(expectedRegex))
	for _, update := range cmd.EnvUpdates {
		if compiledRegex.MatchString(update) {
			return nil
		}
	}
	return fmt.Errorf("expected at least one match for env update %s. All env updates: %s",
		expectedRegex, cmd.EnvUpdates)
}

func verifyNoEnvUpdate(cmd *command, expectedRegex string) error {
	compiledRegex := regexp.MustCompile(matchFullString(expectedRegex))
	updates := cmd.EnvUpdates
	for _, update := range updates {
		if compiledRegex.MatchString(update) {
			return fmt.Errorf("expected no match for env update %s. All env updates: %s",
				expectedRegex, cmd.EnvUpdates)
		}
	}
	return nil
}

func hasInternalError(stderr string) bool {
	return strings.Contains(stderr, "Internal error")
}

func verifyInternalError(stderr string) error {
	if !hasInternalError(stderr) {
		return fmt.Errorf("expected an internal error. Got: %s", stderr)
	}
	if ok, _ := regexp.MatchString(`\w+.go:\d+`, stderr); !ok {
		return fmt.Errorf("expected a source line reference. Got: %s", stderr)
	}
	return nil
}

func verifyNonInternalError(stderr string, expectedRegex string) error {
	if hasInternalError(stderr) {
		return fmt.Errorf("expected a non internal error. Got: %s", stderr)
	}
	if ok, _ := regexp.MatchString(`\w+.go:\d+`, stderr); ok {
		return fmt.Errorf("expected no source line reference. Got: %s", stderr)
	}
	if ok, _ := regexp.MatchString(matchFullString(expectedRegex), strings.TrimSpace(stderr)); !ok {
		return fmt.Errorf("expected stderr matching %s. Got: %s", expectedRegex, stderr)
	}
	return nil
}

func matchFullString(regex string) string {
	return "^" + regex + "$"
}

func newExitCodeError(exitCode int) error {
	// It's actually hard to create an error that represents a command
	// with exit code. Using a real command instead.
	tmpCmd := exec.Command("/bin/sh", "-c", fmt.Sprintf("exit %d", exitCode))
	return tmpCmd.Run()
}
