// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"strings"
	"syscall"
	"testing"
)

func TestAddCommonFlags(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.cfg.commonFlags = []string{"-someflag"}
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyArgOrder(cmd, "-someflag", mainCc); err != nil {
			t.Error(err)
		}
	})
}

func TestAddGccConfigFlags(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.cfg.gccFlags = []string{"-someflag"}
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyArgOrder(cmd, "-someflag", mainCc); err != nil {
			t.Error(err)
		}
	})
}

func TestAddClangConfigFlags(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.cfg.clangFlags = []string{"-someflag"}
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, mainCc)))
		if err := verifyArgOrder(cmd, "-someflag", mainCc); err != nil {
			t.Error(err)
		}
	})
}

func TestLogGeneralExecError(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			return errors.New("someerror")
		}
		stderr := ctx.mustFail(callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyInternalError(stderr); err != nil {
			t.Fatal(err)
		}
		if !strings.Contains(stderr, gccX86_64) {
			t.Errorf("could not find compiler path on stderr. Got: %s", stderr)
		}
		if !strings.Contains(stderr, "someerror") {
			t.Errorf("could not find original error on stderr. Got: %s", stderr)
		}
	})
}

func TestForwardStdin(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		io.WriteString(&ctx.stdinBuffer, "someinput")
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			stdinStr := ctx.readAllString(stdin)
			if stdinStr != "someinput" {
				return fmt.Errorf("unexpected stdin. Got: %s", stdinStr)
			}
			return nil
		}
		ctx.must(callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, "-", mainCc)))
	})
}

func TestLogMissingCCacheExecError(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.cfg.useCCache = true

		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			return syscall.ENOENT
		}
		ctx.stderrBuffer.Reset()
		stderr := ctx.mustFail(callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyNonInternalError(stderr, "ccache not found under .*. Please install it"); err != nil {
			t.Fatal(err)
		}
	})
}

func TestErrorOnLogRusageAndForceDisableWError(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.env = []string{
			"FORCE_DISABLE_WERROR=1",
			"GETRUSAGE=rusage.log",
		}
		stderr := ctx.mustFail(callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyNonInternalError(stderr, "GETRUSAGE is meaningless with FORCE_DISABLE_WERROR"); err != nil {
			t.Error(err)
		}
	})
}

func TestErrorOnLogRusageAndBisect(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.env = []string{
			"BISECT_STAGE=xyz",
			"GETRUSAGE=rusage.log",
		}
		stderr := ctx.mustFail(callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyNonInternalError(stderr, "BISECT_STAGE is meaningless with GETRUSAGE"); err != nil {
			t.Error(err)
		}
	})
}

func TestErrorOnBisectAndForceDisableWError(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.env = []string{
			"BISECT_STAGE=xyz",
			"FORCE_DISABLE_WERROR=1",
		}
		stderr := ctx.mustFail(callCompiler(ctx, ctx.cfg, ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyNonInternalError(stderr, "BISECT_STAGE is meaningless with FORCE_DISABLE_WERROR"); err != nil {
			t.Error(err)
		}
	})
}

func TestPrintUserCompilerError(t *testing.T) {
	buffer := bytes.Buffer{}
	printCompilerError(&buffer, newUserErrorf("abcd"))
	if buffer.String() != "abcd\n" {
		t.Errorf("Unexpected string. Got: %s", buffer.String())
	}
}

func TestPrintOtherCompilerError(t *testing.T) {
	buffer := bytes.Buffer{}
	printCompilerError(&buffer, errors.New("abcd"))
	if buffer.String() != "Internal error. Please report to chromeos-toolchain@google.com.\nabcd\n" {
		t.Errorf("Unexpected string. Got: %s", buffer.String())
	}
}
