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
#include "fsm.h"
#include "lcp.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if_ppp.h>

char pppd_version[] = VERSION;

static struct channel pppol2tp_channel;

/* Options variables */
static int session_fd = -1;
static int tunnel_fd = -1;
static int session_id = 0;
static int tunnel_id = 0;

static int pppol2tp_set_session_fd(char **argv);

static option_t pppol2tp_options[] = {
	{ "session_fd", o_special, pppol2tp_set_session_fd,
		"Session PPPoX data socket", OPT_DEVNAM },
	{ "tunnel_fd", o_int, &tunnel_fd,
		"Tunnel management PPPoX socket", OPT_PRIO },
	{ "session_id", o_int, &session_id, "Session ID", OPT_PRIO },
	{ "tunnel_id", o_int, &tunnel_id, "Tunnel ID", OPT_PRIO },
	{ NULL }
};

static int pppol2tp_set_session_fd(char **argv)
{
	if (!int_option(*argv, &session_fd))
		return 0;

	info("Using PPPoL2TP (socket = %d)", session_fd);
	the_channel = &pppol2tp_channel;
	return 1;
}

/**
 * Set the MRU on the PPP network interface.
 *
 * @param mru New MRU value
 *
 * @note netif_set_mru() is missing in sys-linux.c, so implement it manually
 * @note See net/l2tp/l2tp_ppp.c:pppol2tp_session_ioctl() in kernel for details
 */
static void pppol2tp_set_mru(int mru)
{
	int res;

	if (ifunit < 0)
		return;

	res = ioctl(session_fd, PPPIOCSMRU, (caddr_t)&mru);
	if (res < 0)
		error("ioctl(PPPIOCSMRU): %m (line %d)", __LINE__);
}

/* Set the transmit-side PPP parameters of the channel */
static void pppol2tp_send_config(int mtu, u_int32_t accm, int pcomp, int accomp)
{
	int new_mtu = lcp_allowoptions[0].mru;	/* "mtu" pppd option */

	if (new_mtu <= PPP_MAXMTU && new_mtu >= PPP_MINMTU)
		netif_set_mtu(ifunit, new_mtu);
}

/* Set the receive-side PPP parameters of the channel */
static void pppol2tp_recv_config(int mru, u_int32_t accm, int pcomp, int accomp)
{
	int new_mru = lcp_wantoptions[0].mru;	/* "mru" pppd option */

	if (new_mru <= PPP_MAXMRU && new_mru >= PPP_MINMRU)
		pppol2tp_set_mru(new_mru);
}

static int pppol2tp_connect(void)
{
	return session_fd;
}

static void pppol2tp_disconnect(void)
{
	if (session_fd != -1) {
		close(session_fd);
		session_fd = -1;
	}

	if (tunnel_fd != -1) {
		close(tunnel_fd);
		tunnel_fd = -1;
	}
}

void plugin_init(void)
{
	add_options(pppol2tp_options);
}

static struct channel pppol2tp_channel = {
	.options		= pppol2tp_options,
	.process_extra_options	= NULL,
	.check_options		= NULL,
	.connect		= pppol2tp_connect,
	.disconnect		= pppol2tp_disconnect,
	.establish_ppp		= generic_establish_ppp,
	.disestablish_ppp	= generic_disestablish_ppp,
	.send_config		= pppol2tp_send_config,
	.recv_config		= pppol2tp_recv_config,
	.cleanup		= NULL,
	.close			= NULL,
};
