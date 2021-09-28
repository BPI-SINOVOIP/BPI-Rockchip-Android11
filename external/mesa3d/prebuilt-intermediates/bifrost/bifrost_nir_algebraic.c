#include "bifrost_nir.h"

#include "nir.h"
#include "nir_builder.h"
#include "nir_search.h"
#include "nir_search_helpers.h"

/* What follows is NIR algebraic transform code for the following 79
 * transforms:
 *    ('ineg', 'a') => ('isub', 0, 'a')
 *    ('b2f16', 'a@8') => ('b8csel', 'a', 1.0, 0.0)
 *    ('b2f32', 'a@8') => ('b8csel', 'a', 1.0, 0.0)
 *    ('b2f64', 'a@8') => ('b8csel', 'a', 1.0, 0.0)
 *    ('b2f16', 'a@16') => ('b16csel', 'a', 1.0, 0.0)
 *    ('b2f32', 'a@16') => ('b16csel', 'a', 1.0, 0.0)
 *    ('b2f64', 'a@16') => ('b16csel', 'a', 1.0, 0.0)
 *    ('b2f16', 'a@32') => ('b32csel', 'a', 1.0, 0.0)
 *    ('b2f32', 'a@32') => ('b32csel', 'a', 1.0, 0.0)
 *    ('b2f64', 'a@32') => ('b32csel', 'a', 1.0, 0.0)
 *    ('imin', 'a@8', 'b@8') => ('b8csel', ('ilt8', 'a', 'b'), 'a', 'b')
 *    ('imax', 'a@8', 'b@8') => ('b8csel', ('ilt8', 'b', 'a'), 'a', 'b')
 *    ('umin', 'a@8', 'b@8') => ('b8csel', ('ult8', 'a', 'b'), 'a', 'b')
 *    ('umax', 'a@8', 'b@8') => ('b8csel', ('ult8', 'b', 'a'), 'a', 'b')
 *    ('imin', 'a@16', 'b@16') => ('b16csel', ('ilt16', 'a', 'b'), 'a', 'b')
 *    ('imax', 'a@16', 'b@16') => ('b16csel', ('ilt16', 'b', 'a'), 'a', 'b')
 *    ('umin', 'a@16', 'b@16') => ('b16csel', ('ult16', 'a', 'b'), 'a', 'b')
 *    ('umax', 'a@16', 'b@16') => ('b16csel', ('ult16', 'b', 'a'), 'a', 'b')
 *    ('imin', 'a@32', 'b@32') => ('b32csel', ('ilt32', 'a', 'b'), 'a', 'b')
 *    ('imax', 'a@32', 'b@32') => ('b32csel', ('ilt32', 'b', 'a'), 'a', 'b')
 *    ('umin', 'a@32', 'b@32') => ('b32csel', ('ult32', 'a', 'b'), 'a', 'b')
 *    ('umax', 'a@32', 'b@32') => ('b32csel', ('ult32', 'b', 'a'), 'a', 'b')
 *    ('u2u8', 'a@32') => ('u2u8', ('u2u16', 'a'))
 *    ('u2u8', 'a@64') => ('u2u8', ('u2u16', ('u2u32', 'a')))
 *    ('u2u16', 'a@64') => ('u2u16', ('u2u32', 'a'))
 *    ('u2u32', 'a@8') => ('u2u32', ('u2u16', 'a'))
 *    ('u2u64', 'a@8') => ('u2u64', ('u2u32', ('u2u16', 'a')))
 *    ('u2u64', 'a@16') => ('u2u64', ('u2u32', 'a'))
 *    ('i2i8', 'a@32') => ('i2i8', ('i2i16', 'a'))
 *    ('i2i8', 'a@64') => ('i2i8', ('i2i16', ('i2i32', 'a')))
 *    ('i2i16', 'a@64') => ('i2i16', ('i2i32', 'a'))
 *    ('i2i32', 'a@8') => ('i2i32', ('i2i16', 'a'))
 *    ('i2i64', 'a@8') => ('i2i64', ('i2i32', ('i2i16', 'a')))
 *    ('i2i64', 'a@16') => ('i2i64', ('i2i32', 'a'))
 *    ('f2f16', 'a@64') => ('f2f16', ('f2f32', 'a'))
 *    ('f2f64', 'a@16') => ('f2f64', ('f2f32', 'a'))
 *    ('i2f16', 'a@8') => ('i2f16', ('i2i16', 'a'))
 *    ('i2f16', 'a@32') => ('f2f16', ('i2f32', 'a'))
 *    ('i2f16', 'a@64') => ('f2f16', ('f2f32', ('i2f64', 'a')))
 *    ('i2f32', 'a@8') => ('i2f32', ('i2i32', ('i2i16', 'a')))
 *    ('i2f32', 'a@16') => ('i2f32', ('i2i32', 'a'))
 *    ('i2f32', 'a@64') => ('f2f32', ('i2f64', 'a'))
 *    ('i2f64', 'a@8') => ('i2f64', ('i2i64', ('i2i32', ('i2i16', 'a'))))
 *    ('i2f64', 'a@16') => ('i2f64', ('i2i64', ('i2i32', 'a')))
 *    ('i2f64', 'a@32') => ('i2f64', ('i2i64', 'a'))
 *    ('u2f16', 'a@8') => ('u2f16', ('u2u16', 'a'))
 *    ('u2f16', 'a@32') => ('f2f16', ('u2f32', 'a'))
 *    ('u2f16', 'a@64') => ('f2f16', ('f2f32', ('u2f64', 'a')))
 *    ('u2f32', 'a@8') => ('u2f32', ('u2u32', ('u2u16', 'a')))
 *    ('u2f32', 'a@16') => ('u2f32', ('u2u32', 'a'))
 *    ('u2f32', 'a@64') => ('f2f32', ('u2f64', 'a'))
 *    ('u2f64', 'a@8') => ('u2f64', ('u2u64', ('u2u32', ('u2u16', 'a'))))
 *    ('u2f64', 'a@16') => ('u2f64', ('u2u64', ('u2u32', 'a')))
 *    ('u2f64', 'a@32') => ('u2f64', ('u2u64', 'a'))
 *    ('f2i8', 'a@16') => ('i2i8', ('f2i16', 'a'))
 *    ('f2i8', 'a@32') => ('i2i8', ('i2i16', ('f2i32', 'a')))
 *    ('f2i8', 'a@64') => ('i2i8', ('i2i16', ('i2i32', ('f2i64', 'a'))))
 *    ('f2i16', 'a@32') => ('i2i16', ('f2i32', 'a'))
 *    ('f2i16', 'a@64') => ('i2i16', ('i2i32', ('f2i64', 'a')))
 *    ('f2i32', 'a@16') => ('f2i32', ('f2f32', 'a'))
 *    ('f2i32', 'a@64') => ('i2i32', ('f2i64', 'a'))
 *    ('f2i64', 'a@16') => ('f2i64', ('f2f64', ('f2f32', 'a')))
 *    ('f2i64', 'a@32') => ('f2i64', ('f2f64', 'a'))
 *    ('f2u8', 'a@16') => ('u2u8', ('f2u16', 'a'))
 *    ('f2u8', 'a@32') => ('u2u8', ('u2u16', ('f2u32', 'a')))
 *    ('f2u8', 'a@64') => ('u2u8', ('u2u16', ('u2u32', ('f2u64', 'a'))))
 *    ('f2u16', 'a@32') => ('u2u16', ('f2u32', 'a'))
 *    ('f2u16', 'a@64') => ('u2u16', ('u2u32', ('f2u64', 'a')))
 *    ('f2u32', 'a@16') => ('f2u32', ('f2f32', 'a'))
 *    ('f2u32', 'a@64') => ('u2u32', ('f2u64', 'a'))
 *    ('f2u64', 'a@16') => ('f2u64', ('f2f64', ('f2f32', 'a')))
 *    ('f2u64', 'a@32') => ('f2u64', ('f2f64', 'a'))
 *    ('fexp2@16', 'a') => ('f2f16', ('fexp2', ('f2f32', 'a')))
 *    ('flog2@16', 'a') => ('f2f16', ('flog2', ('f2f32', 'a')))
 *    ('fsin@16', 'a') => ('f2f16', ('fsin', ('f2f32', 'a')))
 *    ('fcos@16', 'a') => ('f2f16', ('fcos', ('f2f32', 'a')))
 *    ('f2b32', 'a') => ('fneu32', 'a', 0.0)
 *    ('i2b32', 'a') => ('ine32', 'a', 0)
 *    ('b2i32', 'a') => ('iand', 'a@32', 1)
 */


   static const nir_search_variable search0_0 = {
   { nir_search_value_variable, -1 },
   0, /* a */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search0 = {
   { nir_search_value_expression, -1 },
   false, false,
   -1, 0,
   nir_op_ineg,
   { &search0_0.value },
   NULL,
};

   static const nir_search_constant replace0_0 = {
   { nir_search_value_constant, -1 },
   nir_type_int, { 0x0 /* 0 */ },
};

/* replace0_1 -> search0_0 in the cache */
static const nir_search_expression replace0 = {
   { nir_search_value_expression, -1 },
   false, false,
   -1, 0,
   nir_op_isub,
   { &replace0_0.value, &search0_0.value },
   NULL,
};

   static const nir_search_variable search1_0 = {
   { nir_search_value_variable, 8 },
   0, /* a */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search1 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_b2f16,
   { &search1_0.value },
   NULL,
};

   /* replace1_0 -> search1_0 in the cache */

static const nir_search_constant replace1_1 = {
   { nir_search_value_constant, 16 },
   nir_type_float, { 0x3ff0000000000000 /* 1.0 */ },
};

static const nir_search_constant replace1_2 = {
   { nir_search_value_constant, 16 },
   nir_type_float, { 0x0 /* 0.0 */ },
};
static const nir_search_expression replace1 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_b8csel,
   { &search1_0.value, &replace1_1.value, &replace1_2.value },
   NULL,
};

   /* search2_0 -> search1_0 in the cache */
