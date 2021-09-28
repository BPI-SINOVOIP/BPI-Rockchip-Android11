// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// +build !libc_exec

package main

import (
	"os/exec"
	"syscall"
)

// Implement exec for users that don't need to dynamically link with glibc
// See b/144783188 and libc_exec.go.

func execCmd(env env, cmd *command) error {
	execCmd := exec.Command(cmd.Path, cmd.Args...)
	mergedEnv := mergeEnvValues(env.environ(), cmd.EnvUpdates)

	ret := syscall.Exec(execCmd.Path, execCmd.Args, mergedEnv)
	return newErrorwithSourceLocf("exec error: %v", ret)
}
