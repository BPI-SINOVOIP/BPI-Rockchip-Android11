/*
 * Copyright © 2008 Intel Corporation
 * Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include "config.h"

#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#ifdef HAVE_STRUCT_SYSINFO_TOTALRAM
#include <sys/sysinfo.h>
#elif defined(HAVE_SWAPCTL) /* Solaris */
#include <sys/swap.h>
#endif
#include <sys/resource.h>

#include "intel_io.h"
#include "drmtest.h"
#include "igt_aux.h"
#include "igt_debugfs.h"
#include "igt_sysfs.h"

/**
 * intel_get_total_ram_mb:
 *
 * Returns:
 * The total amount of system RAM available in MB.
 */
uint64_t
intel_get_total_ram_mb(void)
{
	uint64_t retval;

#ifdef HAVE_STRUCT_SYSINFO_TOTALRAM /* Linux */
	struct sysinfo sysinf;

	igt_assert(sysinfo(&sysinf) == 0);
	retval = sysinf.totalram;
	retval *= sysinf.mem_unit;
#elif defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES) /* Solaris */
	long pagesize, npages;

	pagesize = sysconf(_SC_PAGESIZE);
        npages = sysconf(_SC_PHYS_PAGES);

	retval = (uint64_t) pagesize * npages;
#else
#error "Unknown how to get RAM size for this OS"
#endif

	return retval / (1024*1024);
}

static uint64_t get_meminfo(const char *info, const char *tag)
{
	const char *str;
	unsigned long val;

	str = strstr(info, tag);
	if (str && sscanf(str + strlen(tag), " %lu", &val) == 1)
		return (uint64_t)val << 10;

	igt_warn("Unrecognised /proc/meminfo field: '%s'\n", tag);
	return 0;
}

/**
 * intel_get_avail_ram_mb:
 *
 * Returns:
 * The amount of unused system RAM available in MB.
 */
uint64_t
intel_get_avail_ram_mb(void)
{
	uint64_t retval;

#ifdef HAVE_STRUCT_SYSINFO_TOTALRAM /* Linux */
	char *info;
	int fd;

	fd = drm_open_driver(DRIVER_INTEL);
	intel_purge_vm_caches(fd);
	close(fd);

	fd = open("/proc", O_RDONLY);
	info = igt_sysfs_get(fd, "meminfo");
	close(fd);

	if (info) {
		retval  = get_meminfo(info, "MemAvailable:");
		retval += get_meminfo(info, "Buffers:");
		/*
		 * Include the file+swap cache as "available" for the test.
		 * We believe that we can revoke these pages back to their
		 * on disk counterpart, with no loss of functionality while
		 * the test runs using those pages for ourselves without the
		 * test itself being swapped to disk.
		 */
		retval += get_meminfo(info, "Cached:");
		retval += get_meminfo(info, "SwapCached:");
		free(info);
	} else {
		struct sysinfo sysinf;

		igt_assert(sysinfo(&sysinf) == 0);
		retval = sysinf.freeram;
		retval += min(sysinf.freeswap, sysinf.bufferram);
		retval *= sysinf.mem_unit;
	}
#elif defined(_SC_PAGESIZE) && defined(_SC_AVPHYS_PAGES) /* Solaris */
	long pagesize, npages;

	pagesize = sysconf(_SC_PAGESIZE);
        npages = sysconf(_SC_AVPHYS_PAGES);

	retval = (uint64_t) pagesize * npages;
#else
#error "Unknown how to get available RAM for this OS"
#endif

	return retval / (1024*1024);
}

/**
 * intel_get_total_swap_mb:
 *
 * Returns:
 * The total amount of swap space available in MB.
 */
