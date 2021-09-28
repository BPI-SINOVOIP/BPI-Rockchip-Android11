// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"strings"
)

type env interface {
	getenv(key string) (string, bool)
	environ() []string
	getwd() string
	stdin() io.Reader
	stdout() io.Writer
	stderr() io.Writer
	run(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error
	exec(cmd *command) error
}

type processEnv struct {
	wd string
}

func newProcessEnv() (env, error) {
	// Note: We are not using os.getwd() as this sometimes uses the value of the PWD
	// env variable. This has the following problems:
	// - if PWD=/proc/self/cwd, os.getwd() will return "/proc/self/cwd",
	//   and we need to read the link to get the actual wd. However, we can't always
	//   do this as we are calculating
	//   the path to clang, and following a symlinked cwd first would make
	//   this calculation invalid.
	// - the old python wrapper doesn't respect the PWD env variable either, so if we
	//   did we would fail the comparison to the old wrapper.
	wd, err := os.Readlink("/proc/self/cwd")
	if err != nil {
		return nil, wrapErrorwithSourceLocf(err, "failed to read working directory")
	}
	return &processEnv{wd: wd}, nil
}

var _ env = (*processEnv)(nil)

func (env *processEnv) getenv(key string) (string, bool) {
	return os.LookupEnv(key)
}

func (env *processEnv) environ() []string {
	return os.Environ()
}

func (env *processEnv) getwd() string {
	return env.wd
}

func (env *processEnv) stdin() io.Reader {
	return os.Stdin
}

func (env *processEnv) stdout() io.Writer {
	return os.Stdout
}

func (env *processEnv) stderr() io.Writer {
	return os.Stderr
}

func (env *processEnv) exec(cmd *command) error {
	return execCmd(env, cmd)
}

func (env *processEnv) run(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
	return runCmd(env, cmd, stdin, stdout, stderr)
}

type commandRecordingEnv struct {
	env
	stdinReader io.Reader
	cmdResults  []*commandResult
}
type commandResult struct {
	Cmd      *command `json:"cmd"`
	Stdout   string   `json:"stdout,omitempty"`
	Stderr   string   `json:"stderr,omitempty"`
	ExitCode int      `json:"exitcode,omitempty"`
}

var _ env = (*commandRecordingEnv)(nil)

func (env *commandRecordingEnv) stdin() io.Reader {
	return env.stdinReader
}

func (env *commandRecordingEnv) exec(cmd *command) error {
	// Note: We treat exec the same as run so that we can do work
	// after the call.
	return env.run(cmd, env.stdin(), env.stdout(), env.stderr())
}

func (env *commandRecordingEnv) run(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
	stdoutBuffer := &bytes.Buffer{}
	stderrBuffer := &bytes.Buffer{}
	err := env.env.run(cmd, stdin, io.MultiWriter(stdout, stdoutBuffer), io.MultiWriter(stderr, stderrBuffer))
	if exitCode, ok := getExitCode(err); ok {
		env.cmdResults = append(env.cmdResults, &commandResult{
			Cmd:      cmd,
			Stdout:   stdoutBuffer.String(),
			Stderr:   stderrBuffer.String(),
			ExitCode: exitCode,
		})
	}
	return err
}

type printingEnv struct {
	env
}

var _env = (*printingEnv)(nil)

func (env *printingEnv) exec(cmd *command) error {
	printCmd(env, cmd)
	return env.env.exec(cmd)
}

func (env *printingEnv) run(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
	printCmd(env, cmd)
	return env.env.run(cmd, stdin, stdout, stderr)
}

func printCmd(env env, cmd *command) {
	fmt.Fprintf(env.stderr(), "cd '%s' &&", env.getwd())
	if len(cmd.EnvUpdates) > 0 {
		fmt.Fprintf(env.stderr(), " env '%s'", strings.Join(cmd.EnvUpdates, "' '"))
	}
	fmt.Fprintf(env.stderr(), " '%s'", getAbsCmdPath(env, cmd))
	if len(cmd.Args) > 0 {
		fmt.Fprintf(env.stderr(), " '%s'", strings.Join(cmd.Args, "' '"))
	}
	io.WriteString(env.stderr(), "\n")
}
