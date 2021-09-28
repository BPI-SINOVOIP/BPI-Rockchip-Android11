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
	"io"
	"io/ioutil"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"syscall"
	"time"

	"android/soong/finder"
	"android/soong/ui/build"
	"android/soong/ui/logger"
	"android/soong/ui/status"
	"android/soong/ui/terminal"
	"android/soong/ui/tracer"
	"android/soong/zip"
)

var numJobs = flag.Int("j", 0, "number of parallel jobs [0=autodetect]")

var keepArtifacts = flag.Bool("keep", false, "keep archives of artifacts")
var incremental = flag.Bool("incremental", false, "run in incremental mode (saving intermediates)")

var outDir = flag.String("out", "", "path to store output directories (defaults to tmpdir under $OUT when empty)")
var alternateResultDir = flag.Bool("dist", false, "write select results to $DIST_DIR (or <out>/dist when empty)")

var onlyConfig = flag.Bool("only-config", false, "Only run product config (not Soong or Kati)")
var onlySoong = flag.Bool("only-soong", false, "Only run product config and Soong (not Kati)")

var buildVariant = flag.String("variant", "eng", "build variant to use")

var skipProducts = flag.String("skip-products", "", "comma-separated list of products to skip (known failures, etc)")
var includeProducts = flag.String("products", "", "comma-separated list of products to build")

var shardCount = flag.Int("shard-count", 1, "split the products into multiple shards (to spread the build onto multiple machines, etc)")
var shard = flag.Int("shard", 1, "1-indexed shard to execute")

const errorLeadingLines = 20
const errorTrailingLines = 20

func errMsgFromLog(filename string) string {
	if filename == "" {
		return ""
	}

	data, err := ioutil.ReadFile(filename)
	if err != nil {
		return ""
	}

	lines := strings.Split(strings.TrimSpace(string(data)), "\n")
	if len(lines) > errorLeadingLines+errorTrailingLines+1 {
		lines[errorLeadingLines] = fmt.Sprintf("... skipping %d lines ...",
			len(lines)-errorLeadingLines-errorTrailingLines)

		lines = append(lines[:errorLeadingLines+1],
			lines[len(lines)-errorTrailingLines:]...)
	}
	var buf strings.Builder
	for _, line := range lines {
		buf.WriteString("> ")
		buf.WriteString(line)
		buf.WriteString("\n")
	}
	return buf.String()
}

// TODO(b/70370883): This tool uses a lot of open files -- over the default
// soft limit of 1024 on some systems. So bump up to the hard limit until I fix
// the algorithm.
func setMaxFiles(log logger.Logger) {
	var limits syscall.Rlimit

	err := syscall.Getrlimit(syscall.RLIMIT_NOFILE, &limits)
	if err != nil {
		log.Println("Failed to get file limit:", err)
		return
	}

	log.Verbosef("Current file limits: %d soft, %d hard", limits.Cur, limits.Max)
	if limits.Cur == limits.Max {
		return
	}

	limits.Cur = limits.Max
	err = syscall.Setrlimit(syscall.RLIMIT_NOFILE, &limits)
	if err != nil {
		log.Println("Failed to increase file limit:", err)
	}
}

func inList(str string, list []string) bool {
	for _, other := range list {
		if str == other {
			return true
		}
	}
	return false
}

func copyFile(from, to string) error {
	fromFile, err := os.Open(from)
	if err != nil {
		return err
	}
	defer fromFile.Close()

	toFile, err := os.Create(to)
	if err != nil {
		return err
	}
	defer toFile.Close()

	_, err = io.Copy(toFile, fromFile)
	return err
}

type mpContext struct {
	Context context.Context
	Logger  logger.Logger
	Status  status.ToolStatus
	Tracer  tracer.Tracer
	Finder  *finder.Finder
	Config  build.Config

	LogsDir string
}

