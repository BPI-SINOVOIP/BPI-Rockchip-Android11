// Copyright 2017 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"android/soong/ui/build"
	"android/soong/ui/logger"
	"android/soong/ui/metrics"
	"android/soong/ui/status"
	"android/soong/ui/terminal"
	"android/soong/ui/tracer"
)

// A command represents an operation to be executed in the soong build
// system.
type command struct {
	// the flag name (must have double dashes)
	flag string

	// description for the flag (to display when running help)
	description string

	// Forces the status output into dumb terminal mode.
	forceDumbOutput bool

	// Sets a prefix string to use for filenames of log files.
	logsPrefix string

	// Creates the build configuration based on the args and build context.
	config func(ctx build.Context, args ...string) build.Config

	// Returns what type of IO redirection this Command requires.
	stdio func() terminal.StdioInterface

	// run the command
	run func(ctx build.Context, config build.Config, args []string, logsDir string)
}

const makeModeFlagName = "--make-mode"

// list of supported commands (flags) supported by soong ui
var commands []command = []command{
	{
		flag:        makeModeFlagName,
		description: "build the modules by the target name (i.e. soong_docs)",
		config: func(ctx build.Context, args ...string) build.Config {
			return build.NewConfig(ctx, args...)
		},
		stdio: stdio,
		run:   make,
	}, {
		flag:            "--dumpvar-mode",
		description:     "print the value of the legacy make variable VAR to stdout",
		forceDumbOutput: true,
		logsPrefix:      "dumpvars-",
		config:          dumpVarConfig,
		stdio:           customStdio,
		run:             dumpVar,
	}, {
		flag:            "--dumpvars-mode",
		description:     "dump the values of one or more legacy make variables, in shell syntax",
		forceDumbOutput: true,
		logsPrefix:      "dumpvars-",
		config:          dumpVarConfig,
		stdio:           customStdio,
		run:             dumpVars,
	}, {
		flag:        "--build-mode",
		description: "build modules based on the specified build action",
		config:      buildActionConfig,
		stdio:       stdio,
		run:         make,
	},
}

// indexList returns the index of first found s. -1 is return if s is not
// found.
func indexList(s string, list []string) int {
	for i, l := range list {
		if l == s {
			return i
		}
	}
	return -1
}

// inList returns true if one or more of s is in the list.
func inList(s string, list []string) bool {
	return indexList(s, list) != -1
}

