/*
 * Copyright © 2007 Intel Corporation
 * Copyright © 2009 Intel Corporation
 * Copyright © 2010 Intel Corporation
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
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *    Carl Worth <cworth@cworth.org>
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

/** @file intel_decode.c
 * This file contains code to print out batchbuffer contents in a
 * human-readable format.
 *
 * The current version only supports i915 packets, and only pretty-prints a
 * subset of them.  The intention is for it to make just a best attempt to
 * decode, but never crash in the process.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/stat.h>
#include <err.h>
#include <assert.h>
#include <intel_bufmgr.h>
#include <zlib.h>
#include <ctype.h>

#include "intel_chipset.h"
#include "intel_io.h"
#include "instdone.h"
#include "intel_reg.h"
#include "drmtest.h"

static uint32_t
print_head(unsigned int reg)
{
	printf("    head = 0x%08x, wraps = %d\n", reg & (0x7ffff<<2), reg >> 21);
	return reg & (0x7ffff<<2);
}

static uint32_t
print_ctl(unsigned int reg)
{
	uint32_t ring_length = 	(((reg & (0x1ff << 12)) >> 12) + 1) * 4096;

#define BIT_STR(reg, x, on, off) ((1 << (x)) & reg) ? on : off

	printf("    len=%d%s%s%s\n", ring_length,
	       BIT_STR(reg, 0, ", enabled", ", disabled"),
	       BIT_STR(reg, 10, ", semaphore wait ", ""),
	       BIT_STR(reg, 11, ", rb wait ", "")
		);
#undef BIT_STR
	return ring_length;
}

static void
print_acthd(unsigned int reg, unsigned int ring_length)
{
	if ((reg & (0x7ffff << 2)) < ring_length)
		printf("    at ring: 0x%08x\n", reg & (0x7ffff << 2));
	else
		printf("    at batch: 0x%08x\n", reg);
}

static void
print_instdone(uint32_t devid, unsigned int instdone, unsigned int instdone1)
{
	int i;
	static int once;

	if (!once) {
		if (!init_instdone_definitions(devid))
			return;
		once = 1;
	}

	for (i = 0; i < num_instdone_bits; i++) {
		int busy = 0;

		if (instdone_bits[i].reg == INSTDONE_1) {
			if (!(instdone1 & instdone_bits[i].bit))
				busy = 1;
		} else {
			if (!(instdone & instdone_bits[i].bit))
				busy = 1;
		}

		if (busy)
			printf("    busy: %s\n", instdone_bits[i].name);
	}
}

static void
print_i830_pgtbl_err(unsigned int reg)
{
	const char *str;

	switch((reg >> 3) & 0xf) {
	case 0x1: str = "Overlay TLB"; break;
	case 0x2: str = "Display A TLB"; break;
	case 0x3: str = "Host TLB"; break;
	case 0x4: str = "Render TLB"; break;
	case 0x5: str = "Display C TLB"; break;
	case 0x6: str = "Mapping TLB"; break;
	case 0x7: str = "Command Stream TLB"; break;
	case 0x8: str = "Vertex Buffer TLB"; break;
	case 0x9: str = "Display B TLB"; break;
	case 0xa: str = "Reserved System Memory"; break;
	case 0xb: str = "Compressor TLB"; break;
	case 0xc: str = "Binner TLB"; break;
	default: str = "unknown"; break;
	}

	if (str)
		printf("    source = %s\n", str);

	switch(reg & 0x7) {
	case 0x0: str  = "Invalid GTT"; break;
	case 0x1: str = "Invalid GTT PTE"; break;
	case 0x2: str = "Invalid Memory"; break;
	case 0x3: str = "Invalid TLB miss"; break;
	case 0x4: str = "Invalid PTE data"; break;
	case 0x5: str = "Invalid LocalMemory not present"; break;
	case 0x6: str = "Invalid Tiling"; break;
	case 0x7: str = "Host to CAM"; break;
	}
	printf("    error = %s\n", str);
}

static void
print_i915_pgtbl_err(unsigned int reg)
{
	if (reg & (1 << 29))
		printf("    Cursor A: Invalid GTT PTE\n");
	if (reg & (1 << 28))
		printf("    Cursor B: Invalid GTT PTE\n");
	if (reg & (1 << 27))
		printf("    MT: Invalid tiling\n");
	if (reg & (1 << 26))
		printf("    MT: Invalid GTT PTE\n");
	if (reg & (1 << 25))
		printf("    LC: Invalid tiling\n");
	if (reg & (1 << 24))
		printf("    LC: Invalid GTT PTE\n");
	if (reg & (1 << 23))
		printf("    BIN VertexData: Invalid GTT PTE\n");
	if (reg & (1 << 22))
		printf("    BIN Instruction: Invalid GTT PTE\n");
	if (reg & (1 << 21))
		printf("    CS VertexData: Invalid GTT PTE\n");
	if (reg & (1 << 20))
		printf("    CS Instruction: Invalid GTT PTE\n");
	if (reg & (1 << 19))
		printf("    CS: Invalid GTT\n");
	if (reg & (1 << 18))
		printf("    Overlay: Invalid tiling\n");
	if (reg & (1 << 16))
		printf("    Overlay: Invalid GTT PTE\n");
	if (reg & (1 << 14))
		printf("    Display C: Invalid tiling\n");
	if (reg & (1 << 12))
		printf("    Display C: Invalid GTT PTE\n");
	if (reg & (1 << 10))
		printf("    Display B: Invalid tiling\n");
	if (reg & (1 << 8))
		printf("    Display B: Invalid GTT PTE\n");
	if (reg & (1 << 6))
		printf("    Display A: Invalid tiling\n");
	if (reg & (1 << 4))
		printf("    Display A: Invalid GTT PTE\n");
	if (reg & (1 << 1))
		printf("    Host Invalid PTE data\n");
	if (reg & (1 << 0))
		printf("    Host Invalid GTT PTE\n");
}

static void
print_i965_pgtbl_err(unsigned int reg)
{
	if (reg & (1 << 26))
		printf("    Invalid Sampler Cache GTT entry\n");
	if (reg & (1 << 24))
		printf("    Invalid Render Cache GTT entry\n");
	if (reg & (1 << 23))
		printf("    Invalid Instruction/State Cache GTT entry\n");
	if (reg & (1 << 22))
		printf("    There is no ROC, this cannot occur!\n");
	if (reg & (1 << 21))
		printf("    Invalid GTT entry during Vertex Fetch\n");
	if (reg & (1 << 20))
		printf("    Invalid GTT entry during Command Fetch\n");
	if (reg & (1 << 19))
		printf("    Invalid GTT entry during CS\n");
	if (reg & (1 << 18))
		printf("    Invalid GTT entry during Cursor Fetch\n");
	if (reg & (1 << 17))
		printf("    Invalid GTT entry during Overlay Fetch\n");
	if (reg & (1 << 8))
		printf("    Invalid GTT entry during Display B Fetch\n");
	if (reg & (1 << 4))
		printf("    Invalid GTT entry during Display A Fetch\n");
	if (reg & (1 << 1))
		printf("    Valid PTE references illegal memory\n");
	if (reg & (1 << 0))
		printf("    Invalid GTT entry during fetch for host\n");
}

static void
print_pgtbl_err(unsigned int reg, unsigned int devid)
{
	if (IS_965(devid)) {
		return print_i965_pgtbl_err(reg);
	} else if (IS_GEN3(devid)) {
		return print_i915_pgtbl_err(reg);
	} else {
		return print_i830_pgtbl_err(reg);
	}
}

static void print_ivb_error(unsigned int reg, unsigned int devid)
{
	if (reg & (1 << 0))
		printf("    TLB page fault error (GTT entry not valid)\n");
	if (reg & (1 << 1))
		printf("    Invalid physical address in RSTRM interface (PAVP)\n");
	if (reg & (1 << 2))
		printf("    Invalid page directory entry error\n");
	if (reg & (1 << 3))
		printf("    Invalid physical address in ROSTRM interface (PAVP)\n");
	if (reg & (1 << 4))
		printf("    TLB page VTD translation generated an error\n");
	if (reg & (1 << 5))
		printf("    Invalid physical address in WRITE interface (PAVP)\n");
	if (reg & (1 << 6))
		printf("    Page directory VTD translation generated error\n");
	if (reg & (1 << 8))
		printf("    Cacheline containing a PD was marked as invalid\n");
	if (IS_HASWELL(devid) && (reg >> 10) & 0x1f)
		printf("    %d pending page faults\n", (reg >> 10) & 0x1f);
}

static void print_snb_error(unsigned int reg)
{
	if (reg & (1 << 0))
		printf("    TLB page fault error (GTT entry not valid)\n");
	if (reg & (1 << 1))
		printf("    Context page GTT translation generated a fault (GTT entry not valid)\n");
	if (reg & (1 << 2))
		printf("    Invalid page directory entry error\n");
	if (reg & (1 << 3))
		printf("    HWS page GTT translation generated a page fault (GTT entry not valid)\n");
	if (reg & (1 << 4))
		printf("    TLB page VTD translation generated an error\n");
	if (reg & (1 << 5))
		printf("    Context page VTD translation generated an error\n");
	if (reg & (1 << 6))
		printf("    Page directory VTD translation generated error\n");
	if (reg & (1 << 7))
		printf("    HWS page VTD translation generated an error\n");
	if (reg & (1 << 8))
		printf("    Cacheline containing a PD was marked as invalid\n");
}

static void print_bdw_error(unsigned int reg, unsigned int devid)
{
	print_ivb_error(reg, devid);

	if (reg & (1 << 10))
		printf("    Non WB memory type for Advanced Context\n");
	if (reg & (1 << 11))
		printf("    PASID not enabled\n");
	if (reg & (1 << 12))
		printf("    PASID boundary violation\n");
	if (reg & (1 << 13))
		printf("    PASID not valid\n");
	if (reg & (1 << 14))
		printf("    PASID was zero for untranslated request\n");
	if (reg & (1 << 15))
		printf("    Context was not marked as present when doing DMA\n");
}

static void
print_error(unsigned int reg, unsigned int devid)
{
	switch (intel_gen(devid)) {
	case 8: return print_bdw_error(reg, devid);
	case 7: return print_ivb_error(reg, devid);
	case 6: return print_snb_error(reg);
	}
}

static void
print_snb_fence(unsigned int devid, uint64_t fence)
{
	printf("    %svalid, %c-tiled, pitch: %i, start: 0x%08x, size: %u\n",
			fence & 1 ? "" : "in",
			fence & (1<<1) ? 'y' : 'x',
			(int)(((fence>>32)&0xfff)+1)*128,
			(uint32_t)fence & 0xfffff000,
			(uint32_t)(((fence>>32)&0xfffff000) - (fence&0xfffff000) + 4096));
}

static void
print_i965_fence(unsigned int devid, uint64_t fence)
{
	printf("    %svalid, %c-tiled, pitch: %i, start: 0x%08x, size: %u\n",
			fence & 1 ? "" : "in",
			fence & (1<<1) ? 'y' : 'x',
			(int)(((fence>>2)&0x1ff)+1)*128,
			(uint32_t)fence & 0xfffff000,
			(uint32_t)(((fence>>32)&0xfffff000) - (fence&0xfffff000) + 4096));
}

static void
print_i915_fence(unsigned int devid, uint64_t fence)
{
	unsigned tile_width;
	if ((fence & 12) && !IS_915(devid))
		tile_width = 128;
	else
		tile_width = 512;

	printf("    %svalid, %c-tiled, pitch: %i, start: 0x%08x, size: %i\n",
			fence & 1 ? "" : "in",
			fence & (1<<12) ? 'y' : 'x',
			(1<<((fence>>4)&0xf))*tile_width,
			(uint32_t)fence & 0xff00000,
			1<<(20 + ((fence>>8)&0xf)));
}

static void
print_i830_fence(unsigned int devid, uint64_t fence)
{
	printf("    %svalid, %c-tiled, pitch: %i, start: 0x%08x, size: %i\n",
			fence & 1 ? "" : "in",
			fence & (1<<12) ? 'y' : 'x',
			(1<<((fence>>4)&0xf))*128,
			(uint32_t)fence & 0x7f80000,
			1<<(19 + ((fence>>8)&0xf)));
}

static void
print_fence(unsigned int devid, uint64_t fence)
{
	if (IS_GEN6(devid) || IS_GEN7(devid)) {
		return print_snb_fence(devid, fence);
	} else if (IS_GEN4(devid) || IS_GEN5(devid)) {
		return print_i965_fence(devid, fence);
	} else if (IS_GEN3(devid)) {
		return print_i915_fence(devid, fence);
	} else {
		return print_i830_fence(devid, fence);
	}
}

static void
print_fault_reg(unsigned devid, uint32_t reg)
{
	const char *gen7_types[] = { "Page",
				     "Invalid PD",
				     "Unloaded PD",
				     "Invalid and Unloaded PD" };

	const char *gen8_types[] = { "PTE",
				     "PDE",
				     "PDPE",
				     "PML4E" };

	const char *engine[] = { "GFX", "MFX0", "MFX1", "VEBX",
				 "BLT", "Unknown", "Unknown", "Unknown" };

	if (intel_gen(devid) < 7)
		return;

	if (reg & (1 << 0))
		printf("    Valid\n");
	else
		return;

	if (intel_gen(devid) < 8)
		printf("    %s Fault (%s)\n", gen7_types[reg >> 1 & 0x3],
		       reg & (1 << 11) ? "GGTT" : "PPGTT");
	else
		printf("    Invalid %s Fault\n", gen8_types[reg >> 1 & 0x3]);

	if (intel_gen(devid) < 8)
		printf("    Address 0x%08x\n", reg & ~((1 << 12)-1));
	else
		printf("    Engine %s\n", engine[reg >> 12 & 0x7]);

	printf("    Source ID %d\n", reg >> 3 & 0xff);
}

static void
print_fault_data(unsigned devid, uint32_t data1, uint32_t data0)
{
	uint64_t address;

	if (intel_gen(devid) < 8)
		return;

	address = ((uint64_t)(data0) << 12) | ((uint64_t)data1 & 0xf) << 44;
	printf("    Address 0x%016" PRIx64 " %s\n", address,
	       data1 & (1 << 4) ? "GGTT" : "PPGTT");
}

#define MAX_RINGS 10 /* I really hope this never... */

