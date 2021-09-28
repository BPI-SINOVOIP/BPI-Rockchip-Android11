/*
 * Copyright © 2016 Intel Corporation
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
 */

#include "igt_primes.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

/**
 * SECTION:igt_primes
 * @short_description: Prime numbers helper library
 * @title: Primes
 * @include: igt_primes.h
 */

#define BITS_PER_CHAR 8
#define BITS_PER_LONG (sizeof(long)*BITS_PER_CHAR)

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) & (BITS_PER_LONG - 1)))
#define BITMAP_LAST_WORD_MASK(nbits) (~0UL >> (-(nbits) & (BITS_PER_LONG - 1)))

#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))

#define min(x, y) ({                            \
	typeof(x) _min1 = (x);                  \
	typeof(y) _min2 = (y);                  \
	(void) (&_min1 == &_min2);              \
	_min1 < _min2 ? _min1 : _min2;		\
})

#define max(x, y) ({                            \
	typeof(x) _max1 = (x);                  \
	typeof(y) _max2 = (y);                  \
	(void) (&_max1 == &_max2);              \
	_max1 > _max2 ? _max1 : _max2;		\
})

static inline unsigned long __bit__(unsigned long nr)
{
	return 1UL << (nr % BITS_PER_LONG);
}

static inline void set_bit(unsigned long nr, unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] |= __bit__(nr);
}

static inline void clear_bit(unsigned long nr, unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] &= ~__bit__(nr);
}

static inline bool test_bit(unsigned long nr, const unsigned long *addr)
{
	return addr[nr / BITS_PER_LONG] & __bit__(nr);
}

static unsigned long
__find_next_bit(const unsigned long *addr,
		unsigned long nbits, unsigned long start,
		unsigned long invert)
{
	unsigned long tmp;

	if (!nbits || start >= nbits)
		return nbits;

	tmp = addr[start / BITS_PER_LONG] ^ invert;

	/* Handle 1st word. */
	tmp &= BITMAP_FIRST_WORD_MASK(start);
	start = round_down(start, BITS_PER_LONG);

	while (!tmp) {
		start += BITS_PER_LONG;
		if (start >= nbits)
			return nbits;

		tmp = addr[start / BITS_PER_LONG] ^ invert;
	}

	return min(start + __builtin_ffsl(tmp) - 1, nbits);
}

static unsigned long find_next_bit(const unsigned long *addr,
		unsigned long size,
		unsigned long offset)
{
	return __find_next_bit(addr, size, offset, 0UL);
}

static unsigned long slow_next_prime_number(unsigned long x)
{
	for (;;) {
		unsigned long y = sqrt(++x) + 1;
		while (y > 1) {
			if ((x % y) == 0)
				break;
			y--;
		}
		if (y == 1)
			return x;
	}
}

static unsigned long mark_multiples(unsigned long x,
				    unsigned long *primes,
				    unsigned long start,
				    unsigned long end)
{
	unsigned long m;

	m = 2*x;
	if (m < start)
		m = (start / x + 1) * x;

	while (m < end) {
		clear_bit(m, primes);
		m += x;
	}

	return x;
}

unsigned long igt_next_prime_number(unsigned long x)
{
	static unsigned long *primes;
	static unsigned long last, last_sz;

	if (x == 0)
		return 1; /* a white lie for for_each_prime_number() */
	if (x == 1)
		return 2;

	if (x >= last) {
		unsigned long sz, y;
		unsigned long *nprimes;

		sz = x*x;
		if (sz < x)
			return slow_next_prime_number(x);

		sz = round_up(sz, BITS_PER_LONG);
		nprimes = realloc(primes, sz / sizeof(long));
		if (!nprimes)
			return slow_next_prime_number(x);

		/* Where memory permits, track the primes using the
		 * Sieve of Eratosthenes.
		 */
		memset(nprimes + last_sz / BITS_PER_LONG,
		       0xff, (sz - last_sz) / sizeof(long));
		for (y = 2UL; y < sz; y = find_next_bit(nprimes, sz, y + 1))
			last = mark_multiples(y, nprimes, last_sz, sz);

		primes = nprimes;
		last_sz = sz;
	}

	return find_next_bit(primes, last, x + 1);
}
