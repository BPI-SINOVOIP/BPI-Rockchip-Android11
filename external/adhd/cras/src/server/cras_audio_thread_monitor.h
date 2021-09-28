/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRAS_AUDIO_THREAD_MONITOR_H_
#define CRAS_AUDIO_THREAD_MONITOR_H_

/*
 * Sends a debug event to the audio thread for debugging.
 */
int cras_audio_thread_event_debug();

/*
 * Notifies the main thread when a busyloop event happens.
 */
int cras_audio_thread_event_busyloop();

/*
 * Notifies the main thread when a underrun event happens.
 */
int cras_audio_thread_event_underrun();

/*
 * Notifies the main thread when a severe underrun event happens.
 */
int cras_audio_thread_event_severe_underrun();

/*
 * Notifies the main thread when a drop samples event happens.
 */
int cras_audio_thread_event_drop_samples();

/*
 * Initializes audio thread monitor and sets main thread callback.
 */
int cras_audio_thread_monitor_init();

#endif /* CRAS_AUDIO_THREAD_MONITOR_H_ */