static const nir_search_expression search2 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b2f32,
   { &search1_0.value },
   NULL,
};

   /* replace2_0 -> search1_0 in the cache */

static const nir_search_constant replace2_1 = {
   { nir_search_value_constant, 32 },
   nir_type_float, { 0x3ff0000000000000 /* 1.0 */ },
};

static const nir_search_constant replace2_2 = {
   { nir_search_value_constant, 32 },
   nir_type_float, { 0x0 /* 0.0 */ },
};
static const nir_search_expression replace2 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b8csel,
   { &search1_0.value, &replace2_1.value, &replace2_2.value },
   NULL,
};

   /* search3_0 -> search1_0 in the cache */
static const nir_search_expression search3 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_b2f64,
   { &search1_0.value },
   NULL,
};

   /* replace3_0 -> search1_0 in the cache */

static const nir_search_constant replace3_1 = {
   { nir_search_value_constant, 64 },
   nir_type_float, { 0x3ff0000000000000 /* 1.0 */ },
};

static const nir_search_constant replace3_2 = {
   { nir_search_value_constant, 64 },
   nir_type_float, { 0x0 /* 0.0 */ },
};
static const nir_search_expression replace3 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_b8csel,
   { &search1_0.value, &replace3_1.value, &replace3_2.value },
   NULL,
};

   static const nir_search_variable search4_0 = {
   { nir_search_value_variable, 16 },
   0, /* a */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search4 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_b2f16,
   { &search4_0.value },
   NULL,
};

   /* replace4_0 -> search4_0 in the cache */

/* replace4_1 -> replace1_1 in the cache */

/* replace4_2 -> replace1_2 in the cache */
static const nir_search_expression replace4 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_b16csel,
   { &search4_0.value, &replace1_1.value, &replace1_2.value },
   NULL,
};

   /* search5_0 -> search4_0 in the cache */
static const nir_search_expression search5 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b2f32,
   { &search4_0.value },
   NULL,
};

   /* replace5_0 -> search4_0 in the cache */

/* replace5_1 -> replace2_1 in the cache */

/* replace5_2 -> replace2_2 in the cache */
static const nir_search_expression replace5 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b16csel,
   { &search4_0.value, &replace2_1.value, &replace2_2.value },
   NULL,
};

   /* search6_0 -> search4_0 in the cache */
static const nir_search_expression search6 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_b2f64,
   { &search4_0.value },
   NULL,
};

   /* replace6_0 -> search4_0 in the cache */

/* replace6_1 -> replace3_1 in the cache */

/* replace6_2 -> replace3_2 in the cache */
static const nir_search_expression replace6 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_b16csel,
   { &search4_0.value, &replace3_1.value, &replace3_2.value },
   NULL,
};

   static const nir_search_variable search7_0 = {
   { nir_search_value_variable, 32 },
   0, /* a */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search7 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_b2f16,
   { &search7_0.value },
   NULL,
};

   /* replace7_0 -> search7_0 in the cache */

/* replace7_1 -> replace1_1 in the cache */

/* replace7_2 -> replace1_2 in the cache */
static const nir_search_expression replace7 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_b32csel,
   { &search7_0.value, &replace1_1.value, &replace1_2.value },
   NULL,
};

   /* search8_0 -> search7_0 in the cache */
static const nir_search_expression search8 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b2f32,
   { &search7_0.value },
   NULL,
};

   /* replace8_0 -> search7_0 in the cache */

/* replace8_1 -> replace2_1 in the cache */

/* replace8_2 -> replace2_2 in the cache */
static const nir_search_expression replace8 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b32csel,
   { &search7_0.value, &replace2_1.value, &replace2_2.value },
   NULL,
};

   /* search9_0 -> search7_0 in the cache */
static const nir_search_expression search9 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_b2f64,
   { &search7_0.value },
   NULL,
};

   /* replace9_0 -> search7_0 in the cache */

