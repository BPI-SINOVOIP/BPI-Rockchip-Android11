/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ANDROID_WORKER_H_
#define ANDROID_WORKER_H_

#include <pthread.h>
#include <stdint.h>
#include <string>

namespace android {

class Worker {
 public:
  pthread_mutex_t* getLock();
  int Lock();
  int Unlock();

  // Must be called with the lock acquired
  int SignalLocked();
  int ExitLocked();

  // Convenience versions of above, acquires the lock
  int Signal();
  int Exit();

 protected:
  Worker(const char *name, int priority);
  virtual ~Worker();

  int InitWorker();

  bool initialized() const;

  virtual void Routine() = 0;

  /*
   * Must be called with the lock acquired. max_nanoseconds may be negative to
   * indicate infinite timeout, otherwise it indicates the maximum time span to
   * wait for a signal before returning.
   * Returns -EINTR if interrupted by exit request, or -ETIMEDOUT if timed out
   */
  int WaitForSignalOrExitLocked(int64_t max_nanoseconds = -1);

 private:
  static void *InternalRoutine(void *worker);

  // Must be called with the lock acquired
  int SignalThreadLocked(bool exit);

  std::string name_;
  int priority_;

  pthread_t thread_;
  pthread_mutex_t lock_;
  pthread_cond_t cond_;

  bool exit_;
  bool initialized_;
};
}

#endif
