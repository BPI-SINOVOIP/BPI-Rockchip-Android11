// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"bytes"
	"flag"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

// Attention: The tests in this file execute the test binary again with the `-run` flag.
// This is needed as they want to test an `exec`, which terminates the test process.
var internalexececho = flag.Bool("internalexececho", false, "internal flag used for tests that exec")

func TestProcessEnvExecPathAndArgs(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		if *internalexececho {
			execEcho(ctx, &command{
				Path: "some_binary",
				Args: []string{"arg1", "arg2"},
			})
			return
		}
		logLines := forkAndReadEcho(ctx)
		if !strings.HasSuffix(logLines[0], "/some_binary arg1 arg2") {
			t.Errorf("incorrect path or args: %s", logLines[0])
		}
	})
}

func TestProcessEnvExecAddEnv(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		if *internalexececho {
			execEcho(ctx, &command{
				Path:       "some_binary",
				EnvUpdates: []string{"ABC=xyz"},
			})
			return
		}

		logLines := forkAndReadEcho(ctx)
		for _, ll := range logLines {
			if ll == "ABC=xyz" {
				return
			}
		}
		t.Errorf("could not find new env variable: %s", logLines)
	})
}

func TestProcessEnvExecUpdateEnv(t *testing.T) {
	if os.Getenv("PATH") == "" {
		t.Fatal("no PATH environment variable found!")
	}
	withTestContext(t, func(ctx *testContext) {
		if *internalexececho {
			execEcho(ctx, &command{
				Path:       "some_binary",
				EnvUpdates: []string{"PATH=xyz"},
			})
			return
		}
		logLines := forkAndReadEcho(ctx)
		for _, ll := range logLines {
			if ll == "PATH=xyz" {
				return
			}
		}
		t.Errorf("could not find updated env variable: %s", logLines)
	})
}

func TestProcessEnvExecDeleteEnv(t *testing.T) {
	if os.Getenv("PATH") == "" {
		t.Fatal("no PATH environment variable found!")
	}
	withTestContext(t, func(ctx *testContext) {
		if *internalexececho {
			execEcho(ctx, &command{
				Path:       "some_binary",
				EnvUpdates: []string{"PATH="},
			})
			return
		}
		logLines := forkAndReadEcho(ctx)
		for _, ll := range logLines {
			if strings.HasPrefix(ll, "PATH=") {
				t.Errorf("path env was not removed: %s", ll)
			}
		}
	})
}

func TestProcessEnvRunCmdPathAndArgs(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		cmd := &command{
			Path: "some_binary",
			Args: []string{"arg1", "arg2"},
		}
		logLines := runAndEcho(ctx, cmd)
		if !strings.HasSuffix(logLines[0], "/some_binary arg1 arg2") {
			t.Errorf("incorrect path or args: %s", logLines[0])
		}
	})
}

func TestProcessEnvRunCmdAddEnv(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		cmd := &command{
			Path:       "some_binary",
			EnvUpdates: []string{"ABC=xyz"},
		}
		logLines := runAndEcho(ctx, cmd)
		for _, ll := range logLines {
			if ll == "ABC=xyz" {
				return
			}
		}
		t.Errorf("could not find new env variable: %s", logLines)
	})
}

func TestProcessEnvRunCmdUpdateEnv(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		if os.Getenv("PATH") == "" {
			t.Fatal("no PATH environment variable found!")
		}
		cmd := &command{
			Path:       "some_binary",
			EnvUpdates: []string{"PATH=xyz"},
		}
		logLines := runAndEcho(ctx, cmd)
		for _, ll := range logLines {
			if ll == "PATH=xyz" {
				return
			}
		}
		t.Errorf("could not find updated env variable: %s", logLines)
	})
}

func TestProcessEnvRunCmdDeleteEnv(t *testing.T) {
	withTestContext(t, func(ctx *testContext) {
		if os.Getenv("PATH") == "" {
			t.Fatal("no PATH environment variable found!")
		}
		cmd := &command{
			Path:       "some_binary",
			EnvUpdates: []string{"PATH="},
		}
		logLines := runAndEcho(ctx, cmd)
		for _, ll := range logLines {
			if strings.HasPrefix(ll, "PATH=") {
				t.Errorf("path env was not removed: %s", ll)
			}
		}
	})
}

func execEcho(ctx *testContext, cmd *command) {
	env := &processEnv{}
	err := env.exec(createEcho(ctx, cmd))
	if err != nil {
		os.Stderr.WriteString(err.Error())
	}
	os.Exit(1)
}

func forkAndReadEcho(ctx *testContext) []string {
	testBin, err := os.Executable()
	if err != nil {
		ctx.t.Fatalf("unable to read the executable: %s", err)
	}

	subCmd := exec.Command(testBin, "-internalexececho", "-test.run="+ctx.t.Name())
	output, err := subCmd.CombinedOutput()
	if err != nil {
		ctx.t.Fatalf("error calling test binary again for exec: %s", err)
	}
	return strings.Split(string(output), "\n")
}

func runAndEcho(ctx *testContext, cmd *command) []string {
	env, err := newProcessEnv()
	if err != nil {
		ctx.t.Fatalf("creation of process env failed: %s", err)
	}
	buffer := bytes.Buffer{}
	if err := env.run(createEcho(ctx, cmd), nil, &buffer, &buffer); err != nil {
		ctx.t.Fatalf("run failed: %s", err)
	}
	return strings.Split(buffer.String(), "\n")
}

func createEcho(ctx *testContext, cmd *command) *command {
	content := `
/bin/echo "$0" "$@"
/usr/bin/env
`
	fullPath := filepath.Join(ctx.tempDir, cmd.Path)
	ctx.writeFile(fullPath, content)
	// Note: Using a self executable wrapper does not work due to a race condition
	// on unix systems. See https://github.com/golang/go/issues/22315
	return &command{
		Path:       "bash",
		Args:       append([]string{fullPath}, cmd.Args...),
		EnvUpdates: cmd.EnvUpdates,
	}
}