/* replace9_1 -> replace3_1 in the cache */

/* replace9_2 -> replace3_2 in the cache */
static const nir_search_expression replace9 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_b32csel,
   { &search7_0.value, &replace3_1.value, &replace3_2.value },
   NULL,
};

   /* search10_0 -> search1_0 in the cache */

static const nir_search_variable search10_1 = {
   { nir_search_value_variable, 8 },
   1, /* b */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search10 = {
   { nir_search_value_expression, 8 },
   false, false,
   0, 1,
   nir_op_imin,
   { &search1_0.value, &search10_1.value },
   NULL,
};

   /* replace10_0_0 -> search1_0 in the cache */

/* replace10_0_1 -> search10_1 in the cache */
static const nir_search_expression replace10_0 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_ilt8,
   { &search1_0.value, &search10_1.value },
   NULL,
};

/* replace10_1 -> search1_0 in the cache */

/* replace10_2 -> search10_1 in the cache */
static const nir_search_expression replace10 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_b8csel,
   { &replace10_0.value, &search1_0.value, &search10_1.value },
   NULL,
};

   /* search11_0 -> search1_0 in the cache */

/* search11_1 -> search10_1 in the cache */
static const nir_search_expression search11 = {
   { nir_search_value_expression, 8 },
   false, false,
   0, 1,
   nir_op_imax,
   { &search1_0.value, &search10_1.value },
   NULL,
};

   /* replace11_0_0 -> search10_1 in the cache */

/* replace11_0_1 -> search1_0 in the cache */
static const nir_search_expression replace11_0 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_ilt8,
   { &search10_1.value, &search1_0.value },
   NULL,
};

/* replace11_1 -> search1_0 in the cache */

/* replace11_2 -> search10_1 in the cache */
static const nir_search_expression replace11 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_b8csel,
   { &replace11_0.value, &search1_0.value, &search10_1.value },
   NULL,
};

   /* search12_0 -> search1_0 in the cache */

/* search12_1 -> search10_1 in the cache */
static const nir_search_expression search12 = {
   { nir_search_value_expression, 8 },
   false, false,
   0, 1,
   nir_op_umin,
   { &search1_0.value, &search10_1.value },
   NULL,
};

   /* replace12_0_0 -> search1_0 in the cache */

/* replace12_0_1 -> search10_1 in the cache */
static const nir_search_expression replace12_0 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_ult8,
   { &search1_0.value, &search10_1.value },
   NULL,
};

/* replace12_1 -> search1_0 in the cache */

/* replace12_2 -> search10_1 in the cache */
static const nir_search_expression replace12 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_b8csel,
   { &replace12_0.value, &search1_0.value, &search10_1.value },
   NULL,
};

   /* search13_0 -> search1_0 in the cache */

/* search13_1 -> search10_1 in the cache */
static const nir_search_expression search13 = {
   { nir_search_value_expression, 8 },
   false, false,
   0, 1,
   nir_op_umax,
   { &search1_0.value, &search10_1.value },
   NULL,
};

   /* replace13_0_0 -> search10_1 in the cache */

/* replace13_0_1 -> search1_0 in the cache */
static const nir_search_expression replace13_0 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_ult8,
   { &search10_1.value, &search1_0.value },
   NULL,
};

/* replace13_1 -> search1_0 in the cache */

/* replace13_2 -> search10_1 in the cache */
static const nir_search_expression replace13 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_b8csel,
   { &replace13_0.value, &search1_0.value, &search10_1.value },
   NULL,
};

   /* search14_0 -> search4_0 in the cache */

static const nir_search_variable search14_1 = {
   { nir_search_value_variable, 16 },
   1, /* b */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search14 = {
   { nir_search_value_expression, 16 },
   false, false,
   0, 1,
   nir_op_imin,
   { &search4_0.value, &search14_1.value },
   NULL,
};

   /* replace14_0_0 -> search4_0 in the cache */

/* replace14_0_1 -> search14_1 in the cache */
static const nir_search_expression replace14_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_ilt16,
   { &search4_0.value, &search14_1.value },
   NULL,
};

/* replace14_1 -> search4_0 in the cache */

/* replace14_2 -> search14_1 in the cache */
static const nir_search_expression replace14 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_b16csel,
   { &replace14_0.value, &search4_0.value, &search14_1.value },
   NULL,
};

   /* search15_0 -> search4_0 in the cache */

/* search15_1 -> search14_1 in the cache */
static const nir_search_expression search15 = {
   { nir_search_value_expression, 16 },
   false, false,
   0, 1,
   nir_op_imax,
   { &search4_0.value, &search14_1.value },
   NULL,
};

   /* replace15_0_0 -> search14_1 in the cache */

/* replace15_0_1 -> search4_0 in the cache */
static const nir_search_expression replace15_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_ilt16,
   { &search14_1.value, &search4_0.value },
   NULL,
};

/* replace15_1 -> search4_0 in the cache */

/* replace15_2 -> search14_1 in the cache */
static const nir_search_expression replace15 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_b16csel,
   { &replace15_0.value, &search4_0.value, &search14_1.value },
   NULL,
};

   /* search16_0 -> search4_0 in the cache */

/* search16_1 -> search14_1 in the cache */
static const nir_search_expression search16 = {
   { nir_search_value_expression, 16 },
   false, false,
   0, 1,
   nir_op_umin,
   { &search4_0.value, &search14_1.value },
   NULL,
};

   /* replace16_0_0 -> search4_0 in the cache */

/* replace16_0_1 -> search14_1 in the cache */
static const nir_search_expression replace16_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_ult16,
   { &search4_0.value, &search14_1.value },
   NULL,
};

/* replace16_1 -> search4_0 in the cache */

/* replace16_2 -> search14_1 in the cache */
static const nir_search_expression replace16 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_b16csel,
   { &replace16_0.value, &search4_0.value, &search14_1.value },
   NULL,
};

   /* search17_0 -> search4_0 in the cache */

/* search17_1 -> search14_1 in the cache */
static const nir_search_expression search17 = {
   { nir_search_value_expression, 16 },
   false, false,
   0, 1,
   nir_op_umax,
   { &search4_0.value, &search14_1.value },
   NULL,
};

   /* replace17_0_0 -> search14_1 in the cache */

/* replace17_0_1 -> search4_0 in the cache */
static const nir_search_expression replace17_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_ult16,
   { &search14_1.value, &search4_0.value },
   NULL,
};

/* replace17_1 -> search4_0 in the cache */

/* replace17_2 -> search14_1 in the cache */
static const nir_search_expression replace17 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_b16csel,
   { &replace17_0.value, &search4_0.value, &search14_1.value },
   NULL,
};

   /* search18_0 -> search7_0 in the cache */

static const nir_search_variable search18_1 = {
   { nir_search_value_variable, 32 },
   1, /* b */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search18 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 1,
   nir_op_imin,
   { &search7_0.value, &search18_1.value },
   NULL,
};

   /* replace18_0_0 -> search7_0 in the cache */

