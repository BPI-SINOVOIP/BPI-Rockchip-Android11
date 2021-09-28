// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"io"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"regexp"
	"strings"
)

var updateGoldenFiles = flag.Bool("updategolden", false, "update golden files")
var filterGoldenTests = flag.String("rungolden", "", "regex filter for golden tests to run")

type goldenFile struct {
	Name    string         `json:"name"`
	Records []goldenRecord `json:"records"`
}

type goldenRecord struct {
	Wd  string   `json:"wd"`
	Env []string `json:"env,omitempty"`
	// runGoldenRecords will read cmd and fill
	// stdout, stderr, exitCode.
	WrapperCmd commandResult `json:"wrapper"`
	// runGoldenRecords will read stdout, stderr, err
	// and fill cmd
	Cmds []commandResult `json:"cmds"`
}

func newGoldenCmd(path string, args ...string) commandResult {
	return commandResult{
		Cmd: &command{
			Path: path,
			Args: args,
		},
	}
}

var okResult = commandResult{}
var okResults = []commandResult{okResult}
var errorResult = commandResult{
	ExitCode: 1,
	Stderr:   "someerror",
	Stdout:   "somemessage",
}
var errorResults = []commandResult{errorResult}

func runGoldenRecords(ctx *testContext, goldenDir string, files []goldenFile) {
	if filterPattern := *filterGoldenTests; filterPattern != "" {
		files = filterGoldenRecords(filterPattern, files)
	}
	if len(files) == 0 {
		ctx.t.Errorf("No goldenrecords given.")
		return
	}
	files = fillGoldenResults(ctx, files)
	if *updateGoldenFiles {
		log.Printf("updating golden files under %s", goldenDir)
		if err := os.MkdirAll(goldenDir, 0777); err != nil {
			ctx.t.Fatal(err)
		}
		for _, file := range files {
			fileHandle, err := os.Create(filepath.Join(goldenDir, file.Name))
			if err != nil {
				ctx.t.Fatal(err)
			}
			defer fileHandle.Close()

			writeGoldenRecords(ctx, fileHandle, file.Records)
		}
	} else {
		for _, file := range files {
			compareBuffer := &bytes.Buffer{}
			writeGoldenRecords(ctx, compareBuffer, file.Records)
			filePath := filepath.Join(goldenDir, file.Name)
			goldenFileData, err := ioutil.ReadFile(filePath)
			if err != nil {
				ctx.t.Error(err)
				continue
			}
			if !bytes.Equal(compareBuffer.Bytes(), goldenFileData) {
				ctx.t.Errorf("Commands don't match the golden file under %s. Please regenerate via -updategolden to check the differences.",
					filePath)
			}
		}
	}
}

func filterGoldenRecords(pattern string, files []goldenFile) []goldenFile {
	matcher := regexp.MustCompile(pattern)
	newFiles := []goldenFile{}
	for _, file := range files {
		newRecords := []goldenRecord{}
		for _, record := range file.Records {
			cmd := record.WrapperCmd.Cmd
			str := strings.Join(append(append(record.Env, cmd.Path), cmd.Args...), " ")
			if matcher.MatchString(str) {
				newRecords = append(newRecords, record)
			}
		}
		file.Records = newRecords
		newFiles = append(newFiles, file)
	}
	return newFiles
}

func fillGoldenResults(ctx *testContext, files []goldenFile) []goldenFile {
	newFiles := []goldenFile{}
	for _, file := range files {
		newRecords := []goldenRecord{}
		for _, record := range file.Records {
			newCmds := []commandResult{}
			ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
				if len(newCmds) >= len(record.Cmds) {
					ctx.t.Errorf("Not enough commands specified for wrapperCmd %#v and env %#v. Expected: %#v",
						record.WrapperCmd.Cmd, record.Env, record.Cmds)
					return nil
				}
				cmdResult := record.Cmds[len(newCmds)]
				cmdResult.Cmd = cmd
				if numEnvUpdates := len(cmdResult.Cmd.EnvUpdates); numEnvUpdates > 0 {
					if strings.HasPrefix(cmdResult.Cmd.EnvUpdates[numEnvUpdates-1], "PYTHONPATH") {
						cmdResult.Cmd.EnvUpdates[numEnvUpdates-1] = "PYTHONPATH=/somepath/test_binary"
					}
				}
				newCmds = append(newCmds, cmdResult)
				io.WriteString(stdout, cmdResult.Stdout)
				io.WriteString(stderr, cmdResult.Stderr)
				if cmdResult.ExitCode != 0 {
					return newExitCodeError(cmdResult.ExitCode)
				}
				return nil
			}
			ctx.stdoutBuffer.Reset()
			ctx.stderrBuffer.Reset()
			ctx.env = record.Env
			if record.Wd == "" {
				record.Wd = ctx.tempDir
			}
			ctx.wd = record.Wd
			// Create an empty wrapper at the given path.
			// Needed as we are resolving symlinks which stats the wrapper file.
			ctx.writeFile(record.WrapperCmd.Cmd.Path, "")
			record.WrapperCmd.ExitCode = callCompiler(ctx, ctx.cfg, record.WrapperCmd.Cmd)
			if hasInternalError(ctx.stderrString()) {
				ctx.t.Errorf("found an internal error for wrapperCmd %#v and env #%v. Got: %s",
					record.WrapperCmd.Cmd, record.Env, ctx.stderrString())
			}
			if len(newCmds) < len(record.Cmds) {
				ctx.t.Errorf("Too many commands specified for wrapperCmd %#v and env %#v. Expected: %#v",
					record.WrapperCmd.Cmd, record.Env, record.Cmds)
			}
			record.Cmds = newCmds
			record.WrapperCmd.Stdout = ctx.stdoutString()
			record.WrapperCmd.Stderr = ctx.stderrString()
			newRecords = append(newRecords, record)
		}
		file.Records = newRecords
		newFiles = append(newFiles, file)
	}
	return newFiles
}

func writeGoldenRecords(ctx *testContext, writer io.Writer, records []goldenRecord) {
	// Replace the temp dir with a stable path so that the goldens stay stable.
	stableTempDir := filepath.Join(filepath.Dir(ctx.tempDir), "stable")
	writer = &replacingWriter{
		Writer: writer,
		old:    [][]byte{[]byte(ctx.tempDir)},
		new:    [][]byte{[]byte(stableTempDir)},
	}
	enc := json.NewEncoder(writer)
	enc.SetIndent("", "  ")
	if err := enc.Encode(records); err != nil {
		ctx.t.Fatal(err)
	}
}

type replacingWriter struct {
	io.Writer
	old [][]byte
	new [][]byte
}

func (writer *replacingWriter) Write(p []byte) (n int, err error) {
	// TODO: Use bytes.ReplaceAll once cros sdk uses golang >= 1.12
	for i, old := range writer.old {
		p = bytes.Replace(p, old, writer.new[i], -1)
	}
	return writer.Writer.Write(p)
}
