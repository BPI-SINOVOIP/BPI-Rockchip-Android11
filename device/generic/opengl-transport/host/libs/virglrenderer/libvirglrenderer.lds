/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

{
global:
	# virglrenderer API
	virgl_renderer_init;
	virgl_renderer_poll;
	virgl_renderer_get_cursor_data;
	virgl_renderer_resource_create;
	virgl_renderer_resource_unref;
	virgl_renderer_context_create;
	virgl_renderer_context_destroy;
	virgl_renderer_submit_cmd;
	virgl_renderer_transfer_read_iov;
	virgl_renderer_transfer_write_iov;
	virgl_renderer_get_cap_set;
	virgl_renderer_fill_caps;
	virgl_renderer_resource_attach_iov;
	virgl_renderer_resource_detach_iov;
	virgl_renderer_create_fence;
	virgl_renderer_force_ctx_0;
	virgl_renderer_ctx_attach_resource;
	virgl_renderer_ctx_detach_resource;
	virgl_renderer_resource_get_info;

	# fake gralloc1 implementation
	hw_get_module;

	# fake libsync implementation
	sync_wait;

	# Type-strings and type-infos required by sanitizers
	_ZTS*;
	_ZTI*;

local:
	*;
};