// Main execution of soong_ui. The command format is as follows:
//
//    soong_ui <command> [<arg 1> <arg 2> ... <arg n>]
//
// Command is the type of soong_ui execution. Only one type of
// execution is specified. The args are specific to the command.
func main() {
	buildStarted := time.Now()

	c, args := getCommand(os.Args)
	if c == nil {
		fmt.Fprintf(os.Stderr, "The `soong` native UI is not yet available.\n")
		os.Exit(1)
	}

	output := terminal.NewStatusOutput(c.stdio().Stdout(), os.Getenv("NINJA_STATUS"), c.forceDumbOutput,
		build.OsEnvironment().IsEnvTrue("ANDROID_QUIET_BUILD"))

	log := logger.New(output)
	defer log.Cleanup()

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	trace := tracer.New(log)
	defer trace.Close()

	met := metrics.New()
	met.SetBuildDateTime(buildStarted)

	stat := &status.Status{}
	defer stat.Finish()
	stat.AddOutput(output)
	stat.AddOutput(trace.StatusTracer())

	build.SetupSignals(log, cancel, func() {
		trace.Close()
		log.Cleanup()
		stat.Finish()
	})

	buildCtx := build.Context{ContextImpl: &build.ContextImpl{
		Context: ctx,
		Logger:  log,
		Metrics: met,
		Tracer:  trace,
		Writer:  output,
		Status:  stat,
	}}

	config := c.config(buildCtx, args...)

	build.SetupOutDir(buildCtx, config)

	logsDir := config.OutDir()
	if config.Dist() {
		logsDir = filepath.Join(config.DistDir(), "logs")
	}

	buildErrorFile := filepath.Join(logsDir, c.logsPrefix+"build_error")
	rbeMetricsFile := filepath.Join(logsDir, c.logsPrefix+"rbe_metrics.pb")
	soongMetricsFile := filepath.Join(logsDir, c.logsPrefix+"soong_metrics")
	defer build.UploadMetrics(buildCtx, config, c.forceDumbOutput, buildStarted, buildErrorFile, rbeMetricsFile, soongMetricsFile)

	os.MkdirAll(logsDir, 0777)
	log.SetOutput(filepath.Join(logsDir, c.logsPrefix+"soong.log"))
	trace.SetOutput(filepath.Join(logsDir, c.logsPrefix+"build.trace"))
	stat.AddOutput(status.NewVerboseLog(log, filepath.Join(logsDir, c.logsPrefix+"verbose.log")))
	stat.AddOutput(status.NewErrorLog(log, filepath.Join(logsDir, c.logsPrefix+"error.log")))
	stat.AddOutput(status.NewProtoErrorLog(log, buildErrorFile))
	stat.AddOutput(status.NewCriticalPath(log))

	buildCtx.Verbosef("Detected %.3v GB total RAM", float32(config.TotalRAM())/(1024*1024*1024))
	buildCtx.Verbosef("Parallelism (local/remote/highmem): %v/%v/%v",
		config.Parallel(), config.RemoteParallel(), config.HighmemParallel())

	defer met.Dump(soongMetricsFile)
	defer build.DumpRBEMetrics(buildCtx, config, rbeMetricsFile)

	if start, ok := os.LookupEnv("TRACE_BEGIN_SOONG"); ok {
		if !strings.HasSuffix(start, "N") {
			if start_time, err := strconv.ParseUint(start, 10, 64); err == nil {
				log.Verbosef("Took %dms to start up.",
					time.Since(time.Unix(0, int64(start_time))).Nanoseconds()/time.Millisecond.Nanoseconds())
				buildCtx.CompleteTrace(metrics.RunSetupTool, "startup", start_time, uint64(time.Now().UnixNano()))
			}
		}

		if executable, err := os.Executable(); err == nil {
			trace.ImportMicrofactoryLog(filepath.Join(filepath.Dir(executable), "."+filepath.Base(executable)+".trace"))
		}
	}

	// Fix up the source tree due to a repo bug where it doesn't remove
	// linkfiles that have been removed
	fixBadDanglingLink(buildCtx, "hardware/qcom/sdm710/Android.bp")
	fixBadDanglingLink(buildCtx, "hardware/qcom/sdm710/Android.mk")

	f := build.NewSourceFinder(buildCtx, config)
	defer f.Shutdown()
	build.FindSources(buildCtx, config, f)

	c.run(buildCtx, config, args, logsDir)
}

func fixBadDanglingLink(ctx build.Context, name string) {
	_, err := os.Lstat(name)
	if err != nil {
		return
	}
	_, err = os.Stat(name)
	if os.IsNotExist(err) {
		err = os.Remove(name)
		if err != nil {
			ctx.Fatalf("Failed to remove dangling link %q: %v", name, err)
		}
	}
}

func dumpVar(ctx build.Context, config build.Config, args []string, _ string) {
	flags := flag.NewFlagSet("dumpvar", flag.ExitOnError)
	flags.Usage = func() {
		fmt.Fprintf(ctx.Writer, "usage: %s --dumpvar-mode [--abs] <VAR>\n\n", os.Args[0])
		fmt.Fprintln(ctx.Writer, "In dumpvar mode, print the value of the legacy make variable VAR to stdout")
		fmt.Fprintln(ctx.Writer, "")

		fmt.Fprintln(ctx.Writer, "'report_config' is a special case that prints the human-readable config banner")
		fmt.Fprintln(ctx.Writer, "from the beginning of the build.")
		fmt.Fprintln(ctx.Writer, "")
		flags.PrintDefaults()
	}
	abs := flags.Bool("abs", false, "Print the absolute path of the value")
	flags.Parse(args)

	if flags.NArg() != 1 {
		flags.Usage()
		os.Exit(1)
	}

	varName := flags.Arg(0)
	if varName == "report_config" {
		varData, err := build.DumpMakeVars(ctx, config, nil, build.BannerVars)
		if err != nil {
			ctx.Fatal(err)
		}

		fmt.Println(build.Banner(varData))
	} else {
		varData, err := build.DumpMakeVars(ctx, config, nil, []string{varName})
		if err != nil {
			ctx.Fatal(err)
		}

		if *abs {
			var res []string
			for _, path := range strings.Fields(varData[varName]) {
				if abs, err := filepath.Abs(path); err == nil {
					res = append(res, abs)
				} else {
					ctx.Fatalln("Failed to get absolute path of", path, err)
				}
			}
			fmt.Println(strings.Join(res, " "))
		} else {
			fmt.Println(varData[varName])
		}
	}
}

