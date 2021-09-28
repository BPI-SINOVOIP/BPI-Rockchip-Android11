/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Common utility functions for rclients.
 */
#ifndef CRAS_RCLIENT_UTIL_H_
#define CRAS_RCLIENT_UTIL_H_

#define MSG_LEN_VALID(msg, type) ((msg)->length >= sizeof(type))

struct cras_connect_message;
struct cras_rclient;
struct cras_rclient_message;
struct cras_rstream_config;
struct cras_server_message;

/* Sends a message to the client. */
int rclient_send_message_to_client(const struct cras_rclient *client,
				   const struct cras_client_message *msg,
				   int *fds, unsigned int num_fds);

/* Removes all streams that the client owns and destroys it. */
void rclient_destroy(struct cras_rclient *client);

/* Checks if the number of incoming fds matches the needs of the message from
 * client.
 *
 * Args:
 *   msg - The cras_server_message from client.
 *   fds - The array for incoming fds from client.
 *   num_fds - The number of fds from client.
 *
 * Returns:
 *   0 on success. Or negative value if the number of fds is invalid.
 */
int rclient_validate_message_fds(const struct cras_server_message *msg,
				 int *fds, unsigned int num_fds);

/* Checks if the incoming stream connect message contains
 * - stream_id matches client->id.
 * - direction supported by the client.
 *
 * Args:
 *   client - The cras_rclient which gets the message.
 *   msg - cras_connect_message from client.
 *   audio_fd - Audio fd from client.
 *   client_shm_fd - client shared memory fd from client. It can be -1.
 *
 * Returns:
 *   0 on success, negative error on failure.
 */
int rclient_validate_stream_connect_params(
	const struct cras_rclient *client,
	const struct cras_connect_message *msg, int audio_fd,
	int client_shm_fd);

/* Handles a message from the client to connect a new stream
 *
 * Args:
 *   client - The cras_rclient which gets the message.
 *   msg - The cras_connect_message from client.
 *   aud_fd - The audio fd comes from client. Its ownership will be taken.
 *   client_shm_fd - The client_shm_fd from client. Its ownership will be taken.
 *
 * Returns:
 *   0 on success, negative error on failure.
 */
int rclient_handle_client_stream_connect(struct cras_rclient *client,
					 const struct cras_connect_message *msg,
					 int aud_fd, int client_shm_fd);

/* Handles messages from the client requesting that a stream be removed from the
 * server.
 *
 * Args:
 *   client - The cras_rclient which gets the message.
 *   msg - The cras_disconnect_stream_message from client.
 *
 * Returns:
 *   0 on success, negative error on failure.
 */
int rclient_handle_client_stream_disconnect(
	struct cras_rclient *client,
	const struct cras_disconnect_stream_message *msg);

/*
 * Converts an old version of connect message to the correct
 * cras_connect_message. Returns zero on success, negative on failure.
 * Note that this is special check only for libcras transition in
 * clients, from CRAS_PROTO_VER = 3 to 5.
 * TODO(yuhsuan): clean up the function once clients transition is done.
 */
static inline int
convert_connect_message_old(const struct cras_server_message *msg,
			    struct cras_connect_message *cmsg)
{
	struct cras_connect_message_old *old;

	if (!MSG_LEN_VALID(msg, struct cras_connect_message_old))
		return -EINVAL;

	old = (struct cras_connect_message_old *)msg;
	if (old->proto_version != 3 || CRAS_PROTO_VER != 5)
		return -EINVAL;

	memcpy(cmsg, old, sizeof(*old));
	cmsg->client_type = CRAS_CLIENT_TYPE_LEGACY;
	cmsg->client_shm_size = 0;
	return 0;
}

#endif /* CRAS_RCLIENT_UTIL_H_ */