static bool maybe_ascii(const void *data, int check)
{
	const char *c = data;
	while (check--) {
		if (!isprint(*c++))
			return false;
	}
	return true;
}

static void decode(struct drm_intel_decode *ctx,
		   const char *buffer_name,
		   const char *ring_name,
		   uint64_t gtt_offset,
		   uint32_t head_offset,
		   uint32_t *data, int *count,
		   int decode)
{
	if (!*count)
		return;

	printf("%s (%s) at 0x%08x_%08x", buffer_name, ring_name,
	       (unsigned)(gtt_offset >> 32),
	       (unsigned)(gtt_offset & 0xffffffff));
	if (head_offset != -1)
		printf("; HEAD points to: 0x%08x_%08x",
		       (unsigned)((head_offset + gtt_offset) >> 32),
		       (unsigned)((head_offset + gtt_offset) & 0xffffffff));
	printf("\n");

	if (decode) {
		drm_intel_decode_set_batch_pointer(ctx, data, gtt_offset,
						   *count);
		drm_intel_decode(ctx);
	} else if (maybe_ascii(data, 16)) {
		printf("%*s\n", 4 * *count, (char *)data);
	} else {
		for (int i = 0; i + 4 <= *count; i += 4)
			printf("[%04x] %08x %08x %08x %08x\n",
			       4*i, data[i], data[i+1], data[i+2], data[i+3]);
	}
	*count = 0;
}

