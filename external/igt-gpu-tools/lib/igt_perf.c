#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/sysinfo.h>

#include "igt_perf.h"

uint64_t i915_type_id(void)
{
	char buf[64];
	ssize_t ret;
	int fd;

	fd = open("/sys/bus/event_source/devices/i915/type", O_RDONLY);
	if (fd < 0)
		return 0;

	ret = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (ret < 1)
		return 0;

	buf[ret] = '\0';

	return strtoull(buf, NULL, 0);
}

static int
_perf_open(uint64_t type, uint64_t config, int group, uint64_t format)
{
	struct perf_event_attr attr = { };
	int nr_cpus = get_nprocs_conf();
	int cpu = 0, ret;

	attr.type = type;
	if (attr.type == 0)
		return -ENOENT;

	if (group >= 0)
		format &= ~PERF_FORMAT_GROUP;

	attr.read_format = format;
	attr.config = config;

	do {
		ret = perf_event_open(&attr, -1, cpu++, group, 0);
	} while ((ret < 0 && errno == EINVAL) && (cpu < nr_cpus));

	return ret;
}

int perf_i915_open(uint64_t config)
{
	return _perf_open(i915_type_id(), config, -1,
			  PERF_FORMAT_TOTAL_TIME_ENABLED);
}

int perf_i915_open_group(uint64_t config, int group)
{
	return _perf_open(i915_type_id(), config, group,
			  PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
}

int igt_perf_open(uint64_t type, uint64_t config)
{
	return _perf_open(type, config, -1,
			  PERF_FORMAT_TOTAL_TIME_ENABLED);
}

int igt_perf_open_group(uint64_t type, uint64_t config, int group)
{
	return _perf_open(type, config, group,
			  PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
}
