#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>

#include "igt_gpu_power.h"
#include "igt_perf.h"
#include "igt_sysfs.h"

struct rapl {
	uint64_t power, type;
	double scale;
};

static int rapl_parse(struct rapl *r)
{
	locale_t locale, oldlocale;
	bool result;
	int dir;

	memset(r, 0, sizeof(*r));

	dir = open("/sys/devices/power", O_RDONLY);
	if (dir < 0)
		return -errno;

	/* Replace user environment with plain C to match kernel format */
	locale = newlocale(LC_ALL, "C", 0);
	oldlocale = uselocale(locale);

	result = true;
	result &= igt_sysfs_scanf(dir, "type",
				  "%"PRIu64, &r->type) == 1;
	result &= igt_sysfs_scanf(dir, "events/energy-gpu",
				  "event=%"PRIx64, &r->power) == 1;
	result &= igt_sysfs_scanf(dir, "events/energy-gpu.scale",
				  "%lf", &r->scale) == 1;

	uselocale(oldlocale);
	freelocale(locale);

	close(dir);

	if (!result)
		return -EINVAL;

	if (isnan(r->scale) || !r->scale)
		return -ERANGE;

	return 0;
}

int gpu_power_open(struct gpu_power *power)
{
	struct rapl r;

	power->fd = rapl_parse(&r);
	if (power->fd < 0)
		goto err;

	power->fd = igt_perf_open(r.type, r.power);
	if (power->fd < 0) {
		power->fd = -errno;
		goto err;
	}

	power->scale = r.scale;

	return 0;

err:
	errno = 0;
	return power->fd;
}

bool gpu_power_read(struct gpu_power *power, struct gpu_power_sample *s)
{
	return read(power->fd, s, sizeof(*s)) == sizeof(*s);
}

void gpu_power_close(struct gpu_power *power)
{
	close(power->fd);
}
