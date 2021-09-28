// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"os"
)

func processGomaCccFlags(builder *commandBuilder) (gomaUsed bool, err error) {
	gomaPath := ""
	nextArgIsGomaPath := false
	builder.transformArgs(func(arg builderArg) string {
		if arg.fromUser {
			if arg.value == "--gomacc-path" {
				nextArgIsGomaPath = true
				return ""
			}
			if nextArgIsGomaPath {
				gomaPath = arg.value
				nextArgIsGomaPath = false
				return ""
			}
		}
		return arg.value
	})
	if nextArgIsGomaPath {
		return false, newUserErrorf("--gomacc-path given without value")
	}
	if gomaPath == "" {
		gomaPath, _ = builder.env.getenv("GOMACC_PATH")
	}
	if gomaPath != "" {
		if _, err := os.Lstat(gomaPath); err == nil {
			builder.wrapPath(gomaPath)
			return true, nil
		}
	}
	return false, nil
}
