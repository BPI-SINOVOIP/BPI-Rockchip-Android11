// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"testing"
)

func TestAddPieFlags(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		initPieConfig(ctx.cfg)
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, mainCc)))
		if err := verifyArgOrder(cmd, "-pie", mainCc); err != nil {
			t.Error(err)
		}
		if err := verifyArgOrder(cmd, "-fPIE", mainCc); err != nil {
			t.Error(err)
		}
	})
}

func TestOmitPieFlagsWhenNoPieArgGiven(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		initPieConfig(ctx.cfg)
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-nopie", mainCc)))
		if err := verifyArgCount(cmd, 0, "-nopie"); err != nil {
			t.Error(err)
		}
		if err := verifyArgCount(cmd, 0, "-pie"); err != nil {
			t.Error(err)
		}
		if err := verifyArgCount(cmd, 0, "-fPIE"); err != nil {
			t.Error(err)
		}

		cmd = ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-fno-pie", mainCc)))
		if err := verifyArgCount(cmd, 0, "-pie"); err != nil {
			t.Error(err)
		}
		if err := verifyArgCount(cmd, 0, "-fPIE"); err != nil {
			t.Error(err)
		}
	})
}

func TestOmitPieFlagsWhenKernelDefined(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		initPieConfig(ctx.cfg)
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64, "-D__KERNEL__", mainCc)))
		if err := verifyArgCount(cmd, 0, "-pie"); err != nil {
			t.Error(err)
		}
		if err := verifyArgCount(cmd, 0, "-fPIE"); err != nil {
			t.Error(err)
		}
	})
}

func TestAddPieFlagsForEabiEvenIfNoPieGiven(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		initPieConfig(ctx.cfg)
		cmd := ctx.must(callCompiler(ctx, ctx.cfg,
			ctx.newCommand(gccX86_64Eabi, "-nopie", mainCc)))
		if err := verifyArgCount(cmd, 0, "-nopie"); err != nil {
			t.Error(err)
		}
		if err := verifyArgCount(cmd, 1, "-pie"); err != nil {
			t.Error(err)
		}
		if err := verifyArgCount(cmd, 1, "-fPIE"); err != nil {
			t.Error(err)
		}
	})
}

func initPieConfig(cfg *config) {
	cfg.commonFlags = []string{"-fPIE", "-pie"}
}
