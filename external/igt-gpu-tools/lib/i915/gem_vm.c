/*
 * Copyright © 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <errno.h>
#include <string.h>

#include "ioctl_wrappers.h"
#include "drmtest.h"

#include "i915/gem_vm.h"

/**
 * SECTION:gem_vm
 * @short_description: Helpers for dealing with address spaces (vm/GTT)
 * @title: GEM Virtual Memory
 *
 * This helper library contains functions used for handling gem address
 * spaces.
 */

/**
 * gem_has_vm:
 * @i915: open i915 drm file descriptor
 *
 * Returns: whether VM creation is supported or not.
 */
bool gem_has_vm(int i915)
{
	uint32_t vm_id = 0;

	__gem_vm_create(i915, &vm_id);
	if (vm_id)
		gem_vm_destroy(i915, vm_id);

	return vm_id;
}

/**
 * gem_require_vm:
 * @i915: open i915 drm file descriptor
 *
 * This helper will automatically skip the test on platforms where address
 * space creation is not available.
 */
void gem_require_vm(int i915)
{
	igt_require(gem_has_vm(i915));
}

int __gem_vm_create(int i915, uint32_t *vm_id)
{
       struct drm_i915_gem_vm_control ctl = {};
       int err = 0;

       if (igt_ioctl(i915, DRM_IOCTL_I915_GEM_VM_CREATE, &ctl) == 0) {
               *vm_id = ctl.vm_id;
       } else {
	       err = -errno;
	       igt_assume(err != 0);
       }

       errno = 0;
       return err;
}

/**
 * gem_vm_create:
 * @i915: open i915 drm file descriptor
 *
 * This wraps the VM_CREATE ioctl, which is used to allocate a new
 * address space for use with GEM contexts.
 *
 * Returns: The id of the allocated address space.
 */
uint32_t gem_vm_create(int i915)
{
	uint32_t vm_id;

	igt_assert_eq(__gem_vm_create(i915, &vm_id), 0);
	igt_assert(vm_id != 0);

	return vm_id;
}

int __gem_vm_destroy(int i915, uint32_t vm_id)
{
	struct drm_i915_gem_vm_control ctl = { .vm_id = vm_id };
	int err = 0;

	if (igt_ioctl(i915, DRM_IOCTL_I915_GEM_VM_DESTROY, &ctl)) {
		err = -errno;
		igt_assume(err);
	}

	errno = 0;
	return err;
}

/**
 * gem_vm_destroy:
 * @i915: open i915 drm file descriptor
 * @vm_id: i915 VM id
 *
 * This wraps the VM_DESTROY ioctl, which is used to free an address space
 * handle.
 */
void gem_vm_destroy(int i915, uint32_t vm_id)
{
	igt_assert_eq(__gem_vm_destroy(i915, vm_id), 0);
}
