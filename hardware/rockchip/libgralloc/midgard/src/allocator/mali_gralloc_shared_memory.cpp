/*
 * Copyright (C) 2020 Arm Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
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

/* Using ashmem is deprecated from Android 10. Use memfd on newer Android versions. */
#define GRALLOC_USE_MEMFD ((PLATFORM_SDK_VERSION > 29) || (GRALLOC_VERSION_MAJOR > 3))

#if GRALLOC_USE_MEMFD
#include <sys/syscall.h>
#include <linux/memfd.h>
#else
#include <cutils/ashmem.h>
#endif
#include "mali_gralloc_shared_memory.h"
#include "mali_gralloc_log.h"
#include "mali_gralloc_buffer.h"

static int create_file(const char *name, uint64_t size)
{
#if GRALLOC_USE_MEMFD
	int ret = 0, fd = -1;

	fd = syscall(__NR_memfd_create, name, MFD_ALLOW_SEALING);
	if (fd < 0)
	{
		MALI_GRALLOC_LOGE("memfd_create: %s", strerror(errno));
		goto fail;
	}

	if (size > 0)
	{
		ret = ftruncate(fd, static_cast<off_t>(size));
		if (ret < 0)
		{
			MALI_GRALLOC_LOGE("ftruncate: %s", strerror(errno));
			goto fail;
		}
	}

	ret = fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL);
	if (ret < 0)
	{
		MALI_GRALLOC_LOGW("Failed to seal fd: %s", strerror(errno));
		goto fail;
	}

	return fd;
fail:
	if (fd >= 0)
	{
		close(fd);
	}

	return -1;
#else
	return ashmem_create_region(name, static_cast<size_t>(size));
#endif
}

std::pair<int, void *> gralloc_shared_memory_allocate(const char *name, uint64_t size)
{
	int fd = -1;
	void *mapping = MAP_FAILED;

	fd = create_file(name, size);
	if (fd < 0)
	{
		MALI_GRALLOC_LOGE("Failed to open shared memory file %s: %s", name, strerror(errno));
		goto fail;
	}

	/*
	 * Default protection on the shm region is PROT_EXEC | PROT_READ | PROT_WRITE.
	 *
	 * Personality flag READ_IMPLIES_EXEC which is used by some processes, namely gdbserver,
	 * causes a mmap with PROT_READ to be translated to PROT_READ | PROT_EXEC.
	 *
	 * If we were to drop PROT_EXEC here with a call to ashmem_set_prot_region()
	 * this can potentially cause clients to fail importing this gralloc attribute buffer
	 * with EPERM error since PROT_EXEC is not allowed.
	 *
	 * Because of this we keep the PROT_EXEC flag.
	 */
	mapping = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapping == MAP_FAILED)
	{
		MALI_GRALLOC_LOGE("Failed to mmap %s region: %s", name, strerror(errno));
		goto fail;
	}

	return std::make_pair(fd, mapping);
fail:
	if (fd >= 0)
	{
		close(fd);
	}

	return std::make_pair(-1, MAP_FAILED);
}

void gralloc_shared_memory_free(int fd, void *mapping, uint64_t size)
{
	if (mapping != MAP_FAILED)
	{
		munmap(mapping, size);
	}

	if (fd >= 0)
	{
		close(fd);
	}
}
