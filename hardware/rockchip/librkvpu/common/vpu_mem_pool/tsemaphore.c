/**
  src/tsemaphore.c

  Implements a simple inter-thread semaphore so not to have to deal with IPC
  creation and the like.

  Copyright (C) 2007-2009 STMicroelectronics
  Copyright (C) 2007-2009 Nokia Corporation and/or its subsidiary(-ies).

  This library is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the Free
  Software Foundation; either version 2.1 of the License, or (at your option)
  any later version.

  This library is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  details.

  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St, Fifth Floor, Boston, MA
  02110-1301  USA

*/

#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include "tsemaphore.h"

/** Initializes the semaphore at a given value
 *
 * @param tsem the semaphore to initialize
 * @param val the initial value of the semaphore
 *
 */
OSCL_EXPORT_REF int tsem_init(tsem_t* tsem, unsigned int val) {
	int i;
	i = pthread_cond_init(&tsem->condition, NULL);
	if (i!=0) {
		return -1;
	}
	i = pthread_mutex_init(&tsem->mutex, NULL);
	if (i!=0) {
		return -1;
	}
	tsem->semval = val;
	return 0;
}

/** Destroy the semaphore
 *
 * @param tsem the semaphore to destroy
 */
OSCL_EXPORT_REF void tsem_deinit(tsem_t* tsem) {
  pthread_cond_destroy(&tsem->condition);
  pthread_mutex_destroy(&tsem->mutex);
}

/** Decreases the value of the semaphore. Blocks if the semaphore
 * value is zero. If the timeout is reached the function exits with
 * error ETIMEDOUT
 *
 * @param tsem the semaphore to decrease
 * @param timevalue the value of delay for the timeout
 */
OSCL_EXPORT_REF int tsem_timed_down(tsem_t* tsem, unsigned int milliSecondsDelay) {
	int err = 0;
    struct timespec final_time;
    struct timeval currentTime;
    long int microdelay;

    gettimeofday(&currentTime, NULL);
    /** convert timeval to timespec and add delay in milliseconds for the timeout */
    microdelay = ((milliSecondsDelay * 1000 + currentTime.tv_usec));
	final_time.tv_sec = currentTime.tv_sec + (microdelay / 1000000);
	final_time.tv_nsec = (microdelay % 1000000) * 1000;
	pthread_mutex_lock(&tsem->mutex);
	while (tsem->semval == 0) {
		err = pthread_cond_timedwait(&tsem->condition, &tsem->mutex, &final_time);
		if (err != 0) {
			pthread_mutex_unlock(&tsem->mutex);
			return err;
			//tsem->semval--;
		}
	}
	tsem->semval--;
	pthread_mutex_unlock(&tsem->mutex);
	return err;
}

/** Decreases the value of the semaphore. Blocks if the semaphore
 * value is zero.
 *
 * @param tsem the semaphore to decrease
 */
OSCL_EXPORT_REF void tsem_down(tsem_t* tsem) {
  pthread_mutex_lock(&tsem->mutex);
  while (tsem->semval == 0) {
    pthread_cond_wait(&tsem->condition, &tsem->mutex);
  }
  tsem->semval--;
  pthread_mutex_unlock(&tsem->mutex);
}

/** Increases the value of the semaphore
 *
 * @param tsem the semaphore to increase
 */
OSCL_EXPORT_REF void tsem_up(tsem_t* tsem) {
  pthread_mutex_lock(&tsem->mutex);
  tsem->semval++;
  pthread_cond_signal(&tsem->condition);
  pthread_mutex_unlock(&tsem->mutex);
}

/** Reset the value of the semaphore
 *
 * @param tsem the semaphore to reset
 */
OSCL_EXPORT_REF void tsem_reset(tsem_t* tsem) {
  pthread_mutex_lock(&tsem->mutex);
  tsem->semval=0;
  pthread_mutex_unlock(&tsem->mutex);
}

/** Wait on the condition.
 *
 * @param tsem the semaphore to wait
 */
OSCL_EXPORT_REF void tsem_wait(tsem_t* tsem) {
  pthread_mutex_lock(&tsem->mutex);
  pthread_cond_wait(&tsem->condition, &tsem->mutex);
  pthread_mutex_unlock(&tsem->mutex);
}

/** Signal the condition,if waiting
 *
 * @param tsem the semaphore to signal
 */
OSCL_EXPORT_REF void tsem_signal(tsem_t* tsem) {
  pthread_mutex_lock(&tsem->mutex);
  pthread_cond_signal(&tsem->condition);
  pthread_mutex_unlock(&tsem->mutex);
}

/** Read the semval
 *
 * @param tsem the semaphore to read
 */
OSCL_IMPORT_REF unsigned int tsem_read(tsem_t* tsem) {
  unsigned int val;
  pthread_mutex_lock(&tsem->mutex);
  val = tsem->semval;
  pthread_mutex_unlock(&tsem->mutex);
  return val;
}

