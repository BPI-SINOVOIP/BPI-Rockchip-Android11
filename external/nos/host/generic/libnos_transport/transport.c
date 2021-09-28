/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <nos/transport.h>

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <application.h>

#include "crc16.h"

/* Note: evaluates expressions multiple times */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define DEBUG_LOG 0
#define VERBOSE_LOG 0

#ifdef ANDROID
/* Logging for Android */
#define LOG_TAG "libnos_transport"
#include <android-base/endian.h>
#include <log/log.h>
#include <sys/types.h>

#define NLOGE(...) ALOGE(__VA_ARGS__)
#define NLOGW(...) ALOGW(__VA_ARGS__)
#define NLOGD(...) ALOGD(__VA_ARGS__)
#define NLOGV(...) ALOGV(__VA_ARGS__)

extern int usleep (uint32_t usec);

#else
/* Logging for other platforms */
#include <stdio.h>

#define NLOGE(...) do { fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); } while (0)
#define NLOGW(...) do { printf(__VA_ARGS__); \
  printf("\n"); } while (0)
#define NLOGD(...) do { if (DEBUG_LOG) { \
  printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NLOGV(...) do { if (VERBOSE_LOG) { \
  printf(__VA_ARGS__); printf("\n"); } } while (0)

#endif

/*
 * If Citadel is rebooting it will take a while to become responsive again. We
 * expect a reboot to take around 100ms but we'll keep trying for 300ms to leave
 * plenty of margin.
 */
#define RETRY_COUNT 240
#define RETRY_WAIT_TIME_US 5000

/* In case of CRC error, try to retransmit */
#define CRC_RETRY_COUNT 5

/* How long to poll before giving up */
#define POLL_LIMIT_SECONDS 60

struct transport_context {
  const struct nos_device *dev;
  uint8_t app_id;
  uint16_t params;
  const uint8_t *args;
  uint32_t arg_len;
  uint8_t *reply;
  uint32_t *reply_len;
};

/*
 * Read a datagram from the device, correctly handling retries.
 */
static int nos_device_read(const struct nos_device *dev, uint32_t command,
                           void *buf, uint32_t len) {
  int retries = RETRY_COUNT;
  while (retries--) {
    int err = dev->ops.read(dev->ctx, command, buf, len);

    if (err == -EAGAIN) {
      /* Linux driver returns EAGAIN error if Citadel chip is asleep.
       * Give to the chip a little bit of time to awake and retry reading
       * status again. */
      usleep(RETRY_WAIT_TIME_US);
      continue;
    }

    if (err) {
      NLOGE("Failed to read: %s", strerror(-err));
    }
    return -err;
  }

  return ETIMEDOUT;
}

/*
 * Write a datagram to the device, correctly handling retries.
 */
static int nos_device_write(const struct nos_device *dev, uint32_t command,
                            const void *buf, uint32_t len) {
  int retries = RETRY_COUNT;
  while (retries--) {
    int err = dev->ops.write(dev->ctx, command, buf, len);

    if (err == -EAGAIN) {
      /* Linux driver returns EAGAIN error if Citadel chip is asleep.
       * Give to the chip a little bit of time to awake and retry reading
       * status again. */
      usleep(RETRY_WAIT_TIME_US);
      continue;
    }

    if (err) {
      NLOGE("Failed to write: %s", strerror(-err));
    }
    return -err;
  }

  return ETIMEDOUT;
}

/*
 * Get the status regardless of protocol version. All fields not passed by the
 * slave are set to 0 so the caller must check the version before interpretting
 * them.
 *
 * Returns non-zero on error.
 */
