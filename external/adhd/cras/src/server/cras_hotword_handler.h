/* Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The hotword handler is used to send a DBus signal when a hotword device is
 * triggered.
 *
 * cras_hotword_send_triggered_msg() is called from audio thread to send a
 * hotword message to main thread which in turn sends the DBus signal.
 *
 * cras_hotword_handler_init() is used to setup message handler in main thread
 * to handle the hotword message from audio thread.
 */

#ifndef CRAS_HOTWORD_HANDLER_H_
#define CRAS_HOTWORD_HANDLER_H_

/* Send hotword triggered message. */
int cras_hotword_send_triggered_msg();

/* Initialize hotword handler. */
int cras_hotword_handler_init();

#endif /* CRAS_HOTWORD_HANDLER_H_ */
