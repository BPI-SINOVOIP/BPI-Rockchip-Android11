// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestOmitDoubleBuildForSuccessfulCall(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.must(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc)))
		if ctx.cmdCount != 1 {
			t.Errorf("expected 1 call. Got: %d", ctx.cmdCount)
		}
	})
}

func TestOmitDoubleBuildForGeneralError(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			return errors.New("someerror")
		}
		stderr := ctx.mustFail(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc)))
		if err := verifyInternalError(stderr); err != nil {
			t.Fatal(err)
		}
		if !strings.Contains(stderr, "someerror") {
			t.Errorf("unexpected error. Got: %s", stderr)
		}
		if ctx.cmdCount != 1 {
			t.Errorf("expected 1 call. Got: %d", ctx.cmdCount)
		}
	})
}

func TestDoubleBuildWithWNoErrorFlag(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				if err := verifyArgCount(cmd, 0, "-Wno-error"); err != nil {
					return err
				}
				fmt.Fprint(stderr, "-Werror originalerror")
				return newExitCodeError(1)
			case 2:
				if err := verifyArgCount(cmd, 1, "-Wno-error"); err != nil {
					return err
				}
				return nil
			default:
				t.Fatalf("unexpected command: %#v", cmd)
				return nil
			}
		}
		ctx.must(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc)))
		if ctx.cmdCount != 2 {
			t.Errorf("expected 2 calls. Got: %d", ctx.cmdCount)
		}
	})
}

func TestDoubleBuildWithWNoErrorAndCCache(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.cfg.useCCache = true
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				// TODO: This is a bug in the old wrapper that it drops the ccache path
				// during double build. Fix this once we don't compare to the old wrapper anymore.
				if err := verifyPath(cmd, "ccache"); err != nil {
					return err
				}
				fmt.Fprint(stderr, "-Werror originalerror")
				return newExitCodeError(1)
			case 2:
				if err := verifyPath(cmd, "ccache"); err != nil {
					return err
				}
				return nil
			default:
				t.Fatalf("unexpected command: %#v", cmd)
				return nil
			}
		}
		ctx.must(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc)))
		if ctx.cmdCount != 2 {
			t.Errorf("expected 2 calls. Got: %d", ctx.cmdCount)
		}
	})
}

func TestForwardStdoutAndStderrWhenDoubleBuildSucceeds(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				fmt.Fprint(stdout, "originalmessage")
				fmt.Fprint(stderr, "-Werror originalerror")
				return newExitCodeError(1)
			case 2:
				fmt.Fprint(stdout, "retrymessage")
				fmt.Fprint(stderr, "retryerror")
				return nil
			default:
				t.Fatalf("unexpected command: %#v", cmd)
				return nil
			}
		}
		ctx.must(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc)))
		if err := verifyNonInternalError(ctx.stderrString(), "retryerror"); err != nil {
			t.Error(err)
		}
		if !strings.Contains(ctx.stdoutString(), "retrymessage") {
			t.Errorf("unexpected stdout. Got: %s", ctx.stdoutString())
		}
	})
}

func TestForwardStdoutAndStderrWhenDoubleBuildFails(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				fmt.Fprint(stdout, "originalmessage")
				fmt.Fprint(stderr, "-Werror originalerror")
				return newExitCodeError(3)
			case 2:
				fmt.Fprint(stdout, "retrymessage")
				fmt.Fprint(stderr, "retryerror")
				return newExitCodeError(5)
			default:
				t.Fatalf("unexpected command: %#v", cmd)
				return nil
			}
		}
		exitCode := callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc))
		if exitCode != 5 {
			t.Errorf("unexpected exitcode. Got: %d", exitCode)
		}
		if err := verifyNonInternalError(ctx.stderrString(), "-Werror originalerror"); err != nil {
			t.Error(err)
		}
		if !strings.Contains(ctx.stdoutString(), "originalmessage") {
			t.Errorf("unexpected stdout. Got: %s", ctx.stdoutString())
		}
	})
}

func TestForwardStdinFromDoubleBuild(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			// Note: This is called for the clang syntax call as well as for
			// the gcc call, and we assert that stdin is cloned and forwarded
			// to both.
			stdinStr := ctx.readAllString(stdin)
			if stdinStr != "someinput" {
				return fmt.Errorf("unexpected stdin. Got: %s", stdinStr)
			}

			switch ctx.cmdCount {
			case 1:
				fmt.Fprint(stderr, "-Werror originalerror")
				return newExitCodeError(1)
			case 2:
				return nil
			default:
				t.Fatalf("unexpected command: %#v", cmd)
				return nil
			}
		}
		io.WriteString(&ctx.stdinBuffer, "someinput")
		ctx.must(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, "-", mainCc)))
	})
}

func TestForwardGeneralErrorWhenDoubleBuildFails(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				fmt.Fprint(stderr, "-Werror originalerror")
				return newExitCodeError(3)
			case 2:
				return errors.New("someerror")
			default:
				t.Fatalf("unexpected command: %#v", cmd)
				return nil
			}
		}
		stderr := ctx.mustFail(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc)))
		if err := verifyInternalError(stderr); err != nil {
			t.Error(err)
		}
		if !strings.Contains(stderr, "someerror") {
			t.Errorf("unexpected stderr. Got: %s", stderr)
		}
	})
}