static int get_status(const struct transport_context *ctx,
                      struct transport_status *out) {
  union {
    struct transport_status status;
    uint8_t data[STATUS_MAX_LENGTH];
  } st;
  int retries = CRC_RETRY_COUNT;
  while (retries--) {
    /* Get the status from the device */
    const uint32_t command = CMD_ID(ctx->app_id) | CMD_IS_READ | CMD_TRANSPORT;
    if (nos_device_read(ctx->dev, command, &st, STATUS_MAX_LENGTH) != 0) {
      NLOGE("Failed to read app %d status", ctx->app_id);
      return -1;
    }

    /* All unset fields will be 0. */
    memset(out, 0, sizeof(*out));

    /* Examine v0 fields */
    out->status = le32toh(st.status.status);
    out->reply_len = le16toh(st.status.reply_len);

    /* Identify v0 as length will be an invalid value */
    const uint16_t length = le16toh(st.status.length);
    if (length < STATUS_MIN_LENGTH || length > STATUS_MAX_LENGTH) {
      out->version = TRANSPORT_V0;
      return 0;
    }

    /* Examine v1 fields */
    out->length = length;
    out->version = le16toh(st.status.version);
    out->flags = le16toh(st.status.flags);
    out->crc = le16toh(st.status.crc);
    out->reply_crc = le16toh(st.status.reply_crc);

    /* Calculate the CRC of the status message */
    st.status.crc = 0;
    const uint16_t our_crc = crc16(&st.status, length);

    /* Check the CRC, if it fails we will retry */
    if (out->crc != our_crc) {
      NLOGW("App %d status CRC mismatch: theirs=%04x ours=%04x",
            ctx->app_id, out->crc, our_crc);
      continue;
    }

    /* Identify and examine v2+ fields here */

    return 0;
  }

  NLOGE("Unable to get valid checksum on app %d status", ctx->app_id);
  return -1;
}

/*
 * Try and reset the protocol state on Citadel for a new transaction.
 */
static int clear_status(const struct transport_context *ctx) {
  const uint32_t command = CMD_ID(ctx->app_id) | CMD_TRANSPORT;
  if (nos_device_write(ctx->dev, command, NULL, 0) != 0) {
    NLOGE("Failed to clear app %d status", ctx->app_id);
    return -1;
  }
  return 0;
}

/*
 * Ensure that the app is in an idle state ready to handle the transaction.
 */
static uint32_t make_ready(const struct transport_context *ctx) {
  struct transport_status status;

  if (get_status(ctx, &status) != 0) {
    NLOGE("Failed to inspect app %d", ctx->app_id);
    return APP_ERROR_IO;
  }
  NLOGD("App %d inspection status=0x%08x reply_len=%d protocol=%d flags=0x%04x",
        ctx->app_id, status.status, status.reply_len, status.version, status.flags);

  /* If it's already idle then we're ready to proceed */
  if (status.status == APP_STATUS_IDLE) {
    if (status.version != TRANSPORT_V0
        && (status.flags & STATUS_FLAG_WORKING)) {
      /* The app is still working when we don't expect it to be. We won't be
       * able to clear the state so might need to force a reset to recover. */
      NLOGE("App %d is still working", ctx->app_id);
      return APP_ERROR_BUSY;
    }
    return APP_SUCCESS;
  }

  /* Try clearing the status */
  NLOGD("Clearing previous app %d status", ctx->app_id);
  if (clear_status(ctx) != 0) {
    NLOGE("Failed to force app %d to idle status", ctx->app_id);
    return APP_ERROR_IO;
  }

  /* Check again */
  if (get_status(ctx, &status) != 0) {
    NLOGE("Failed to get app %d's cleared status", ctx->app_id);
    return APP_ERROR_IO;
  }
  NLOGD("Cleared app %d status=0x%08x reply_len=%d flags=0x%04x",
        ctx->app_id, status.status, status.reply_len, status.flags);

  /* It's ignoring us and is still not ready, so it's broken */
  if (status.status != APP_STATUS_IDLE) {
    NLOGE("App %d is not responding", ctx->app_id);
    return APP_ERROR_IO;
  }

  return APP_SUCCESS;
}

/*
 * Split request into datagrams and send command to have app process it.
 */
