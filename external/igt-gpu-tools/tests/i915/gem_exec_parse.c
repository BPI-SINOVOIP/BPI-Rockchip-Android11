/*
 * Copyright © 2013 Intel Corporation
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
 *
 */

#include "igt.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include <drm.h>

#ifndef I915_PARAM_CMD_PARSER_VERSION
#define I915_PARAM_CMD_PARSER_VERSION       28
#endif

#define DERRMR 0x44050
#define OASTATUS2 0x2368
#define OACONTROL 0x2360
#define SO_WRITE_OFFSET_0 0x5280

#define HSW_CS_GPR(n) (0x2600 + 8*(n))
#define HSW_CS_GPR0 HSW_CS_GPR(0)
#define HSW_CS_GPR1 HSW_CS_GPR(1)

/* To help craft commands known to be invalid across all engines */
#define INSTR_CLIENT_SHIFT	29
#define   INSTR_INVALID_CLIENT  0x7

#define MI_LOAD_REGISTER_REG (0x2a << 23)
#define MI_STORE_REGISTER_MEM (0x24 << 23)
#define MI_ARB_ON_OFF (0x8 << 23)
#define MI_DISPLAY_FLIP ((0x14 << 23) | 1)

#define GFX_OP_PIPE_CONTROL	((0x3<<29)|(0x3<<27)|(0x2<<24)|2)
#define   PIPE_CONTROL_QW_WRITE	(1<<14)
#define   PIPE_CONTROL_LRI_POST_OP (1<<23)

static int parser_version;

static int command_parser_version(int fd)
{
	int version = -1;
	drm_i915_getparam_t gp;

	gp.param = I915_PARAM_CMD_PARSER_VERSION;
	gp.value = &version;

	if (drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp) == 0)
		return version;

	return -1;
}

static uint64_t __exec_batch_patched(int fd, uint32_t cmd_bo, uint32_t *cmds,
				     int size, int patch_offset)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[2];
	struct drm_i915_gem_relocation_entry reloc[1];

	uint32_t target_bo = gem_create(fd, 4096);
	uint64_t actual_value = 0;

	gem_write(fd, cmd_bo, 0, cmds, size);

	memset(obj, 0, sizeof(obj));
	obj[0].handle = target_bo;
	obj[1].handle = cmd_bo;

	memset(reloc, 0, sizeof(reloc));
	reloc[0].offset = patch_offset;
	reloc[0].target_handle = obj[0].handle;
	reloc[0].delta = 0;
	reloc[0].read_domains = I915_GEM_DOMAIN_COMMAND;
	reloc[0].write_domain = I915_GEM_DOMAIN_COMMAND;
	obj[1].relocs_ptr = to_user_pointer(reloc);
	obj[1].relocation_count = 1;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;
	execbuf.batch_len = size;
	execbuf.flags = I915_EXEC_RENDER;

	gem_execbuf(fd, &execbuf);
	gem_sync(fd, cmd_bo);

	gem_read(fd,target_bo, 0, &actual_value, sizeof(actual_value));

	gem_close(fd, target_bo);

	return actual_value;
}

static void exec_batch_patched(int fd, uint32_t cmd_bo, uint32_t *cmds,
			       int size, int patch_offset,
			       uint64_t expected_value)
{
	igt_assert_eq(__exec_batch_patched(fd, cmd_bo, cmds,
					   size, patch_offset),
		      expected_value);
}

static int __exec_batch(int fd, uint32_t cmd_bo, uint32_t *cmds,
			int size, int ring)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[1];

	gem_write(fd, cmd_bo, 0, cmds, size);

	memset(obj, 0, sizeof(obj));
	obj[0].handle = cmd_bo;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 1;
	execbuf.batch_len = size;
	execbuf.flags = ring;

	return __gem_execbuf(fd, &execbuf);
}
#define exec_batch(fd, bo, cmds, sz, ring, expected) \
	igt_assert_eq(__exec_batch(fd, bo, cmds, sz, ring), expected)