uint64_t
intel_get_total_swap_mb(void)
{
	uint64_t retval;

#ifdef HAVE_STRUCT_SYSINFO_TOTALRAM /* Linux */
	struct sysinfo sysinf;

	igt_assert(sysinfo(&sysinf) == 0);
	retval = sysinf.freeswap;
	retval *= sysinf.mem_unit;
#elif defined(HAVE_SWAPCTL) /* Solaris */
	long pagesize = sysconf(_SC_PAGESIZE);
	uint64_t totalpages = 0;
	swaptbl_t *swt;
	char *buf;
	int n, i;

	if ((n = swapctl(SC_GETNSWP, NULL)) == -1) {
	    igt_warn("swapctl: GETNSWP");
	    return 0;
	}
	if (n == 0) {
	    /* no error, but no swap devices either */
	    return 0;
	}

	swt = malloc(sizeof(struct swaptable) + (n * sizeof(swapent_t)));
	buf = malloc(n * MAXPATHLEN);
	if (!swt || !buf) {
	    igt_warn("malloc");
	} else {
	    swt->swt_n = n;
	    for (i = 0 ; i < n; i++) {
		swt->swt_ent[i].ste_path = buf + (i * MAXPATHLEN);
	    }

	    if ((n = swapctl(SC_LIST, swt)) == -1) {
		igt_warn("swapctl: LIST");
	    } else {
		for (i = 0; i < swt->swt_n; i++) {
		    totalpages += swt->swt_ent[i].ste_pages;
		}
	    }
	}
	free(swt);
	free(buf);

	retval = (uint64_t) pagesize * totalpages;
#else
#warning "Unknown how to get swap size for this OS"
	return 0;
#endif

	return retval / (1024*1024);
}

/**
 * intel_get_total_pinnable_mem:
 *
 * Compute the amount of memory that we're able to safely lock.
 * Note that in order to achieve this, we're attempting to repeatedly lock more
 * and more memory, which is a time consuming process.
 *
 * Returns: Amount of memory that can be safely pinned, in bytes.
 */
