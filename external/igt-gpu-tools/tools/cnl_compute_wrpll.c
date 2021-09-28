/*
 * Copyright © 2017 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define U32_MAX         ((uint32_t)~0ULL)
#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

static inline uint64_t div_u64(uint64_t dividend, uint32_t divisor)
{
	return dividend / divisor;
}

struct skl_wrpll_params {
	uint32_t dco_fraction;
	uint32_t dco_integer;
	uint32_t qdiv_ratio;
	uint32_t qdiv_mode;
	uint32_t kdiv;
	uint32_t pdiv;

	/* for this test code only */
	unsigned int ref_clock;
} __attribute__((packed));

static void dump_params(const char *name, struct skl_wrpll_params *params)
{
	printf("%s:\n", name);
	printf("Pdiv: %d\n", params->pdiv);
	printf("Qdiv: %d\n", params->qdiv_ratio);
	printf("Kdiv: %d\n", params->kdiv);
	printf("qdiv mode: %d\n", params->qdiv_mode);
	printf("dco integer: %d\n", params->dco_integer);
	printf("dco fraction: %d\n", params->dco_fraction);
}

static void compare_params(unsigned int clock,
			   const char *name1, struct skl_wrpll_params *p1,
			   const char *name2, struct skl_wrpll_params *p2)
{
	if (memcmp(p1, p2, sizeof(struct skl_wrpll_params)) == 0)
		return;

	printf("=======================================\n");
	printf("Difference with clock: %10.6f MHz\n", clock/1000000.0);
	printf("Reference clock:       %10.6f MHz\n\n", p1->ref_clock/1000.0);
	dump_params(name1, p1);
	printf("\n");
	dump_params(name2, p2);
	printf("=======================================\n");
}

static void cnl_wrpll_params_populate(struct skl_wrpll_params *params,
				      uint32_t dco_freq, uint32_t ref_freq,
				      uint32_t pdiv, uint32_t qdiv,
				      uint32_t kdiv)
{
	uint32_t dco;

	params->qdiv_ratio = qdiv;
	params->qdiv_mode = (qdiv == 1) ? 0 : 1;
	params->pdiv = pdiv;
	params->kdiv = kdiv;

	if (kdiv != 2 && qdiv != 1)
		printf("kdiv != 2 and qdiv != 1\n");

	dco = div_u64((uint64_t)dco_freq << 15, ref_freq);

	params->dco_integer = dco >> 15;
	params->dco_fraction = dco & 0x7fff;
}

static void cnl_wrpll_get_multipliers(int bestdiv,
				      int *pdiv,
				      int *qdiv,
				      int *kdiv)
{
	/* even dividers */
	if (bestdiv % 2 == 0) {
		if (bestdiv == 2) {
			*pdiv = 2;
			*qdiv = 1;
			*kdiv = 1;
		} else if (bestdiv % 4 == 0) {
			*pdiv = 2;
			*qdiv = bestdiv / 4;
			*kdiv = 2;
		} else if (bestdiv % 6 == 0) {
			*pdiv = 3;
			*qdiv = bestdiv / 6;
			*kdiv = 2;
		} else if (bestdiv % 5 == 0) {
			*pdiv = 5;
			*qdiv = bestdiv / 10;
			*kdiv = 2;
		} else if (bestdiv % 14 == 0) {
			*pdiv = 7;
			*qdiv = bestdiv / 14;
			*kdiv = 2;
		}
	} else {
		if (bestdiv == 3 || bestdiv == 5 || bestdiv == 7) {
			*pdiv = bestdiv;
			*qdiv = 1;
			*kdiv = 1;
		} else { /* 9, 15, 21 */
			*pdiv = bestdiv / 3;
			*qdiv = 1;
			*kdiv = 3;
		}
	}
}

static bool
cnl_ddi_calculate_wrpll1(int clock /* in Hz */,
			 struct skl_wrpll_params *params)
{
	double afe_clock = (clock/1000000.0) * 5; /* clocks in MHz */
	double dco_min = 7998;
	double dco_max = 10000;
	double dco_mid = (dco_min + dco_max) / 2;
	static const int dividers[] = {  2,  4,  6,  8, 10, 12,  14,  16,
					 18, 20, 24, 28, 30, 32,  36,  40,
					 42, 44, 48, 50, 52, 54,  56,  60,
					 64, 66, 68, 70, 72, 76,  78,  80,
					 84, 88, 90, 92, 96, 98, 100, 102,
					 3,  5,  7,  9, 15, 21 };
	double dco, dco_centrality = 0;
	double best_dco_centrality = 999999;
	int d, best_div = 0, pdiv = 0, qdiv = 0, kdiv = 0;
	double ref_clock = params->ref_clock/1000.0; /* MHz */
	uint32_t dco_int, dco_frac;

