// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"syscall"
	"time"
)

func getRusageLogFilename(env env) string {
	value, _ := env.getenv("GETRUSAGE")
	return value
}

func logRusage(env env, logFileName string, compilerCmd *command) (exitCode int, err error) {
	rusageBefore := syscall.Rusage{}
	if err := syscall.Getrusage(syscall.RUSAGE_CHILDREN, &rusageBefore); err != nil {
		return 0, err
	}
	compilerCmdWithoutRusage := &command{
		Path:       compilerCmd.Path,
		Args:       compilerCmd.Args,
		EnvUpdates: append(compilerCmd.EnvUpdates, "GETRUSAGE="),
	}
	startTime := time.Now()
	exitCode, err = wrapSubprocessErrorWithSourceLoc(compilerCmdWithoutRusage,
		env.run(compilerCmdWithoutRusage, env.stdin(), env.stdout(), env.stderr()))
	if err != nil {
		return 0, err
	}
	elapsedRealTime := time.Since(startTime)
	rusageAfter := syscall.Rusage{}
	if err := syscall.Getrusage(syscall.RUSAGE_CHILDREN, &rusageAfter); err != nil {
		return 0, err
	}
	elapsedSysTime := time.Duration(rusageAfter.Stime.Nano()-rusageBefore.Stime.Nano()) * time.Nanosecond
	elapsedUserTime := time.Duration(rusageAfter.Utime.Nano()-rusageBefore.Utime.Nano()) * time.Nanosecond
	// Note: We assume that the compiler takes more heap than any other
	// subcommands that we might have executed before.
	maxMemUsed := rusageAfter.Maxrss
	absCompilerPath := getAbsCmdPath(env, compilerCmd)

	if err := os.MkdirAll(filepath.Dir(logFileName), 0777); err != nil {
		return 0, wrapErrorwithSourceLocf(err, "error creating rusage log directory %s", logFileName)
	}
	// Note: using file mode 0666 so that a root-created log is writable by others.
	logFile, err := os.OpenFile(logFileName, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0666)
	if err != nil {
		return 0, wrapErrorwithSourceLocf(err, "error creating rusage logfile %s", logFileName)
	}
	timeUnit := float64(time.Second)
	if _, err := fmt.Fprintf(logFile, "%.5f : %.5f : %.5f : %d : %s : %s\n",
		float64(elapsedRealTime)/timeUnit, float64(elapsedUserTime)/timeUnit, float64(elapsedSysTime)/timeUnit,
		maxMemUsed, absCompilerPath,
		strings.Join(append([]string{filepath.Base(absCompilerPath)}, compilerCmd.Args...), " ")); err != nil {
		_ = logFile.Close()
		return 0, wrapErrorwithSourceLocf(err, "error writing rusage logfile %s", logFileName)
	}
	if err := logFile.Close(); err != nil {
		return 0, wrapErrorwithSourceLocf(err, "error closing rusage logfile %s", logFileName)
	}

	return exitCode, nil
}