static uint32_t send_command(const struct transport_context *ctx) {
  const uint8_t *args = ctx->args;
  uint16_t arg_len = ctx->arg_len;
  uint16_t crc;

  NLOGD("Send app %d command data (%d bytes)", ctx->app_id, arg_len);
  uint32_t command = CMD_ID(ctx->app_id) | CMD_IS_DATA | CMD_TRANSPORT;
  /* This always sends at least 1 packet to support the v0 protocol */
  do {
    /*
     * We can't send more per datagram than the device can accept. For Citadel
     * using the TPM Wait protocol on SPS, this is a constant. For other buses
     * it may not be, but this is what we support here. Due to peculiarities of
     * Citadel's SPS hardware, our protocol requires that we specify the length
     * of what we're about to send in the params field of each Write.
     */
    const uint16_t ulen = MIN(arg_len, MAX_DEVICE_TRANSFER);
    CMD_SET_PARAM(command, ulen);

    NLOGV("Write app %d command 0x%08x, bytes %d", ctx->app_id, command, ulen);
    if (nos_device_write(ctx->dev, command, args, ulen) != 0) {
      NLOGE("Failed to send datagram to app %d", ctx->app_id);
      return APP_ERROR_IO;
    }

    /* Any further Writes needed to send all the args must set the MORE bit */
    command |= CMD_MORE_TO_COME;
    args += ulen;
    arg_len -= ulen;
  } while (arg_len);

  /* Finally, send the "go" command */
  command = CMD_ID(ctx->app_id) | CMD_PARAM(ctx->params);

  /*
   * The outgoing crc covers:
   *
   *   1. the (16-bit) length of args
   *   2. the args buffer (if any)
   *   3. the (32-bit) "go" command
   *   4. the command info with crc set to 0
   */
  struct transport_command_info command_info = {
    .length = sizeof(command_info),
    .version = htole16(TRANSPORT_V1),
    .crc = 0,
    .reply_len_hint = ctx->reply_len ? htole16(*ctx->reply_len) : 0,
  };
  arg_len = ctx->arg_len;
  crc = crc16(&arg_len, sizeof(arg_len));
  crc = crc16_update(ctx->args, ctx->arg_len, crc);
  crc = crc16_update(&command, sizeof(command), crc);
  crc = crc16_update(&command_info, sizeof(command_info), crc);
  command_info.crc = htole16(crc);

  /* Tell the app to handle the request while also sending the command_info
   * which will be ignored by the v0 protocol. */
  NLOGD("Send app %d go command 0x%08x", ctx->app_id, command);
  if (0 != nos_device_write(ctx->dev, command, &command_info, sizeof(command_info))) {
    NLOGE("Failed to send command datagram to app %d", ctx->app_id);
    return APP_ERROR_IO;
  }

  return APP_SUCCESS;
}

static bool timespec_before(const struct timespec *lhs, const struct timespec *rhs) {
  if (lhs->tv_sec == rhs->tv_sec) {
    return lhs->tv_nsec < rhs->tv_nsec;
  } else {
    return lhs->tv_sec < rhs->tv_sec;
  }
}

/*
 * Keep polling until the app says it is done.
 */
static uint32_t poll_until_done(const struct transport_context *ctx,
                                struct transport_status *status) {
  uint32_t poll_count = 0;

  /* Start the timer */
  struct timespec now;
  struct timespec abort_at;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    NLOGE("clock_gettime() failing: %s", strerror(errno));
    return APP_ERROR_IO;
  }
  abort_at.tv_sec = now.tv_sec + POLL_LIMIT_SECONDS;
  abort_at.tv_nsec = now.tv_nsec;

  NLOGD("Polling app %d", ctx->app_id);
  do {
    /* Poll the status */
    if (get_status(ctx, status) != 0) {
      return APP_ERROR_IO;
    }
    poll_count++;
    /* Log at higher priority every 16 polls */
    if ((poll_count & (16 - 1)) == 0) {
      NLOGD("App %d poll=%d status=0x%08x reply_len=%d flags=0x%04x",
            ctx->app_id, poll_count, status->status, status->reply_len, status->flags);
    } else {
      NLOGV("App %d poll=%d status=0x%08x reply_len=%d flags=0x%04x",
            ctx->app_id, poll_count, status->status, status->reply_len, status->flags);
    }

    /* Check whether the app is done */
    if (status->status & APP_STATUS_DONE) {
      NLOGD("App %d polled=%d status=0x%08x reply_len=%d flags=0x%04x",
            ctx->app_id, poll_count, status->status, status->reply_len, status->flags);
      return APP_STATUS_CODE(status->status);
    }

    /* Check that the app is still working on it */
    if (status->version != TRANSPORT_V0
        && !(status->flags & STATUS_FLAG_WORKING)) {
      /* The slave has stopped working without being done so it's misbehaving */
      NLOGE("App %d just stopped working", ctx->app_id);
      return APP_ERROR_INTERNAL;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
      NLOGE("clock_gettime() failing: %s", strerror(errno));
      return APP_ERROR_IO;
    }
  } while (timespec_before(&now, &abort_at));

  NLOGE("App %d not done after polling %d times in %d seconds",
        ctx->app_id, poll_count, POLL_LIMIT_SECONDS);
  return APP_ERROR_TIMEOUT;
}

/*
 * Reconstruct the reply data from datagram stream.
 */
