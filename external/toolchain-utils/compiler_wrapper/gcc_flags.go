// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

func processGccFlags(builder *commandBuilder) {
	if !builder.cfg.isHostWrapper {
		// Flags not supported by GCC.
		unsupported := map[string]bool{"-Xcompiler": true}

		// Conversion for flags supported by clang but not gcc.
		clangToGcc := map[string]string{
			"-march=goldmont":      "-march=silvermont",
			"-march=goldmont-plus": "-march=silvermont",
			"-march=skylake":       "-march=corei7",
			"-march=tremont":       "-march=silvermont",
		}

		builder.transformArgs(func(arg builderArg) string {
			if unsupported[arg.value] {
				return ""
			}
			if mapped, ok := clangToGcc[arg.value]; ok {
				return mapped
			}
			return arg.value
		})
	}

	builder.path += ".real"
}