	for (d = 0; d < ARRAY_SIZE(dividers); d++) {
		dco = afe_clock * dividers[d];

		if ((dco <= dco_max) && (dco >= dco_min)) {
			dco_centrality = fabs(dco - dco_mid);

			if (dco_centrality < best_dco_centrality) {
				best_dco_centrality = dco_centrality;
				best_div = dividers[d];
				dco_int = (uint32_t)(dco/ref_clock);
				dco_frac = round((dco/ref_clock - dco_int) * (1<<15));
			}
		}
	}

	if (best_div != 0) {
		cnl_wrpll_get_multipliers(best_div, &pdiv, &qdiv, &kdiv);

		params->qdiv_ratio = qdiv;
		params->qdiv_mode = (qdiv == 1) ? 0 : 1;
		params->pdiv = pdiv;
		params->kdiv = kdiv;
		params->dco_integer = dco_int;
		params->dco_fraction = dco_frac;
	} else {
		return false;
	}

	return true;
}

static bool
cnl_ddi_calculate_wrpll2(int clock,
			 struct skl_wrpll_params *params)
{
	uint32_t afe_clock = clock * 5 / 1000; /* clock in kHz */
	uint32_t dco_min = 7998000;
	uint32_t dco_max = 10000000;
	uint32_t dco_mid = (dco_min + dco_max) / 2;
	static const int dividers[] = {  2,  4,  6,  8, 10, 12,  14,  16,
					 18, 20, 24, 28, 30, 32,  36,  40,
					 42, 44, 48, 50, 52, 54,  56,  60,
					 64, 66, 68, 70, 72, 76,  78,  80,
					 84, 88, 90, 92, 96, 98, 100, 102,
					  3,  5,  7,  9, 15, 21 };
	uint32_t dco, best_dco = 0, dco_centrality = 0;
	uint32_t best_dco_centrality = U32_MAX; /* Spec meaning of 999999 MHz */
	int d, best_div = 0, pdiv = 0, qdiv = 0, kdiv = 0;
	uint32_t ref_clock = params->ref_clock;

	for (d = 0; d < ARRAY_SIZE(dividers); d++) {
		dco = afe_clock * dividers[d];

		if ((dco <= dco_max) && (dco >= dco_min)) {
			dco_centrality = abs(dco - dco_mid);

			if (dco_centrality < best_dco_centrality) {
				best_dco_centrality = dco_centrality;
				best_div = dividers[d];
				best_dco = dco;
			}
		}
	}

	if (best_div == 0)
		return false;

	cnl_wrpll_get_multipliers(best_div, &pdiv, &qdiv, &kdiv);

	cnl_wrpll_params_populate(params, best_dco, ref_clock,
				  pdiv, qdiv, kdiv);

	return true;
}

static void test_multipliers(unsigned int clock)
{
	uint64_t afe_clock = clock * 5 / 1000; /* clocks in kHz */
	unsigned int dco_min = 7998000;
	unsigned int dco_max = 10000000;
	unsigned int dco_mid = (dco_min + dco_max) / 2;

	static const int dividerlist[] = {  2,  4,  6,  8, 10, 12,  14,  16,
					   18, 20, 24, 28, 30, 32,  36,  40,
					   42, 44, 48, 50, 52, 54,  56,  60,
					   64, 66, 68, 70, 72, 76,  78,  80,
					   84, 88, 90, 92, 96, 98, 100, 102,
					    3,  5,  7,  9, 15, 21 };
	unsigned int dco, dco_centrality = 0;
	unsigned int best_dco_centrality = U32_MAX;
	int d, best_div = 0, pdiv = 0, qdiv = 0, kdiv = 0;

	for (d = 0; d < ARRAY_SIZE(dividerlist); d++) {
		dco = afe_clock * dividerlist[d];

		if ((dco <= dco_max) && (dco >= dco_min)) {
			dco_centrality = abs(dco - dco_mid);

			if (dco_centrality < best_dco_centrality) {
				best_dco_centrality = dco_centrality;
				best_div = dividerlist[d];
			}
		}

		if (best_div != 0) {
			cnl_wrpll_get_multipliers(best_div, &pdiv, &qdiv, &kdiv);

			if ((kdiv != 2) && (qdiv == 1))
				continue;
			else
				break;
		}
	}

	assert(pdiv);
	assert(qdiv);
	assert(kdiv);

	if (kdiv != 2)
		assert(qdiv == 1);
}

