#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/reboot.h>

#include "igt_core.h"

#include "igt_sysrq.h"

/**
 * igt_sysrq_reboot: Reboots the machine
 *
 * Syncs filesystems and immediately reboots the machine.
 */
void igt_sysrq_reboot(void)
{
	sync();

	/* Try to be nice at first, and if that fails pull the trigger */
	if (reboot(RB_AUTOBOOT)) {
		int fd = open("/proc/sysrq-trigger", O_WRONLY);
		igt_ignore_warn(write(fd, "b", 2));
		close(fd);
	}

	abort();
}