func TestOmitLogWarningsIfNoDoubleBuild(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.must(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc)))
		if ctx.cmdCount != 1 {
			t.Errorf("expected 1 call. Got: %d", ctx.cmdCount)
		}
		if loggedWarnings := readLoggedWarnings(ctx); loggedWarnings != nil {
			t.Errorf("expected no logged warnings. Got: %#v", loggedWarnings)
		}
	})
}

func TestLogWarningsWhenDoubleBuildSucceeds(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				fmt.Fprint(stdout, "originalmessage")
				fmt.Fprint(stderr, "-Werror originalerror")
				return newExitCodeError(1)
			case 2:
				fmt.Fprint(stdout, "retrymessage")
				fmt.Fprint(stderr, "retryerror")
				return nil
			default:
				t.Fatalf("unexpected command: %#v", cmd)
				return nil
			}
		}
		ctx.must(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc)))
		loggedWarnings := readLoggedWarnings(ctx)
		if loggedWarnings == nil {
			t.Fatal("expected logged warnings")
		}
		if loggedWarnings.Cwd != ctx.getwd() {
			t.Fatalf("unexpected cwd. Got: %s", loggedWarnings.Cwd)
		}
		loggedCmd := &command{
			Path: loggedWarnings.Command[0],
			Args: loggedWarnings.Command[1:],
		}
		if err := verifyPath(loggedCmd, "usr/bin/clang"); err != nil {
			t.Error(err)
		}
		if err := verifyArgOrder(loggedCmd, "--sysroot=.*", mainCc); err != nil {
			t.Error(err)
		}
	})
}

func TestLogWarningsWhenDoubleBuildFails(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				fmt.Fprint(stdout, "originalmessage")
				fmt.Fprint(stderr, "-Werror originalerror")
				return newExitCodeError(1)
			case 2:
				fmt.Fprint(stdout, "retrymessage")
				fmt.Fprint(stderr, "retryerror")
				return newExitCodeError(1)
			default:
				t.Fatalf("unexpected command: %#v", cmd)
				return nil
			}
		}
		ctx.mustFail(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc)))
		loggedWarnings := readLoggedWarnings(ctx)
		if loggedWarnings == nil {
			t.Fatal("expected logged warnings")
		}
	})
}

func withForceDisableWErrorTestContext(t *testing.T, work func(ctx *testContext)) {
	withTestContext(t, func(ctx *testContext) {
		ctx.env = []string{"FORCE_DISABLE_WERROR=1"}
		work(ctx)
	})
}

func readLoggedWarnings(ctx *testContext) *warningsJSONData {
	files, err := ioutil.ReadDir(ctx.cfg.newWarningsDir)
	if err != nil {
		if _, ok := err.(*os.PathError); ok {
			return nil
		}
		ctx.t.Fatal(err)
	}
	if len(files) != 1 {
		ctx.t.Fatalf("expected 1 warning log file. Got: %s", files)
	}
	data, err := ioutil.ReadFile(filepath.Join(ctx.cfg.newWarningsDir, files[0].Name()))
	if err != nil {
		ctx.t.Fatal(err)
	}
	jsonData := warningsJSONData{}
	if err := json.Unmarshal(data, &jsonData); err != nil {
		ctx.t.Fatal(err)
	}
	return &jsonData
}

func TestDoubleBuildWerrorChmodsThingsAppropriately(t *testing.T) {
	withForceDisableWErrorTestContext(t, func(ctx *testContext) {
		ctx.cmdMock = func(cmd *command, stdin io.Reader, stdout io.Writer, stderr io.Writer) error {
			switch ctx.cmdCount {
			case 1:
				if err := verifyArgCount(cmd, 0, "-Wno-error"); err != nil {
					return err
				}
				fmt.Fprint(stderr, "-Werror originalerror")
				return newExitCodeError(1)
			case 2:
				if err := verifyArgCount(cmd, 1, "-Wno-error"); err != nil {
					return err
				}
				return nil
			default:
				t.Fatalf("unexpected command: %#v", cmd)
				return nil
			}
		}
		ctx.must(callCompiler(ctx, ctx.cfg, ctx.newCommand(clangX86_64, mainCc)))
		if ctx.cmdCount != 2 {
			// Later errors are likely senseless if we didn't get called twice.
			t.Fatalf("expected 2 calls. Got: %d", ctx.cmdCount)
		}

		t.Logf("Warnings dir is at %q", ctx.cfg.newWarningsDir)
		warningsDir, err := os.Open(ctx.cfg.newWarningsDir)
		if err != nil {
			t.Fatalf("failed to open the new warnings dir: %v", err)
		}
		defer warningsDir.Close()

		fi, err := warningsDir.Stat()
		if err != nil {
			t.Fatalf("failed stat'ing the warnings dir: %v", err)
		}

		permBits := func(mode os.FileMode) int { return int(mode & 0777) }

		if perms := permBits(fi.Mode()); perms != 0777 {
			t.Errorf("mode for tempdir are %#o; expected 0777", perms)
		}

		entries, err := warningsDir.Readdir(0)
		if err != nil {
			t.Fatalf("failed reading entries of the tempdir: %v", err)
		}

		if len(entries) != 1 {
			t.Errorf("found %d tempfiles in the tempdir; expected 1", len(entries))
		}

		for _, e := range entries {
			if perms := permBits(e.Mode()); perms != 0666 {
				t.Errorf("mode for %q is %#o; expected 0666", e.Name(), perms)
			}
		}
	})
}
