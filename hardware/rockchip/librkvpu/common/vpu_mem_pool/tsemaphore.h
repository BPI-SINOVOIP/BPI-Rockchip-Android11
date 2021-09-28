/**
  src/tsemaphore.h

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

#ifndef __TSEMAPHORE_H__
#define __TSEMAPHORE_H__
#ifdef ANDROID_COMPILATION
#include <oscl_base_macros.h>
#else
#define OSCL_IMPORT_REF
#define OSCL_EXPORT_REF
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** The structure contains the semaphore value, mutex and green light flag
 */
typedef struct tsem_t{
  pthread_cond_t condition;
  pthread_mutex_t mutex;
  unsigned int semval;
}tsem_t;

/** Initializes the semaphore at a given value
 *
 * @param tsem the semaphore to initialize
 *
 * @param val the initial value of the semaphore
 */
OSCL_IMPORT_REF int tsem_init(tsem_t* tsem, unsigned int val);

/** Destroy the semaphore
 *
 * @param tsem the semaphore to destroy
 */
OSCL_IMPORT_REF void tsem_deinit(tsem_t* tsem);

/** Decreases the value of the semaphore. Blocks if the semaphore
 * value is zero.
 *
 * @param tsem the semaphore to decrease
 */
OSCL_IMPORT_REF void tsem_down(tsem_t* tsem);

/** Decreases the value of the semaphore. Blocks if the semaphore
 * value is zero. If the timeout is reached the function exits with
 * error ETIMEDOUT
 *
 * @param tsem the semaphore to decrease
 * @param timevalue the value of delay for the timeout
 */
OSCL_IMPORT_REF int tsem_timed_down(tsem_t* tsem, unsigned int milliSecondsDelay);

/** Increases the value of the semaphore
 *
 * @param tsem the semaphore to increase
 */
OSCL_IMPORT_REF void tsem_up(tsem_t* tsem);

/** Reset the value of the semaphore
 *
 * @param tsem the semaphore to reset
 */
OSCL_IMPORT_REF void tsem_reset(tsem_t* tsem);

/** Wait on the condition.
 *
 * @param tsem the semaphore to wait
 */
OSCL_IMPORT_REF void tsem_wait(tsem_t* tsem);

/** Signal the condition,if waiting
 *
 * @param tsem the semaphore to signal
 */
OSCL_IMPORT_REF void tsem_signal(tsem_t* tsem);

/** Read the semval
 *
 * @param tsem the semaphore to read
 */
OSCL_IMPORT_REF unsigned int tsem_read(tsem_t* tsem);

#ifdef __cplusplus
}
#endif

#endif
