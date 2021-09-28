/*
 * Copyright 2018, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fork.h"

#include "log.h"

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

bool forkAndExec(const char* argv[]) {
    pid_t pid = ::fork();
    if (pid < 0) {
        // Failed to fork
        LOGE("fork() failed: %s", strerror(errno));
        return false;
    } else if (pid == 0) {
        // Child
        char buffer[32768];
        size_t offset = 0;
        for (size_t i = 0; argv[i]; ++i) {
            offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                               "%s ", argv[i]);
        }
        LOGE("Running '%s'", buffer);
        execvp(argv[0], const_cast<char* const*>(argv));
        LOGE("Failed to run '%s': %s", argv[0], strerror(errno));
        _exit(1);
    } else {
        // Parent
        int status = 0;
        do {
            ::waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                int exitStatus = WEXITSTATUS(status);
                if (exitStatus == 0) {
                    return true;
                }
                LOGE("Error: '%s' exited with code: %d", argv[0], exitStatus);
            } else if (WIFSIGNALED(status)) {
                LOGE("Error: '%s' terminated with signal: %d",
                     argv[0], WTERMSIG(status));
            }
            // Other possibilities include being stopped and continued as part
            // of a trace but we don't really care about that. The important
            // part is that unless the process explicitly exited or was killed
            // by a signal we have to keep waiting.
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        return false;
    }
}
