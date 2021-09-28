/*
 * Copyright (C) 2018 The Android Open Source Project
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
// This file contains a testcase for vtable_dumper.
// The compiled shared library should contain vtables for all the classes in
// this file.

class VirtualBase {
 public:
  virtual ~VirtualBase() = 0;
  virtual void foo() = 0;
  virtual void bar() {}
  virtual void baz() {}
};

class VirtualDerived : public VirtualBase {
 public:
  virtual ~VirtualDerived() {}
  virtual void foo() override {}
  virtual void baz() override = 0;
};

class ConcreteDerived : public VirtualDerived {
 public:
  virtual ~ConcreteDerived() {}
  virtual void bar() override {}
  virtual void baz() override {}
};

// This ensures that the vtables are emitted.
void dummy_init() { VirtualBase *base_ptr = new ConcreteDerived(); }