/* replace18_0_1 -> search18_1 in the cache */
static const nir_search_expression replace18_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_ilt32,
   { &search7_0.value, &search18_1.value },
   NULL,
};

/* replace18_1 -> search7_0 in the cache */

/* replace18_2 -> search18_1 in the cache */
static const nir_search_expression replace18 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b32csel,
   { &replace18_0.value, &search7_0.value, &search18_1.value },
   NULL,
};

   /* search19_0 -> search7_0 in the cache */

/* search19_1 -> search18_1 in the cache */
static const nir_search_expression search19 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 1,
   nir_op_imax,
   { &search7_0.value, &search18_1.value },
   NULL,
};

   /* replace19_0_0 -> search18_1 in the cache */

/* replace19_0_1 -> search7_0 in the cache */
static const nir_search_expression replace19_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_ilt32,
   { &search18_1.value, &search7_0.value },
   NULL,
};

/* replace19_1 -> search7_0 in the cache */

/* replace19_2 -> search18_1 in the cache */
static const nir_search_expression replace19 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b32csel,
   { &replace19_0.value, &search7_0.value, &search18_1.value },
   NULL,
};

   /* search20_0 -> search7_0 in the cache */

/* search20_1 -> search18_1 in the cache */
static const nir_search_expression search20 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 1,
   nir_op_umin,
   { &search7_0.value, &search18_1.value },
   NULL,
};

   /* replace20_0_0 -> search7_0 in the cache */

/* replace20_0_1 -> search18_1 in the cache */
static const nir_search_expression replace20_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_ult32,
   { &search7_0.value, &search18_1.value },
   NULL,
};

/* replace20_1 -> search7_0 in the cache */

/* replace20_2 -> search18_1 in the cache */
static const nir_search_expression replace20 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b32csel,
   { &replace20_0.value, &search7_0.value, &search18_1.value },
   NULL,
};

   /* search21_0 -> search7_0 in the cache */

/* search21_1 -> search18_1 in the cache */
static const nir_search_expression search21 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 1,
   nir_op_umax,
   { &search7_0.value, &search18_1.value },
   NULL,
};

   /* replace21_0_0 -> search18_1 in the cache */

/* replace21_0_1 -> search7_0 in the cache */
static const nir_search_expression replace21_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_ult32,
   { &search18_1.value, &search7_0.value },
   NULL,
};

/* replace21_1 -> search7_0 in the cache */

/* replace21_2 -> search18_1 in the cache */
static const nir_search_expression replace21 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b32csel,
   { &replace21_0.value, &search7_0.value, &search18_1.value },
   NULL,
};

   /* search22_0 -> search7_0 in the cache */
static const nir_search_expression search22 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_u2u8,
   { &search7_0.value },
   NULL,
};

   /* replace22_0_0 -> search7_0 in the cache */
static const nir_search_expression replace22_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_u2u16,
   { &search7_0.value },
   NULL,
};
static const nir_search_expression replace22 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_u2u8,
   { &replace22_0.value },
   NULL,
};

   static const nir_search_variable search23_0 = {
   { nir_search_value_variable, 64 },
   0, /* a */
   false,
   nir_type_invalid,
   NULL,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};
static const nir_search_expression search23 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_u2u8,
   { &search23_0.value },
   NULL,
};

   /* replace23_0_0_0 -> search23_0 in the cache */
static const nir_search_expression replace23_0_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2u32,
   { &search23_0.value },
   NULL,
};
static const nir_search_expression replace23_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_u2u16,
   { &replace23_0_0.value },
   NULL,
};
static const nir_search_expression replace23 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_u2u8,
   { &replace23_0.value },
   NULL,
};

   /* search24_0 -> search23_0 in the cache */
static const nir_search_expression search24 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_u2u16,
   { &search23_0.value },
   NULL,
};

   /* replace24_0_0 -> search23_0 in the cache */
/* replace24_0 -> replace23_0_0 in the cache */
/* replace24 -> replace23_0 in the cache */

   /* search25_0 -> search1_0 in the cache */
static const nir_search_expression search25 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2u32,
   { &search1_0.value },
   NULL,
};

   /* replace25_0_0 -> search1_0 in the cache */
static const nir_search_expression replace25_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_u2u16,
   { &search1_0.value },
   NULL,
};
static const nir_search_expression replace25 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2u32,
   { &replace25_0.value },
   NULL,
};

   /* search26_0 -> search1_0 in the cache */
static const nir_search_expression search26 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2u64,
   { &search1_0.value },
   NULL,
};

   /* replace26_0_0_0 -> search1_0 in the cache */
/* replace26_0_0 -> replace25_0 in the cache */
/* replace26_0 -> replace25 in the cache */
static const nir_search_expression replace26 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2u64,
   { &replace25.value },
   NULL,
};

   /* search27_0 -> search4_0 in the cache */
static const nir_search_expression search27 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2u64,
   { &search4_0.value },
   NULL,
};

   /* replace27_0_0 -> search4_0 in the cache */
static const nir_search_expression replace27_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2u32,
   { &search4_0.value },
   NULL,
};
static const nir_search_expression replace27 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2u64,
   { &replace27_0.value },
   NULL,
};

   /* search28_0 -> search7_0 in the cache */
static const nir_search_expression search28 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_i2i8,
   { &search7_0.value },
   NULL,
};

   /* replace28_0_0 -> search7_0 in the cache */
static const nir_search_expression replace28_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_i2i16,
   { &search7_0.value },
   NULL,
};
static const nir_search_expression replace28 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_i2i8,
   { &replace28_0.value },
   NULL,
};

   /* search29_0 -> search23_0 in the cache */
static const nir_search_expression search29 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_i2i8,
   { &search23_0.value },
   NULL,
};

   /* replace29_0_0_0 -> search23_0 in the cache */
static const nir_search_expression replace29_0_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2i32,
   { &search23_0.value },
   NULL,
};
static const nir_search_expression replace29_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_i2i16,
   { &replace29_0_0.value },
   NULL,
};
static const nir_search_expression replace29 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_i2i8,
   { &replace29_0.value },
   NULL,
};

   /* search30_0 -> search23_0 in the cache */
static const nir_search_expression search30 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_i2i16,
   { &search23_0.value },
   NULL,
};

   /* replace30_0_0 -> search23_0 in the cache */
/* replace30_0 -> replace29_0_0 in the cache */
/* replace30 -> replace29_0 in the cache */

   /* search31_0 -> search1_0 in the cache */
static const nir_search_expression search31 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2i32,
   { &search1_0.value },
   NULL,
};

   /* replace31_0_0 -> search1_0 in the cache */
static const nir_search_expression replace31_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_i2i16,
   { &search1_0.value },
   NULL,
};
static const nir_search_expression replace31 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2i32,
   { &replace31_0.value },
   NULL,
};

   /* search32_0 -> search1_0 in the cache */
static const nir_search_expression search32 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2i64,
   { &search1_0.value },
   NULL,
};

   /* replace32_0_0_0 -> search1_0 in the cache */
