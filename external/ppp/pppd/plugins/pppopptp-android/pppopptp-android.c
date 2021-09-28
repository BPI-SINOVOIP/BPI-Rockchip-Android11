/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "pppd.h"
#include <unistd.h>

char pppd_version[] = VERSION;

static struct channel pppopptp_channel;

/* Option variables */
static int pptp_socket = -1;

static option_t pppopptp_options[] =
{
	{ "pptp_socket", o_int, &pptp_socket, "PPTP socket FD", OPT_PRIO },
	{ NULL }
};

static int pptp_connect(void)
{
	info("Using PPPoPPTP (socket = %d)", pptp_socket);
	return pptp_socket;
}

static void pptp_disconnect(void)
{
	if (pptp_socket != -1) {
		close(pptp_socket);
		pptp_socket = -1;
	}
}

void plugin_init(void)
{
	add_options(pppopptp_options);
	the_channel = &pppopptp_channel;
}

static struct channel pppopptp_channel = {
	.options		= pppopptp_options,
	.process_extra_options	= NULL,
	.check_options		= NULL,
	.connect		= pptp_connect,
	.disconnect		= pptp_disconnect,
	.establish_ppp		= generic_establish_ppp,
	.disestablish_ppp	= generic_disestablish_ppp,
	.send_config		= NULL,
	.recv_config		= NULL,
	.close			= NULL,
	.cleanup		= NULL,
};
