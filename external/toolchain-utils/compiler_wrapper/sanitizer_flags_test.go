// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"testing"
)

func TestFilterUnsupportedSanitizerFlagsIfSanitizeGiven(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-fsanitize=kernel-address", "-Wl,--no-undefined", mainCc)))
		if err := verifyArgCount(cmd, 0, "-Wl,--no-undefined"); err != nil {
			t.Error(err)
		}

		cmd = ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-fsanitize=kernel-address", "-Wl,-z,defs", mainCc)))
		if err := verifyArgCount(cmd, 0, "-Wl,-z,defs"); err != nil {
			t.Error(err)
		}

		cmd = ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-fsanitize=kernel-address", "-D_FORTIFY_SOURCE=1", mainCc)))
		if err := verifyArgCount(cmd, 0, "-D_FORTIFY_SOURCE=1"); err != nil {
			t.Error(err)
		}

		cmd = ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-fsanitize=kernel-address", "-D_FORTIFY_SOURCE=2", mainCc)))
		if err := verifyArgCount(cmd, 0, "-D_FORTIFY_SOURCE=2"); err != nil {
			t.Error(err)
		}
	})
}

func TestFilterUnsupportedDefaultSanitizerFlagsIfSanitizeGivenForClang(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.cfg.commonFlags = []string{"-D_FORTIFY_SOURCE=1"}
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, "-fsanitize=kernel-address", mainCc)))
		if err := verifyArgCount(cmd, 0, "-D_FORTIFY_SOURCE=1"); err != nil {
			t.Error(err)
		}
	})
}

func TestKeepUnsupportedDefaultSanitizerFlagsIfSanitizeGivenForGcc(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.cfg.commonFlags = []string{"-D_FORTIFY_SOURCE=1"}
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-fsanitize=kernel-address", mainCc)))
		if err := verifyArgCount(cmd, 1, "-D_FORTIFY_SOURCE=1"); err != nil {
			t.Error(err)
		}
	})
}

// TODO: This is a bug in the old wrapper to not filter
// non user args for gcc. Fix this once we don't compare to the old wrapper anymore.
func TestKeepSanitizerFlagsIfNoSanitizeGiven(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-Wl,--no-undefined", mainCc)))
		if err := verifyArgCount(cmd, 1, "-Wl,--no-undefined"); err != nil {
			t.Error(err)
		}

		cmd = ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-Wl,-z,defs", mainCc)))
		if err := verifyArgCount(cmd, 1, "-Wl,-z,defs"); err != nil {
			t.Error(err)
		}

		cmd = ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-D_FORTIFY_SOURCE=1", mainCc)))
		if err := verifyArgCount(cmd, 1, "-D_FORTIFY_SOURCE=1"); err != nil {
			t.Error(err)
		}

		cmd = ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-D_FORTIFY_SOURCE=2", mainCc)))
		if err := verifyArgCount(cmd, 1, "-D_FORTIFY_SOURCE=2"); err != nil {
			t.Error(err)
		}
	})
}

func TestKeepSanitizerFlagsIfSanitizeGivenInCommonFlags(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		ctx.cfg.commonFlags = []string{"-fsanitize=kernel-address"}
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-Wl,--no-undefined", mainCc)))
		if err := verifyArgCount(cmd, 1, "-Wl,--no-undefined"); err != nil {
			t.Error(err)
		}
	})
}

func TestAddFuzzerFlagsForClang(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, "-fsanitize=fuzzer", mainCc)))
		if err := verifyArgOrder(cmd, "-fno-experimental-new-pass-manager",
			"-fsanitize=fuzzer", mainCc); err != nil {
			t.Error(err)
		}
	})
}

func TestOmitFuzzerFlagsForGcc(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-fsanitize=fuzzer", mainCc)))
		if err := verifyArgCount(cmd, 0, "-fno-experimental-new-pass-manager"); err != nil {
			t.Error(err)
		}
	})
}

func TestAddSanitizerCoverageFlagsForClang(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, "-fsanitize=address", "-fprofile-instr-generate", mainCc)))
		if err := verifyArgOrder(cmd, "-fno-experimental-new-pass-manager",
			"-fsanitize=address", "-fprofile-instr-generate", mainCc); err != nil {
			t.Error(err)
		}
	})
}

func TestOmitSanitizerCoverageFlagsForClang(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, "-fsanitize=address", mainCc)))
		if err := verifyArgCount(cmd, 0, "-fno-experimental-new-pass-manager"); err != nil {
			t.Error(err)
		}
	})
}

func TestKeepSanitizerCoverageFlagsForClang(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(clangX86_64, "-fprofile-instr-generate", mainCc)))
		if err := verifyArgCount(cmd, 0, "-fno-experimental-new-pass-manager"); err != nil {
			t.Error(err)
		}
	})
}