func main() {
	stdio := terminal.StdioImpl{}

	output := terminal.NewStatusOutput(stdio.Stdout(), "", false,
		build.OsEnvironment().IsEnvTrue("ANDROID_QUIET_BUILD"))

	log := logger.New(output)
	defer log.Cleanup()

	flag.Parse()

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	trace := tracer.New(log)
	defer trace.Close()

	stat := &status.Status{}
	defer stat.Finish()
	stat.AddOutput(output)

	var failures failureCount
	stat.AddOutput(&failures)

	build.SetupSignals(log, cancel, func() {
		trace.Close()
		log.Cleanup()
		stat.Finish()
	})

	buildCtx := build.Context{ContextImpl: &build.ContextImpl{
		Context: ctx,
		Logger:  log,
		Tracer:  trace,
		Writer:  output,
		Status:  stat,
	}}

	config := build.NewConfig(buildCtx)
	if *outDir == "" {
		name := "multiproduct"
		if !*incremental {
			name += "-" + time.Now().Format("20060102150405")
		}

		*outDir = filepath.Join(config.OutDir(), name)

		// Ensure the empty files exist in the output directory
		// containing our output directory too. This is mostly for
		// safety, but also triggers the ninja_build file so that our
		// build servers know that they can parse the output as if it
		// was ninja output.
		build.SetupOutDir(buildCtx, config)

		if err := os.MkdirAll(*outDir, 0777); err != nil {
			log.Fatalf("Failed to create tempdir: %v", err)
		}
	}
	config.Environment().Set("OUT_DIR", *outDir)
	log.Println("Output directory:", *outDir)

	logsDir := filepath.Join(config.OutDir(), "logs")
	os.MkdirAll(logsDir, 0777)

	build.SetupOutDir(buildCtx, config)
	if *alternateResultDir {
		distLogsDir := filepath.Join(config.DistDir(), "logs")
		os.MkdirAll(distLogsDir, 0777)
		log.SetOutput(filepath.Join(distLogsDir, "soong.log"))
		trace.SetOutput(filepath.Join(distLogsDir, "build.trace"))
	} else {
		log.SetOutput(filepath.Join(config.OutDir(), "soong.log"))
		trace.SetOutput(filepath.Join(config.OutDir(), "build.trace"))
	}

	var jobs = *numJobs
	if jobs < 1 {
		jobs = runtime.NumCPU() / 4

		ramGb := int(config.TotalRAM() / 1024 / 1024 / 1024)
		if ramJobs := ramGb / 20; ramGb > 0 && jobs > ramJobs {
			jobs = ramJobs
		}

		if jobs < 1 {
			jobs = 1
		}
	}
	log.Verbosef("Using %d parallel jobs", jobs)

	setMaxFiles(log)

	finder := build.NewSourceFinder(buildCtx, config)
	defer finder.Shutdown()

	build.FindSources(buildCtx, config, finder)

	vars, err := build.DumpMakeVars(buildCtx, config, nil, []string{"all_named_products"})
	if err != nil {
		log.Fatal(err)
	}
	var productsList []string
	allProducts := strings.Fields(vars["all_named_products"])

	if *includeProducts != "" {
		missingProducts := []string{}
		for _, product := range strings.Split(*includeProducts, ",") {
			if inList(product, allProducts) {
				productsList = append(productsList, product)
			} else {
				missingProducts = append(missingProducts, product)
			}
		}
		if len(missingProducts) > 0 {
			log.Fatalf("Products don't exist: %s\n", missingProducts)
		}
	} else {
		productsList = allProducts
	}

	finalProductsList := make([]string, 0, len(productsList))
	skipList := strings.Split(*skipProducts, ",")
	skipProduct := func(p string) bool {
		for _, s := range skipList {
			if p == s {
				return true
			}
		}
		return false
	}
	for _, product := range productsList {
		if !skipProduct(product) {
			finalProductsList = append(finalProductsList, product)
		} else {
			log.Verbose("Skipping: ", product)
		}
	}

	if *shard < 1 {
		log.Fatalf("--shard value must be >= 1, not %d\n", *shard)
	} else if *shardCount < 1 {
		log.Fatalf("--shard-count value must be >= 1, not %d\n", *shardCount)
	} else if *shard > *shardCount {
		log.Fatalf("--shard (%d) must not be greater than --shard-count (%d)\n", *shard,
			*shardCount)
	} else if *shardCount > 1 {
		finalProductsList = splitList(finalProductsList, *shardCount)[*shard-1]
	}

	log.Verbose("Got product list: ", finalProductsList)

	s := buildCtx.Status.StartTool()
	s.SetTotalActions(len(finalProductsList))

	mpCtx := &mpContext{
		Context: ctx,
		Logger:  log,
		Status:  s,
		Tracer:  trace,

		Finder: finder,
		Config: config,

		LogsDir: logsDir,
	}

	products := make(chan string, len(productsList))
	go func() {
		defer close(products)
		for _, product := range finalProductsList {
			products <- product
		}
	}()

	var wg sync.WaitGroup
	for i := 0; i < jobs; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for {
				select {
				case product := <-products:
					if product == "" {
						return
					}
					buildProduct(mpCtx, product)
				}
			}
		}()
	}
	wg.Wait()

	if *alternateResultDir {
		args := zip.ZipArgs{
			FileArgs: []zip.FileArg{
				{GlobDir: logsDir, SourcePrefixToStrip: logsDir},
			},
			OutputFilePath:   filepath.Join(config.DistDir(), "logs.zip"),
			NumParallelJobs:  runtime.NumCPU(),
			CompressionLevel: 5,
		}
		if err := zip.Zip(args); err != nil {
			log.Fatalf("Error zipping logs: %v", err)
		}
	}

	s.Finish()

	if failures == 1 {
		log.Fatal("1 failure")
	} else if failures > 1 {
		log.Fatalf("%d failures", failures)
	} else {
		fmt.Fprintln(output, "Success")
	}
}

