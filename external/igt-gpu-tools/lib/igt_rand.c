#include "igt_rand.h"

/**
 * SECTION:igt_rand
 * @short_description: Random numbers helper library
 * @title: Random
 * @include: igt_rand.h
 */

static uint32_t global = 0x12345678;

uint32_t hars_petruska_f54_1_random_seed(uint32_t new_state)
{
	uint32_t old_state = global;
	global = new_state;
	return old_state;
}

uint32_t hars_petruska_f54_1_random(uint32_t *s)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
	return *s = (*s ^ rol(*s, 5) ^ rol(*s, 24)) + 0x37798849;
#undef rol
}

uint32_t hars_petruska_f54_1_random_unsafe(void)
{
	return hars_petruska_f54_1_random(&global);
}
