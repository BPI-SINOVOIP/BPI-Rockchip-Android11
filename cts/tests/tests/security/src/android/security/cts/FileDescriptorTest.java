/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.security.cts;

import static android.system.OsConstants.*;

import static org.junit.Assert.fail;

import android.system.ErrnoException;
import android.system.Os;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileDescriptor;

@RunWith(AndroidJUnit4.class)
public class FileDescriptorTest {
  /**
   * Verify that all file descriptors (excluding STDIN, STDOUT, and STDERR)
   * have enabled O_CLOEXEC. O_CLOEXEC is essential for avoiding
   * file descriptor leaks across exec() boundaries. Leaking FDs
   * wastes resources and can lead to security bugs if an exec()ed
   * process improperly uses the file descriptor.
   *
   * When using native code, we cannot rely on Runtime.exec and
   * ProcessBuilder.start closing file descriptors for us. The
   * native code may make it's own calls to fork() / exec()
   * without pruning open file descriptors.
   *
   * This test works by scanning all open file descriptors in
   * /proc/self/fd/*, calling fcntl(F_GETFD) on the file descriptor,
   * and verifying that the FD_CLOEXEC flag is set.
   */
  @Test
  public void testCLOEXEC() throws Exception {
    StringBuilder failMsg = new StringBuilder(
        "The following FDs do not have O_CLOEXEC enabled:\n");
    boolean failed = false;

    File[] files = new File("/proc/self/fd").listFiles();
    for (File file : files) {
      int fdint = Integer.parseInt(file.getName());
      if (fdint < 3) {
        // STDIN, STDOUT, and STDERR are expected to not have O_CLOEXEC
        continue;
      }
      FileDescriptor fd = new FileDescriptor();
      fd.setInt$(fdint);
      try {
        int flags = Os.fcntlInt(fd, F_GETFD, 0);
        if ((flags & FD_CLOEXEC) == 0) {
          failed = true;
          String link = Os.readlink(file.toString());
          failMsg.append("  ").append(file).append(" -> \"")
              .append(link).append("\"\n");
        }
      } catch (ErrnoException e) {
        // There's no guarantee that the file descriptor will still
        // be available when we try to run fcntl on it. The primary
        // reason why this will occur is because the file descriptor
        // underlying the listFiles() call above will be gone by
        // the time we go to examine it, but it could also happen
        // due to multi-threaded race conditions.
        if (e.errno != EBADF) {
          throw e;
        }
      }
    }
    if (failed) {
      fail(failMsg.toString());
    }
  }
}