/* replace32_0_0 -> replace31_0 in the cache */
/* replace32_0 -> replace31 in the cache */
static const nir_search_expression replace32 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2i64,
   { &replace31.value },
   NULL,
};

   /* search33_0 -> search4_0 in the cache */
static const nir_search_expression search33 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2i64,
   { &search4_0.value },
   NULL,
};

   /* replace33_0_0 -> search4_0 in the cache */
static const nir_search_expression replace33_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2i32,
   { &search4_0.value },
   NULL,
};
static const nir_search_expression replace33 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2i64,
   { &replace33_0.value },
   NULL,
};

   /* search34_0 -> search23_0 in the cache */
static const nir_search_expression search34 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2f16,
   { &search23_0.value },
   NULL,
};

   /* replace34_0_0 -> search23_0 in the cache */
static const nir_search_expression replace34_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2f32,
   { &search23_0.value },
   NULL,
};
static const nir_search_expression replace34 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2f16,
   { &replace34_0.value },
   NULL,
};

   /* search35_0 -> search4_0 in the cache */
static const nir_search_expression search35 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2f64,
   { &search4_0.value },
   NULL,
};

   /* replace35_0_0 -> search4_0 in the cache */
static const nir_search_expression replace35_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2f32,
   { &search4_0.value },
   NULL,
};
static const nir_search_expression replace35 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2f64,
   { &replace35_0.value },
   NULL,
};

   /* search36_0 -> search1_0 in the cache */
static const nir_search_expression search36 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_i2f16,
   { &search1_0.value },
   NULL,
};

   /* replace36_0_0 -> search1_0 in the cache */
/* replace36_0 -> replace31_0 in the cache */
static const nir_search_expression replace36 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_i2f16,
   { &replace31_0.value },
   NULL,
};

   /* search37_0 -> search7_0 in the cache */
static const nir_search_expression search37 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_i2f16,
   { &search7_0.value },
   NULL,
};

   /* replace37_0_0 -> search7_0 in the cache */
static const nir_search_expression replace37_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2f32,
   { &search7_0.value },
   NULL,
};
static const nir_search_expression replace37 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2f16,
   { &replace37_0.value },
   NULL,
};

   /* search38_0 -> search23_0 in the cache */
static const nir_search_expression search38 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_i2f16,
   { &search23_0.value },
   NULL,
};

   /* replace38_0_0_0 -> search23_0 in the cache */
static const nir_search_expression replace38_0_0 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2f64,
   { &search23_0.value },
   NULL,
};
static const nir_search_expression replace38_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2f32,
   { &replace38_0_0.value },
   NULL,
};
static const nir_search_expression replace38 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2f16,
   { &replace38_0.value },
   NULL,
};

   /* search39_0 -> search1_0 in the cache */
static const nir_search_expression search39 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2f32,
   { &search1_0.value },
   NULL,
};

   /* replace39_0_0_0 -> search1_0 in the cache */
/* replace39_0_0 -> replace31_0 in the cache */
/* replace39_0 -> replace31 in the cache */
static const nir_search_expression replace39 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2f32,
   { &replace31.value },
   NULL,
};

   /* search40_0 -> search4_0 in the cache */
static const nir_search_expression search40 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2f32,
   { &search4_0.value },
   NULL,
};

   /* replace40_0_0 -> search4_0 in the cache */
/* replace40_0 -> replace33_0 in the cache */
static const nir_search_expression replace40 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2f32,
   { &replace33_0.value },
   NULL,
};

   /* search41_0 -> search23_0 in the cache */
static const nir_search_expression search41 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2f32,
   { &search23_0.value },
   NULL,
};

   /* replace41_0_0 -> search23_0 in the cache */
/* replace41_0 -> replace38_0_0 in the cache */
/* replace41 -> replace38_0 in the cache */

   /* search42_0 -> search1_0 in the cache */
static const nir_search_expression search42 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2f64,
   { &search1_0.value },
   NULL,
};

   /* replace42_0_0_0_0 -> search1_0 in the cache */
/* replace42_0_0_0 -> replace31_0 in the cache */
/* replace42_0_0 -> replace31 in the cache */
/* replace42_0 -> replace32 in the cache */
static const nir_search_expression replace42 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2f64,
   { &replace32.value },
   NULL,
};

   /* search43_0 -> search4_0 in the cache */
static const nir_search_expression search43 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2f64,
   { &search4_0.value },
   NULL,
};

   /* replace43_0_0_0 -> search4_0 in the cache */
/* replace43_0_0 -> replace33_0 in the cache */
/* replace43_0 -> replace33 in the cache */
static const nir_search_expression replace43 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2f64,
   { &replace33.value },
   NULL,
};

   /* search44_0 -> search7_0 in the cache */
static const nir_search_expression search44 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2f64,
   { &search7_0.value },
   NULL,
};

   /* replace44_0_0 -> search7_0 in the cache */
static const nir_search_expression replace44_0 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2i64,
   { &search7_0.value },
   NULL,
};
static const nir_search_expression replace44 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_i2f64,
   { &replace44_0.value },
   NULL,
};

   /* search45_0 -> search1_0 in the cache */
static const nir_search_expression search45 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_u2f16,
   { &search1_0.value },
   NULL,
};

   /* replace45_0_0 -> search1_0 in the cache */
/* replace45_0 -> replace25_0 in the cache */
static const nir_search_expression replace45 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_u2f16,
   { &replace25_0.value },
   NULL,
};

   /* search46_0 -> search7_0 in the cache */
static const nir_search_expression search46 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_u2f16,
   { &search7_0.value },
   NULL,
};

   /* replace46_0_0 -> search7_0 in the cache */
static const nir_search_expression replace46_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2f32,
   { &search7_0.value },
   NULL,
};
static const nir_search_expression replace46 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2f16,
   { &replace46_0.value },
   NULL,
};

   /* search47_0 -> search23_0 in the cache */
static const nir_search_expression search47 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_u2f16,
   { &search23_0.value },
   NULL,
};

   /* replace47_0_0_0 -> search23_0 in the cache */
static const nir_search_expression replace47_0_0 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2f64,
   { &search23_0.value },
   NULL,
};
static const nir_search_expression replace47_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2f32,
   { &replace47_0_0.value },
   NULL,
};
static const nir_search_expression replace47 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2f16,
   { &replace47_0.value },
   NULL,
};

   /* search48_0 -> search1_0 in the cache */
static const nir_search_expression search48 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2f32,
   { &search1_0.value },
   NULL,
};

   /* replace48_0_0_0 -> search1_0 in the cache */
/* replace48_0_0 -> replace25_0 in the cache */
/* replace48_0 -> replace25 in the cache */
static const nir_search_expression replace48 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2f32,
   { &replace25.value },
   NULL,
};

   /* search49_0 -> search4_0 in the cache */
static const nir_search_expression search49 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2f32,
   { &search4_0.value },
   NULL,
};

   /* replace49_0_0 -> search4_0 in the cache */
