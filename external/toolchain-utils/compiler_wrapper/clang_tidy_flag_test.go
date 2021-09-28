// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"errors"
	"fmt"
	"io"
	"path"
	"strings"
	"testing"
)

func TestClangTidyBasename(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		testData := []struct {
			in  string
			out string
		}{
			{"./x86_64-cros-linux-gnu-clang", ".*/clang-tidy"},
			{"./x86_64-cros-linux-gnu-clang++", ".*/clang-tidy"},
		}

		var clangTidyCmd *command
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			if ctx.cmdCount == 2 {
				clangTidyCmd = cmd
			}
			return nil
		}

		for _, tt := range testData {
			ctx.cmdCount = 0
			clangTidyCmd = nil
			ctx.must(callCompiler(ctx, ctx.cfg,
				ctx.newCommand(tt.in, mainCc)))
			if ctx.cmdCount != 3 {
				t.Errorf("expected 3 calls. Got: %d", ctx.cmdCount)
			}
			if err := verifyPath(clangTidyCmd, tt.out); err != nil {
				t.Error(err)
			}
		}
	})
}

func TestClangTidyClangResourceDir(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				if err := verifyPath(cmd, "usr/bin/clang"); err != nil {
					t.Error(err)
				}
				if err := verifyArgOrder(cmd, "--print-resource-dir"); err != nil {
					t.Error(err)
				}
				fmt.Fprint(stdout, "someResourcePath")
				return nil
			case 2:
				if err := verifyPath(cmd, "usr/bin/clang-tidy"); err != nil {
					return err
				}
				if err := verifyArgOrder(cmd, "-resource-dir=someResourcePath", mainCc); err != nil {
					return err
				}
				return nil
			case 3:
				if err := verifyPath(cmd, "usr/bin/clang"); err != nil {
					t.Error(err)
				}
				return nil
			default:
				t.Fatalf("unexpected command %#v", cmd)
				return nil
			}
		}
		ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, mainCc)))
		if ctx.cmdCount != 3 {
			t.Errorf("expected 3 calls. Got: %d", ctx.cmdCount)
		}
	})
}

func TestClangTidyArgOrder(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			if ctx.cmdCount == 2 {
				if err := verifyArgOrder(cmd, "-checks=.*", mainCc, "--", "-resource-dir=.*", mainCc, "--some_arg"); err != nil {
					return err
				}
			}
			return nil
		}
		ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, mainCc, "--some_arg")))
		if ctx.cmdCount != 3 {
			t.Errorf("expected 3 calls. Got: %d", ctx.cmdCount)
		}
	})
}

func TestForwardStdOutAndStderrFromClangTidyCall(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			if ctx.cmdCount == 2 {
				fmt.Fprint(stdout, "somemessage")
				fmt.Fprint(stderr, "someerror")
			}
			return nil
		}
		ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, mainCc)))
		if ctx.stdoutString() != "somemessage" {
			t.Errorf("stdout was not forwarded. Got: %s", ctx.stdoutString())
		}
		if ctx.stderrString() != "someerror" {
			t.Errorf("stderr was not forwarded. Got: %s", ctx.stderrString())
		}
	})
}

func TestIgnoreNonZeroExitCodeFromClangTidy(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			if ctx.cmdCount == 2 {
				return newExitCodeError(23)
			}
			return nil
		}
		ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, mainCc)))
		stderr := ctx.stderrString()
		if err := verifyNonInternalError(stderr, "clang-tidy failed"); err != nil {
			t.Error(err)
		}
	})
}

func TestReportGeneralErrorsFromClangTidy(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			if ctx.cmdCount == 2 {
				return errors.New("someerror")
			}
			return nil
		}
		stderr := ctx.mustFail(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, mainCc)))
		if err := verifyInternalError(stderr); err != nil {
			t.Fatal(err)
		}
		if !strings.Contains(stderr, "someerror") {
			t.Errorf("unexpected error. Got: %s", stderr)
		}
	})
}

func TestOmitClangTidyForGcc(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, mainCc)))
		if ctx.cmdCount > 1 {
			t.Errorf("expected 1 command. Got: %d", ctx.cmdCount)
		}
	})
}

func TestOmitClangTidyForGccWithClangSyntax(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-clang-syntax", mainCc)))
		if ctx.cmdCount > 2 {
			t.Errorf("expected 2 commands. Got: %d", ctx.cmdCount)
		}
	})
}

func TestUseClangTidyBasedOnFileExtension(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		testData := []struct {
			args      []string
			clangTidy bool
		}{
			{[]string{"main.cc"}, true},
			{[]string{"main.cc"}, true},
			{[]string{"main.C"}, true},
			{[]string{"main.cxx"}, true},
			{[]string{"main.c++"}, true},
			{[]string{"main.xy"}, false},
			{[]string{"-o", "main.cc"}, false},
			{[]string{}, false},
		}
		for _, tt := range testData {
			ctx.cmdCount = 0
			ctx.must(callCompiler(ctx, ctx.cfg,
				ctx.newCommand(clangX86_64, tt.args...)))
			if ctx.cmdCount > 1 && !tt.clangTidy {
				t.Errorf("expected a call to clang tidy but got none for args %s", tt.args)
			}
			if ctx.cmdCount == 1 && tt.clangTidy {
				t.Errorf("expected no call to clang tidy but got one for args %s", tt.args)
			}
		}
	})
}

func TestOmitCCacheWithClangTidy(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		ctx.cfg.useCCache = true

		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				if err := verifyPath(cmd, "usr/bin/clang"); err != nil {
					t.Error(err)
				}
				return nil
			case 2:
				if err := verifyPath(cmd, "usr/bin/clang-tidy"); err != nil {
					return err
				}
				return nil
			default:
				return nil
			}
		}
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, mainCc)))
		if ctx.cmdCount != 3 {
			t.Errorf("expected 3 calls. Got: %d", ctx.cmdCount)
		}
		if err := verifyPath(cmd, "usr/bin/clang"); err != nil {
			t.Error(err)
		}
	})
}

func TestPartiallyOmitGomaWithClangTidy(t *testing.T) {
	withClangTidyTestContext(t, func(ctx *testContext) {
		gomaPath := path.Join(ctx.tempDir, "gomacc")
		// Create a file so the gomacc path is valid.
		ctx.writeFile(gomaPath, "")
		ctx.env = append(ctx.env, "GOMACC_PATH="+gomaPath)

		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				if err := verifyPath(cmd, "usr/bin/clang"); err != nil {
					t.Error(err)
				}
				return nil
			case 2:
				if err := verifyPath(cmd, "usr/bin/clang-tidy"); err != nil {
					return err
				}
				return nil
			default:
				return nil
			}
		}
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, mainCc)))
		if ctx.cmdCount != 3 {
			t.Errorf("expected 3 calls. Got: %d", ctx.cmdCount)
		}
		if err := verifyPath(cmd, gomaPath); err != nil {
			t.Error(err)
		}
	})
}

func withClangTidyTestContext(t *testing.T, work func(ctx *testContext)) {
	withTestContext(t, func(ctx *testContext) {
		ctx.env = []string{"WITH_TIDY=1"}
		work(ctx)
	})
}