static int zlib_inflate(uint32_t **ptr, int len)
{
	struct z_stream_s zstream;
	void *out;

	memset(&zstream, 0, sizeof(zstream));

	zstream.next_in = (unsigned char *)*ptr;
	zstream.avail_in = 4*len;

	if (inflateInit(&zstream) != Z_OK)
		return 0;

	out = malloc(128*4096); /* approximate obj size */
	zstream.next_out = out;
	zstream.avail_out = 128*4096;

	do {
		switch (inflate(&zstream, Z_SYNC_FLUSH)) {
		case Z_STREAM_END:
			goto end;
		case Z_OK:
			break;
		default:
			inflateEnd(&zstream);
			return 0;
		}

		if (zstream.avail_out)
			break;

		out = realloc(out, 2*zstream.total_out);
		if (out == NULL) {
			inflateEnd(&zstream);
			return 0;
		}

		zstream.next_out = (unsigned char *)out + zstream.total_out;
		zstream.avail_out = zstream.total_out;
	} while (1);
end:
	inflateEnd(&zstream);
	free(*ptr);
	*ptr = out;
	return zstream.total_out / 4;
}

static int ascii85_decode(const char *in, uint32_t **out, bool inflate)
{
	int len = 0, size = 1024;

	*out = realloc(*out, sizeof(uint32_t)*size);
	if (*out == NULL)
		return 0;

	while (*in >= '!' && *in <= 'z') {
		uint32_t v = 0;

		if (len == size) {
			size *= 2;
			*out = realloc(*out, sizeof(uint32_t)*size);
			if (*out == NULL)
				return 0;
		}

		if (*in == 'z') {
			in++;
		} else {
			v += in[0] - 33; v *= 85;
			v += in[1] - 33; v *= 85;
			v += in[2] - 33; v *= 85;
			v += in[3] - 33; v *= 85;
			v += in[4] - 33;
			in += 5;
		}
		(*out)[len++] = v;
	}

	if (!inflate)
		return len;

	return zlib_inflate(out, len);
}