static void exec_split_batch(int fd, uint32_t *cmds,
			     int size, int ring, int expected_ret)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[1];
	uint32_t cmd_bo;
	uint32_t noop[1024] = { 0 };
	const int alloc_size = 4096 * 2;
	const int actual_start_offset = 4096-sizeof(uint32_t);

	/* Allocate and fill a 2-page batch with noops */
	cmd_bo = gem_create(fd, alloc_size);
	gem_write(fd, cmd_bo, 0, noop, sizeof(noop));
	gem_write(fd, cmd_bo, 4096, noop, sizeof(noop));

	/* Write the provided commands such that the first dword
	 * of the command buffer is the last dword of the first
	 * page (i.e. the command is split across the two pages).
	 */
	gem_write(fd, cmd_bo, actual_start_offset, cmds, size);

	memset(obj, 0, sizeof(obj));
	obj[0].handle = cmd_bo;

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 1;
	/* NB: We want batch_start_offset and batch_len to point to the block
	 * of the actual commands (i.e. at the last dword of the first page),
	 * but have to adjust both the start offset and length to meet the
	 * kernel driver's requirements on the alignment of those fields.
	 */
	execbuf.batch_start_offset = actual_start_offset & ~0x7;
	execbuf.batch_len =
		ALIGN(size + actual_start_offset - execbuf.batch_start_offset,
		      0x8);
	execbuf.flags = ring;

	igt_assert_eq(__gem_execbuf(fd, &execbuf), expected_ret);

	gem_sync(fd, cmd_bo);
	gem_close(fd, cmd_bo);
}

static void exec_batch_chained(int fd, uint32_t cmd_bo, uint32_t *cmds,
			       int size, int patch_offset,
			       uint64_t expected_value)
{
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[3];
	struct drm_i915_gem_relocation_entry reloc[1];
	struct drm_i915_gem_relocation_entry first_level_reloc;

	uint32_t target_bo = gem_create(fd, 4096);
	uint32_t first_level_bo = gem_create(fd, 4096);
	uint64_t actual_value = 0;

	static uint32_t first_level_cmds[] = {
		MI_BATCH_BUFFER_START | MI_BATCH_NON_SECURE_I965,
		0,
		MI_BATCH_BUFFER_END,
		0,
	};

	if (IS_HASWELL(intel_get_drm_devid(fd)))
		first_level_cmds[0] |= MI_BATCH_NON_SECURE_HSW;

	gem_write(fd, first_level_bo, 0,
		  first_level_cmds, sizeof(first_level_cmds));
	gem_write(fd, cmd_bo, 0, cmds, size);

	memset(obj, 0, sizeof(obj));
	obj[0].handle = target_bo;
	obj[1].handle = cmd_bo;
	obj[2].handle = first_level_bo;

	memset(reloc, 0, sizeof(reloc));
	reloc[0].offset = patch_offset;
	reloc[0].delta = 0;
	reloc[0].target_handle = target_bo;
	reloc[0].read_domains = I915_GEM_DOMAIN_COMMAND;
	reloc[0].write_domain = I915_GEM_DOMAIN_COMMAND;
	obj[1].relocation_count = 1;
	obj[1].relocs_ptr = to_user_pointer(&reloc);

	memset(&first_level_reloc, 0, sizeof(first_level_reloc));
	first_level_reloc.offset = 4;
	first_level_reloc.delta = 0;
	first_level_reloc.target_handle = cmd_bo;
	first_level_reloc.read_domains = I915_GEM_DOMAIN_COMMAND;
	first_level_reloc.write_domain = 0;
	obj[2].relocation_count = 1;
	obj[2].relocs_ptr = to_user_pointer(&first_level_reloc);

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 3;
	execbuf.batch_len = sizeof(first_level_cmds);
	execbuf.flags = I915_EXEC_RENDER;

	gem_execbuf(fd, &execbuf);
	gem_sync(fd, cmd_bo);

	gem_read(fd,target_bo, 0, &actual_value, sizeof(actual_value));
	igt_assert_eq(expected_value, actual_value);

	gem_close(fd, first_level_bo);
	gem_close(fd, target_bo);
}

/* Be careful to take into account what register bits we can store and read
 * from...
 */
struct test_lri {
	const char *name; /* register name for debug info */
	uint32_t reg; /* address to test */
	uint32_t read_mask; /* ignore things like HW status bits */
	uint32_t init_val; /* initial identifiable value to set without LRI */
	uint32_t test_val; /* value to attempt loading via LRI command */
	bool whitelisted; /* expect to become NOOP / fail if not whitelisted */
	int min_ver; /* required command parser version to test */
};

