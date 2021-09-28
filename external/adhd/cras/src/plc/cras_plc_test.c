/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "cras_sbc_codec.h"
#include "cras_plc.h"

#define MSBC_CODE_SIZE 240
#define MSBC_PKT_FRAME_LEN 57
#define RND_SEED 7

static const uint8_t msbc_zero_frame[] = {
	0xad, 0x00, 0x00, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x77, 0x6d, 0xb6, 0xdd,
	0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d, 0xb6,
	0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77, 0x6d,
	0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6d, 0xdd, 0xb6, 0xdb, 0x77,
	0x6d, 0xb6, 0xdd, 0xdb, 0x6d, 0xb7, 0x76, 0xdb, 0x6c
};

bool *generate_pl_seq(unsigned pk_count, unsigned loss_count)
{
	bool *seq = (bool *)calloc(pk_count, sizeof(*seq));
	srand(RND_SEED);
	while (loss_count > 0) {
		bool *missed = &seq[rand() % pk_count];
		if (!*missed) {
			*missed = true;
			loss_count--;
		}
	}
	return seq;
}

void plc_experiment(char *input_filename, float pl_percent, bool with_plc)
{
	char output_filename[255];
	int input_fd, output_fd, rc;
	struct stat st;
	bool *pl_seq;
	struct cras_audio_codec *msbc_input = cras_msbc_codec_create();
	struct cras_audio_codec *msbc_output = cras_msbc_codec_create();
	struct cras_msbc_plc *plc = cras_msbc_plc_create();
	uint8_t buffer[MSBC_CODE_SIZE], packet_buffer[MSBC_PKT_FRAME_LEN];
	size_t encoded, decoded;
	unsigned pk_count, pl_count, count = 0;

	input_fd = open(input_filename, O_RDONLY);
	if (input_fd == -1) {
		fprintf(stderr, "Cannout open input file %s\n", input_filename);
		return;
	}

	if (with_plc)
		sprintf(output_filename, "output_%2.2f_plc.raw", pl_percent);
	else
		sprintf(output_filename, "output_%2.2f_zero.raw", pl_percent);

	output_fd = open(output_filename, O_CREAT | O_RDWR | O_TRUNC, 0644);
	if (output_fd == -1) {
		fprintf(stderr, "Cannot open output file %s\n",
			output_filename);
		return;
	}

	fstat(input_fd, &st);
	pk_count = st.st_size / MSBC_CODE_SIZE;
	pl_count = pk_count * (pl_percent / 100.0);
	pl_seq = generate_pl_seq(pk_count, pl_count);

	while (1) {
		rc = read(input_fd, buffer, MSBC_CODE_SIZE);
		if (rc < 0) {
			fprintf(stderr, "Cannot read file %s", input_filename);
			return;
		} else if (rc == 0 || rc < MSBC_CODE_SIZE)
			break;

		msbc_input->encode(msbc_input, buffer, MSBC_CODE_SIZE,
				   packet_buffer, MSBC_PKT_FRAME_LEN, &encoded);

		if (pl_seq[count]) {
			if (with_plc) {
				cras_msbc_plc_handle_bad_frames(
					plc, msbc_output, buffer);
				decoded = MSBC_CODE_SIZE;
			} else
				msbc_output->decode(msbc_output,
						    msbc_zero_frame,
						    MSBC_PKT_FRAME_LEN, buffer,
						    MSBC_CODE_SIZE, &decoded);
		} else {
			msbc_output->decode(msbc_output, packet_buffer,
					    MSBC_PKT_FRAME_LEN, buffer,
					    MSBC_CODE_SIZE, &decoded);
			cras_msbc_plc_handle_good_frames(plc, buffer, buffer);
		}

		count++;
		rc = write(output_fd, buffer, decoded);
		if (rc < 0) {
			fprintf(stderr, "Cannot write file %s\n",
				output_filename);
			return;
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: cras_plc_test input.raw pl_percentage\n"
		       "This test only supports reading/writing files with "
		       "format:\n"
		       "- raw pcm\n"
		       "- 16000 sample rate\n"
		       "- mono channel\n"
		       "- S16_LE sample format\n");
		return 1;
	}

	plc_experiment(argv[1], atof(argv[2]), true);
	plc_experiment(argv[1], atof(argv[2]), false);
}