static void
read_data_file(FILE *file)
{
	struct drm_intel_decode *decode_ctx = NULL;
	uint32_t devid = PCI_CHIP_I855_GM;
	uint32_t *data = NULL;
	uint32_t head[MAX_RINGS];
	int head_idx = 0;
	int num_rings = 0;
	long long unsigned fence;
	int data_size = 0, count = 0, matched;
	char *line = NULL;
	size_t line_size;
	uint32_t offset, value, ring_length = 0;
	uint64_t gtt_offset = 0;
	uint32_t head_offset = -1;
	const char *buffer_name = "batch buffer";
	char *ring_name = NULL;
	int do_decode = 1;

	while (getline(&line, &line_size, file) > 0) {
		char *dashes;

		if (line[0] == ':' || line[0] == '~') {
			count = ascii85_decode(line+1, &data, line[0] == ':');
			if (count == 0)
				fprintf(stderr, "ASCII85 decode failed (%s - %s).\n",
					ring_name, buffer_name);
			decode(decode_ctx,
			       buffer_name, ring_name,
			       gtt_offset, head_offset,
			       data, &count, do_decode);
			continue;
		}

		dashes = strstr(line, "---");
		if (dashes) {
			const struct {
				const char *match;
				const char *name;
				int do_decode;
			} buffers[] = {
				{ "ringbuffer", "ring", 1 },
				{ "gtt_offset", "batch", 1 },
				{ "hw context", "HW context", 1 },
				{ "hw status", "HW status", 0 },
				{ "wa context", "WA context", 1 },
				{ "wa batchbuffer", "WA batch", 1 },
				{ "user", "user", 0 },
				{ "semaphores", "semaphores", 0 },
				{ "guc log buffer", "GuC log", 0 },
				{ },
			}, *b;
			char *new_ring_name;

			new_ring_name = malloc(dashes - line);
			strncpy(new_ring_name, line, dashes - line);
			new_ring_name[dashes - line - 1] = '\0';

			decode(decode_ctx,
			       buffer_name, ring_name,
			       gtt_offset, head_offset,
			       data, &count, do_decode);
			gtt_offset = 0;
			head_offset = -1;

			free(ring_name);
			ring_name = new_ring_name;

			dashes += 4;
			for (b = buffers; b->match; b++) {
				uint32_t lo, hi;

				if (strncasecmp(dashes, b->match,
						strlen(b->match)))
					continue;

				dashes = strchr(dashes, '=');
				if (!dashes)
					break;

				matched = sscanf(dashes, "= 0x%08x %08x\n",
						 &hi, &lo);
				if (matched > 0) {
					gtt_offset = hi;
					if (matched == 2) {
						gtt_offset <<= 32;
						gtt_offset |= lo;
					}
				}

				do_decode = b->do_decode;
				buffer_name = b->name;
				if (b == buffers)
					head_offset = head[head_idx++];
				break;
			}

			continue;
		}

		matched = sscanf(line, "%08x : %08x", &offset, &value);
		if (matched != 2) {
			unsigned int reg, reg2;

			/* display reg section is after the ringbuffers, don't mix them */
			decode(decode_ctx,
			       buffer_name, ring_name,
			       gtt_offset, head_offset,
			       data, &count, do_decode);

			printf("%s", line);

			matched = sscanf(line, "PCI ID: 0x%04x\n", &reg);
			if (matched == 0)
				matched = sscanf(line, " PCI ID: 0x%04x\n", &reg);
			if (matched == 0) {
				const char *pci_id_start = strstr(line, "PCI ID");
				if (pci_id_start)
					matched = sscanf(pci_id_start, "PCI ID: 0x%04x\n", &reg);
			}
			if (matched == 1) {
				devid = reg;
				printf("Detected GEN%i chipset\n",
						intel_gen(devid));

				decode_ctx = drm_intel_decode_context_alloc(devid);
			}

			matched = sscanf(line, "  CTL: 0x%08x\n", &reg);
			if (matched == 1)
				ring_length = print_ctl(reg);

			matched = sscanf(line, "  HEAD: 0x%08x\n", &reg);
			if (matched == 1) {
				head[num_rings++] = print_head(reg);
			}

			matched = sscanf(line, "  ACTHD: 0x%08x\n", &reg);
			if (matched == 1) {
				print_acthd(reg, ring_length);
				drm_intel_decode_set_head_tail(decode_ctx, reg, 0xffffffff);
			}

			matched = sscanf(line, "  PGTBL_ER: 0x%08x\n", &reg);
			if (matched == 1 && reg)
				print_pgtbl_err(reg, devid);

			matched = sscanf(line, "  ERROR: 0x%08x\n", &reg);
			if (matched == 1 && reg)
				print_error(reg, devid);

			matched = sscanf(line, "  INSTDONE: 0x%08x\n", &reg);
			if (matched == 1)
				print_instdone(devid, reg, -1);

			matched = sscanf(line, "  INSTDONE1: 0x%08x\n", &reg);
			if (matched == 1)
				print_instdone(devid, -1, reg);

			matched = sscanf(line, "  fence[%i] = %Lx\n", &reg, &fence);
			if (matched == 2)
				print_fence(devid, fence);

			matched = sscanf(line, "  FAULT_REG: 0x%08x\n", &reg);
			if (matched == 1 && reg)
				print_fault_reg(devid, reg);

			matched = sscanf(line, "  FAULT_TLB_DATA: 0x%08x 0x%08x\n", &reg, &reg2);
			if (matched == 2)
				print_fault_data(devid, reg, reg2);

			continue;
		}

		count++;

		if (count > data_size) {
			data_size = data_size ? data_size * 2 : 1024;
			data = realloc(data, data_size * sizeof (uint32_t));
			if (data == NULL) {
				fprintf(stderr, "Out of memory.\n");
				exit(1);
			}
		}

		data[count-1] = value;
	}

	decode(decode_ctx,
	       buffer_name, ring_name,
	       gtt_offset, head_offset,
	       data, &count, do_decode);

	free(data);
	free(line);
	free(ring_name);
}

