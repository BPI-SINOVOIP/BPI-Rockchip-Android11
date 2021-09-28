/*
* Copyright (c) 2014, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted
* provided that the following conditions are met:
*    * Redistributions of source code must retain the above copyright notice, this list of
*      conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above copyright notice, this list of
*      conditions and the following disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of The Linux Foundation nor the names of its contributors may be used to
*      endorse or promote products derived from this software without specific prior written
*      permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __DUMP_IMPL_H__
#define __DUMP_IMPL_H__

#include <core/dump_interface.h>

namespace sdm {

class DumpImpl {
 public:
  // To be implemented in the modules which will add dump information to final dump buffer.
  // buffer address & length will be already adjusted before calling into these modules.
  virtual void AppendDump(char *buffer, uint32_t length) = 0;
  static void AppendString(char *buffer, uint32_t length, const char *format, ...);

 protected:
  DumpImpl();
  virtual ~DumpImpl();

 private:
  static const uint32_t kMaxDumpObjects = 32;

  static void Register(DumpImpl *dump_impl);
  static void Unregister(DumpImpl *dump_impl);

  static DumpImpl *dump_list_[kMaxDumpObjects];
  static uint32_t dump_count_;

  friend class DumpInterface;
};

}  // namespace sdm

#endif  // __DUMP_IMPL_H__