func buildProduct(mpctx *mpContext, product string) {
	var stdLog string

	outDir := filepath.Join(mpctx.Config.OutDir(), product)
	logsDir := filepath.Join(mpctx.LogsDir, product)

	if err := os.MkdirAll(outDir, 0777); err != nil {
		mpctx.Logger.Fatalf("Error creating out directory: %v", err)
	}
	if err := os.MkdirAll(logsDir, 0777); err != nil {
		mpctx.Logger.Fatalf("Error creating log directory: %v", err)
	}

	stdLog = filepath.Join(logsDir, "std.log")
	f, err := os.Create(stdLog)
	if err != nil {
		mpctx.Logger.Fatalf("Error creating std.log: %v", err)
	}
	defer f.Close()

	log := logger.New(f)
	defer log.Cleanup()
	log.SetOutput(filepath.Join(logsDir, "soong.log"))

	action := &status.Action{
		Description: product,
		Outputs:     []string{product},
	}
	mpctx.Status.StartAction(action)
	defer logger.Recover(func(err error) {
		mpctx.Status.FinishAction(status.ActionResult{
			Action: action,
			Error:  err,
			Output: errMsgFromLog(stdLog),
		})
	})

	ctx := build.Context{ContextImpl: &build.ContextImpl{
		Context: mpctx.Context,
		Logger:  log,
		Tracer:  mpctx.Tracer,
		Writer:  f,
		Thread:  mpctx.Tracer.NewThread(product),
		Status:  &status.Status{},
	}}
	ctx.Status.AddOutput(terminal.NewStatusOutput(ctx.Writer, "", false,
		build.OsEnvironment().IsEnvTrue("ANDROID_QUIET_BUILD")))

	config := build.NewConfig(ctx, flag.Args()...)
	config.Environment().Set("OUT_DIR", outDir)
	if !*keepArtifacts {
		config.Environment().Set("EMPTY_NINJA_FILE", "true")
	}
	build.FindSources(ctx, config, mpctx.Finder)
	config.Lunch(ctx, product, *buildVariant)

	defer func() {
		if *keepArtifacts {
			args := zip.ZipArgs{
				FileArgs: []zip.FileArg{
					{
						GlobDir:             outDir,
						SourcePrefixToStrip: outDir,
					},
				},
				OutputFilePath:   filepath.Join(mpctx.Config.OutDir(), product+".zip"),
				NumParallelJobs:  runtime.NumCPU(),
				CompressionLevel: 5,
			}
			if err := zip.Zip(args); err != nil {
				log.Fatalf("Error zipping artifacts: %v", err)
			}
		}
		if !*incremental {
			os.RemoveAll(outDir)
		}
	}()

	buildWhat := build.BuildProductConfig
	if !*onlyConfig {
		buildWhat |= build.BuildSoong
		if !*onlySoong {
			buildWhat |= build.BuildKati
		}
	}

	before := time.Now()
	build.Build(ctx, config, buildWhat)

	// Save std_full.log if Kati re-read the makefiles
	if buildWhat&build.BuildKati != 0 {
		if after, err := os.Stat(config.KatiBuildNinjaFile()); err == nil && after.ModTime().After(before) {
			err := copyFile(stdLog, filepath.Join(filepath.Dir(stdLog), "std_full.log"))
			if err != nil {
				log.Fatalf("Error copying log file: %s", err)
			}
		}
	}

	mpctx.Status.FinishAction(status.ActionResult{
		Action: action,
	})
}

type failureCount int

func (f *failureCount) StartAction(action *status.Action, counts status.Counts) {}

func (f *failureCount) FinishAction(result status.ActionResult, counts status.Counts) {
	if result.Error != nil {
		*f += 1
	}
}

func (f *failureCount) Message(level status.MsgLevel, message string) {
	if level >= status.ErrorLvl {
		*f += 1
	}
}

func (f *failureCount) Flush() {}

func (f *failureCount) Write(p []byte) (int, error) {
	// discard writes
	return len(p), nil
}

func splitList(list []string, shardCount int) (ret [][]string) {
	each := len(list) / shardCount
	extra := len(list) % shardCount
	for i := 0; i < shardCount; i++ {
		count := each
		if extra > 0 {
			count += 1
			extra -= 1
		}
		ret = append(ret, list[:count])
		list = list[count:]
	}
	return
}