static void setup_pager(void)
{
	int fds[2];

	if (pipe(fds) == -1)
		return;

	switch (fork()) {
	case -1:
		break;
	case 0:
		close(fds[1]);
		dup2(fds[0], 0);
		execlp("less", "less", "-FRSi", NULL);
		break;

	default:
		close(fds[0]);
		dup2(fds[1], 1);
		close(fds[1]);
		break;
	}
}

int
main(int argc, char *argv[])
{
	FILE *file;
	const char *path;
	char *filename = NULL;
	struct stat st;
	int error;

	if (argc > 2) {
		fprintf(stderr,
				"intel_gpu_decode: Parse an Intel GPU i915_error_state\n"
				"Usage:\n"
				"\t%s [<file>]\n"
				"\n"
				"With no arguments, debugfs-dri-directory is probed for in "
				"/debug and \n"
				"/sys/kernel/debug.  Otherwise, it may be "
				"specified.  If a file is given,\n"
				"it is parsed as an GPU dump in the format of "
				"/debug/dri/0/i915_error_state.\n",
				argv[0]);
		return 1;
	}

	if (isatty(1))
		setup_pager();

	if (argc == 1) {
		if (isatty(0)) {
			path = "/sys/class/drm/card0/error";
			error = stat(path, &st);
			if (error != 0) {
				path = "/debug/dri";
				error = stat(path, &st);
			}
			if (error != 0) {
				path = "/sys/kernel/debug/dri";
				error = stat(path, &st);
			}
			if (error != 0) {
				errx(1,
				     "Couldn't find i915 debugfs directory.\n\n"
				     "Is debugfs mounted? You might try mounting it with a command such as:\n\n"
				     "\tsudo mount -t debugfs debugfs /sys/kernel/debug\n");
			}
		} else {
			read_data_file(stdin);
			exit(0);
		}
	} else {
		path = argv[1];
		error = stat(path, &st);
		if (error != 0) {
			fprintf(stderr, "Error opening %s: %s\n",
					path, strerror(errno));
			exit(1);
		}
	}

	if (S_ISDIR(st.st_mode)) {
		int ret;

		ret = asprintf(&filename, "%s/i915_error_state", path);
		assert(ret > 0);
		file = fopen(filename, "r");
		if (!file) {
			int minor;
			for (minor = 0; minor < 64; minor++) {
				free(filename);
				ret = asprintf(&filename, "%s/%d/i915_error_state", path, minor);
				assert(ret > 0);

				file = fopen(filename, "r");
				if (file)
					break;
			}
		}
		if (!file) {
			fprintf(stderr, "Failed to find i915_error_state beneath %s\n",
					path);
			exit (1);
		}
	} else {
		file = fopen(path, "r");
		if (!file) {
			fprintf(stderr, "Failed to open %s: %s\n",
					path, strerror(errno));
			exit (1);
		}
	}

	read_data_file(file);
	fclose(file);

	if (filename != path)
		free(filename);

	return 0;
}

/* vim: set ts=8 sw=8 tw=0 cino=:0,(0 noet :*/
