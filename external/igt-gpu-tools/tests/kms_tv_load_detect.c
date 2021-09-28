/*
 * Copyright © 2014 Intel Corporation
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

IGT_TEST_DESCRIPTION("Check tv load detection works correctly.");

igt_main
{
	/* force the VGA output and test that it worked */
	int drm_fd = 0;
	drmModeRes *res;
	drmModeConnector *tv_connector = NULL, *temp;

	igt_fixture {
		drm_fd = drm_open_driver_master(DRIVER_INTEL);

		res = drmModeGetResources(drm_fd);
		igt_require(res);

		/* find the TV connector */
		for (int i = 0; i < res->count_connectors; i++) {

			tv_connector = drmModeGetConnectorCurrent(drm_fd,
								   res->connectors[i]);

			if (tv_connector->connector_type == DRM_MODE_CONNECTOR_SVIDEO) {
				if (kmstest_get_property(drm_fd, tv_connector->connector_id,
							 DRM_MODE_OBJECT_CONNECTOR, "mode", NULL, NULL, NULL))
					break;

				igt_info("Skipping tv output \"%s-%d\": No tv \"mode\" property found\n",
					 kmstest_connector_type_str(tv_connector->connector_type),
					 tv_connector->connector_type_id);
			}

			drmModeFreeConnector(tv_connector);

			tv_connector = NULL;
		}

		igt_require(tv_connector);
	}

	igt_subtest("load-detect") {
		/*
		 * disable all outputs to make sure we have a
		 * free crtc available for load detect
		 */
		kmstest_set_vt_graphics_mode();
		kmstest_unset_all_crtcs(drm_fd, res);

		/* This can't use drmModeGetConnectorCurrent
		 * because connector probing is the point of this test.
		 */
		temp = drmModeGetConnector(drm_fd, tv_connector->connector_id);

		igt_assert(temp->connection != DRM_MODE_UNKNOWNCONNECTION);

		drmModeFreeConnector(temp);
	}

	igt_fixture {
		drmModeFreeConnector(tv_connector);
		close(drm_fd);
	}
}
