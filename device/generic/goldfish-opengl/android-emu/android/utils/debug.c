// Copyright 2007-2008 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "android/utils/debug.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

uint64_t android_verbose = 0;

void
dprint( const char*  format,  ... )
{
    va_list  args;
    va_start( args, format );
    fprintf( stdout, "emulator: ");
    vfprintf( stdout, format, args );
    fprintf( stdout, "\n" );
    va_end( args );
}


void
dprintn( const char*  format, ... )
{
    va_list  args;
    va_start( args, format );
    vfprintf( stdout, format, args );
    va_end( args );
}

void
dprintnv( const char*  format, va_list args )
{
    vfprintf( stdout, format, args );
}


void
dwarning( const char*  format, ... )
{
    va_list  args;
    va_start( args, format );
    dprintn( "emulator: WARNING: " );
    dprintnv( format, args );
    dprintn( "\n" );
    va_end( args );
}


void
derror( const char*  format, ... )
{
    va_list  args;
    va_start( args, format );
    dprintn( "emulator: ERROR: " );
    dprintnv( format, args );
    dprintn( "\n" );
    va_end( args );
}

void
android_tid_function_print(
        bool use_emulator_prefix,
        const char* function,
        const char* format, ... )
{
    int tid = syscall(SYS_gettid);
    va_list args;
    va_start(args, format);
    const char* prefix = use_emulator_prefix ? "emulator: " : "";
    if (function) {
        printf("%stid=0x%x: %s: ", prefix, tid,
               function);
    } else {
        printf("%stid=0x%x: ", prefix, tid);
    }

    vprintf(format, args);
    printf("\n");
    va_end(args);
}

/** STDOUT/STDERR REDIRECTION
 **
 ** allows you to shut temporarily shutdown stdout/stderr
 ** this is useful to get rid of debug messages from ALSA and esd
 ** on Linux.
 **/
static int    stdio_disable_count;
static int    stdio_save_out_fd;
static int    stdio_save_err_fd;

#ifdef _WIN32
extern void
stdio_disable( void )
{
    if (++stdio_disable_count == 1) {
        int  null_fd, out_fd, err_fd;
        fflush(stdout);
        out_fd = _fileno(stdout);
        err_fd = _fileno(stderr);
        stdio_save_out_fd = _dup(out_fd);
        stdio_save_err_fd = _dup(err_fd);
        null_fd = _open( "NUL", _O_WRONLY );
        _dup2(null_fd, out_fd);
        _dup2(null_fd, err_fd);
        close(null_fd);
    }
}

extern void
stdio_enable( void )
{
    if (--stdio_disable_count == 0) {
        int  out_fd, err_fd;
        fflush(stdout);
        out_fd = _fileno(stdout);
        err_fd = _fileno(stderr);
        _dup2(stdio_save_out_fd, out_fd);
        _dup2(stdio_save_err_fd, err_fd);
        _close(stdio_save_out_fd);
        _close(stdio_save_err_fd);
    }
}
#else
extern void
stdio_disable( void )
{
    if (++stdio_disable_count == 1) {
        int  null_fd, out_fd, err_fd;
        fflush(stdout);
        out_fd = fileno(stdout);
        err_fd = fileno(stderr);
        stdio_save_out_fd = dup(out_fd);
        stdio_save_err_fd = dup(err_fd);
        null_fd = open( "/dev/null", O_WRONLY );
        dup2(null_fd, out_fd);
        dup2(null_fd, err_fd);
        close(null_fd);
    }
}

extern void
stdio_enable( void )
{
    if (--stdio_disable_count == 0) {
        int  out_fd, err_fd;
        fflush(stdout);
        out_fd = fileno(stdout);
        err_fd = fileno(stderr);
        dup2(stdio_save_out_fd, out_fd);
        dup2(stdio_save_err_fd, err_fd);
        close(stdio_save_out_fd);
        close(stdio_save_err_fd);
    }
}
#endif