/* replace49_0 -> replace27_0 in the cache */
static const nir_search_expression replace49 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2f32,
   { &replace27_0.value },
   NULL,
};

   /* search50_0 -> search23_0 in the cache */
static const nir_search_expression search50 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2f32,
   { &search23_0.value },
   NULL,
};

   /* replace50_0_0 -> search23_0 in the cache */
/* replace50_0 -> replace47_0_0 in the cache */
/* replace50 -> replace47_0 in the cache */

   /* search51_0 -> search1_0 in the cache */
static const nir_search_expression search51 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2f64,
   { &search1_0.value },
   NULL,
};

   /* replace51_0_0_0_0 -> search1_0 in the cache */
/* replace51_0_0_0 -> replace25_0 in the cache */
/* replace51_0_0 -> replace25 in the cache */
/* replace51_0 -> replace26 in the cache */
static const nir_search_expression replace51 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2f64,
   { &replace26.value },
   NULL,
};

   /* search52_0 -> search4_0 in the cache */
static const nir_search_expression search52 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2f64,
   { &search4_0.value },
   NULL,
};

   /* replace52_0_0_0 -> search4_0 in the cache */
/* replace52_0_0 -> replace27_0 in the cache */
/* replace52_0 -> replace27 in the cache */
static const nir_search_expression replace52 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2f64,
   { &replace27.value },
   NULL,
};

   /* search53_0 -> search7_0 in the cache */
static const nir_search_expression search53 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2f64,
   { &search7_0.value },
   NULL,
};

   /* replace53_0_0 -> search7_0 in the cache */
static const nir_search_expression replace53_0 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2u64,
   { &search7_0.value },
   NULL,
};
static const nir_search_expression replace53 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_u2f64,
   { &replace53_0.value },
   NULL,
};

   /* search54_0 -> search4_0 in the cache */
static const nir_search_expression search54 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_f2i8,
   { &search4_0.value },
   NULL,
};

   /* replace54_0_0 -> search4_0 in the cache */
static const nir_search_expression replace54_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2i16,
   { &search4_0.value },
   NULL,
};
static const nir_search_expression replace54 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_i2i8,
   { &replace54_0.value },
   NULL,
};

   /* search55_0 -> search7_0 in the cache */
static const nir_search_expression search55 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_f2i8,
   { &search7_0.value },
   NULL,
};

   /* replace55_0_0_0 -> search7_0 in the cache */
static const nir_search_expression replace55_0_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2i32,
   { &search7_0.value },
   NULL,
};
static const nir_search_expression replace55_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_i2i16,
   { &replace55_0_0.value },
   NULL,
};
static const nir_search_expression replace55 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_i2i8,
   { &replace55_0.value },
   NULL,
};

   /* search56_0 -> search23_0 in the cache */
static const nir_search_expression search56 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_f2i8,
   { &search23_0.value },
   NULL,
};

   /* replace56_0_0_0_0 -> search23_0 in the cache */
static const nir_search_expression replace56_0_0_0 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2i64,
   { &search23_0.value },
   NULL,
};
static const nir_search_expression replace56_0_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2i32,
   { &replace56_0_0_0.value },
   NULL,
};
static const nir_search_expression replace56_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_i2i16,
   { &replace56_0_0.value },
   NULL,
};
static const nir_search_expression replace56 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_i2i8,
   { &replace56_0.value },
   NULL,
};

   /* search57_0 -> search7_0 in the cache */
static const nir_search_expression search57 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2i16,
   { &search7_0.value },
   NULL,
};

   /* replace57_0_0 -> search7_0 in the cache */
/* replace57_0 -> replace55_0_0 in the cache */
/* replace57 -> replace55_0 in the cache */

   /* search58_0 -> search23_0 in the cache */
static const nir_search_expression search58 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2i16,
   { &search23_0.value },
   NULL,
};

   /* replace58_0_0_0 -> search23_0 in the cache */
/* replace58_0_0 -> replace56_0_0_0 in the cache */
/* replace58_0 -> replace56_0_0 in the cache */
/* replace58 -> replace56_0 in the cache */

   /* search59_0 -> search4_0 in the cache */
static const nir_search_expression search59 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2i32,
   { &search4_0.value },
   NULL,
};

   /* replace59_0_0 -> search4_0 in the cache */
/* replace59_0 -> replace35_0 in the cache */
static const nir_search_expression replace59 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2i32,
   { &replace35_0.value },
   NULL,
};

   /* search60_0 -> search23_0 in the cache */
static const nir_search_expression search60 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2i32,
   { &search23_0.value },
   NULL,
};

   /* replace60_0_0 -> search23_0 in the cache */
/* replace60_0 -> replace56_0_0_0 in the cache */
/* replace60 -> replace56_0_0 in the cache */

   /* search61_0 -> search4_0 in the cache */
static const nir_search_expression search61 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2i64,
   { &search4_0.value },
   NULL,
};

   /* replace61_0_0_0 -> search4_0 in the cache */
/* replace61_0_0 -> replace35_0 in the cache */
/* replace61_0 -> replace35 in the cache */
static const nir_search_expression replace61 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2i64,
   { &replace35.value },
   NULL,
};

   /* search62_0 -> search7_0 in the cache */
static const nir_search_expression search62 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2i64,
   { &search7_0.value },
   NULL,
};

   /* replace62_0_0 -> search7_0 in the cache */
static const nir_search_expression replace62_0 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2f64,
   { &search7_0.value },
   NULL,
};
static const nir_search_expression replace62 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2i64,
   { &replace62_0.value },
   NULL,
};

   /* search63_0 -> search4_0 in the cache */
static const nir_search_expression search63 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_f2u8,
   { &search4_0.value },
   NULL,
};

   /* replace63_0_0 -> search4_0 in the cache */
static const nir_search_expression replace63_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2u16,
   { &search4_0.value },
   NULL,
};
static const nir_search_expression replace63 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_u2u8,
   { &replace63_0.value },
   NULL,
};

   /* search64_0 -> search7_0 in the cache */
static const nir_search_expression search64 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_f2u8,
   { &search7_0.value },
   NULL,
};

   /* replace64_0_0_0 -> search7_0 in the cache */
static const nir_search_expression replace64_0_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2u32,
   { &search7_0.value },
   NULL,
};
static const nir_search_expression replace64_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_u2u16,
   { &replace64_0_0.value },
   NULL,
};
static const nir_search_expression replace64 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_u2u8,
   { &replace64_0.value },
   NULL,
};

   /* search65_0 -> search23_0 in the cache */
static const nir_search_expression search65 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_f2u8,
   { &search23_0.value },
   NULL,
};

   /* replace65_0_0_0_0 -> search23_0 in the cache */
