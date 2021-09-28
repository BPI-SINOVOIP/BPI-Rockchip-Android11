/*
 * Copyright © 2015 Intel Corporation
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

#ifndef IGT_PM_H
#define IGT_PM_H

void igt_pm_enable_audio_runtime_pm(void);
int8_t *igt_pm_enable_sata_link_power_management(void);
void igt_pm_restore_sata_link_power_management(int8_t *pm_data);

/**
 * igt_runtime_pm_status:
 * @IGT_RUNTIME_PM_STATUS_ACTIVE: device is active
 * @IGT_RUNTIME_PM_STATUS_SUSPENDED: device is suspended
 * @IGT_RUNTIME_PM_STATUS_SUSPENDING: device is in the process of suspending
 * @IGT_RUNTIME_PM_STATUS_RESUMING: device is in the process of resuming
 * @IGT_RUNTIME_PM_STATUS_UNKNOWN: unknown runtime PM status
 *
 * Symbolic values for runtime PM device status.
 */
enum igt_runtime_pm_status {
	IGT_RUNTIME_PM_STATUS_ACTIVE,
	IGT_RUNTIME_PM_STATUS_SUSPENDED,
	IGT_RUNTIME_PM_STATUS_SUSPENDING,
	IGT_RUNTIME_PM_STATUS_RESUMING,
	IGT_RUNTIME_PM_STATUS_UNKNOWN,
};

bool igt_setup_runtime_pm(void);
void igt_restore_runtime_pm(void);
enum igt_runtime_pm_status igt_get_runtime_pm_status(void);
bool igt_wait_for_pm_status(enum igt_runtime_pm_status status);

#endif /* IGT_PM_H */