func dumpVars(ctx build.Context, config build.Config, args []string, _ string) {
	flags := flag.NewFlagSet("dumpvars", flag.ExitOnError)
	flags.Usage = func() {
		fmt.Fprintf(ctx.Writer, "usage: %s --dumpvars-mode [--vars=\"VAR VAR ...\"]\n\n", os.Args[0])
		fmt.Fprintln(ctx.Writer, "In dumpvars mode, dump the values of one or more legacy make variables, in")
		fmt.Fprintln(ctx.Writer, "shell syntax. The resulting output may be sourced directly into a shell to")
		fmt.Fprintln(ctx.Writer, "set corresponding shell variables.")
		fmt.Fprintln(ctx.Writer, "")

		fmt.Fprintln(ctx.Writer, "'report_config' is a special case that dumps a variable containing the")
		fmt.Fprintln(ctx.Writer, "human-readable config banner from the beginning of the build.")
		fmt.Fprintln(ctx.Writer, "")
		flags.PrintDefaults()
	}

	varsStr := flags.String("vars", "", "Space-separated list of variables to dump")
	absVarsStr := flags.String("abs-vars", "", "Space-separated list of variables to dump (using absolute paths)")

	varPrefix := flags.String("var-prefix", "", "String to prepend to all variable names when dumping")
	absVarPrefix := flags.String("abs-var-prefix", "", "String to prepent to all absolute path variable names when dumping")

	flags.Parse(args)

	if flags.NArg() != 0 {
		flags.Usage()
		os.Exit(1)
	}

	vars := strings.Fields(*varsStr)
	absVars := strings.Fields(*absVarsStr)

	allVars := append([]string{}, vars...)
	allVars = append(allVars, absVars...)

	if i := indexList("report_config", allVars); i != -1 {
		allVars = append(allVars[:i], allVars[i+1:]...)
		allVars = append(allVars, build.BannerVars...)
	}

	if len(allVars) == 0 {
		return
	}

	varData, err := build.DumpMakeVars(ctx, config, nil, allVars)
	if err != nil {
		ctx.Fatal(err)
	}

	for _, name := range vars {
		if name == "report_config" {
			fmt.Printf("%sreport_config='%s'\n", *varPrefix, build.Banner(varData))
		} else {
			fmt.Printf("%s%s='%s'\n", *varPrefix, name, varData[name])
		}
	}
	for _, name := range absVars {
		var res []string
		for _, path := range strings.Fields(varData[name]) {
			abs, err := filepath.Abs(path)
			if err != nil {
				ctx.Fatalln("Failed to get absolute path of", path, err)
			}
			res = append(res, abs)
		}
		fmt.Printf("%s%s='%s'\n", *absVarPrefix, name, strings.Join(res, " "))
	}
}

func stdio() terminal.StdioInterface {
	return terminal.StdioImpl{}
}

func customStdio() terminal.StdioInterface {
	return terminal.NewCustomStdio(os.Stdin, os.Stderr, os.Stderr)
}

// dumpVarConfig does not require any arguments to be parsed by the NewConfig.
func dumpVarConfig(ctx build.Context, args ...string) build.Config {
	return build.NewConfig(ctx)
}