void *intel_get_total_pinnable_mem(size_t *total)
{
	uint64_t *can_mlock, pin, avail;

	pin = (intel_get_total_ram_mb() + 1) << 20;
	avail = (intel_get_avail_ram_mb() + 1) << 20;

	can_mlock = mmap(NULL, pin, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	igt_require(can_mlock != MAP_FAILED);

	/*
	 * We can reasonably assume that we should be able to lock at
	 * least 3/4 of available RAM
	 */
	*can_mlock = (avail >> 1) + (avail >> 2);
	if (mlock(can_mlock, *can_mlock)) {
		munmap(can_mlock, pin);
		return MAP_FAILED;
	}

	for (uint64_t inc = 1024 << 20; inc >= 4 << 10; inc >>= 2) {
		uint64_t locked = *can_mlock;

		igt_debug("Testing mlock %'"PRIu64"B (%'"PRIu64"MiB) + %'"PRIu64"B\n",
			  locked, locked >> 20, inc);

		igt_fork(child, 1) {
			uint64_t bytes = *can_mlock;

			while (bytes <= pin) {
				if (mlock((void *)can_mlock + bytes, inc))
					break;

				*can_mlock = bytes += inc;
				__sync_synchronize();
			}
		}
		__igt_waitchildren();

		if (*can_mlock > locked + inc) { /* Weird bit of mm/ lore */
			*can_mlock -= inc;
			igt_debug("Claiming mlock %'"PRIu64"B (%'"PRIu64"MiB)\n",
				  *can_mlock, *can_mlock >> 20);
			igt_assert(!mlock((void *)can_mlock + locked,
					  *can_mlock - locked));
		}
	}

	*total = pin;
	return can_mlock;
}

static unsigned max_open_files(void)
{
	struct rlimit rlim;

	if (getrlimit(RLIMIT_NOFILE, &rlim))
		rlim.rlim_cur = 64 << 10;

	return rlim.rlim_cur;
}

/**
 * intel_require_files:
 * @count: number of files that will be created
 *
 * Does the system support enough file descriptors for the test?
 */
void intel_require_files(uint64_t count)
{
	igt_require_f(count < max_open_files(),
		      "Estimated that we need %'llu files, but the process maximum is only %'llu\n",
		      (long long)count, (long long)max_open_files());
}

int __intel_check_memory(uint64_t count, uint64_t size, unsigned mode,
			 uint64_t *out_required, uint64_t *out_total)
{
/* rough estimate of how many bytes the kernel requires to track each object */
#define KERNEL_BO_OVERHEAD 512
	uint64_t required, total;

	required = count;
	required *= size + KERNEL_BO_OVERHEAD;
	required = ALIGN(required, 4096);

	igt_debug("Checking %'llu surfaces of size %'llu bytes (total %'llu) against %s%s\n",
		  (long long)count, (long long)size, (long long)required,
		  mode & (CHECK_RAM | CHECK_SWAP) ? "RAM" : "",
		  mode & CHECK_SWAP ? " + swap": "");

	total = 0;
	if (mode & (CHECK_RAM | CHECK_SWAP))
		total += intel_get_avail_ram_mb();
	if (mode & CHECK_SWAP)
		total += intel_get_total_swap_mb();
	total *= 1024 * 1024;

	if (out_required)
		*out_required = required;

	if (out_total)
		*out_total = total;

	if (count > vfs_file_max())
		return false;

	return required < total;
}

/**
 * intel_require_memory:
 * @count: number of surfaces that will be created
 * @size: the size in bytes of each surface
 * @mode: a bit field declaring whether the test will be run in RAM or in SWAP
 *
 * Computes the total amount of memory required to allocate @count surfaces,
 * each of @size bytes, and includes an estimate for kernel overhead. It then
 * queries the kernel for the available amount of memory on the system (either
 * RAM and/or SWAP depending upon @mode) and determines whether there is
 * sufficient to run the test.
 *
 * Most tests should check that there is enough RAM to hold their working set.
 * The rare swap thrashing tests should check that there is enough RAM + SWAP
 * for their tests. oom-killer tests should only run if this reports that
 * there is not enough RAM + SWAP!
 *
 * If there is not enough RAM this function calls igt_skip with an appropriate
 * message. It only ever returns if the requirement is fulfilled. This function
 * also causes the test to be skipped automatically on simulation under the
 * assumption that any test that needs to check for memory requirements is a
 * thrashing test unsuitable for slow simulated systems.
 */
void intel_require_memory(uint64_t count, uint64_t size, unsigned mode)
{
	uint64_t required, total;
	bool sufficient_memory;

	igt_skip_on_simulation();

	sufficient_memory = __intel_check_memory(count, size, mode,
						 &required, &total);
	if (!sufficient_memory) {
		int dir = open("/proc", O_RDONLY);
		char *info;

		info = igt_sysfs_get(dir, "meminfo");
		if (info) {
			igt_warn("Insufficient free memory; /proc/meminfo:\n%s",
				 info);
			free(info);
		}

		info = igt_sysfs_get(dir, "slabinfo");
		if (info) {
			igt_warn("Insufficient free memory; /proc/slabinfo:\n%s",
				 info);
			free(info);
		}

		close(dir);
	}

	igt_require_f(sufficient_memory,
		      "Estimated that we need %'llu objects and %'llu MiB for the test, but only have %'llu MiB available (%s%s) and a maximum of %'llu objects\n",
		      (long long)count,
		      (long long)((required + ((1<<20) - 1)) >> 20),
		      (long long)(total >> 20),
		      mode & (CHECK_RAM | CHECK_SWAP) ? "RAM" : "",
		      mode & CHECK_SWAP ? " + swap": "",
		      (long long)vfs_file_max());
}

void intel_purge_vm_caches(int drm_fd)
{
	int fd;

	fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
	if (fd >= 0) {
		/*
		 * BIT(2): Be quiet. Cannot be combined with other operations,
		 * the sysctl has a max value of 4.
		 */
		igt_ignore_warn(write(fd, "4\n", 2));
		close(fd);
	}

	for (int loop = 0; loop < 2; loop++) {
		igt_drop_caches_set(drm_fd,
				    DROP_SHRINK_ALL | DROP_IDLE | DROP_FREED);

		fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
		if (fd < 0)
			continue;

		/*
		 * BIT(0): Drop page cache
		 * BIT(1): Drop slab cache
		 */
		igt_ignore_warn(write(fd, "3\n", 2));
		close(fd);
	}

	errno = 0;
}

/*
 * When testing a port to a new platform, create a standalone test binary
 * by running:
 * cc -o porttest intel_drm.c -I.. -DSTANDALONE_TEST `pkg-config --cflags libdrm`
 * and then running the resulting porttest program.
 */
#ifdef STANDALONE_TEST
void *mmio;

int main(int argc, char **argv)
{
    igt_info("Total RAM:  %"PRIu64" Mb\n", intel_get_total_ram_mb());
    igt_info("Total Swap: %"PRIu64" Mb\n", intel_get_total_swap_mb());

    return 0;
}
#endif /* STANDALONE_TEST */
