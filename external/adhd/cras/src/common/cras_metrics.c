/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#ifdef HAVE_LIB_METRICS
#include <metrics/c_metrics_library.h>

void cras_metrics_log_event(const char *event)
{
	CMetricsLibrary handle;

	syslog(LOG_DEBUG, "UMA event: %s", event);
	handle = CMetricsLibraryNew();
	CMetricsLibrarySendCrosEventToUMA(handle, event);
	CMetricsLibraryDelete(handle);
}

void cras_metrics_log_histogram(const char *name, int sample, int min, int max,
				int nbuckets)
{
	CMetricsLibrary handle;

	syslog(LOG_DEBUG, "UMA name: %s", name);
	handle = CMetricsLibraryNew();
	CMetricsLibrarySendToUMA(handle, name, sample, min, max, nbuckets);
	CMetricsLibraryDelete(handle);
}

void cras_metrics_log_sparse_histogram(const char *name, int sample)
{
	CMetricsLibrary handle;

	syslog(LOG_DEBUG, "UMA name: %s", name);
	handle = CMetricsLibraryNew();
	CMetricsLibrarySendSparseToUMA(handle, name, sample);
	CMetricsLibraryDelete(handle);
}

#else
void cras_metrics_log_event(const char *event)
{
}
void cras_metrics_log_histogram(const char *name, int sample, int min, int max,
				int nbuckets)
{
}
void cras_metrics_log_enum_histogram(const char *name, int sample, int max)
{
}
void cras_metrics_log_sparse_histogram(const char *name, int sample)
{
}
#endif