func buildActionConfig(ctx build.Context, args ...string) build.Config {
	flags := flag.NewFlagSet("build-mode", flag.ContinueOnError)
	flags.Usage = func() {
		fmt.Fprintf(ctx.Writer, "usage: %s --build-mode --dir=<path> <build action> [<build arg 1> <build arg 2> ...]\n\n", os.Args[0])
		fmt.Fprintln(ctx.Writer, "In build mode, build the set of modules based on the specified build")
		fmt.Fprintln(ctx.Writer, "action. The --dir flag is required to determine what is needed to")
		fmt.Fprintln(ctx.Writer, "build in the source tree based on the build action. See below for")
		fmt.Fprintln(ctx.Writer, "the list of acceptable build action flags.")
		fmt.Fprintln(ctx.Writer, "")
		flags.PrintDefaults()
	}

	buildActionFlags := []struct {
		name        string
		description string
		action      build.BuildAction
		set         bool
	}{{
		name:        "all-modules",
		description: "Build action: build from the top of the source tree.",
		action:      build.BUILD_MODULES,
	}, {
		// This is redirecting to mma build command behaviour. Once it has soaked for a
		// while, the build command is deleted from here once it has been removed from the
		// envsetup.sh.
		name:        "modules-in-a-dir-no-deps",
		description: "Build action: builds all of the modules in the current directory without their dependencies.",
		action:      build.BUILD_MODULES_IN_A_DIRECTORY,
	}, {
		// This is redirecting to mmma build command behaviour. Once it has soaked for a
		// while, the build command is deleted from here once it has been removed from the
		// envsetup.sh.
		name:        "modules-in-dirs-no-deps",
		description: "Build action: builds all of the modules in the supplied directories without their dependencies.",
		action:      build.BUILD_MODULES_IN_DIRECTORIES,
	}, {
		name:        "modules-in-a-dir",
		description: "Build action: builds all of the modules in the current directory and their dependencies.",
		action:      build.BUILD_MODULES_IN_A_DIRECTORY,
	}, {
		name:        "modules-in-dirs",
		description: "Build action: builds all of the modules in the supplied directories and their dependencies.",
		action:      build.BUILD_MODULES_IN_DIRECTORIES,
	}}
	for i, flag := range buildActionFlags {
		flags.BoolVar(&buildActionFlags[i].set, flag.name, false, flag.description)
	}
	dir := flags.String("dir", "", "Directory of the executed build command.")

	// Only interested in the first two args which defines the build action and the directory.
	// The remaining arguments are passed down to the config.
	const numBuildActionFlags = 2
	if len(args) < numBuildActionFlags {
		flags.Usage()
		ctx.Fatalln("Improper build action arguments.")
	}
	flags.Parse(args[0:numBuildActionFlags])

	// The next block of code is to validate that exactly one build action is set and the dir flag
	// is specified.
	buildActionCount := 0
	var buildAction build.BuildAction
	for _, flag := range buildActionFlags {
		if flag.set {
			buildActionCount++
			buildAction = flag.action
		}
	}
	if buildActionCount != 1 {
		ctx.Fatalln("Build action not defined.")
	}
	if *dir == "" {
		ctx.Fatalln("-dir not specified.")
	}

	// Remove the build action flags from the args as they are not recognized by the config.
	args = args[numBuildActionFlags:]
	return build.NewBuildActionConfig(buildAction, *dir, ctx, args...)
}

func make(ctx build.Context, config build.Config, _ []string, logsDir string) {
	if config.IsVerbose() {
		writer := ctx.Writer
		fmt.Fprintln(writer, "! The argument `showcommands` is no longer supported.")
		fmt.Fprintln(writer, "! Instead, the verbose log is always written to a compressed file in the output dir:")
		fmt.Fprintln(writer, "!")
		fmt.Fprintf(writer, "!   gzip -cd %s/verbose.log.gz | less -R\n", logsDir)
		fmt.Fprintln(writer, "!")
		fmt.Fprintln(writer, "! Older versions are saved in verbose.log.#.gz files")
		fmt.Fprintln(writer, "")
		select {
		case <-time.After(5 * time.Second):
		case <-ctx.Done():
			return
		}
	}

	if _, ok := config.Environment().Get("ONE_SHOT_MAKEFILE"); ok {
		writer := ctx.Writer
		fmt.Fprintln(writer, "! The variable `ONE_SHOT_MAKEFILE` is obsolete.")
		fmt.Fprintln(writer, "!")
		fmt.Fprintln(writer, "! If you're using `mm`, you'll need to run `source build/envsetup.sh` to update.")
		fmt.Fprintln(writer, "!")
		fmt.Fprintln(writer, "! Otherwise, either specify a module name with m, or use mma / MODULES-IN-...")
		fmt.Fprintln(writer, "")
		ctx.Fatal("done")
	}

	toBuild := build.BuildAll
	if config.Checkbuild() {
		toBuild |= build.RunBuildTests
	}
	build.Build(ctx, config, toBuild)
}

// getCommand finds the appropriate command based on args[1] flag. args[0]
// is the soong_ui filename.
func getCommand(args []string) (*command, []string) {
	if len(args) < 2 {
		return nil, args
	}

	for _, c := range commands {
		if c.flag == args[1] {
			return &c, args[2:]
		}

		// special case for --make-mode: if soong_ui was called from
		// build/make/core/main.mk, the makeparallel with --ninja
		// option specified puts the -j<num> before --make-mode.
		// TODO: Remove this hack once it has been fixed.
		if c.flag == makeModeFlagName {
			if inList(makeModeFlagName, args) {
				return &c, args[1:]
			}
		}
	}

	// command not found
	return nil, args
}