static void
test_lri(int fd, uint32_t handle, struct test_lri *test)
{
	uint32_t lri[] = {
		MI_LOAD_REGISTER_IMM,
		test->reg,
		test->test_val,
		MI_BATCH_BUFFER_END,
	};
	int bad_lri_errno = parser_version >= 8 ? 0 : -EINVAL;
	int expected_errno = test->whitelisted ? 0 : bad_lri_errno;
	uint32_t expect = test->whitelisted ? test->test_val : test->init_val;

	igt_debug("Testing %s LRI: addr=%x, val=%x, expected errno=%d, expected val=%x\n",
		  test->name, test->reg, test->test_val,
		  expected_errno, expect);

	intel_register_write(test->reg, test->init_val);

	igt_assert_eq_u32((intel_register_read(test->reg) &
			   test->read_mask),
			  test->init_val);

	exec_batch(fd, handle,
		   lri, sizeof(lri),
		   I915_EXEC_RENDER,
		   expected_errno);
	gem_sync(fd, handle);

	igt_assert_eq_u32((intel_register_read(test->reg) &
			   test->read_mask),
			  expect);
}

static void test_allocations(int fd)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[17];
	unsigned long count;

	intel_require_memory(2, 1ull<<(12 + ARRAY_SIZE(obj)), CHECK_RAM);

	memset(obj, 0, sizeof(obj));
	for (int i = 0; i < ARRAY_SIZE(obj); i++) {
		uint64_t size = 1ull << (12 + i);

		obj[i].handle = gem_create(fd, size);
		for (uint64_t page = 4096; page <= size; page += 4096)
			gem_write(fd, obj[i].handle,
				  page - sizeof(bbe), &bbe, sizeof(bbe));
	}

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffer_count = 1;

	count = 0;
	igt_until_timeout(20) {
		int i = rand() % ARRAY_SIZE(obj);
		execbuf.buffers_ptr = to_user_pointer(&obj[i]);
		execbuf.batch_start_offset = (rand() % (1ull<<i)) << 12;
		execbuf.batch_start_offset += 64 * (rand() % 64);
		execbuf.batch_len = (1ull<<(12+i)) - execbuf.batch_start_offset;
		gem_execbuf(fd, &execbuf);
		count++;
	}
	igt_info("Submitted %lu execbufs\n", count);
	igt_drop_caches_set(fd, DROP_RESET_ACTIVE); /* Cancel the queued work */

	for (int i = 0; i < ARRAY_SIZE(obj); i++) {
		gem_sync(fd, obj[i].handle);
		gem_close(fd, obj[i].handle);
	}
}

static void hsw_load_register_reg(void)
{
	uint32_t init_gpr0[16] = {
		MI_LOAD_REGISTER_IMM | (3 - 2),
		HSW_CS_GPR0,
		0xabcdabc0, /* leave [1:0] zero */
		MI_BATCH_BUFFER_END,
	};
	uint32_t store_gpr0[16] = {
		MI_STORE_REGISTER_MEM | (3 - 2),
		HSW_CS_GPR0,
		0, /* reloc*/
		MI_BATCH_BUFFER_END,
	};
	uint32_t do_lrr[16] = {
		MI_LOAD_REGISTER_REG | (3 - 2),
		0, /* [1] = src */
		HSW_CS_GPR0, /* dst */
		MI_BATCH_BUFFER_END,
	};
	uint32_t allowed_regs[] = {
		HSW_CS_GPR1,
		SO_WRITE_OFFSET_0,
	};
	uint32_t disallowed_regs[] = {
		0,
		OACONTROL, /* filtered */
		DERRMR, /* master only */
		0x2038, /* RING_START: invalid */
	};
	int fd;
	uint32_t handle;
	int bad_lrr_errno = parser_version >= 8 ? 0 : -EINVAL;

	/* Open again to get a non-master file descriptor */
	fd = drm_open_driver(DRIVER_INTEL);

	igt_require(IS_HASWELL(intel_get_drm_devid(fd)));
	igt_require(parser_version >= 7);

	handle = gem_create(fd, 4096);

	for (int i = 0 ; i < ARRAY_SIZE(allowed_regs); i++) {
		uint32_t var;

		exec_batch(fd, handle, init_gpr0, sizeof(init_gpr0),
			   I915_EXEC_RENDER,
			   0);
		exec_batch_patched(fd, handle,
				   store_gpr0, sizeof(store_gpr0),
				   2 * sizeof(uint32_t), /* reloc */
				   0xabcdabc0);
		do_lrr[1] = allowed_regs[i];
		exec_batch(fd, handle, do_lrr, sizeof(do_lrr),
			   I915_EXEC_RENDER,
			   0);
		var = __exec_batch_patched(fd, handle,
					   store_gpr0, sizeof(store_gpr0),
					   2 * sizeof(uint32_t)); /* reloc */
		igt_assert_neq(var, 0xabcdabc0);
	}

	for (int i = 0 ; i < ARRAY_SIZE(disallowed_regs); i++) {
		exec_batch(fd, handle, init_gpr0, sizeof(init_gpr0),
			   I915_EXEC_RENDER,
			   0);
		exec_batch_patched(fd, handle,
				   store_gpr0, sizeof(store_gpr0),
				   2 * sizeof(uint32_t), /* reloc */
				   0xabcdabc0);
		do_lrr[1] = disallowed_regs[i];
		exec_batch(fd, handle, do_lrr, sizeof(do_lrr),
			   I915_EXEC_RENDER,
			   bad_lrr_errno);
		exec_batch_patched(fd, handle,
				   store_gpr0, sizeof(store_gpr0),
				   2 * sizeof(uint32_t), /* reloc */
				   0xabcdabc0);
	}

	close(fd);
}

