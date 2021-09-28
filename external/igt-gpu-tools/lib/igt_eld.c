/*
 * Copyright © 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors: Simon Ser <simon.ser@intel.com>
 */

#include "config.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "igt_core.h"
#include "igt_eld.h"

#define ELD_PREFIX "eld#"
#define ELD_DELIM " \t"

/**
 * EDID-Like Data (ELD) is metadata parsed and exposed by ALSA for HDMI and
 * DisplayPort connectors supporting audio. This includes the monitor name and
 * the supported audio parameters (formats, sampling rates, sample sizes and so
 * on).
 *
 * Audio parameters come from Short Audio Descriptors (SAD) blocks in the
 * EDID. Enumerations from igt_edid are used since they are the same.
 */

static enum cea_sad_format parse_sad_coding_type(const char *value)
{
	if (strcmp(value, "LPCM") == 0)
		return CEA_SAD_FORMAT_PCM;
	else
		return 0;
}

static enum cea_sad_sampling_rate parse_sad_rate(const char *value)
{
	switch (atoi(value)) {
	case 32000:
		return CEA_SAD_SAMPLING_RATE_32KHZ;
	case 44100:
		return CEA_SAD_SAMPLING_RATE_44KHZ;
	case 48000:
		return CEA_SAD_SAMPLING_RATE_48KHZ;
	case 88000:
		return CEA_SAD_SAMPLING_RATE_88KHZ;
	case 96000:
		return CEA_SAD_SAMPLING_RATE_96KHZ;
	case 176000:
		return CEA_SAD_SAMPLING_RATE_176KHZ;
	case 192000:
		return CEA_SAD_SAMPLING_RATE_192KHZ;
	default:
		return 0;
	}
}

static enum cea_sad_pcm_sample_size parse_sad_bit(const char *value)
{
	switch (atoi(value)) {
	case 16:
		return CEA_SAD_SAMPLE_SIZE_16;
	case 20:
		return CEA_SAD_SAMPLE_SIZE_20;
	case 24:
		return CEA_SAD_SAMPLE_SIZE_24;
	default:
		return 0;
	}
}

static void parse_sad_field(struct eld_sad *sad, const char *key, char *value)
{
	char *tok;

	/* Some fields are prefixed with the raw hex value, strip it */
	if (value[0] == '[') {
		value = strchr(value, ' ');
		igt_assert(value != NULL);
		value++; /* skip the space */
	}

	/* Single-value fields */
	if (strcmp(key, "coding_type") == 0)
		sad->coding_type = parse_sad_coding_type(value);
	else if (strcmp(key, "channels") == 0)
		sad->channels = atoi(value);

	/* Multiple-value fields */
	tok = strtok(value, " ");
	while (tok) {
		if (strcmp(key, "rates") == 0)
			sad->rates |= parse_sad_rate(tok);
		else if (strcmp(key, "bits") == 0)
			sad->bits |= parse_sad_bit(tok);

		tok = strtok(NULL, " ");
	}
}

/** eld_parse_entry: parse an ELD entry
 *
 * Here is an example of an ELD entry:
 *
 *     $ cat /proc/asound/card0/eld#0.2
 *     monitor_present         1
 *     eld_valid               1
 *     monitor_name            U2879G6
 *     connection_type         DisplayPort
 *     eld_version             [0x2] CEA-861D or below
 *     edid_version            [0x3] CEA-861-B, C or D
 *     manufacture_id          0xe305
 *     product_id              0x2879
 *     port_id                 0x800
 *     support_hdcp            0
 *     support_ai              0
 *     audio_sync_delay        0
 *     speakers                [0x1] FL/FR
 *     sad_count               1
 *     sad0_coding_type        [0x1] LPCM
 *     sad0_channels           2
 *     sad0_rates              [0xe0] 32000 44100 48000
 *     sad0_bits               [0xe0000] 16 20 24
 *
 * Each entry contains one or more SAD blocks. Their contents is exposed in
 * sadN_* fields.
 */
static bool eld_parse_entry(const char *path, struct eld_entry *eld)
{
	FILE *f;
	char buf[1024];
	char *key, *value, *sad_key;
	size_t len;
	bool monitor_present = false;
	int sad_index;

	memset(eld, 0, sizeof(*eld));

	f = fopen(path, "r");
	if (!f) {
		igt_debug("Failed to open ELD file: %s\n", path);
		return false;
	}

	while ((fgets(buf, sizeof(buf), f)) != NULL) {
		len = strlen(buf);
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';

		key = strtok(buf, ELD_DELIM);
		value = strtok(NULL, "");
		/* Skip whitespace at the beginning */
		value += strspn(value, ELD_DELIM);

		if (strcmp(key, "monitor_present") == 0)
			monitor_present = strcmp(value, "1") == 0;
		else if (strcmp(key, "eld_valid") == 0)
			eld->valid = strcmp(value, "1") == 0;
		else if (strcmp(key, "monitor_name") == 0)
			snprintf(eld->monitor_name, sizeof(eld->monitor_name),
				 "%s", value);
		else if (strcmp(key, "sad_count") == 0)
			eld->sads_len = atoi(value);
		else if (sscanf(key, "sad%d_%ms", &sad_index, &sad_key) == 2) {
			igt_assert(sad_index < ELD_SADS_CAP);
			igt_assert(sad_index < eld->sads_len);
			parse_sad_field(&eld->sads[sad_index], sad_key, value);
			free(sad_key);
		}
	}

	if (ferror(f) != 0) {
		igt_debug("Failed to read ELD file %s: %d\n", path, ferror(f));
		return false;
	}

	fclose(f);

	if (!monitor_present)
		igt_debug("Monitor not present in ELD: %s\n", path);
	return monitor_present;
}

/** eld_get_igt: retrieve the ALSA ELD entry matching the IGT EDID */
bool eld_get_igt(struct eld_entry *eld)
{
	DIR *dir;
	struct dirent *dirent;
	int i, n_elds;
	char card[64];
	char path[PATH_MAX];

	n_elds = 0;
	for (i = 0; i < 8; i++) {
		snprintf(card, sizeof(card), "/proc/asound/card%d", i);
		dir = opendir(card);
		if (!dir)
			continue;

		while ((dirent = readdir(dir))) {
			if (strncmp(dirent->d_name, ELD_PREFIX,
				    strlen(ELD_PREFIX)) != 0)
				continue;

			n_elds++;

			snprintf(path, sizeof(path), "%s/%s", card,
				 dirent->d_name);
			if (!eld_parse_entry(path, eld)) {
				continue;
			}

			if (!eld->valid) {
				igt_debug("Skipping invalid ELD: %s\n", path);
				continue;
			}

			if (strcmp(eld->monitor_name, "IGT") != 0) {
				igt_debug("Skipping non-IGT ELD: %s "
					  "(monitor name: %s)\n",
					  path, eld->monitor_name);
				continue;
			}

			closedir(dir);
			return true;
		}
		closedir(dir);
	}

	if (n_elds == 0)
		igt_debug("Found zero ELDs\n");

	return false;
}

/** eld_has_igt: check whether ALSA has detected the audio-capable IGT EDID by
 * parsing ELD entries */
bool eld_has_igt(void)
{
	struct eld_entry eld;
	return eld_get_igt(&eld);
}