static uint32_t receive_reply(const struct transport_context *ctx,
                              const struct transport_status *status) {
  int retries = CRC_RETRY_COUNT;
  while (retries--) {
    NLOGD("Read app %d reply data (%d bytes)", ctx->app_id, status->reply_len);

    uint32_t command = CMD_ID(ctx->app_id) | CMD_IS_READ | CMD_TRANSPORT | CMD_IS_DATA;
    uint8_t *reply = ctx->reply;
    uint16_t left = MIN(*ctx->reply_len, status->reply_len);
    uint16_t got = 0;
    uint16_t crc = 0;
    while (left) {
      /* We can't read more per datagram than the device can send */
      const uint16_t gimme = MIN(left, MAX_DEVICE_TRANSFER);
      NLOGV("Read app %d command=0x%08x, bytes=%d", ctx->app_id, command, gimme);
      if (nos_device_read(ctx->dev, command, reply, gimme) != 0) {
        NLOGE("Failed to receive datagram from app %d", ctx->app_id);
        return APP_ERROR_IO;
      }

      /* Any further Reads should set the MORE bit. This only works if Nugget
       * OS sends back CRCs, but that's the only time we'd retry anyway. */
      command |= CMD_MORE_TO_COME;

      crc = crc16_update(reply, gimme, crc);
      reply += gimme;
      left -= gimme;
      got += gimme;
    }
    /* got it all */
    *ctx->reply_len = got;

    /* v0 protocol doesn't support CRC so hopefully it's ok */
    if (status->version == TRANSPORT_V0) return APP_SUCCESS;

    if (crc == status->reply_crc) return APP_SUCCESS;
    NLOGW("App %d reply CRC mismatch: theirs=%04x ours=%04x", ctx->app_id, status->reply_crc, crc);
  }

  NLOGE("Unable to get valid checksum on app %d reply data", ctx->app_id);
  return APP_ERROR_IO;
}

/*
 * Driver for the master of the transport protocol.
 */
uint32_t nos_call_application(const struct nos_device *dev,
                              uint8_t app_id, uint16_t params,
                              const uint8_t *args, uint32_t arg_len,
                              uint8_t *reply, uint32_t *reply_len)
{
  uint32_t res;
  const struct transport_context ctx = {
    .dev = dev,
    .app_id = app_id,
    .params = params,
    .args = args,
    .arg_len = arg_len,
    .reply = reply,
    .reply_len = reply_len,
  };

  if ((ctx.arg_len && !ctx.args) ||
      (ctx.reply_len && *ctx.reply_len && !ctx.reply)) {
    NLOGE("Invalid args to %s()", __func__);
    return APP_ERROR_IO;
  }

  NLOGD("Calling app %d with params 0x%04x", app_id, params);

  struct transport_status status;
  uint32_t status_code;
  int retries = CRC_RETRY_COUNT;
  while (retries--) {
    /* Wake up and wait for Citadel to be ready */
    res = make_ready(&ctx);
    if (res) return res;

    /* Tell the app what to do */
    res = send_command(&ctx);
    if (res) return res;

    /* Wait until the app has finished */
    status_code = poll_until_done(&ctx, &status);

    /* Citadel chip complained we sent it a count different from what we claimed
     * or more than it can accept but this should not happen. Give to the chip a
     * little bit of time and retry calling again. */
    if (status_code == APP_ERROR_TOO_MUCH) {
      NLOGD("App %d returning 0x%x, give a retry(%d/%d)",
            app_id, status_code, retries, CRC_RETRY_COUNT);
      usleep(RETRY_WAIT_TIME_US);
      continue;
    }
    if (status_code != APP_ERROR_CHECKSUM) break;
    NLOGW("App %d request checksum error", app_id);
  }
  if (status_code == APP_ERROR_CHECKSUM) {
    NLOGE("App %d equest checksum failed too many times", app_id);
    status_code = APP_ERROR_IO;
  }

  /* Get the reply, but only if the app produced data and the caller wants it */
  if (ctx.reply && ctx.reply_len && *ctx.reply_len && status.reply_len) {
    res = receive_reply(&ctx, &status);
    if (res) return res;
  } else if (reply_len) {
    *reply_len = 0;
  }

  NLOGV("Clear app %d reply for the next caller", app_id);
  /* This should work, but isn't completely fatal if it doesn't because the
   * next call will try again. */
  (void)clear_status(&ctx);

  NLOGD("App %d returning 0x%x", app_id, status_code);
  return status_code;
}
