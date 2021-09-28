// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

type command struct {
	Path string   `json:"path"`
	Args []string `json:"args"`
	// Updates and additions have the form:
	// `NAME=VALUE`
	// Removals have the form:
	// `NAME=`.
	EnvUpdates []string `json:"env_updates,omitempty"`
}

func newProcessCommand() *command {
	// This is a workaround for the fact that ld.so does not support
	// passing in the executable name when ld.so is invoked as
	// an executable (crbug/1003841).
	path := os.Getenv("LD_ARGV0")
	if path == "" {
		path = os.Args[0]
	}
	return &command{
		Path: path,
		Args: os.Args[1:],
	}
}

func mergeEnvValues(values []string, updates []string) []string {
	envMap := map[string]string{}
	for _, entry := range values {
		equalPos := strings.IndexRune(entry, '=')
		envMap[entry[:equalPos]] = entry[equalPos+1:]
	}
	for _, update := range updates {
		equalPos := strings.IndexRune(update, '=')
		key := update[:equalPos]
		value := update[equalPos+1:]
		if value == "" {
			delete(envMap, key)
		} else {
			envMap[key] = value
		}
	}
	env := []string{}
	for key, value := range envMap {
		env = append(env, key+"="+value)
	}
	return env
}

func runCmd(env env, cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
	execCmd := exec.Command(cmd.Path, cmd.Args...)
	execCmd.Env = mergeEnvValues(env.environ(), cmd.EnvUpdates)
	execCmd.Dir = env.getwd()
	execCmd.Stdin = stdin
	execCmd.Stdout = stdout
	execCmd.Stderr = stderr
	return execCmd.Run()
}

func resolveAgainstPathEnv(env env, cmd string) (string, error) {
	path, _ := env.getenv("PATH")
	for _, path := range strings.Split(path, ":") {
		resolvedPath := filepath.Join(path, cmd)
		if _, err := os.Lstat(resolvedPath); err == nil {
			return resolvedPath, nil
		}
	}
	return "", fmt.Errorf("Couldn't find cmd %q in path", cmd)
}

func getAbsCmdPath(env env, cmd *command) string {
	path := cmd.Path
	if !filepath.IsAbs(path) {
		path = filepath.Join(env.getwd(), path)
	}
	return path
}

func newCommandBuilder(env env, cfg *config, cmd *command) (*commandBuilder, error) {
	basename := filepath.Base(cmd.Path)
	var nameParts []string
	if basename == "clang-tidy" {
		nameParts = []string{basename}
	} else {
		nameParts = strings.Split(basename, "-")
	}
	target := builderTarget{}
	switch len(nameParts) {
	case 1:
		// E.g. gcc
		target = builderTarget{
			compiler: nameParts[0],
		}
	case 4:
		// E.g. armv7m-cros-eabi-gcc
		target = builderTarget{
			arch:     nameParts[0],
			vendor:   nameParts[1],
			abi:      nameParts[2],
			compiler: nameParts[3],
			target:   basename[:strings.LastIndex(basename, "-")],
		}
	case 5:
		// E.g. x86_64-cros-linux-gnu-gcc
		target = builderTarget{
			arch:     nameParts[0],
			vendor:   nameParts[1],
			sys:      nameParts[2],
			abi:      nameParts[3],
			compiler: nameParts[4],
			target:   basename[:strings.LastIndex(basename, "-")],
		}
	default:
		return nil, newErrorwithSourceLocf("unexpected compiler name pattern. Actual: %s", basename)
	}

	var compilerType compilerType
	switch {
	case strings.HasPrefix(target.compiler, "clang-tidy"):
		compilerType = clangTidyType
	case strings.HasPrefix(target.compiler, "clang"):
		compilerType = clangType
	default:
		compilerType = gccType
	}
	target.compilerType = compilerType
	absWrapperPath, err := getAbsWrapperPath(env, cmd)
	if err != nil {
		return nil, err
	}
	rootPath := filepath.Join(filepath.Dir(absWrapperPath), cfg.rootRelPath)
	return &commandBuilder{
		path:           cmd.Path,
		args:           createBuilderArgs( /*fromUser=*/ true, cmd.Args),
		env:            env,
		cfg:            cfg,
		rootPath:       rootPath,
		absWrapperPath: absWrapperPath,
		target:         target,
	}, nil
}

type commandBuilder struct {
	path           string
	target         builderTarget
	args           []builderArg
	envUpdates     []string
	env            env
	cfg            *config
	rootPath       string
	absWrapperPath string
}

type builderArg struct {
	value    string
	fromUser bool
}

type compilerType int32

const (
	gccType compilerType = iota
	clangType
	clangTidyType
)

type builderTarget struct {
	target       string
	arch         string
	vendor       string
	sys          string
	abi          string
	compiler     string
	compilerType compilerType
}

func createBuilderArgs(fromUser bool, args []string) []builderArg {
	builderArgs := make([]builderArg, len(args))
	for i, arg := range args {
		builderArgs[i] = builderArg{value: arg, fromUser: fromUser}
	}
	return builderArgs
}

func (builder *commandBuilder) clone() *commandBuilder {
	return &commandBuilder{
		path:           builder.path,
		args:           append([]builderArg{}, builder.args...),
		env:            builder.env,
		cfg:            builder.cfg,
		rootPath:       builder.rootPath,
		target:         builder.target,
		absWrapperPath: builder.absWrapperPath,
	}
}

func (builder *commandBuilder) wrapPath(path string) {
	builder.args = append([]builderArg{{value: builder.path, fromUser: false}}, builder.args...)
	builder.path = path
}

func (builder *commandBuilder) addPreUserArgs(args ...string) {
	index := 0
	for _, arg := range builder.args {
		if arg.fromUser {
			break
		}
		index++
	}
	builder.args = append(builder.args[:index], append(createBuilderArgs( /*fromUser=*/ false, args), builder.args[index:]...)...)
}

func (builder *commandBuilder) addPostUserArgs(args ...string) {
	builder.args = append(builder.args, createBuilderArgs( /*fromUser=*/ false, args)...)
}

// Allows to map and filter arguments. Filters when the callback returns an empty string.
func (builder *commandBuilder) transformArgs(transform func(arg builderArg) string) {
	// See https://github.com/golang/go/wiki/SliceTricks
	newArgs := builder.args[:0]
	for _, arg := range builder.args {
		newArg := transform(arg)
		if newArg != "" {
			newArgs = append(newArgs, builderArg{
				value:    newArg,
				fromUser: arg.fromUser,
			})
		}
	}
	builder.args = newArgs
}

func (builder *commandBuilder) updateEnv(updates ...string) {
	builder.envUpdates = append(builder.envUpdates, updates...)
}

func (builder *commandBuilder) build() *command {
	cmdArgs := make([]string, len(builder.args))
	for i, builderArg := range builder.args {
		cmdArgs[i] = builderArg.value
	}
	return &command{
		Path:       builder.path,
		Args:       cmdArgs,
		EnvUpdates: builder.envUpdates,
	}
}