static const nir_search_expression replace65_0_0_0 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2u64,
   { &search23_0.value },
   NULL,
};
static const nir_search_expression replace65_0_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_u2u32,
   { &replace65_0_0_0.value },
   NULL,
};
static const nir_search_expression replace65_0 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_u2u16,
   { &replace65_0_0.value },
   NULL,
};
static const nir_search_expression replace65 = {
   { nir_search_value_expression, 8 },
   false, false,
   -1, 0,
   nir_op_u2u8,
   { &replace65_0.value },
   NULL,
};

   /* search66_0 -> search7_0 in the cache */
static const nir_search_expression search66 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2u16,
   { &search7_0.value },
   NULL,
};

   /* replace66_0_0 -> search7_0 in the cache */
/* replace66_0 -> replace64_0_0 in the cache */
/* replace66 -> replace64_0 in the cache */

   /* search67_0 -> search23_0 in the cache */
static const nir_search_expression search67 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2u16,
   { &search23_0.value },
   NULL,
};

   /* replace67_0_0_0 -> search23_0 in the cache */
/* replace67_0_0 -> replace65_0_0_0 in the cache */
/* replace67_0 -> replace65_0_0 in the cache */
/* replace67 -> replace65_0 in the cache */

   /* search68_0 -> search4_0 in the cache */
static const nir_search_expression search68 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2u32,
   { &search4_0.value },
   NULL,
};

   /* replace68_0_0 -> search4_0 in the cache */
/* replace68_0 -> replace35_0 in the cache */
static const nir_search_expression replace68 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2u32,
   { &replace35_0.value },
   NULL,
};

   /* search69_0 -> search23_0 in the cache */
static const nir_search_expression search69 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2u32,
   { &search23_0.value },
   NULL,
};

   /* replace69_0_0 -> search23_0 in the cache */
/* replace69_0 -> replace65_0_0_0 in the cache */
/* replace69 -> replace65_0_0 in the cache */

   /* search70_0 -> search4_0 in the cache */
static const nir_search_expression search70 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2u64,
   { &search4_0.value },
   NULL,
};

   /* replace70_0_0_0 -> search4_0 in the cache */
/* replace70_0_0 -> replace35_0 in the cache */
/* replace70_0 -> replace35 in the cache */
static const nir_search_expression replace70 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2u64,
   { &replace35.value },
   NULL,
};

   /* search71_0 -> search7_0 in the cache */
static const nir_search_expression search71 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2u64,
   { &search7_0.value },
   NULL,
};

   /* replace71_0_0 -> search7_0 in the cache */
/* replace71_0 -> replace62_0 in the cache */
static const nir_search_expression replace71 = {
   { nir_search_value_expression, 64 },
   false, false,
   -1, 0,
   nir_op_f2u64,
   { &replace62_0.value },
   NULL,
};

   /* search72_0 -> search4_0 in the cache */
static const nir_search_expression search72 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_fexp2,
   { &search4_0.value },
   NULL,
};

   /* replace72_0_0_0 -> search4_0 in the cache */
/* replace72_0_0 -> replace35_0 in the cache */
static const nir_search_expression replace72_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_fexp2,
   { &replace35_0.value },
   NULL,
};
static const nir_search_expression replace72 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2f16,
   { &replace72_0.value },
   NULL,
};

   /* search73_0 -> search4_0 in the cache */
static const nir_search_expression search73 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_flog2,
   { &search4_0.value },
   NULL,
};

   /* replace73_0_0_0 -> search4_0 in the cache */
/* replace73_0_0 -> replace35_0 in the cache */
static const nir_search_expression replace73_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_flog2,
   { &replace35_0.value },
   NULL,
};
static const nir_search_expression replace73 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2f16,
   { &replace73_0.value },
   NULL,
};

   /* search74_0 -> search4_0 in the cache */
static const nir_search_expression search74 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_fsin,
   { &search4_0.value },
   NULL,
};

   /* replace74_0_0_0 -> search4_0 in the cache */
/* replace74_0_0 -> replace35_0 in the cache */
static const nir_search_expression replace74_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_fsin,
   { &replace35_0.value },
   NULL,
};
static const nir_search_expression replace74 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2f16,
   { &replace74_0.value },
   NULL,
};

   /* search75_0 -> search4_0 in the cache */
static const nir_search_expression search75 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_fcos,
   { &search4_0.value },
   NULL,
};

   /* replace75_0_0_0 -> search4_0 in the cache */
/* replace75_0_0 -> replace35_0 in the cache */
static const nir_search_expression replace75_0 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_fcos,
   { &replace35_0.value },
   NULL,
};
static const nir_search_expression replace75 = {
   { nir_search_value_expression, 16 },
   false, false,
   -1, 0,
   nir_op_f2f16,
   { &replace75_0.value },
   NULL,
};

   /* search76_0 -> search0_0 in the cache */
static const nir_search_expression search76 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_f2b32,
   { &search0_0.value },
   NULL,
};

   /* replace76_0 -> search0_0 in the cache */

static const nir_search_constant replace76_1 = {
   { nir_search_value_constant, -1 },
   nir_type_float, { 0x0 /* 0.0 */ },
};
static const nir_search_expression replace76 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 1,
   nir_op_fneu32,
   { &search0_0.value, &replace76_1.value },
   NULL,
};

   /* search77_0 -> search0_0 in the cache */
static const nir_search_expression search77 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_i2b32,
   { &search0_0.value },
   NULL,
};

   /* replace77_0 -> search0_0 in the cache */

/* replace77_1 -> replace0_0 in the cache */
static const nir_search_expression replace77 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 1,
   nir_op_ine32,
   { &search0_0.value, &replace0_0.value },
   NULL,
};

   /* search78_0 -> search7_0 in the cache */
static const nir_search_expression search78 = {
   { nir_search_value_expression, 32 },
   false, false,
   -1, 0,
   nir_op_b2i32,
   { &search7_0.value },
   NULL,
};

   /* replace78_0 -> search7_0 in the cache */

static const nir_search_constant replace78_1 = {
   { nir_search_value_constant, 32 },
   nir_type_int, { 0x1 /* 1 */ },
};
static const nir_search_expression replace78 = {
   { nir_search_value_expression, 32 },
   false, false,
   0, 1,
   nir_op_iand,
   { &search7_0.value, &replace78_1.value },
   NULL,
};


