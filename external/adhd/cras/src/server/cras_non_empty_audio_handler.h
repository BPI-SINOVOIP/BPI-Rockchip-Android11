/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The non-empty audio state handler is used to send a DBus signal when the
 * system-level non-empty audio state changes.
 *
 * cras_non_empty_audio_msg() is called from audio thread to update the
 * non-empty audio state in the main thread, which in turn sends the DBus
 * signal.
 *
 * cras_non_empty_audio_handler_init() is used to setup the message handler
 * in the main thread to handle the non-empty audiomessage from audio thread.
 */

#ifndef CRAS_NON_EMPTY_AUDIO_HANDLER_H_
#define CRAS_NON_EMPTY_AUDIO_HANDLER_H_

/* Send non-empty audio state message. */
int cras_non_empty_audio_send_msg(int32_t non_empty);

/* Initialize non-empty audio handler. */
int cras_non_empty_audio_handler_init();

#endif /* CRAS_NON_EMPTY_AUDIO_HANDLER_H_ */
