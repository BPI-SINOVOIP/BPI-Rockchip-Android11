// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"errors"
	"fmt"
	"io"
	"path/filepath"
	"strings"
	"testing"
)

func TestCallBisectDriver(t *testing.T) {
	withBisectTestContext(t, func(ctx *testContext) {
		ctx.env = []string{
			"BISECT_STAGE=someBisectStage",
			"BISECT_DIR=someBisectDir",
		}
		cmd := mustCallBisectDriver(ctx, callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyPath(cmd, "bisect_driver"); err != nil {
			t.Error(err)
		}
		if err := verifyArgOrder(cmd,
			"someBisectStage", "someBisectDir", filepath.Join(ctx.tempDir, gccX86_64+".real"), "--sysroot=.*", mainCc); err != nil {
			t.Error(err)
		}
	})
}

func TestCallBisectDriverWithParamsFile(t *testing.T) {
	withBisectTestContext(t, func(ctx *testContext) {
		ctx.env = []string{
			"BISECT_STAGE=someBisectStage",
			"BISECT_DIR=someBisectDir",
		}
		paramsFile1 := filepath.Join(ctx.tempDir, "params1")
		ctx.writeFile(paramsFile1, "a\n#comment\n@params2")
		paramsFile2 := filepath.Join(ctx.tempDir, "params2")
		ctx.writeFile(paramsFile2, "b\n"+mainCc)

		cmd := mustCallBisectDriver(ctx, callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, "@"+paramsFile1)))
		if err := verifyArgOrder(cmd,
			"a", "b", mainCc); err != nil {
			t.Error(err)
		}
	})
}

func TestCallBisectDriverWithCCache(t *testing.T) {
	withBisectTestContext(t, func(ctx *testContext) {
		ctx.cfg.useCCache = true
		cmd := ctx.must(callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyPath(cmd, "/usr/bin/env"); err != nil {
			t.Error(err)
		}
		if err := verifyArgOrder(cmd, "python", "/usr/bin/ccache"); err != nil {
			t.Error(err)
		}
		if err := verifyEnvUpdate(cmd, "CCACHE_DIR=.*"); err != nil {
			t.Error(err)
		}
	})
}

func TestDefaultBisectDirCros(t *testing.T) {
	withBisectTestContext(t, func(ctx *testContext) {
		ctx.env = []string{
			"BISECT_STAGE=someBisectStage",
		}
		cmd := mustCallBisectDriver(ctx, callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyArgOrder(cmd,
			"someBisectStage", "/tmp/sysroot_bisect"); err != nil {
			t.Error(err)
		}
	})
}

func TestDefaultBisectDirAndroid(t *testing.T) {
	withBisectTestContext(t, func(ctx *testContext) {
		ctx.env = []string{
			"BISECT_STAGE=someBisectStage",
			"HOME=/somehome",
		}
		ctx.cfg.isAndroidWrapper = true
		cmd := mustCallBisectDriver(ctx, callCompiler(ctx, ctx.cfg, ctx.newCommand(clangAndroid, mainCc)))
		if err := verifyArgOrder(cmd,
			"someBisectStage", filepath.Join("/somehome", "ANDROID_BISECT")); err != nil {
			t.Error(err)
		}
	})
}

func TestForwardStdOutAndStdErrAndExitCodeFromBisect(t *testing.T) {
	withBisectTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			fmt.Fprint(stdout, "somemessage")
			fmt.Fprint(stderr, "someerror")
			return newExitCodeError(23)
		}
		exitCode := callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, mainCc))
		if exitCode != 23 {
			t.Errorf("unexpected exit code. Got: %d", exitCode)
		}
		if ctx.stdoutString() != "somemessage" {
			t.Errorf("stdout was not forwarded. Got: %s", ctx.stdoutString())
		}
		if ctx.stderrString() != "someerror" {
			t.Errorf("stderr was not forwarded. Got: %s", ctx.stderrString())
		}
	})
}

func TestForwardGeneralErrorFromBisect(t *testing.T) {
	withBisectTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			return errors.New("someerror")
		}
		stderr := ctx.mustFail(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyInternalError(stderr); err != nil {
			t.Fatal(err)
		}
		if !strings.Contains(stderr, "someerror") {
			t.Errorf("unexpected error. Got: %s", stderr)
		}
	})
}

func withBisectTestContext(t *testing.T, work func(ctx *testContext)) {
	withTestContext(t, func(ctx *testContext) {
		ctx.env = []string{"BISECT_STAGE=xyz"}
		// We execute the python script but replace the call to the bisect_driver with
		// a mock that logs the data.
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			if err := verifyPath(cmd, "/usr/bin/env"); err != nil {
				return err
			}
			if cmd.Args[0] != "python" {
				return fmt.Errorf("expected a call to python. Got: %s", cmd.Args[0])
			}
			if cmd.Args[1] != "-c" {
				return fmt.Errorf("expected an inline python script. Got: %s", cmd.Args)
			}
			script := cmd.Args[2]
			mock := `
class BisectDriver:
	def __init__(self):
		self.VALID_MODES = ['POPULATE_GOOD', 'POPULATE_BAD', 'TRIAGE']
	def bisect_driver(self, bisect_stage, bisect_dir, execargs):
		print('command bisect_driver')
		print('arg %s' % bisect_stage)
		print('arg %s' % bisect_dir)
		for arg in execargs:
			print('arg %s' % arg)

bisect_driver = BisectDriver()
`
			script = mock + script
			script = strings.Replace(script, "import bisect_driver", "", -1)
			cmdCopy := *cmd
			cmdCopy.Args = append(append(cmd.Args[:2], script), cmd.Args[3:]...)
			// Evaluate the python script, but replace the call to the bisect_driver
			// with a log statement so that we can assert it.
			return runCmd(ctx, &cmdCopy, nil, stdout, stderr)
		}
		work(ctx)
	})
}

func mustCallBisectDriver(ctx *testContext, exitCode int) *command {
	ctx.must(exitCode)
	cmd := &command{}
	for _, line := range strings.Split(ctx.stdoutString(), "\n") {
		if prefix := "command "; strings.HasPrefix(line, prefix) {
			cmd.Path = line[len(prefix):]
		} else if prefix := "arg "; strings.HasPrefix(line, prefix) {
			cmd.Args = append(cmd.Args, line[len(prefix):])
		}
	}
	return cmd
}