static const struct transform bifrost_nir_lower_algebraic_late_state2_xforms[] = {
  { &search0, &replace0.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state3_xforms[] = {
  { &search1, &replace1.value, 0 },
  { &search2, &replace2.value, 0 },
  { &search3, &replace3.value, 0 },
  { &search4, &replace4.value, 0 },
  { &search5, &replace5.value, 0 },
  { &search6, &replace6.value, 0 },
  { &search7, &replace7.value, 0 },
  { &search8, &replace8.value, 0 },
  { &search9, &replace9.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state4_xforms[] = {
  { &search10, &replace10.value, 0 },
  { &search14, &replace14.value, 0 },
  { &search18, &replace18.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state5_xforms[] = {
  { &search11, &replace11.value, 0 },
  { &search15, &replace15.value, 0 },
  { &search19, &replace19.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state6_xforms[] = {
  { &search12, &replace12.value, 0 },
  { &search16, &replace16.value, 0 },
  { &search20, &replace20.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state7_xforms[] = {
  { &search13, &replace13.value, 0 },
  { &search17, &replace17.value, 0 },
  { &search21, &replace21.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state8_xforms[] = {
  { &search22, &replace22.value, 0 },
  { &search23, &replace23.value, 0 },
  { &search24, &replace23_0.value, 0 },
  { &search25, &replace25.value, 0 },
  { &search26, &replace26.value, 0 },
  { &search27, &replace27.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state9_xforms[] = {
  { &search28, &replace28.value, 0 },
  { &search29, &replace29.value, 0 },
  { &search30, &replace29_0.value, 0 },
  { &search31, &replace31.value, 0 },
  { &search32, &replace32.value, 0 },
  { &search33, &replace33.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state10_xforms[] = {
  { &search34, &replace34.value, 0 },
  { &search35, &replace35.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state11_xforms[] = {
  { &search36, &replace36.value, 0 },
  { &search37, &replace37.value, 0 },
  { &search38, &replace38.value, 0 },
  { &search39, &replace39.value, 0 },
  { &search40, &replace40.value, 0 },
  { &search41, &replace38_0.value, 0 },
  { &search42, &replace42.value, 0 },
  { &search43, &replace43.value, 0 },
  { &search44, &replace44.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state12_xforms[] = {
  { &search45, &replace45.value, 0 },
  { &search46, &replace46.value, 0 },
  { &search47, &replace47.value, 0 },
  { &search48, &replace48.value, 0 },
  { &search49, &replace49.value, 0 },
  { &search50, &replace47_0.value, 0 },
  { &search51, &replace51.value, 0 },
  { &search52, &replace52.value, 0 },
  { &search53, &replace53.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state13_xforms[] = {
  { &search54, &replace54.value, 0 },
  { &search55, &replace55.value, 0 },
  { &search56, &replace56.value, 0 },
  { &search57, &replace55_0.value, 0 },
  { &search58, &replace56_0.value, 0 },
  { &search59, &replace59.value, 0 },
  { &search60, &replace56_0_0.value, 0 },
  { &search61, &replace61.value, 0 },
  { &search62, &replace62.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state14_xforms[] = {
  { &search63, &replace63.value, 0 },
  { &search64, &replace64.value, 0 },
  { &search65, &replace65.value, 0 },
  { &search66, &replace64_0.value, 0 },
  { &search67, &replace65_0.value, 0 },
  { &search68, &replace68.value, 0 },
  { &search69, &replace65_0_0.value, 0 },
  { &search70, &replace70.value, 0 },
  { &search71, &replace71.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state15_xforms[] = {
  { &search72, &replace72.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state16_xforms[] = {
  { &search73, &replace73.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state17_xforms[] = {
  { &search74, &replace74.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state18_xforms[] = {
  { &search75, &replace75.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state19_xforms[] = {
  { &search76, &replace76.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state20_xforms[] = {
  { &search77, &replace77.value, 0 },
};
static const struct transform bifrost_nir_lower_algebraic_late_state21_xforms[] = {
  { &search78, &replace78.value, 0 },
};

static const struct per_op_table bifrost_nir_lower_algebraic_late_table[nir_num_search_ops] = {
   [nir_op_ineg] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         2,
      },
   },
   [nir_search_op_b2f] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         3,
      },
   },
   [nir_op_imin] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         4,
      },
   },
   [nir_op_imax] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         5,
      },
   },
   [nir_op_umin] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         6,
      },
   },
   [nir_op_umax] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         7,
      },
   },
   [nir_search_op_u2u] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         8,
      },
   },
   [nir_search_op_i2i] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         9,
      },
   },
   [nir_search_op_f2f] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         10,
      },
   },
   [nir_search_op_i2f] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         11,
      },
   },
   [nir_search_op_u2f] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         12,
      },
   },
   [nir_search_op_f2i] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         13,
      },
   },
   [nir_search_op_f2u] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         14,
      },
   },
   [nir_op_fexp2] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         15,
      },
   },
   [nir_op_flog2] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         16,
      },
   },
   [nir_op_fsin] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         17,
      },
   },
   [nir_op_fcos] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         18,
      },
   },
   [nir_search_op_f2b] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         19,
      },
   },
   [nir_search_op_i2b] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         20,
      },
   },
   [nir_search_op_b2i] = {
      .filter = (uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 1,
      .table = (uint16_t []) {
      
         21,
      },
   },
};

const struct transform *bifrost_nir_lower_algebraic_late_transforms[] = {
   NULL,
   NULL,
   bifrost_nir_lower_algebraic_late_state2_xforms,
   bifrost_nir_lower_algebraic_late_state3_xforms,
   bifrost_nir_lower_algebraic_late_state4_xforms,
   bifrost_nir_lower_algebraic_late_state5_xforms,
   bifrost_nir_lower_algebraic_late_state6_xforms,
   bifrost_nir_lower_algebraic_late_state7_xforms,
   bifrost_nir_lower_algebraic_late_state8_xforms,
   bifrost_nir_lower_algebraic_late_state9_xforms,
   bifrost_nir_lower_algebraic_late_state10_xforms,
   bifrost_nir_lower_algebraic_late_state11_xforms,
   bifrost_nir_lower_algebraic_late_state12_xforms,
   bifrost_nir_lower_algebraic_late_state13_xforms,
   bifrost_nir_lower_algebraic_late_state14_xforms,
   bifrost_nir_lower_algebraic_late_state15_xforms,
   bifrost_nir_lower_algebraic_late_state16_xforms,
   bifrost_nir_lower_algebraic_late_state17_xforms,
   bifrost_nir_lower_algebraic_late_state18_xforms,
   bifrost_nir_lower_algebraic_late_state19_xforms,
   bifrost_nir_lower_algebraic_late_state20_xforms,
   bifrost_nir_lower_algebraic_late_state21_xforms,
};

const uint16_t bifrost_nir_lower_algebraic_late_transform_counts[] = {
   0,
   0,
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state2_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state3_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state4_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state5_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state6_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state7_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state8_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state9_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state10_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state11_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state12_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state13_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state14_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state15_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state16_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state17_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state18_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state19_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state20_xforms),
   (uint16_t)ARRAY_SIZE(bifrost_nir_lower_algebraic_late_state21_xforms),
};

bool
bifrost_nir_lower_algebraic_late(nir_shader *shader)
{
   bool progress = false;
   bool condition_flags[1];
   const nir_shader_compiler_options *options = shader->options;
   const shader_info *info = &shader->info;
   (void) options;
   (void) info;

   condition_flags[0] = true;

   nir_foreach_function(function, shader) {
      if (function->impl) {
         progress |= nir_algebraic_impl(function->impl, condition_flags,
                                        bifrost_nir_lower_algebraic_late_transforms,
                                        bifrost_nir_lower_algebraic_late_transform_counts,
                                        bifrost_nir_lower_algebraic_late_table);
      }
   }

   return progress;
}