igt_main
{
	uint32_t handle;
	int fd;

	igt_fixture {
		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);

		parser_version = command_parser_version(fd);
		igt_require(parser_version != -1);

		igt_require(gem_uses_ppgtt(fd));

		handle = gem_create(fd, 4096);

		/* ATM cmd parser only exists on gen7. */
		igt_require(intel_gen(intel_get_drm_devid(fd)) == 7);
		igt_fork_hang_detector(fd);
	}

	igt_subtest("basic-allowed") {
		uint32_t pc[] = {
			GFX_OP_PIPE_CONTROL,
			PIPE_CONTROL_QW_WRITE,
			0, /* To be patched */
			0x12000000,
			0,
			MI_BATCH_BUFFER_END,
		};
		exec_batch_patched(fd, handle,
				   pc, sizeof(pc),
				   8, /* patch offset, */
				   0x12000000);
	}

	igt_subtest("basic-rejected") {
		uint32_t invalid_cmd[] = {
			INSTR_INVALID_CLIENT << INSTR_CLIENT_SHIFT,
			MI_BATCH_BUFFER_END,
		};
		uint32_t invalid_set_context[] = {
			MI_SET_CONTEXT | 32, /* invalid length */
			MI_BATCH_BUFFER_END,
		};
		exec_batch(fd, handle,
			   invalid_cmd, sizeof(invalid_cmd),
			   I915_EXEC_RENDER,
			   -EINVAL);
		exec_batch(fd, handle,
			   invalid_cmd, sizeof(invalid_cmd),
			   I915_EXEC_BSD,
			   -EINVAL);
		if (gem_has_blt(fd)) {
			exec_batch(fd, handle,
				   invalid_cmd, sizeof(invalid_cmd),
				   I915_EXEC_BLT,
				   -EINVAL);
		}
		if (gem_has_vebox(fd)) {
			exec_batch(fd, handle,
				   invalid_cmd, sizeof(invalid_cmd),
				   I915_EXEC_VEBOX,
				   -EINVAL);
		}

		exec_batch(fd, handle,
			   invalid_set_context, sizeof(invalid_set_context),
			   I915_EXEC_RENDER,
			   -EINVAL);
	}

	igt_subtest("basic-allocation") {
		test_allocations(fd);
	}

	igt_subtest_group {
#define REG(R, MSK, INI, V, OK, MIN_V) { #R, R, MSK, INI, V, OK, MIN_V }
		struct test_lri lris[] = {
			/* dummy head pointer */
			REG(OASTATUS2,
			    0xffffff80, 0xdeadf000, 0xbeeff000, false, 0),
			/* NB: [1:0] MBZ */
			REG(SO_WRITE_OFFSET_0,
			    0xfffffffc, 0xabcdabc0, 0xbeefbee0, true, 0),

			/* It's really important for us to check that
			 * an LRI to OACONTROL doesn't result in an
			 * EINVAL error because Mesa attempts writing
			 * to OACONTROL to determine what extensions to
			 * expose and will abort() for execbuffer()
			 * errors.
			 *
			 * Mesa can gracefully recognise and handle the
			 * LRI becoming a NOOP.
			 *
			 * The test values represent dummy context IDs
			 * while leaving the OA unit disabled
			 */
			REG(OACONTROL,
			    0xfffff000, 0xfeed0000, 0x31337000, false, 9)
		};
#undef REG

		igt_fixture {
			intel_register_access_init(intel_get_pci_device(), 0, fd);
		}

		for (int i = 0; i < ARRAY_SIZE(lris); i++) {
			igt_subtest_f("test-lri-%s", lris[i].name) {
				igt_require_f(parser_version >= lris[i].min_ver,
					      "minimum required parser version for test = %d\n",
					      lris[i].min_ver);
				test_lri(fd, handle, lris + i);
			}
		}

		igt_fixture {
			intel_register_access_fini();
		}
	}

	igt_subtest("bitmasks") {
		uint32_t pc[] = {
			GFX_OP_PIPE_CONTROL,
			(PIPE_CONTROL_QW_WRITE |
			 PIPE_CONTROL_LRI_POST_OP),
			0, /* To be patched */
			0x12000000,
			0,
			MI_BATCH_BUFFER_END,
		};
		if (parser_version >= 8) {
			/* Expect to read back zero since the command should be
			 * squashed to a NOOP
			 */
			exec_batch_patched(fd, handle,
					   pc, sizeof(pc),
					   8, /* patch offset, */
					   0x0);
		} else {
			exec_batch(fd, handle,
				   pc, sizeof(pc),
				   I915_EXEC_RENDER,
				   -EINVAL);
		}
	}

	igt_subtest("batch-without-end") {
		uint32_t noop[1024] = { 0 };
		exec_batch(fd, handle,
			   noop, sizeof(noop),
			   I915_EXEC_RENDER,
			   -EINVAL);
	}

	igt_subtest("cmd-crossing-page") {
		uint32_t lri_ok[] = {
			MI_LOAD_REGISTER_IMM,
			SO_WRITE_OFFSET_0, /* allowed register address */
			0xdcbaabc0, /* [1:0] MBZ */
			MI_BATCH_BUFFER_END,
		};
		uint32_t store_reg[] = {
			MI_STORE_REGISTER_MEM | (3 - 2),
			SO_WRITE_OFFSET_0,
			0, /* reloc */
			MI_BATCH_BUFFER_END,
		};
		exec_split_batch(fd,
				 lri_ok, sizeof(lri_ok),
				 I915_EXEC_RENDER,
				 0);
		exec_batch_patched(fd, handle,
				   store_reg,
				   sizeof(store_reg),
				   2 * sizeof(uint32_t), /* reloc */
				   0xdcbaabc0);
	}

	igt_subtest("oacontrol-tracking") {
		uint32_t lri_ok[] = {
			MI_LOAD_REGISTER_IMM,
			OACONTROL,
			0x31337000,
			MI_LOAD_REGISTER_IMM,
			OACONTROL,
			0x0,
			MI_BATCH_BUFFER_END,
			0
		};
		uint32_t lri_bad[] = {
			MI_LOAD_REGISTER_IMM,
			OACONTROL,
			0x31337000,
			MI_BATCH_BUFFER_END,
		};
		uint32_t lri_extra_bad[] = {
			MI_LOAD_REGISTER_IMM,
			OACONTROL,
			0x31337000,
			MI_LOAD_REGISTER_IMM,
			OACONTROL,
			0x0,
			MI_LOAD_REGISTER_IMM,
			OACONTROL,
			0x31337000,
			MI_BATCH_BUFFER_END,
		};

		igt_require(parser_version < 9);

		exec_batch(fd, handle,
			   lri_ok, sizeof(lri_ok),
			   I915_EXEC_RENDER,
			   0);
		exec_batch(fd, handle,
			   lri_bad, sizeof(lri_bad),
			   I915_EXEC_RENDER,
			   -EINVAL);
		exec_batch(fd, handle,
			   lri_extra_bad, sizeof(lri_extra_bad),
			   I915_EXEC_RENDER,
			   -EINVAL);
	}

	igt_subtest("chained-batch") {
		uint32_t pc[] = {
			GFX_OP_PIPE_CONTROL,
			PIPE_CONTROL_QW_WRITE,
			0, /* To be patched */
			0x12000000,
			0,
			MI_BATCH_BUFFER_END,
		};
		exec_batch_chained(fd, handle,
				   pc, sizeof(pc),
				   8, /* patch offset, */
				   0x12000000);
	}

	igt_subtest("load-register-reg")
		hsw_load_register_reg();

	igt_fixture {
		igt_stop_hang_detector();
		gem_close(fd, handle);

		close(fd);
	}
}