static const struct {
	uint32_t clock; /* in Hz */
} modes[] = {
	{19750000},
	{23500000},
	{23750000},
	{25175000},
	{25200000},
	{26000000},
	{27000000},
	{27027000},
	{27500000},
	{28750000},
	{29750000},
	{30750000},
	{31500000},
	{35000000},
	{35500000},
	{36750000},
	{37000000},
	{37088000},
	{37125000},
	{37762500},
	{37800000},
	{38250000},
	{40500000},
	{40541000},
	{40750000},
	{41000000},
	{41500000},
	{42500000},
	{45250000},
	{46360000},
	{46406000},
	{46750000},
	{49000000},
	{50500000},
	{52000000},
	{54000000},
	{54054000},
	{54500000},
	{55632000},
	{55688000},
	{56000000},
	{56750000},
	{58250000},
	{58750000},
	{59341000},
	{59400000},
	{60500000},
	{62250000},
	{63500000},
	{64000000},
	{65250000},
	{65500000},
	{66750000},
	{67750000},
	{68250000},
	{69000000},
	{72000000},
	{74176000},
	{74250000},
	{74500000},
	{75250000},
	{76000000},
	{79500000},
	{81000000},
	{81081000},
	{82000000},
	{83000000},
	{84750000},
	{85250000},
	{85750000},
	{88500000},
	{89012000},
	{89100000},
	{91000000},
	{92719800},
	{92812500},
	{94500000},
	{95750000},
	{97750000},
	{99000000},
	{99750000},
	{100000000},
	{100500000},
	{101000000},
	{101250000},
	{102250000},
	{107892000},
	{108000000},
	{108108000},
	{109000000},
	{110250000},
	{110500000},
	{111264000},
	{111375000},
	{112500000},
	{117500000},
	{119000000},
	{119500000},
	{121250000},
	{121750000},
	{125250000},
	{125750000},
	{127250000},
	{130000000},
	{130250000},
	{131000000},
	{131500000},
	{132750000},
	{135250000},
	{138500000},
	{138750000},
	{141500000},
	{146250000},
	{148250000},
	{148352000},
	{148500000},
	{154000000},
	{155250000},
	{155750000},
	{156000000},
	{158250000},
	{159500000},
	{161000000},
	{162000000},
	{162162000},
	{162500000},
	{169500000},
	{172750000},
	{173000000},
	{175000000},
	{178500000},
	{179500000},
	{184750000},
	{185440000},
	{185625000},
	{187000000},
	{192250000},
	{193250000},
	{197750000},
	{198500000},
	{204750000},
	{207500000},
	{209250000},
	{213750000},
	{214750000},
	{216000000},
	{218750000},
	{219000000},
	{220750000},
	{222525000},
	{222750000},
	{227000000},
	{230250000},
	{233500000},
	{235000000},
	{238000000},
	{241500000},
	{243000000},
	{245250000},
	{247750000},
	{253250000},
	{256250000},
	{262500000},
	{267250000},
	{268500000},
	{270000000},
	{272500000},
	{273750000},
	{280750000},
	{281250000},
	{286000000},
	{291750000},
	{296703000},
	{297000000},
	{298000000},
	{303750000},
	{322250000},
	{324000000},
	{337750000},
	{370878750},
	{371250000},
	{373250000},
	{414500000},
	{432000000},
	{445054500},
	{445500000},
	{497750000},
	{533250000},
	{540000000},
	{592500000},
	{594000000},
	{648000000},
	{810000000},
};

static void test_run(unsigned int ref_clock)
{
	unsigned int m;
	struct skl_wrpll_params params[2];

	for (m = 0; m < ARRAY_SIZE(modes); m++) {
		int clock = modes[m].clock;
		bool skip = false;

		params[0].ref_clock = params[1].ref_clock = ref_clock;

		if (!cnl_ddi_calculate_wrpll1(clock, &params[0])) {
			fprintf(stderr, "Reference: Couldn't compute divider for %dHz, reference %dHz\n",
				clock, params[0].ref_clock*1000);
			skip = true;
		}

		if (!skip) {
			if (!cnl_ddi_calculate_wrpll2(clock, &params[1])) {
				fprintf(stderr, "i915 implementation: Couldn't compute divider for %dHz, reference %dHz\n",
					clock, params[1].ref_clock*1000);
			}

			compare_params(clock, "Reference", &params[0],
				       "i915 implementation", &params[1]);
		}
	}
}

int main(int argc, char **argv)
{
	unsigned int m;
	unsigned int f;
	unsigned int ref_clocks[] = {19200, 24000}; /* in kHz */

	for (m = 0; m < ARRAY_SIZE(modes); m++)
		test_multipliers(modes[m].clock);

	for (f = 0; f < ARRAY_SIZE(ref_clocks); f++) {
		printf("=== Testing with ref clock %d kHz\n", ref_clocks[f]);
		test_run(ref_clocks[f]);
	}

	return 0;
}
